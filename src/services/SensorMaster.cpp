#include "SensorMaster.h"
#include "services/PartnerLog.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <cstring>

namespace partner {
namespace {
constexpr uint8_t broadcast[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
constexpr uint32_t heartbeatMs = 500;
}

std::atomic<SensorMaster*> SensorMaster::instance_ {nullptr};

bool SensorMaster::begin() {
  if (!queue_) queue_ = xQueueCreate(24, sizeof(sensor_protocol::Event));
  if (!queue_) { partner::log.println("[sensors] queue allocation failed"); return false; }
  const esp_err_t init = esp_now_init();
  if (init != ESP_OK) { partner::log.printf("[sensors] ESP-NOW init: %s\n", esp_err_to_name(init)); return false; }
  instance_.store(this);
  const esp_err_t receiveResult = esp_now_register_recv_cb(receive);
  esp_now_peer_info_t peer {};
  std::memcpy(peer.peer_addr, broadcast, 6);
  peer.ifidx = WIFI_IF_STA;
  peer.channel = 0;  // Follow the current STA channel; never retune connected Wi-Fi.
  const esp_err_t addResult = receiveResult == ESP_OK ? esp_now_add_peer(&peer) : receiveResult;
  if (receiveResult != ESP_OK || (addResult != ESP_OK && addResult != ESP_ERR_ESPNOW_EXIST)) {
    partner::log.printf("[sensors] ESP-NOW setup: %s\n", esp_err_to_name(addResult));
    instance_.store(nullptr);
    esp_now_deinit();
    return false;
  }
  started_ = true;
  partner::log.println("[sensors] ESP-NOW master ready; using current Wi-Fi channel");
  return true;
}

void SensorMaster::receive(const esp_now_recv_info_t* info, const uint8_t* data, int length) {
  auto* instance = instance_.load();
  if (!instance || !info || !data || length < 0) return;
  sensor_protocol::Event event;
  if (!sensor_protocol::decode(info->src_addr, data, static_cast<size_t>(length), event)) return;
  if (xQueueSend(instance->queue_, &event, 0) != pdTRUE) instance->dropped_.fetch_add(1);
}

void SensorMaster::consume(const sensor_protocol::Event& event, uint32_t now) {
  if (!registry_.accept(event, now)) return;
  switch (event.kind) {
    case sensor_protocol::Kind::Identity:
      partner::log.printf("[sensors] node %02x:%02x:%02x:%02x:%02x:%02x name=%s\n",
                    event.mac[0], event.mac[1], event.mac[2], event.mac[3], event.mac[4], event.mac[5], event.name);
      break;
    case sensor_protocol::Kind::Climate:
      partner::log.printf("[sensors] DHT temp=%.1f C humidity=%.1f%% from %02x:%02x:%02x:%02x:%02x:%02x\n",
                    event.temperature10 / 10.0, event.humidity10 / 10.0,
                    event.mac[0], event.mac[1], event.mac[2], event.mac[3], event.mac[4], event.mac[5]);
      break;
    case sensor_protocol::Kind::Presence: {
      const int presence = event.frames == 0 ? -1 : event.detected ? 1 : 0;
      if (lastPresenceState_ != presence || now - lastPresenceLog_ >= 5000) {
        partner::log.printf("[sensors] presence=%s distance=%s%u cm radar_frames=%u radar_bytes=%u from %02x:%02x:%02x:%02x:%02x:%02x\n",
                      presence < 0 ? "unknown" : presence ? "detected" : "clear", event.hasDistance ? "" : "unknown/",
                      event.hasDistance ? event.distanceCm : 0,
                      event.frames, event.bytes,
                      event.mac[0], event.mac[1], event.mac[2], event.mac[3], event.mac[4], event.mac[5]);
        lastPresenceState_ = presence; lastPresenceLog_ = now;
      }
      break;
    }
    default: break;
  }
}

void SensorMaster::tick() {
  const uint32_t now = millis();
  if (!started_) {
    if (!attempted_ || now - lastAttempt_ >= 5000) {
      attempted_ = true; lastAttempt_ = now;
      begin();
    }
    if (!started_) return;
  }
  if (!heartbeatSent_ || now - lastHeartbeat_ >= heartbeatMs) {
    uint8_t channel = 0;
    wifi_second_chan_t secondary = WIFI_SECOND_CHAN_NONE;
    if (esp_wifi_get_channel(&channel, &secondary) == ESP_OK && channel != lastChannel_) {
      lastChannel_ = channel;
      partner::log.printf("[sensors] current Wi-Fi channel=%u\n", unsigned(channel));
    }
    uint8_t frame[32];
    const size_t size = sensor_protocol::heartbeat(frame, sizeof(frame), sequence_++, now);
    const esp_err_t result = esp_now_send(broadcast, frame, size);
    if (result == ESP_OK) { heartbeatSent_ = true; lastHeartbeat_ = now; }
    else if (now - lastHeartbeat_ >= 5000) {
      partner::log.printf("[sensors] heartbeat: %s\n", esp_err_to_name(result));
      lastHeartbeat_ = now;
    }
  }
  sensor_protocol::Event event;
  for (unsigned i = 0; i < 24 && xQueueReceive(queue_, &event, 0) == pdTRUE; ++i) consume(event, now);
  const SensorSnapshot snapshot = registry_.snapshot(now);
  state_.updateSensor(snapshot);
  state_.updateNodes(registry_.snapshots(now));
  if (now - lastDropLog_ >= 5000) {
    const auto dropped = dropped_.exchange(0);
    if (dropped) partner::log.printf("[sensors] dropped %u received packets\n", unsigned(dropped));
    lastDropLog_ = now;
  }
}

}  // namespace partner
