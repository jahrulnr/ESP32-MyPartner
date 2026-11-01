#pragma once

#include <atomic>
#include <esp_now.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "DeviceState.h"
#include "domain/SensorRegistry.h"

namespace partner {

class SensorMaster {
 public:
  explicit SensorMaster(DeviceState& state) : state_(state) {}
  void tick();

 private:
  bool begin();
  static void receive(const esp_now_recv_info_t* info, const uint8_t* data, int length);
  void consume(const sensor_protocol::Event& event, uint32_t now);

  static std::atomic<SensorMaster*> instance_;
  DeviceState& state_;
  SensorRegistry registry_;
  QueueHandle_t queue_ = nullptr;
  std::atomic<uint32_t> dropped_ {0};
  bool started_ = false, attempted_ = false, heartbeatSent_ = false;
  int lastPresenceState_ = -2;
  uint8_t lastChannel_ = 0;
  uint32_t lastAttempt_ = 0, lastHeartbeat_ = 0, lastDropLog_ = 0, lastPresenceLog_ = 0;
  uint16_t sequence_ = 0;
};

}  // namespace partner
