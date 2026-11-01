#include "PicoGreeting.h"
#include "services/PartnerLog.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <picotts.h>
#include <cstring>

namespace partner {

PicoGreeting* PicoGreeting::preparing_ = nullptr;

void PicoGreeting::receive(int16_t* samples, unsigned count) {
  auto* self = preparing_;
  if (!self) return;
  if (count > capacity_ - self->count_) { self->failed_ = true; return; }
  std::memcpy(self->samples_ + self->count_, samples, count * sizeof(int16_t));
  self->count_ += count;
}
void PicoGreeting::idle() {
  if (preparing_) xSemaphoreGive(preparing_->complete_);
}
void PicoGreeting::failed() {
  if (preparing_) { preparing_->failed_ = true; xSemaphoreGive(preparing_->complete_); }
}

bool PicoGreeting::prepare(std::string& error) {
  samples_ = static_cast<int16_t*>(heap_caps_malloc(capacity_ * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  complete_ = xSemaphoreCreateBinary();
  if (!samples_ || !complete_) {
    if (complete_) vSemaphoreDelete(complete_);
    complete_ = nullptr;
    clear();
    error = "Hi PCM cache allocation failed";
    return false;
  }
  preparing_ = this;
  picotts_set_idle_notify(idle);
  picotts_set_error_notify(failed);
  heap_caps_malloc_extmem_enable(4096);
  const bool initialized = picotts_init(1, receive, 1);
  bool complete = false;
  if (initialized) {
    const char text[] = "Hi.";
    picotts_add(text, sizeof(text));
    complete = xSemaphoreTake(complete_, pdMS_TO_TICKS(10000)) == pdTRUE;
  }
  picotts_shutdown();
  picotts_set_idle_notify(nullptr);
  picotts_set_error_notify(nullptr);
  preparing_ = nullptr;
  vSemaphoreDelete(complete_);
  complete_ = nullptr;
  if (!initialized || !complete || failed_ || !count_) {
    heap_caps_free(samples_);
    samples_ = nullptr;
    error = "Unable to prepare Hi; check picoTTS model partitions";
    return false;
  }
  partner::log.printf("[voice] Hi cached: %u samples / %u ms; picoTTS shut down\n",
                static_cast<unsigned>(count_), static_cast<unsigned>(count_ * 1000 / 16000));
  return true;
}

void PicoGreeting::clear() {
  heap_caps_free(samples_);
  samples_ = nullptr;
  count_ = 0;
}

bool PicoGreeting::play(I2sAudio& audio, std::string& error) {
  if (!samples_) { error = "Hi clip is not prepared"; return false; }
  I2sAudio::Playback playback(audio);
  if (!playback.acquired()) { error = "Audio is unavailable"; return false; }
  return audio.writePcm(reinterpret_cast<const uint8_t*>(samples_), count_ * 2, error) && audio.finish(error);
}

}  // namespace partner
