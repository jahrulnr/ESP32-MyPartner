#pragma once

#include <Arduino.h>
#include "services/PartnerLog.h"
#include <freertos/FreeRTOS.h>

#include "domain/DeviceSnapshot.h"

namespace partner {

// Only small value types cross cores. No String or peripheral is shared.
class DeviceState : public IReminderStatus {
 public:
  void updateClock(const LocalTime& time, bool connected) {
    portENTER_CRITICAL(&lock_);
    snapshot_.time = time;
    snapshot_.connected = connected;
    portEXIT_CRITICAL(&lock_);
  }

  void updateSensor(const SensorSnapshot& sensor) {
    portENTER_CRITICAL(&lock_);
    snapshot_.sensor = sensor;
    portEXIT_CRITICAL(&lock_);
  }

  void updateNodes(const SensorCollection& nodes) {
    portENTER_CRITICAL(&lock_);
    nodes_ = nodes;
    portEXIT_CRITICAL(&lock_);
  }
  SensorCollection nodes() {
    portENTER_CRITICAL(&lock_);
    const SensorCollection copy = nodes_;
    portEXIT_CRITICAL(&lock_);
    return copy;
  }

  void markArrival() {
    portENTER_CRITICAL(&lock_);
    ++snapshot_.arrivalSequence;
    portEXIT_CRITICAL(&lock_);
  }

  void setScreen(Screen screen) {
    portENTER_CRITICAL(&lock_);
    snapshot_.screen = screen;
    portEXIT_CRITICAL(&lock_);
  }
  void cycleScreen() {
    portENTER_CRITICAL(&lock_);
    snapshot_.screen = static_cast<Screen>((static_cast<unsigned>(snapshot_.screen) + 1) % 3);
    portEXIT_CRITICAL(&lock_);
  }
  void setExpression(Expression expression) {
    portENTER_CRITICAL(&lock_);
    snapshot_.expression = expression;
    portEXIT_CRITICAL(&lock_);
  }

  DeviceSnapshot snapshot() {
    portENTER_CRITICAL(&lock_);
    const DeviceSnapshot copy = snapshot_;
    portEXIT_CRITICAL(&lock_);
    return copy;
  }

  void setPhase(ReminderPhase phase, const std::string& error = {}) override {
    portENTER_CRITICAL(&lock_);
    snapshot_.phase = phase;
    portEXIT_CRITICAL(&lock_);
    partner::log.printf("[reminder] phase=%u%s%s\n", static_cast<unsigned>(phase),
                  error.empty() ? "" : " error=", error.c_str());
  }

 private:
  portMUX_TYPE lock_ = portMUX_INITIALIZER_UNLOCKED;
  DeviceSnapshot snapshot_;
  SensorCollection nodes_;
};

}  // namespace partner
