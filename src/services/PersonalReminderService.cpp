#include "PersonalReminderService.h"
#include "services/PartnerLog.h"

#include <ArduinoJson.h>
#include "domain/SpeechText.h"
#include <cstdlib>
#include <cstring>

namespace partner {
namespace {
constexpr char path[] = "/partner-reminders.json";
constexpr char temporary[] = "/partner-reminders.tmp";
}

bool PersonalReminderService::begin(bool filesystemReady) {
  jobs_ = xQueueCreate(1, sizeof(PersonalReminder));
  results_ = xQueueCreate(1, sizeof(PersonalReminderResult));
  if (!jobs_ || !results_ || !filesystemReady) {
    partner::log.println("[personal] unavailable: queue or filesystem");
    return false;
  }
  if (!load()) { partner::log.println("[personal] cannot load reminders; API disabled"); return false; }
  available_ = true;
  return true;
}

bool PersonalReminderService::load() {
  if (!filesystem_.exists(path)) return true;
  File file = filesystem_.open(path, "r");
  if (!file || file.size() > 8192) return false;
  JsonDocument doc;
  if (deserializeJson(doc, file) || !doc.is<JsonArray>()) return false;
  PersonalReminders loaded;
  for (JsonObjectConst entry : doc.as<JsonArrayConst>()) {
    const char* text = entry["text"] | "";
    if (std::strlen(text) >= sizeof(PersonalReminder::text)) return false;
    PersonalReminder item {};
    item.id = entry["id"] | 0u;
    item.dueEpoch = entry["due_epoch"] | int64_t(0);
    std::strcpy(item.text, text);
    if (!loaded.restore(item)) return false;
  }
  reminders_ = loaded;
  return true;
}

bool PersonalReminderService::save() {
  JsonDocument doc;
  JsonArray array = doc.to<JsonArray>();
  for (const auto& item : reminders_.all()) if (item.id) {
    JsonObject entry = array.add<JsonObject>();
    entry["id"] = item.id; entry["due_epoch"] = item.dueEpoch; entry["text"] = item.text;
  }
  File file = filesystem_.open(temporary, "w");
  if (!file) return false;
  const size_t expected = measureJson(doc);
  const size_t written = serializeJson(doc, file);
  file.flush(); file.close();
  if (written != expected) return false;
  return filesystem_.rename(temporary, path);
}

bool PersonalReminderService::list(JsonObject output, std::string& error) {
  if (!available_) { error = "reminder storage unavailable"; return false; }
  JsonArray array = output["reminders"].to<JsonArray>();
  for (const auto& item : reminders_.all()) if (item.id) {
    JsonObject entry = array.add<JsonObject>();
    entry["id"] = item.id; entry["due_epoch"] = item.dueEpoch; entry["text"] = item.text;
    entry["delivering"] = item.id == reminders_.inFlight();
  }
  return true;
}
bool PersonalReminderService::create(JsonVariantConst args, JsonObject output, std::string& error) {
  if (!available_) { error = "reminder storage unavailable"; return false; }
  if (!args["text"].is<const char*>()) { error = "text is required"; return false; }
  const JsonString input = args["text"].as<JsonString>();
  const char* text = input.c_str();
  if (input.size() != std::strlen(text) || input.size() > 160 || !validSpeechText(text)) {
    error = "text must be valid UTF-8, 1 to 160 bytes, without NUL"; return false;
  }
  const bool hasDue = !args["due_epoch"].isUnbound();
  const bool hasDelay = !args["after_seconds"].isUnbound();
  if (hasDue == hasDelay) { error = "provide exactly one of due_epoch or after_seconds"; return false; }
  int64_t due = 0;
  if (hasDelay) {
    if (!args["after_seconds"].is<int32_t>() || args["after_seconds"].as<int32_t>() < 0) { error = "invalid after_seconds"; return false; }
    const auto snapshot = state_.snapshot();
    if (!snapshot.time.valid) { error = "clock unavailable"; return false; }
    due = snapshot.time.epoch + args["after_seconds"].as<int32_t>();
  } else {
    if (!args["due_epoch"].is<int64_t>()) { error = "invalid due_epoch"; return false; }
    due = args["due_epoch"].as<int64_t>();
  }
  PersonalReminders before = reminders_;
  uint32_t id = 0;
  if (!reminders_.add(due, text, id)) { error = "invalid due time or reminder list full"; return false; }
  if (!save()) { reminders_ = before; error = "could not save reminder"; return false; }
  output["id"] = id; output["due_epoch"] = due; output["text"] = std::string(text);
  return true;
}
bool PersonalReminderService::remove(JsonVariantConst args, JsonObject output, std::string& error) {
  if (!available_) { error = "reminder storage unavailable"; return false; }
  if (!args["id"].is<uint32_t>() || !args["id"].as<uint32_t>()) { error = "invalid id"; return false; }
  PersonalReminders before = reminders_;
  if (!reminders_.remove(args["id"].as<uint32_t>())) { error = "reminder not found or delivering"; return false; }
  if (!save()) { reminders_ = before; error = "could not save reminder"; return false; }
  output["ok"] = true; return true;
}
void PersonalReminderService::complete(uint32_t id, bool delivered) {
  PersonalReminderResult result {id, delivered};
  xQueueSend(results_, &result, portMAX_DELAY);
}
void PersonalReminderService::tick() {
  if (!available_) return;
  PersonalReminderResult result;
  if (xQueueReceive(results_, &result, 0) == pdTRUE && reminders_.finish(result.id, result.delivered)) {
    if (result.delivered && !save()) partner::log.println("[personal] failed saving delivery; may repeat after reboot");
    if (!result.delivered) { lastFailure_ = millis(); retryDelay_ = true; }
    partner::log.printf("[personal] id=%u %s\n", result.id, result.delivered ? "delivered" : "deferred");
  }
  if (retryDelay_ && millis() - lastFailure_ < 10000) return;
  retryDelay_ = false;
  const auto snapshot = state_.snapshot();
  const bool present = snapshot.sensor.online && snapshot.sensor.hasPresence && snapshot.sensor.detected;
  PersonalReminder job;
  if (snapshot.time.valid && snapshot.connected && reminders_.takeDue(snapshot.time.epoch, present, job)) {
    if (xQueueSend(jobs_, &job, 0) != pdTRUE) reminders_.finish(job.id, false);
  }
}
}  // namespace partner
