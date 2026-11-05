#pragma once

#include <atomic>
#include <driver/i2s_std.h>
#include <hw.h>
#if !MIC_USE_INMP441
#include <esp_adc/adc_continuous.h>
#endif
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "domain/Pcm16.h"
#include "domain/AudioActivityMeter.h"

namespace partner {

// One controller owns the shared microphone/speaker clock. All PCM at this
// boundary is mono signed 16-bit, 16 kHz; the wire uses stereo 32-bit slots.
class I2sAudio : public IPcmSink {
 public:
  bool begin(std::string& error);
  bool ready() const { return ready_.load(); }
  bool writePcm(const uint8_t* bytes, size_t size, std::string& error) override;
  bool writeUsbPcm(const uint8_t* bytes, size_t size, std::string& error);
  bool readMicrophone(int16_t* samples, size_t count, std::string& error);
  bool finish(std::string& error);
  bool suppressRecognition() const;
  uint8_t microphoneLevel() const { return micLevel_.load(); }
  AudioActivity microphoneActivity() const {
    const uint32_t value = activity_.load();
    return {static_cast<uint16_t>(value >> 16), static_cast<uint16_t>(value)};
  }

  class Playback {
   public:
    explicit Playback(I2sAudio& audio, TickType_t timeout = portMAX_DELAY);
    ~Playback();
    Playback(const Playback&) = delete;
    Playback& operator=(const Playback&) = delete;
    bool acquired() const { return acquired_; }
   private:
    I2sAudio& audio_;
    bool acquired_ = false;
  };

 private:
  bool writeWithGain(const uint8_t* bytes, size_t size, float gain, std::string& error);
  i2s_chan_handle_t tx_ = nullptr;
  i2s_chan_handle_t rx_ = nullptr;
#if !MIC_USE_INMP441
  adc_continuous_handle_t adc_ = nullptr;
  int32_t dcQ8_ = -1;
  int16_t adcSamples_[64] {};
  size_t adcCount_ = 0;
  size_t adcOffset_ = 0;
#endif
  SemaphoreHandle_t playback_ = nullptr;
  std::atomic<bool> ready_ {false};
  std::atomic<bool> playing_ {false};
  std::atomic<uint32_t> endedAt_ {0};
  std::atomic<uint8_t> micLevel_ {0};
  AudioActivityMeter meter_;
  std::atomic<uint32_t> activity_ {0};
  bool micSelected_ = false;
  unsigned micSlot_ = 0;
  uint32_t micLoggedAt_ = 0;
};

}  // namespace partner
