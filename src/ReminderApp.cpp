#include "ReminderApp.h"
#include "services/PartnerLog.h"

#include <Wire.h>
#include <app_config.h>
#include <esp_heap_caps.h>
#include <esp_random.h>

namespace partner {

ReminderApp::ReminderApp()
    : wifi_(WIFI_SSID, WIFI_PASSWORD),
      sensors_(state_),
      personal_(state_, LittleFS),
      generator_(transport_, state_, {AI_BASE_URL, AI_ENDPOINT, AI_MODEL, AI_API_KEY}),
      mcp_(state_, speaker_, personal_),
      media_(speaker_),
      voice_(speaker_, capture_, micStream_, media_),
      files_(LittleFS, capture_),
      speech_(speaker_, state_, {TTS_BASE_URL, TTS_MODEL, TTS_VOICE, TTS_API_KEY}),
      workflow_(generator_, speech_, state_),
      display_(U8G2_R0, U8X8_PIN_NONE, SCL_PIN, SDA_PIN), screen_(display_) {}

bool ReminderApp::begin() {
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);
  Wire.beginTransmission(config::oledAddress);
  displayPresent_ = Wire.endTransmission() == 0;
  if (displayPresent_) {
    display_.setI2CAddress(config::oledAddress << 1);
    display_.setBusClock(400000);
    display_.begin();
    screen_.begin();
  } else {
    partner::log.println("[oled] no display at configured I2C address");
  }
  if (!media_.begin()) partner::log.println("[usb] initialization failed");
  files_.begin();
  personal_.begin(files_.mounted());
  mcp_.begin();
  reminders_ = xQueueCreate(1, sizeof(LocalTime));
  arrivals_ = xQueueCreate(1, sizeof(uint32_t));
  start_ = xEventGroupCreate();
  TaskHandle_t tasks[5] = {};
  const bool ready = reminders_ && arrivals_ && start_ &&
      xTaskCreatePinnedToCore(networkTask, "wifi-clock", config::networkStackBytes, this, 1, &tasks[0], 0) == pdPASS &&
      xTaskCreatePinnedToCore(reminderTask, "reminder", config::reminderStackBytes, this, 1, &tasks[1], 1) == pdPASS &&
      xTaskCreatePinnedToCore(displayTask, "oled", config::displayStackBytes, this, 2, &tasks[2], 1) == pdPASS &&
      xTaskCreatePinnedToCore(voiceTask, "voice-afe", 6144, this, 5, &tasks[3], 1) == pdPASS &&
      xTaskCreatePinnedToCore(filesTask, "files", 8192, this, 1, &tasks[4], 0) == pdPASS;
  if (!ready) {
    for (auto task : tasks) if (task) vTaskDelete(task);
    if (reminders_) vQueueDelete(reminders_);
    if (arrivals_) vQueueDelete(arrivals_);
    if (start_) vEventGroupDelete(start_);
    return false;
  }
  if (!micStream_.begin() || xTaskCreatePinnedToCore(micStreamTask, "mic-ws", 6144, this, 1, nullptr, 0) != pdPASS)
    partner::log.println("[mic-ws] initialization failed");
  xEventGroupSetBits(start_, BIT0);
  return true;
}

