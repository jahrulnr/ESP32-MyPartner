#include <Arduino.h>
#include "services/PartnerLog.h"

#include "ReminderApp.h"

static partner::ReminderApp app;

void setup() {
  Serial.begin(115200);
  partner::log.begin();
  partner::log.printf("\nPartner AI reminder | PSRAM: %u bytes\n", ESP.getPsramSize());
  if (!app.begin()) partner::log.println("[fatal] failed to allocate FreeRTOS application tasks");
}

void loop() { vTaskDelete(NULL); }
