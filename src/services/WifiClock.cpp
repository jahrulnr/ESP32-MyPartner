#include "WifiClock.h"
#include "services/PartnerLog.h"

#include <app_config.h>

namespace partner {

void WifiClock::begin() {
  setenv("TZ", config::timezone, 1);
  tzset();
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid_, password_);
  lastConnectMs_ = millis();
}

LocalTime WifiClock::tick() {
  const bool online = connected();
  if (online != wasConnected_) {
    partner::log.printf("[wifi] %s\n", online ? "connected" : "disconnected");
    wasConnected_ = online;
  }
  if (online && !ntpStarted_) {
    configTzTime(config::timezone, config::ntpPrimary, config::ntpSecondary);
    ntpStarted_ = true;
  }
  if (!online && millis() - lastConnectMs_ >= config::reconnectMs) {
    WiFi.reconnect();
    lastConnectMs_ = millis();
  }

  LocalTime result;
  const time_t epoch = time(nullptr);
  // Before the first NTP sync the ESP32 clock is near the Unix epoch.
  if (epoch < 1704067200) return result;
  struct tm local {};
  if (!localtime_r(&epoch, &local)) return result;
  result.epoch = epoch;
  result.year = local.tm_year + 1900;
  result.month = local.tm_mon + 1;
  result.day = local.tm_mday;
  result.hour = local.tm_hour;
  result.minute = local.tm_min;
  result.second = local.tm_sec;
  result.valid = true;
  return result;
}

}  // namespace partner
