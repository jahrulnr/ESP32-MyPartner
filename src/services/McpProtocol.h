#pragma once

#include <ArduinoJson.h>
#include <cstring>
#include <string>

namespace partner {

class IMcpActions {
 public:
  virtual ~IMcpActions() = default;
  virtual bool call(const char* name, JsonVariantConst arguments, std::string& result, std::string& error) = 0;
  virtual bool readResource(const char*, std::string&) { return false; }
  virtual bool subscribeResource(const char*, bool) { return false; }
};

class McpProtocol {
 public:
  explicit McpProtocol(IMcpActions& actions) : actions_(actions) {}

  // False means a valid notification and no response is due.
  bool handle(const std::string& input, std::string& output) {
    JsonDocument request, response;
    if (deserializeJson(request, input)) return error(response, JsonVariantConst(), -32700, "Parse error", output);
    if (!request.is<JsonObject>() || std::strcmp(request["jsonrpc"] | "", "2.0") != 0 || !request["method"].is<const char*>())
      return error(response, JsonVariantConst(), -32600, "Invalid Request", output);
    JsonVariantConst id = request["id"];
    const bool hasId = !id.isUnbound();
    if (hasId && !(id.is<const char*>() || id.is<int64_t>()))
      return error(response, JsonVariantConst(), -32600, "Invalid Request", output);
    if (!hasId) { output.clear(); return false; }
    const char* method = request["method"];
    response["jsonrpc"] = "2.0";
    response["id"].set(id);
    if (std::strcmp(method, "initialize") == 0) {
      if (!request["params"].is<JsonObjectConst>() || !request["params"]["protocolVersion"].is<const char*>())
        return error(response, id, -32602, "Invalid params", output);
      const char* requested = request["params"]["protocolVersion"];
      const bool supported = std::strcmp(requested, "2025-06-18") == 0 || std::strcmp(requested, "2025-03-26") == 0 || std::strcmp(requested, "2024-11-05") == 0;
      response["result"]["protocolVersion"] = supported ? requested : "2025-06-18";
      response["result"]["capabilities"]["tools"]["listChanged"] = false;
      response["result"]["capabilities"]["resources"]["subscribe"] = true;
      response["result"]["serverInfo"]["name"] = "partner";
      response["result"]["serverInfo"]["version"] = "1.0.0";
    } else if (std::strcmp(method, "ping") == 0) {
      response["result"].to<JsonObject>();
    } else if (std::strcmp(method, "resources/list") == 0) {
      auto resources = response["result"]["resources"].to<JsonArray>();
      for (const char* uri : {"partner://sensors/presence", "partner://sensors/climate"}) {
        auto resource = resources.add<JsonObject>(); resource["uri"] = uri;
        resource["name"] = std::strcmp(uri, "partner://sensors/presence") == 0 ? "Presence" : "Climate";
        resource["mimeType"] = "application/json";
      }
    } else if (std::strcmp(method, "resources/read") == 0 || std::strcmp(method, "resources/subscribe") == 0 || std::strcmp(method, "resources/unsubscribe") == 0) {
      if (!request["params"].is<JsonObjectConst>() || !request["params"]["uri"].is<const char*>()) return error(response, id, -32602, "Resource URI required", output);
      const char* uri = request["params"]["uri"];
      if (std::strcmp(method, "resources/read") == 0) {
        std::string contents;
        if (!actions_.readResource(uri, contents)) return error(response, id, -32002, "Resource not found", output);
        auto item = response["result"]["contents"].add<JsonObject>();
        item["uri"] = uri; item["mimeType"] = "application/json"; item["text"] = contents;
      } else {
        if (!actions_.subscribeResource(uri, std::strcmp(method, "resources/subscribe") == 0)) return error(response, id, -32002, "Resource not found or session unavailable", output);
        response["result"].to<JsonObject>();
      }
    } else if (std::strcmp(method, "tools/list") == 0) {
      JsonDocument catalog;
      if (deserializeJson(catalog, toolsJson())) return error(response, id, -32603, "Internal error", output);
      response["result"]["tools"].set(catalog.as<JsonArrayConst>());
    } else if (std::strcmp(method, "tools/call") == 0) {
      JsonVariantConst params = request["params"];
      JsonVariantConst arguments = params["arguments"];
      if (!params.is<JsonObjectConst>() || !params["name"].is<const char*>() ||
          (!arguments.isUnbound() && !arguments.is<JsonObjectConst>()))
        return error(response, id, -32602, "Invalid params", output);
      const char* name = params["name"];
      if (!knownTool(name)) return error(response, id, -32602, "Unknown tool", output);
      std::string content, detail;
      const bool ok = actions_.call(name, arguments, content, detail);
      JsonObject block = response["result"]["content"].add<JsonObject>();
      block["type"] = "text";
      block["text"] = ok ? content : detail;
      if (!ok) response["result"]["isError"] = true;
    } else return error(response, id, -32601, "Method not found", output);
    output.clear(); serializeJson(response, output); return true;
  }
 private:
  static bool error(JsonDocument& response, JsonVariantConst id, int code, const char* message, std::string& output) {
    response.clear(); response["jsonrpc"] = "2.0";
    if (id.isNull()) response["id"] = nullptr; else response["id"].set(id);
    response["error"]["code"] = code; response["error"]["message"] = message;
    output.clear(); serializeJson(response, output); return true;
  }
  static bool knownTool(const char* name) {
    for (const char* candidate : {"contract", "menu", "menu_list", "info", "speak", "expression", "expression_list", "logs", "reminder_list", "reminder_create", "reminder_delete"})
      if (std::strcmp(name, candidate) == 0) return true;
    return false;
  }
  static const char* toolsJson() {
    return R"json([
{"name":"contract","description":"Read Partner operating guidance; optionally select a reference.","inputSchema":{"type":"object","properties":{"reference":{"type":"string","enum":["core","screens","sensors","speech","reminders"]}}}},
{"name":"menu_list","description":"List available OLED screens.","inputSchema":{"type":"object","properties":{}}},
{"name":"menu","description":"Select an OLED screen.","inputSchema":{"type":"object","properties":{"screen":{"type":"string","enum":["home","clock","sensors"]}},"required":["screen"]}},
{"name":"info","description":"Read Partner status and available node sensor data.","inputSchema":{"type":"object","properties":{}}},
{"name":"speak","description":"Queue text for speech; playback happens asynchronously.","inputSchema":{"type":"object","properties":{"text":{"type":"string","minLength":1,"maxLength":255}},"required":["text"]}},
{"name":"expression_list","description":"List available face expressions.","inputSchema":{"type":"object","properties":{}}},
{"name":"expression","description":"Set face expression; use auto to restore automatic behavior.","inputSchema":{"type":"object","properties":{"choose":{"type":"string","enum":["auto","normal","happy","sad","worried","focused","surprised","angry","sleepy"]}},"required":["choose"]}},
{"name":"logs","description":"Read the retained application and ESP-IDF logs from this boot.","inputSchema":{"type":"object","properties":{}}},
{"name":"reminder_list","description":"List persistent personal reminders and delivery status.","inputSchema":{"type":"object","properties":{}}},
{"name":"reminder_create","description":"Create a personal reminder; text must be 1 to 160 UTF-8 bytes. Specify exactly one of due_epoch or after_seconds.","inputSchema":{"type":"object","properties":{"text":{"type":"string","minLength":1,"maxLength":160},"due_epoch":{"type":"integer","minimum":1,"description":"Unix epoch seconds"},"after_seconds":{"type":"integer","minimum":0,"maximum":2147483647,"description":"Seconds from current synchronized time"}},"required":["text"],"oneOf":[{"required":["due_epoch"],"not":{"required":["after_seconds"]}},{"required":["after_seconds"],"not":{"required":["due_epoch"]}}]}},
{"name":"reminder_delete","description":"Delete a reminder that is not being delivered.","inputSchema":{"type":"object","properties":{"id":{"type":"integer","minimum":1,"maximum":4294967295}},"required":["id"]}}
])json";
  }
  IMcpActions& actions_;
};
}  // namespace partner
