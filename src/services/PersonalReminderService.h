#pragma once

#include <LittleFS.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "domain/PersonalReminders.h"
#include "DeviceState.h"

namespace partner {

struct PersonalReminderResult { uint32_t id; bool delivered; };

// Network-task owner: durable state and dispatch into the speech task.
class PersonalReminderService {
 public:
  PersonalReminderService(DeviceState& state, fs::LittleFSFS& filesystem)
      : state_(state), filesystem_(filesystem) {}
  bool begin(bool filesystemReady);
  void tick();
  bool receive(PersonalReminder& job) { return available_ && xQueueReceive(jobs_, &job, 0) == pdTRUE; }
  void complete(uint32_t id, bool delivered);
  bool list(JsonObject output, std::string& error);
  bool create(JsonVariantConst args, JsonObject output, std::string& error);
  bool remove(JsonVariantConst args, JsonObject output, std::string& error);
 private:
  bool load();
  bool save();
  DeviceState& state_;
  fs::LittleFSFS& filesystem_;
  PersonalReminders reminders_;
  QueueHandle_t jobs_ = nullptr, results_ = nullptr;
  uint32_t lastFailure_ = 0;
  bool available_ = false, retryDelay_ = false;
};

}  // namespace partner
