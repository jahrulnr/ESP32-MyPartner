#include "UsbMedia.h"
#include "services/PartnerLog.h"
#include <Arduino.h>
#include <cmath>

namespace partner {
UsbMedia::UsbMedia(I2sAudio& audio)
    : output_(audio), control_(usb_), camera_(usb_), audio_(usb_),
      playback_(audio_.addPlaybackStream()), capture_(audio_.addCaptureStream()) {}

bool UsbMedia::begin() {
  const bool cameraReady = camera_.begin();
  if (!playback_.addFormat({16000, 1, 2, 16}) || !capture_.addFormat({16000, 1, 2, 16})) return false;
  EspUsbDeviceConfig config;
  config.taskCoreId = 1;
  config.vid = 0x303a; config.pid = 0x4000;
  config.manufacturer = "MyMedia"; config.product = "ESP32-S3 MyMedia"; config.serialNumber = "mymedia";
  if (!usb_.begin(config)) {
    partner::log.printf("[usb] startup failed: %s\n", usb_.lastErrorName());
    return false;
  }
  partner::log.printf("[usb] camera=%d audio=ready\n", cameraReady);
  if (xTaskCreatePinnedToCore(task, "usb-media", 6144, this, 3, nullptr, 1) != pdPASS) return false;
  if (xTaskCreatePinnedToCore(cameraTask, "usb-camera", 4096, this, 2, nullptr, 0) != pdPASS) return false;
  return true;
}
void UsbMedia::publish(const int16_t* samples, size_t count) {
  if (micActive_.load()) capture_.write(samples, count * sizeof(int16_t));
}
void UsbMedia::task(void* context) { static_cast<UsbMedia*>(context)->run(); }
void UsbMedia::cameraTask(void* context) {
  auto& self = *static_cast<UsbMedia*>(context);
  for (;;) { self.camera_.poll(); vTaskDelay(pdMS_TO_TICKS(1)); }
}
void UsbMedia::run() {
  bool speakerActive = false;
  for (;;) {
    EspUsbAudioEvent event;
    while (audio_.pollEvent(event)) {
      if (event.type != EspUsbAudioEventType::StreamStateChanged) continue;
      if (event.target == EspUsbAudioEventTarget::Playback) speakerActive = event.enabled;
      if (event.target == EspUsbAudioEventTarget::Capture) micActive_.store(event.enabled);
    }
    if (speakerActive && output_.ready()) {
      int16_t samples[64];
      const size_t n = playback_.read(samples, sizeof(samples));
      if (n) {
        bool mute = false, found = false;
        int16_t volume = 0;
        for (uint8_t channel = 0; channel < 2; ++channel) {
          bool m = false; int16_t v = 0;
          if (audio_.getMute(m, EspUsbAudioDirection::Playback, channel)) mute |= m;
          if (audio_.getVolume(v, EspUsbAudioDirection::Playback, channel)) {
            if (!found || v < volume) volume = v;
            found = true;
          }
        }
        const float gain = mute ? 0.f : (!found || volume >= 0 ? 1.f : powf(10.f, volume / (256.f * 20.f)));
        for (size_t i = 0; i < n / 2; ++i) samples[i] = static_cast<int16_t>(samples[i] * gain);
        I2sAudio::Playback owner(output_, 0);
        std::string error;
        if (owner.acquired() && !output_.writeUsbPcm(reinterpret_cast<uint8_t*>(samples), n, error))
          partner::log.printf("[usb] playback: %s\n", error.c_str());
      }
    }
    control_.poll(camera_.available(), output_.ready());
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
}
