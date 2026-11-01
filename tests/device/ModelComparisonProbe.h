#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include <algorithm>
#include <cstring>
#include "services/SpeechRecognizer.h"

// Optional offline probe: identical pre-AFE PCM for each installed model.
inline void compareVoiceModel(partner::SpeechRecognizer& recognizer) {
  const int size = recognizer.sampleCount();
  if (size <= 0 || size > 1024) {
    Serial.printf("[compare] unsupported model frame size %d\n", size);
    return;
  }
  for (const char* path : {"/compare-drama.wav", "/compare-greeting.wav"}) {
    auto file = LittleFS.open(path, FILE_READ);
    if (!file || !file.seek(44)) {
      Serial.printf("[compare] missing %s\n", path);
      continue;
    }
    int16_t frame[1024];
    unsigned samples = 0, hits = 0;
    float maximum = 0;
    recognizer.reset();
    while (file.available()) {
      std::memset(frame, 0, size * sizeof(int16_t));
      size_t bytes = file.read(reinterpret_cast<uint8_t*>(frame), size * sizeof(int16_t));
      if (!bytes) break;
      samples += bytes / sizeof(int16_t);
      const auto state = recognizer.detect(frame);
      if (state == ESP_MN_STATE_DETECTED) {
        const auto* result = recognizer.results();
        if (result && result->num) {
          ++hits;
          maximum = std::max(maximum, result->prob[0]);
          Serial.printf("[compare] model=%s input=%s t=%.3f confidence=%.3f\n",
                        recognizer.name(), path, samples / 16000.0f, result->prob[0]);
        }
        recognizer.reset();
      } else if (state == ESP_MN_STATE_TIMEOUT) recognizer.reset();
      vTaskDelay(1);
    }
    file.close();
    Serial.printf("[compare] RESULT model=%s input=%s seconds=%.3f hits=%u max=%.3f\n",
                  recognizer.name(), path, samples / 16000.0f, hits, maximum);
  }
  recognizer.reset();
  Serial.println("[compare] COMPLETE; normal microphone processing resumes");
}
