#include "LocalVoice.h"
#include "services/PartnerLog.h"

#include <Arduino.h>
#include <app_config.h>
#include <esp_heap_caps.h>
#include <algorithm>

namespace partner {

bool LocalVoice::initialize(std::string& error) {
  if (!audio_.begin(error)) return false;
  models_ = esp_srmodel_init("model");
  if (!models_) { error = "Speech model partition is missing"; return false; }
  auto* config = afe_config_init("M", models_, AFE_TYPE_SR, AFE_MODE_HIGH_PERF);
  if (!config) { error = "AFE config allocation failed"; return false; }
  config->wakenet_init = false;
  config->aec_init = false;  // No playback reference; ignore recognition while audio plays.
  config->se_init = false;
  config->vad_init = true;
  config->vad_min_speech_ms = 64;
  config->vad_min_noise_ms = 200;
  config->agc_init = true;
  config->agc_mode = AFE_AGC_MODE_WEBRTC;
  config->agc_compression_gain_db = 18;
  config->agc_target_level_dbfs = 9;
  config->fixed_first_channel = true;
  config->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;
  config->afe_perferred_core = 1;
  config->afe_perferred_priority = 5;
  config = afe_config_check(config);
  afe_config_print(config);
  afe_ = esp_afe_handle_from_config(config);
  afeData_ = afe_ ? afe_->create_from_config(config) : nullptr;
  afe_config_free(config);
  if (!afeData_) { error = "AFE init failed"; return false; }
  feedSamples_ = afe_->get_feed_chunksize(afeData_);
  if (afe_->get_feed_channel_num(afeData_) != 1) {
    error = "AFE microphone channel mismatch";
    return false;
  }
  input_ = static_cast<int16_t*>(heap_caps_malloc(feedSamples_ * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!input_) { error = "Voice buffer allocation failed"; return false; }
  capture_.start();
  partner::log.printf("[voice] AFE M / WakeNet OFF, feed=%d\n", feedSamples_);
  return true;
}

void LocalVoice::cleanup() {
  capture_.cancel();
  if (afeData_) { afe_->destroy(afeData_); afeData_ = nullptr; }
  if (models_) { esp_srmodel_deinit(models_); models_ = nullptr; }
  heap_caps_free(input_);
  input_ = nullptr;
}

void LocalVoice::run() {
  std::string error;
  if (!initialize(error)) {
    partner::log.printf("[voice] initialization failed: %s\n", error.c_str());
    cleanup();
    vTaskDelete(nullptr);
    return;
  }
  TaskHandle_t feedHandle = nullptr;
  if (xTaskCreatePinnedToCore(feedTask, "mic-afe", 6144, this, 5, &feedHandle, 0) != pdPASS) {
    partner::log.println("[voice] task allocation failed");
    cleanup();
    vTaskDelete(nullptr);
    return;
  }
  partner::log.println("[voice] microphone and AFE ready");
  detect();
}

void LocalVoice::feedTask(void* context) { static_cast<LocalVoice*>(context)->feed(); }

void LocalVoice::feed() {
  uint32_t loggedAt = 0;
  for (;;) {
    std::string error;
    if (!audio_.readMicrophone(input_, feedSamples_, error)) {
      partner::log.printf("[voice] %s\n", error.c_str());
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }
    capture_.microphone(input_, feedSamples_);
    stream_.publish(input_, feedSamples_);
    media_.publish(input_, feedSamples_);
    afe_->feed(afeData_, input_);
    if (millis() - loggedAt >= 5000) {
      int peak = 0;
      for (int i = 0; i < feedSamples_; ++i) peak = std::max(peak, std::abs(static_cast<int>(input_[i])));
      partner::log.printf("[voice] mic peak=%d feed_stack_free=%u\n", peak,
                    static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
      loggedAt = millis();
    }
  }
}

void LocalVoice::detect() {
  for (;;) {
    auto* result = afe_->fetch_with_delay(afeData_, pdMS_TO_TICKS(250));
    if (result && result->ret_value == ESP_OK && result->data)
      capture_.processed(result->data, result->data_size / 2);
  }
}

}  // namespace partner
