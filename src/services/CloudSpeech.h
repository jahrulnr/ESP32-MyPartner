#pragma once

#include "I2sAudio.h"
#include "domain/Reminder.h"

namespace partner {

struct SpeechApiConfig {
  const char* baseUrl;
  const char* model;
  const char* voice;
  const char* apiKey;
};

class CloudSpeech : public ISpeechOutput {
 public:
  CloudSpeech(I2sAudio& speaker, IReminderStatus& status, const SpeechApiConfig& config)
      : speaker_(speaker), status_(status), config_(config) {}
  bool speak(const std::string& text, std::string& error) override;

 private:
  I2sAudio& speaker_;
  IReminderStatus& status_;
  SpeechApiConfig config_;
};

}  // namespace partner
