#pragma once

#include "Reminder.h"
#include "SensorRegistry.h"
#include "DeviceUi.h"

namespace partner {

struct DeviceSnapshot {
  LocalTime time;
  bool connected = false;
  ReminderPhase phase = ReminderPhase::Idle;
  uint8_t microphoneLevel = 0;
  uint32_t arrivalSequence = 0;
  SensorSnapshot sensor;
  Screen screen = Screen::Home;
  Expression expression = Expression::Auto;
};

}  // namespace partner
