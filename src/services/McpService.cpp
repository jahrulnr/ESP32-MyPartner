#include "McpService.h"

#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_random.h>
#include <sys/socket.h>
#include <cstring>
#include "domain/PartnerContract.h"
#include "services/PartnerLog.h"

namespace partner {
namespace {
bool supportedVersion(const String& version) {
  return version.isEmpty() || version == "2025-06-18" || version == "2025-03-26" || version == "2024-11-05";
}
}

bool McpService::begin() {
  speechStorage_ = static_cast<uint8_t*>(heap_caps_malloc(4 * sizeof(McpSpeechJob), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  speech_ = speechStorage_
      ? xQueueCreateStatic(4, sizeof(McpSpeechJob), speechStorage_, &speechControl_)
      : xQueueCreate(4, sizeof(McpSpeechJob));
  if (!speech_) { partner::log.println("[mcp] speech queue allocation failed"); return false; }
  const char* headers[] = {"Origin", "MCP-Protocol-Version", "MCP-Session-Id", "Accept"};
  server_.collectHeaders(headers, 4);
  server_.on("/mcp", HTTP_POST, [this] { handlePost(); });
  server_.on("/mcp", HTTP_GET, [this] { handleStream(); });
  server_.on("/mcp", HTTP_DELETE, [this] {
    if (!validOrigin()) { server_.send(403, "text/plain", "Invalid Origin"); return; }
    int index = findSession(server_.header("MCP-Session-Id"));
    if (index < 0) { server_.send(404, "text/plain", "Unknown session"); return; }
    sessions_[index].stream.stop(); sessions_[index] = Session{}; server_.send(200, "text/plain", "");
  });
  server_.onNotFound([this] { server_.send(404, "text/plain", "Not found"); });
  ready_ = true;
  return true;
}
void McpService::tick() {
  if (!ready_) return;
  if (!started_ && WiFi.status() == WL_CONNECTED) {
    server_.begin(); started_ = true;
    partner::log.printf("[mcp] ready: http://%s:83/mcp\n", WiFi.localIP().toString().c_str());
  }
  if (started_) { server_.handleClient(); publishChanges(); }
}
bool McpService::validOrigin() {
  if (!server_.hasHeader("Origin")) return true;
  const String origin = server_.header("Origin");
  const String prefix = "http://" + server_.client().localIP().toString() + ":83";
  return origin == prefix;
}
int McpService::findSession(const String& id) {
  if (id.isEmpty()) return -1;
  for (int i = 0; i < 4; ++i) if (sessions_[i].id == id) return i;
  return -1;
}
int McpService::createSession() {
  for (int i = 0; i < 4; ++i) {
    auto& s = sessions_[i];
    if (!s.id.isEmpty() && (!s.stream.connected()) && millis() - s.touched > 300000) { s.stream.stop(); s = Session{}; }
    if (!s.id.isEmpty()) continue;
    char id[33];
    snprintf(id, sizeof(id), "%08lx%08lx%08lx%08lx", static_cast<unsigned long>(esp_random()), static_cast<unsigned long>(esp_random()), static_cast<unsigned long>(esp_random()), static_cast<unsigned long>(esp_random()));
    s.id = id; s.touched = millis(); return i;
  }
  return -1;
}
void McpService::handleStream() {
  if (!validOrigin()) { server_.send(403, "text/plain", "Invalid Origin"); return; }
  if (!supportedVersion(server_.header("MCP-Protocol-Version"))) { server_.send(400, "text/plain", "Unsupported protocol version"); return; }
  int index = findSession(server_.header("MCP-Session-Id"));
  if (index < 0) { server_.send(404, "text/plain", "Unknown session"); return; }
  if (server_.header("Accept").indexOf("text/event-stream") < 0) { server_.send(406, "text/plain", "Accept text/event-stream required"); return; }
  auto& s = sessions_[index];
  if (s.stream.connected()) { server_.send(409, "text/plain", "Stream already open"); return; }
  s.stream = server_.client();
  const char* header = "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: no-cache\r\nConnection: keep-alive\r\n\r\n: connected\n\n";
  if (s.stream.write(reinterpret_cast<const uint8_t*>(header), strlen(header)) != strlen(header)) { s.stream.stop(); return; }
  s.touched = s.heartbeat = millis();
}
bool McpService::readResource(const char* uri, std::string& result) {
  const auto sensor = state_.snapshot().sensor;
  JsonDocument doc;
  doc["online"] = sensor.online;
  if (!std::strcmp(uri, "partner://sensors/presence")) {
    const bool available = sensor.online && sensor.hasPresence;
    doc["available"] = available;
    if (available) { doc["detected"] = sensor.detected; if (sensor.hasDistance) doc["distance_cm"] = sensor.distanceCm; }
  } else if (!std::strcmp(uri, "partner://sensors/climate")) {
    const bool available = sensor.online && sensor.hasClimate;
    doc["available"] = available;
    if (available) { doc["temperature_c"] = sensor.temperature10 / 10.0; doc["humidity_percent"] = sensor.humidity10 / 10.0; }
  } else return false;
  result.clear(); serializeJson(doc, result); return true;
}
bool McpService::subscribeResource(const char* uri, bool enabled) {
  if (activeSession_ < 0) return false;
  const uint8_t bit = !std::strcmp(uri, "partner://sensors/presence") ? 1 : !std::strcmp(uri, "partner://sensors/climate") ? 2 : 0;
  if (!bit) return false;
  auto& mask = sessions_[activeSession_].subscriptions;
  if (enabled) mask |= bit; else mask &= ~bit;
  return true;
}
void McpService::publishChanges() {
  bool watching = false;
  for (auto& s : sessions_) if (s.subscriptions && s.stream.connected()) { watching = true; break; }
  uint8_t changed = 0;
  if (watching) {
    std::string presence, climate;
    readResource("partner://sensors/presence", presence); readResource("partner://sensors/climate", climate);
    if (!lastPresence_.empty() && lastPresence_ != presence) changed |= 1;
    if (!lastClimate_.empty() && lastClimate_ != climate) changed |= 2;
    lastPresence_ = std::move(presence); lastClimate_ = std::move(climate);
  } else { lastPresence_.clear(); lastClimate_.clear(); }
  for (auto& s : sessions_) {
    if (!s.stream.connected()) continue;
    for (uint8_t bit : {uint8_t(1), uint8_t(2)}) {
      if (!(changed & s.subscriptions & bit)) continue;
      const char* event = bit == 1 ? "event: message\ndata: {\"jsonrpc\":\"2.0\",\"method\":\"notifications/resources/updated\",\"params\":{\"uri\":\"partner://sensors/presence\"}}\n\n" : "event: message\ndata: {\"jsonrpc\":\"2.0\",\"method\":\"notifications/resources/updated\",\"params\":{\"uri\":\"partner://sensors/climate\"}}\n\n";
      const size_t length = strlen(event);
      if (send(s.stream.fd(), event, length, MSG_DONTWAIT) != static_cast<int>(length)) { s.stream.stop(); break; }
    }
    if (s.stream.connected() && millis() - s.heartbeat >= 15000) {
      const char heartbeat[] = ": keepalive\n\n";
      if (send(s.stream.fd(), heartbeat, sizeof(heartbeat)-1, MSG_DONTWAIT) != sizeof(heartbeat)-1) s.stream.stop();
      s.heartbeat = millis();
    }
  }
}
void McpService::handlePost() {
  if (!validOrigin()) { server_.send(403, "text/plain", "Invalid Origin"); return; }
  if (!supportedVersion(server_.header("MCP-Protocol-Version"))) { server_.send(400, "text/plain", "Unsupported protocol version"); return; }
  const String body = server_.arg("plain");
  if (body.length() > 4096) { server_.send(413, "text/plain", "Request too large"); return; }
  JsonDocument envelope;
  const bool init = !deserializeJson(envelope, body) && envelope["method"] == "initialize";
  activeSession_ = init ? createSession() : findSession(server_.header("MCP-Session-Id"));
  if (activeSession_ < 0) { server_.send(init ? 503 : 404, "text/plain", init ? "Session capacity reached" : "Unknown session"); return; }
  sessions_[activeSession_].touched = millis();
  std::string output;
  if (!protocol_.handle(std::string(body.c_str(), body.length()), output)) { server_.send(202, "application/json", ""); activeSession_ = -1; return; }
  if (init) {
    JsonDocument reply;
    if (!deserializeJson(reply, output) && reply["result"].is<JsonObject>()) server_.sendHeader("MCP-Session-Id", sessions_[activeSession_].id);
    else sessions_[activeSession_] = Session{};
  }
  server_.send(200, "application/json", output.c_str());
  activeSession_ = -1;
}
bool McpService::call(const char* name, JsonVariantConst args, std::string& result, std::string& error) {
  if (!std::strcmp(name, "contract")) {
    if (!args["reference"].isNull() && !args["reference"].is<const char*>()) { error = "reference must be a string"; return false; }
    const char* ref = args["reference"] | "core";
    if (!std::strcmp(ref, "core")) result = contract::core;
    else if (!std::strcmp(ref, "screens")) result = contract::screens;
    else if (!std::strcmp(ref, "sensors")) result = contract::sensors;
    else if (!std::strcmp(ref, "speech")) result = contract::speech;
    else if (!std::strcmp(ref, "reminders")) result = contract::reminders;
    else { error = "unknown reference"; return false; }
    return true;
  }
  if (!std::strcmp(name, "logs")) { result = partner::log.snapshot(); return true; }
  JsonDocument output;
  if (!std::strcmp(name, "reminder_list") || !std::strcmp(name, "reminder_create") || !std::strcmp(name, "reminder_delete")) {
    JsonObject obj = output.to<JsonObject>();
    const bool ok = !std::strcmp(name, "reminder_list") ? personal_.list(obj, error) :
                    !std::strcmp(name, "reminder_create") ? personal_.create(args, obj, error) : personal_.remove(args, obj, error);
    if (!ok) return false;
  } else if (!std::strcmp(name, "menu_list")) {
    JsonArray screens = output["screens"].to<JsonArray>();
    for (unsigned i = 0; i < 3; ++i) screens.add(screenName(static_cast<Screen>(i)));
    output["current"] = screenName(state_.snapshot().screen);
  } else if (!std::strcmp(name, "menu")) {
    Screen screen;
    if (!args["screen"].is<const char*>() || !parseScreen(args["screen"], screen)) { error = "screen must be home, clock, or sensors"; return false; }
    state_.setScreen(screen); output["screen"] = screenName(screen);
    partner::log.printf("[mcp] menu=%s\n", screenName(screen));
  } else if (!std::strcmp(name, "expression_list")) {
    JsonArray expressions = output["expressions"].to<JsonArray>();
    for (unsigned i = 0; i < 9; ++i) expressions.add(expressionName(static_cast<Expression>(i)));
    output["current"] = expressionName(state_.snapshot().expression);
  } else if (!std::strcmp(name, "expression")) {
    Expression expression;
    if (!args["choose"].is<const char*>() || !parseExpression(args["choose"], expression)) { error = "unknown expression; use expression_list"; return false; }
    state_.setExpression(expression); output["expression"] = expressionName(expression);
    partner::log.printf("[mcp] expression=%s\n", expressionName(expression));
  } else if (!std::strcmp(name, "speak")) {
    if (!args["text"].is<const char*>()) { error = "text is required"; return false; }
    const JsonString input = args["text"].as<JsonString>();
    const char* text = input.c_str();
    const size_t bytes = input.size();
    if (std::strlen(text) != bytes || !validSpeechText(text)) { error = "text must contain 1 to 255 valid Unicode characters without NUL"; return false; }
    McpSpeechJob job; std::memcpy(job.text, text, bytes + 1);
    if (!speech_ || xQueueSend(speech_, &job, 0) != pdTRUE) { error = "speech queue full or unavailable"; return false; }
    output["status"] = "queued";
    partner::log.printf("[mcp] speech queued: %s\n", text);
  } else if (!std::strcmp(name, "info")) {
    const DeviceSnapshot snapshot = state_.snapshot();
    output["screen"] = screenName(snapshot.screen);
    output["expression"] = expressionName(snapshot.expression);
    output["uptime_ms"] = millis();
    output["wifi"]["connected"] = snapshot.connected;
    if (snapshot.connected) {
      const std::string ip = WiFi.localIP().toString().c_str();
      output["wifi"]["ip"] = ip; output["wifi"]["rssi_dbm"] = WiFi.RSSI();
    }
    output["time"]["valid"] = snapshot.time.valid;
    if (snapshot.time.valid) {
      char local[24];
      snprintf(local, sizeof(local), "%04d-%02d-%02d %02d:%02d:%02d", snapshot.time.year, snapshot.time.month,
               snapshot.time.day, snapshot.time.hour, snapshot.time.minute, snapshot.time.second);
      output["time"]["epoch"] = snapshot.time.epoch; output["time"]["timezone"] = "WIB"; output["time"]["local"] = std::string(local);
    }
    output["reminder_phase"] = static_cast<unsigned>(snapshot.phase);
    output["mic_level_percent"] = audio_.microphoneLevel();
    output["speaker_ready"] = audio_.ready();
    output["heap_free_bytes"] = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    output["psram_free_bytes"] = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    auto node = output["node"].to<JsonObject>();
    node["online"] = snapshot.sensor.online;
    if (snapshot.sensor.name[0]) node["name"] = std::string(snapshot.sensor.name);
    node["climate_available"] = snapshot.sensor.online && snapshot.sensor.hasClimate;
    if (snapshot.sensor.online && snapshot.sensor.hasClimate) {
      node["temperature_c"] = snapshot.sensor.temperature10 / 10.0;
      node["humidity_percent"] = snapshot.sensor.humidity10 / 10.0;
    }
    node["presence_available"] = snapshot.sensor.online && snapshot.sensor.hasPresence;
    if (snapshot.sensor.online && snapshot.sensor.hasPresence) {
      node["detected"] = snapshot.sensor.detected;
      if (snapshot.sensor.hasDistance) node["distance_cm"] = snapshot.sensor.distanceCm;
    }
    const auto nodes = state_.nodes();
    JsonArray all = output["nodes"].to<JsonArray>();
    for (unsigned i = 0; i < nodes.count; ++i) {
      const auto& sensor = nodes.nodes[i];
      JsonObject item = all.add<JsonObject>();
      char mac[18];
      snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x", sensor.mac[0], sensor.mac[1], sensor.mac[2], sensor.mac[3], sensor.mac[4], sensor.mac[5]);
      item["mac"] = std::string(mac); item["name"] = std::string(sensor.name); item["online"] = sensor.online;
      item["last_seen_ms_ago"] = sensor.lastSeenAgeMs;
      item["climate_fresh"] = sensor.hasClimate;
      if (sensor.hasClimate) {
        item["temperature_c"] = sensor.temperature10 / 10.0;
        item["humidity_percent"] = sensor.humidity10 / 10.0;
        item["climate_age_ms"] = sensor.climateAgeMs;
      }
      item["presence_fresh"] = sensor.hasPresence;
      if (sensor.hasPresence) {
        item["detected"] = sensor.detected; item["presence_age_ms"] = sensor.presenceAgeMs;
        if (sensor.hasDistance) item["distance_cm"] = sensor.distanceCm;
      }
    }
  } else { error = "unknown tool"; return false; }
  result.clear(); serializeJson(output, result); return true;
}
}  // namespace partner
