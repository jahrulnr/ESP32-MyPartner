#pragma once
#include <EspUsbDevice.h>
#include <atomic>
#include "Camera.h"
#include "Control.h"
#include "services/I2sAudio.h"

namespace partner {
class UsbMedia {
 public:
  explicit UsbMedia(I2sAudio& audio);
  bool begin();
  void publish(const int16_t* samples, size_t count);
 private:
  static void task(void* context);
  static void cameraTask(void* context);
  void run();
  I2sAudio& output_;
  EspUsbDevice usb_;
  ControlModule control_;
  CameraModule camera_;
  EspUsbAudioFunction audio_;
  EspUsbAudioPlaybackStream& playback_;
  EspUsbAudioCaptureStream& capture_;
  std::atomic<bool> micActive_ {false};
};
}
