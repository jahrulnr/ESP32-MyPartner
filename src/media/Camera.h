#pragma once
#include <EspUsbDevice.h>
#include <esp_camera.h>
class CameraModule {
public:
  explicit CameraModule(EspUsbDevice &usb);
  bool begin();
  void poll();
  bool available() const { return ready_; }
private:
  EspUsbDeviceVideo video_;
  camera_fb_t *frame_ = nullptr;
  uint32_t due_ = 0;
  bool ready_ = false;
};
