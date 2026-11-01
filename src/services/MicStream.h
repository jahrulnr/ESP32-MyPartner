#pragma once

#include <Arduino.h>
#include <WebSocketsServer.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace partner {

// The microphone producer never waits for the network task.
class MicStream {
 public:
  bool begin();
  void tick();
  void publish(const int16_t* samples, size_t count);

 private:
  // Batch 100 ms of audio into each WebSocket packet to keep Wi-Fi overhead low.
  static constexpr size_t maxSamples = 1600;
  struct Frame {
    uint32_t generation;
    uint32_t sampleIndex;
    uint16_t count;
    int16_t pcm[maxSamples];
  };
  void event(uint8_t client, WStype_t type, uint8_t* payload, size_t length);
  void stop(int requester);
  WebSocketsServer server_ {81};
  QueueHandle_t frames_ = nullptr;
  StaticQueue_t frameControl_ {};
  uint8_t* frameStorage_ = nullptr;
  std::atomic<bool> active_ {false};
  std::atomic<uint32_t> generation_ {0}, sampleIndex_ {0}, dropped_ {0};
  // Written only by the microphone task; discarded when the session changes.
  Frame producer_ {};
  // Used only by the network task, keeping large buffers off task stacks.
  Frame outgoing_ {};
  uint8_t packet_[4 + maxSamples * sizeof(int16_t)] {};
  int owner_ = -1;
  bool listening_ = false;
  IPAddress announcedIp_;
};

}  // namespace partner
