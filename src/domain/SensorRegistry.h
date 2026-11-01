#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include "SensorNodeProtocol.h"

namespace partner {

struct SensorSnapshot {
  char name[25] {};
  uint8_t mac[6] {};
  bool online = false, hasClimate = false, hasPresence = false;
  bool detected = false, hasDistance = false;
  int16_t temperature10 = 0;
  uint16_t humidity10 = 0, distanceCm = 0;
  uint32_t lastSeenAgeMs = 0, climateAgeMs = 0, presenceAgeMs = 0;
};

struct SensorCollection {
  std::array<SensorSnapshot, 4> nodes {};
  uint8_t count = 0;
};

// Bounded multi-node registry; use receive time, never a remote node's clock.
class SensorRegistry {
 public:
  static constexpr uint32_t nodeStaleMs = 45000;
  static constexpr uint32_t sensorStaleMs = 5 * 60 * 1000;
  static constexpr size_t capacity = 4;

  bool accept(const sensor_protocol::Event& event, uint32_t now) {
    Node* node = nullptr;
    for (auto& candidate : nodes_) {
      if (candidate.used && std::memcmp(candidate.mac, event.mac, 6) == 0) { node = &candidate; break; }
    }
    if (!node) {
      for (auto& candidate : nodes_) {
        if (!candidate.used || now - candidate.seenAt > nodeStaleMs) { node = &candidate; break; }
      }
      if (!node) return false;
      *node = {};
      node->used = true;
      std::memcpy(node->mac, event.mac, 6);
    }
    node->seenAt = now;
    switch (event.kind) {
      case sensor_protocol::Kind::Identity:
        std::memcpy(node->name, event.name, sizeof(node->name)); break;
      case sensor_protocol::Kind::Climate:
        node->temperature10 = event.temperature10; node->humidity10 = event.humidity10;
        node->climateAt = now; node->climateSeen = true; break;
      case sensor_protocol::Kind::Presence:
        node->detected = event.detected; node->hasDistance = event.hasDistance;
        // The node can send a snapshot even if its radar UART decoded no frames.
        node->distanceCm = event.distanceCm; node->presenceAt = now; node->presenceSeen = event.frames > 0; break;
      default: break;
    }
    return true;
  }

  SensorSnapshot snapshot(uint32_t now) const {
    const Node* selected = nullptr;
    for (const auto& node : nodes_) {
      if (!node.used || now - node.seenAt > nodeStaleMs) continue;
      if (!selected) selected = &node;
      if ((node.climateSeen && now - node.climateAt < sensorStaleMs) ||
          (node.presenceSeen && now - node.presenceAt < sensorStaleMs)) { selected = &node; break; }
    }
    if (!selected) return {};
    SensorSnapshot out {};
    std::memcpy(out.name, selected->name, sizeof(out.name));
    std::memcpy(out.mac, selected->mac, sizeof(out.mac));
    out.online = true;
    out.hasClimate = selected->climateSeen && now - selected->climateAt < sensorStaleMs;
    out.hasPresence = selected->presenceSeen && now - selected->presenceAt < sensorStaleMs;
    out.temperature10 = selected->temperature10; out.humidity10 = selected->humidity10;
    out.detected = selected->detected; out.hasDistance = selected->hasDistance;
    out.distanceCm = selected->distanceCm;
    out.lastSeenAgeMs = now - selected->seenAt;
    out.climateAgeMs = now - selected->climateAt;
    out.presenceAgeMs = now - selected->presenceAt;
    return out;
  }

  SensorCollection snapshots(uint32_t now) const {
    SensorCollection out {};
    for (const auto& node : nodes_) {
      if (!node.used) continue;
      auto& item = out.nodes[out.count++];
      std::memcpy(item.name, node.name, sizeof(item.name));
      std::memcpy(item.mac, node.mac, sizeof(item.mac));
      item.lastSeenAgeMs = now - node.seenAt;
      item.climateAgeMs = now - node.climateAt;
      item.presenceAgeMs = now - node.presenceAt;
      item.online = item.lastSeenAgeMs <= nodeStaleMs;
      item.hasClimate = node.climateSeen && item.climateAgeMs < sensorStaleMs;
      item.hasPresence = node.presenceSeen && item.presenceAgeMs < sensorStaleMs;
      item.temperature10 = node.temperature10; item.humidity10 = node.humidity10;
      item.detected = node.detected; item.hasDistance = node.hasDistance; item.distanceCm = node.distanceCm;
    }
    return out;
  }

 private:
  struct Node {
    uint8_t mac[6] {};
    char name[25] {};
    uint32_t seenAt = 0, climateAt = 0, presenceAt = 0;
    int16_t temperature10 = 0;
    uint16_t humidity10 = 0, distanceCm = 0;
    bool used = false, climateSeen = false, presenceSeen = false, detected = false, hasDistance = false;
  };
  std::array<Node, capacity> nodes_ {};
};

}  // namespace partner
