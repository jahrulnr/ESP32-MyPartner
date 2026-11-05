#pragma once

#include <U8g2lib.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <transport/custom_http_transport.h>

#include "domain/Button.h"
#include "services/CloudSpeech.h"
#include "services/DeviceState.h"
#include "services/OpenAiReminder.h"
#include "services/WifiClock.h"
#include "services/LocalVoice.h"
#include "services/FtpFiles.h"
#include "services/SensorMaster.h"
#include "services/PersonalReminderService.h"
#include "services/McpService.h"
#include "ui/ReminderScreen.h"

namespace partner {

class ReminderApp {
 public:
  ReminderApp();
  bool begin();

 private:
  static void networkTask(void* context);
  static void reminderTask(void* context);
  static void displayTask(void* context);
  static void voiceTask(void* context);
  static void filesTask(void* context);
  static void micStreamTask(void* context);
  void awaitStart();
  void runNetwork();
  void runReminder();
  void runDisplay();

  DeviceState state_;
  WifiClock wifi_;
  SensorMaster sensors_;
  PersonalReminderService personal_;
  ai::provider::CustomHttpTransport transport_;
  OpenAiReminder generator_;
  I2sAudio speaker_;
  McpService mcp_;
  BootAudioCapture capture_;
  MicStream micStream_;
  UsbMedia media_;
  LocalVoice voice_;
  FtpFiles files_;
  CloudSpeech speech_;
  ReminderWorkflow workflow_;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C display_;
  ReminderScreen screen_;
  bool displayPresent_ = false;
  QueueHandle_t reminders_ = nullptr;
  QueueHandle_t arrivals_ = nullptr;
  EventGroupHandle_t start_ = nullptr;
};

}  // namespace partner
