#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

#include "domain/Reminder.h"

namespace partner {

class WifiClock {
 public:
  WifiClock(const char* ssid, const char* password) : ssid_(ssid), password_(password) {}
  void begin();
  LocalTime tick();
  bool connected() const { return WiFi.status() == WL_CONNECTED; }

 private:
  const char* ssid_;
  const char* password_;
  bool ntpStarted_ = false;
  bool wasConnected_ = false;
  uint32_t lastConnectMs_ = 0;
};

}  // namespace partner
