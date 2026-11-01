#pragma once

#include <WebServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "DeviceState.h"
#include "I2sAudio.h"
#include "McpProtocol.h"
#include "domain/SpeechText.h"
#include "PersonalReminderService.h"

namespace partner {
struct McpSpeechJob { char text[kMaxSpeechBytes + 1] {}; };

// HTTP is served by the network task; audio is consumed by the reminder task.
class McpService : public IMcpActions {
 public:
  McpService(DeviceState& state, I2sAudio& audio, PersonalReminderService& personal) : state_(state), audio_(audio), personal_(personal), protocol_(*this) {}
  bool begin();
  void tick();
  bool receiveSpeech(McpSpeechJob& job) { return speech_ && xQueueReceive(speech_, &job, 0) == pdTRUE; }
  bool call(const char* name, JsonVariantConst args, std::string& result, std::string& error) override;
  bool readResource(const char* uri, std::string& result) override;
  bool subscribeResource(const char* uri, bool enabled) override;
 private:
  void handlePost();
  void handleStream();
  void publishChanges();
  int findSession(const String& id);
  int createSession();
  bool validOrigin();
  struct Session { String id; NetworkClient stream; uint8_t subscriptions = 0; uint32_t touched = 0, heartbeat = 0; };
  Session sessions_[4];
  int activeSession_ = -1;
  std::string lastPresence_, lastClimate_;
  DeviceState& state_;
  I2sAudio& audio_;
  PersonalReminderService& personal_;
  WebServer server_ {83};
  McpProtocol protocol_;
  QueueHandle_t speech_ = nullptr;
  StaticQueue_t speechControl_ {};
  uint8_t* speechStorage_ = nullptr;
  bool ready_ = false, started_ = false;
};
}  // namespace partner
