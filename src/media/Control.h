#pragma once
#include <EspUsbDevice.h>
class ControlModule {
public:
  explicit ControlModule(EspUsbDevice &usb) : serial_(usb) {}
  void poll(bool cameraReady, bool audioReady);
private:
  EspUsbDeviceCdcSerial serial_;
  char command_[64] = {};
  size_t used_ = 0;
};
