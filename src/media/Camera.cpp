#include "Camera.h"
#include <Arduino.h>
CameraModule::CameraModule(EspUsbDevice &usb) : video_(usb, "MyMedia Camera") {}
bool CameraModule::begin() {
  const bool configured = video_.setFormat(EspUsbDeviceVideoFormat::Mjpeg) && video_.setFrameSize(320,240) && video_.setFrameRate(10) && video_.setMaxFrameSize(65536);
  if (!configured) return false;
  camera_config_t c = {};
  c.pin_pwdn=-1; c.pin_reset=-1; c.pin_xclk=15; c.pin_sccb_sda=-1; c.pin_sccb_scl=-1; c.sccb_i2c_port=0;
  c.pin_d0=11; c.pin_d1=9; c.pin_d2=8; c.pin_d3=10; c.pin_d4=12; c.pin_d5=18; c.pin_d6=17; c.pin_d7=16;
  c.pin_vsync=6; c.pin_href=7; c.pin_pclk=13; c.xclk_freq_hz=10000000;
  c.ledc_timer=LEDC_TIMER_0; c.ledc_channel=LEDC_CHANNEL_0;
  c.pixel_format=PIXFORMAT_JPEG; c.frame_size=FRAMESIZE_QVGA; c.jpeg_quality=18;
  c.fb_count=1; c.fb_location=CAMERA_FB_IN_PSRAM; c.grab_mode=CAMERA_GRAB_WHEN_EMPTY;
  esp_err_t error=esp_camera_init(&c);
  if (error != ESP_OK) { Serial.printf("Camera init failed: %s\n", esp_err_to_name(error)); return false; }
  ready_ = true;
  return ready_;
}
void CameraModule::poll() {
  if (!ready_) return;
  if (frame_ && !video_.frameInFlight()) { esp_camera_fb_return(frame_); frame_ = nullptr; }
  if (!video_.streaming() || frame_ || static_cast<int32_t>(millis()-due_)<0) return;
  due_ = millis()+100;
  frame_ = esp_camera_fb_get();
  if (frame_ && !video_.sendFrame(frame_->buf,frame_->len)) { esp_camera_fb_return(frame_); frame_=nullptr; }
}
