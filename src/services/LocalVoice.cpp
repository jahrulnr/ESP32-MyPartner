#include "LocalVoice.h"
#include "services/PartnerLog.h"

#include <Arduino.h>
#include <app_config.h>
#include <esp_heap_caps.h>
#include <algorithm>
#include "domain/SpeechSession.h"
#ifdef PARTNER_MODEL_COMPARE_PROBE
#include "../../tests/device/ModelComparisonProbe.h"
#endif

namespace partner {

bool LocalVoice::initialize(std::string& error) {
  if (!audio_.begin(error) || !greeting_.prepare(error)) return false;
  models_ = esp_srmodel_init("model");
  if (!models_) { error = "Speech model partition is missing"; return false; }
  if (!recognizer_.begin(models_, error)) return false;
#ifdef PARTNER_MODEL_COMPARE_PROBE
  compareVoiceModel(recognizer_);
#endif

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
  if (afe_->get_feed_channel_num(afeData_) != 1 ||
      afe_->get_fetch_chunksize(afeData_) != recognizer_.sampleCount()) {
    error = "AFE/MultiNet audio frame mismatch";
    return false;
  }
  input_ = static_cast<int16_t*>(heap_caps_malloc(feedSamples_ * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  greetings_ = xQueueCreate(1, sizeof(uint32_t));
  if (!input_ || !greetings_) { error = "Voice buffer allocation failed"; return false; }
  capture_.start();
  partner::log.printf("[voice] model=%s, AFE M / WakeNet OFF, feed=%d\n", recognizer_.name(), feedSamples_);
  return true;
}

void LocalVoice::cleanup() {
  capture_.cancel();
  recognizer_.end();
  if (afeData_) { afe_->destroy(afeData_); afeData_ = nullptr; }
  if (models_) { esp_srmodel_deinit(models_); models_ = nullptr; }
  if (greetings_) { vQueueDelete(greetings_); greetings_ = nullptr; }
  heap_caps_free(input_);
  input_ = nullptr;
  greeting_.clear();
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
  TaskHandle_t greetingHandle = nullptr;
  if (xTaskCreatePinnedToCore(greetingTask, "local-hi", 4096, this, 3, &greetingHandle, 1) != pdPASS ||
      xTaskCreatePinnedToCore(feedTask, "mic-afe", 6144, this, 5, &feedHandle, 0) != pdPASS) {
    if (greetingHandle) vTaskDelete(greetingHandle);
    partner::log.println("[voice] task allocation failed");
    cleanup();
    vTaskDelete(nullptr);
    return;
  }
  partner::log.println("[voice] ready: say HI PARTNER or HEY PARTNER");
  detect();
}

void LocalVoice::feedTask(void* context) { static_cast<LocalVoice*>(context)->feed(); }
void LocalVoice::greetingTask(void* context) { static_cast<LocalVoice*>(context)->greet(); }

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
  GreetingGate gate(config::greetingMinMicRms);
  SpeechSession session;
  unsigned frames = 0, speechFrames = 0, mnFrames = 0, errors = 0;
  int peak = 0, lastReturn = 0;
  uint32_t loggedAt = millis();
  for (;;) {
    auto* result = afe_->fetch_with_delay(afeData_, pdMS_TO_TICKS(250));
    if (result) {
      lastReturn = result->ret_value;
      ++frames;
      if (result->ret_value == ESP_OK) {
        if (result->vad_state == VAD_SPEECH) ++speechFrames;
        for (int i = 0; result->data && i < result->data_size / 2; ++i)
          peak = std::max(peak, std::abs(static_cast<int>(result->data[i])));
      } else ++errors;
    }
    if (millis() - loggedAt >= 5000) {
      partner::log.printf("[voice] AFE frames=%u speech=%u peak=%d ret=%d errors=%u MNframes=%u\n",
                    frames, speechFrames, peak, lastReturn, errors, mnFrames);
      frames = speechFrames = mnFrames = errors = 0;
      peak = 0;
      loggedAt = millis();
    }
    if (!result || result->ret_value != ESP_OK) continue;
    capture_.processed(result->data, result->data_size / 2);
    const auto action = session.next(result->vad_state == VAD_SPEECH, audio_.suppressRecognition());
    if (action == SpeechFrame::Suppressed) {
      recognizer_.reset();
      continue;
    }
    // Feed quiet frames too: VAD controls response eligibility, not acoustic context.
    ++mnFrames;
    const auto detected = recognizer_.detect(result->data);
    if (detected == ESP_MN_STATE_TIMEOUT) {
      const auto* commands = recognizer_.results();
      if (commands) partner::log.printf("[voice] timeout: text='%s' candidates=%d\n", commands->string, commands->num);
      session.finish();
      recognizer_.reset();
      continue;
    }
    if (detected != ESP_MN_STATE_DETECTED) continue;
    const auto* commands = recognizer_.results();
    const uint32_t now = millis();
    const auto mic = audio_.microphoneActivity();
    if (commands && commands->num) {
      partner::log.printf("[voice] candidate id=%d phrase=%d text='%s' confidence=%.3f mic_peak=%u mic_rms=%u\n",
                    commands->command_id[0], commands->phrase_id[0], commands->string, commands->prob[0],
                    mic.peak, mic.rms);
    }
    if (action == SpeechFrame::Recognize && commands && commands->num && commands->command_id[0] == 1 &&
        gate.accept(now, audio_.suppressRecognition(), commands->prob[0], mic.rms)) {
      partner::log.printf("[voice] partner greeting recognized confidence=%.3f\n", commands->prob[0]);
      xQueueOverwrite(greetings_, &now);
    } else if (action == SpeechFrame::Recognize && commands && commands->num &&
               commands->command_id[0] == 1 && mic.rms < config::greetingMinMicRms) {
      partner::log.printf("[voice] ignored: mic_rms=%u below %u\n", mic.rms, config::greetingMinMicRms);
    }
    session.finish();
    recognizer_.reset();
  }
}

void LocalVoice::greet() {
  for (;;) {
    uint32_t recognizedAt;
    if (xQueueReceive(greetings_, &recognizedAt, portMAX_DELAY) != pdTRUE) continue;
    std::string error;
    partner::log.printf("[voice] play cached Hi, dispatch=%u ms\n", millis() - recognizedAt);
    if (!greeting_.play(audio_, error)) partner::log.printf("[voice] Hi playback failed: %s\n", error.c_str());
    else partner::log.println("[voice] Hi playback complete");
  }
}

}  // namespace partner