void ReminderApp::awaitStart() { xEventGroupWaitBits(start_, BIT0, pdFALSE, pdTRUE, portMAX_DELAY); }
void ReminderApp::networkTask(void* context) {
  auto& app = *static_cast<ReminderApp*>(context);
  app.awaitStart();
  app.runNetwork();
}
void ReminderApp::reminderTask(void* context) {
  auto& app = *static_cast<ReminderApp*>(context);
  app.awaitStart();
  app.runReminder();
}
void ReminderApp::displayTask(void* context) {
  auto& app = *static_cast<ReminderApp*>(context);
  app.awaitStart();
  app.runDisplay();
}
void ReminderApp::voiceTask(void* context) {
  auto& app = *static_cast<ReminderApp*>(context);
  app.awaitStart();
  app.voice_.run();
}
void ReminderApp::filesTask(void* context) {
  auto& app = *static_cast<ReminderApp*>(context);
  app.awaitStart();
  for (;;) {
    app.files_.tick();
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}
void ReminderApp::micStreamTask(void* context) {
  auto& app = *static_cast<ReminderApp*>(context);
  app.awaitStart();
  for (;;) {
    app.micStream_.tick();
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

void ReminderApp::runNetwork() {
  wifi_.begin();
  HourlySchedule schedule;
  PresenceTransitions presence;
  TickType_t wake = xTaskGetTickCount();
  for (;;) {
    const auto now = wifi_.tick();
    sensors_.tick();
    state_.updateClock(now, wifi_.connected());
    const auto sensor = state_.snapshot().sensor;
    if (presence.update(sensor.online && sensor.hasPresence, sensor.detected)) {
      state_.markArrival();
      const uint32_t event = millis();
      xQueueOverwrite(arrivals_, &event);
      partner::log.println("[presence] arrival detected");
    }
    personal_.tick();
    mcp_.tick();
    if (schedule.update(now)) {
      xQueueOverwrite(reminders_, &now);
      partner::log.printf("[clock] queued %02d:%02d WIB\n", now.hour, now.minute);
    }
    vTaskDelayUntil(&wake, pdMS_TO_TICKS(config::networkPeriodMs));
  }
}

void ReminderApp::runReminder() {
  for (;;) {
    if (!state_.snapshot().connected || !speaker_.ready()) {
      vTaskDelay(pdMS_TO_TICKS(250));
      continue;
    }
    PersonalReminder personal;
    if (personal_.receive(personal)) {
      const auto sensor = state_.snapshot().sensor;
      const bool present = sensor.online && sensor.hasPresence && sensor.detected;
      std::string error;
      bool delivered = false;
      if (present) {
        state_.setPhase(ReminderPhase::Synthesizing);
        delivered = speech_.speak(personal.text, error);
        state_.setPhase(delivered ? ReminderPhase::Idle : ReminderPhase::Error, error);
      }
      personal_.complete(personal.id, delivered);
      continue;
    }
    uint32_t arrival;
    if (xQueueReceive(arrivals_, &arrival, 0) == pdTRUE) {
      const auto sensor = state_.snapshot().sensor;
      if (sensor.online && sensor.hasPresence && sensor.detected && millis() - arrival < 30000) {
        static constexpr const char* greetings[] = {"Hai, selamat datang kembali!", "Hai, kamu sudah kembali.",
                                                     "Halo lagi!", "Selamat datang kembali!"};
        std::string error;
        state_.setPhase(ReminderPhase::Synthesizing);
        const bool ok = speech_.speak(greetings[esp_random() % 4], error);
        state_.setPhase(ok ? ReminderPhase::Idle : ReminderPhase::Error, error);
      }
      continue;
    }
    McpSpeechJob speechJob;
    if (mcp_.receiveSpeech(speechJob)) {
      std::string error;
      state_.setPhase(ReminderPhase::Synthesizing);
      const bool ok = speech_.speak(speechJob.text, error);
      state_.setPhase(ok ? ReminderPhase::Idle : ReminderPhase::Error, error);
      partner::log.printf("[mcp] speech %s%s%s\n", ok ? "complete" : "failed", error.empty() ? "" : ": ", error.c_str());
      continue;
    }
    LocalTime job;
    if (xQueueReceive(reminders_, &job, pdMS_TO_TICKS(250)) != pdTRUE) continue;
    const auto current = state_.snapshot();
    if (!current.time.valid || job.hourId() != current.time.hourId()) continue;
    // Use the actual current WIB time after any offline queue wait.
    workflow_.run(current.time);
    partner::log.printf("[runtime] reminder stack_free=%u internal_free=%u psram_free=%u\n",
                  static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)),
                  static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                  static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
  }
}

void ReminderApp::runDisplay() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Button button;
  bool screenOff = false;
  TickType_t wake = xTaskGetTickCount();
  for (;;) {
    auto snapshot = state_.snapshot();
    snapshot.microphoneLevel = speaker_.microphoneLevel();
    switch (button.update(digitalRead(BUTTON_PIN) == LOW, millis())) {
      case ButtonAction::Click: state_.cycleScreen(); break;
      case ButtonAction::Hold:
        if (snapshot.time.valid) xQueueOverwrite(reminders_, &snapshot.time);
        else partner::log.println("[reminder] waiting for NTP before manual request");
        break;
      default: break;
    }
    if (displayPresent_) {
      screen_.setPage(snapshot.screen);
      screen_.render(snapshot, millis());
      if (screen_.poweredOff() != screenOff) {
        screenOff = screen_.poweredOff();
        partner::log.printf("[oled] %s (node=%s radar=%s)\n", screenOff ? "off" : "on",
                      snapshot.sensor.online ? "online" : "offline",
                      !snapshot.sensor.hasPresence ? "unknown" : snapshot.sensor.detected ? "present" : "clear");
      }
    }
    vTaskDelayUntil(&wake, pdMS_TO_TICKS(config::displayPeriodMs));
  }
}

}  // namespace partner
