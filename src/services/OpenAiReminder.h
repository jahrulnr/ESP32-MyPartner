#pragma once

#include <core/ai_transport.h>
#include <app_config.h>

#include "domain/Reminder.h"
#include "domain/ReminderVariation.h"
#include "DeviceState.h"

namespace partner {

struct TextApiConfig {
  const char* baseUrl;
  const char* endpoint;
  const char* model;
  const char* apiKey;
};

class OpenAiReminder : public IReminderText {
 public:
  OpenAiReminder(ai::provider::IAiTransport& transport, DeviceState& state, const TextApiConfig& config)
      : transport_(transport), state_(state), config_(config),
        variation_(config::reminderTemperatureMin, config::reminderTemperatureMax) {}
  bool generate(const LocalTime& now, std::string& text, std::string& error) override;

 private:
  ai::provider::IAiTransport& transport_;
  DeviceState& state_;
  TextApiConfig config_;
  ReminderVariation variation_;
};

}  // namespace partner
