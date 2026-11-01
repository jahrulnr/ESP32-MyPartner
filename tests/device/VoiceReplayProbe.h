#pragma once

#include <LittleFS.h>
#include <Arduino.h>
#include <algorithm>
#include <cstring>
#include <esp_mn_iface.h>
#include <esp_mn_speech_commands.h>
#include "domain/GreetingGate.h"

// Diagnostic only: compile with PARTNER_VOICE_REPLAY_PROBE.
// Compare raw vs AGC from the same capture and command graphs at equal threshold.
inline void replayVoiceProbe(const esp_mn_iface_t* model, model_iface_data_t* data) {
  if (model->get_samp_chunksize(data) != 512) return;
  for (bool high : {false, true}) {
    esp_mn_commands_clear();
    esp_mn_commands_add(1, "HI PARTNER");
    esp_mn_commands_add(1, "HEY PARTNER");
    if (high) esp_mn_commands_add(1, "HIGH PARTNER");
    esp_mn_commands_update();
    model->set_det_threshold(data, 0.3f);
    for (const char* path : {"/mic-baseline.wav", "/afe-boot.wav"}) {
      auto file = LittleFS.open(path, FILE_READ);
      if (!file || !file.seek(44)) {
        Serial.printf("[probe] missing %s\n", path);
        continue;
      }
      int16_t frame[512];
      model->clean(data);
      unsigned sample = 0, hits = 0;
      float maximum = 0;
      const uint32_t started = millis();
      while (file.available()) {
        std::memset(frame, 0, sizeof(frame));
        const size_t bytes = file.read(reinterpret_cast<uint8_t*>(frame), sizeof(frame));
        if (!bytes) break;
        const auto state = model->detect(data, frame);
        sample += bytes / 2;
        if (state == ESP_MN_STATE_DETECTED) {
          const auto* result = model->get_results(data);
          if (result && result->num) {
            ++hits;
            maximum = std::max(maximum, result->prob[0]);
            Serial.printf("[probe] graph=%s input=%s t=%.3f id=%d p=%.3f\n",
                          high ? "HI+HEY+HIGH" : "HI+HEY", path, sample / 16000.0f,
                          result->command_id[0], result->prob[0]);
          }
          model->clean(data);
        } else if (state == ESP_MN_STATE_TIMEOUT) model->clean(data);
        vTaskDelay(1);
      }
      file.close();
      Serial.printf("[probe] RESULT graph=%s input=%s hits=%u max=%.3f elapsed=%u ms\n",
                    high ? "HI+HEY+HIGH" : "HI+HEY", path, hits, maximum, millis() - started);
    }
  }
  esp_mn_commands_clear();
  esp_mn_commands_add(1, "HI PARTNER");
  esp_mn_commands_add(1, "HEY PARTNER");
  esp_mn_commands_update();
  model->set_det_threshold(data, partner::GreetingGate::minimumConfidence);
  model->clean(data);
  Serial.println("[probe] COMPLETE; normal microphone processing resumes");
}
