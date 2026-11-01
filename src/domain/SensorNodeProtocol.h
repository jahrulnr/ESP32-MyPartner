#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace partner::sensor_protocol {

constexpr size_t maxPayload = 200;
constexpr size_t frameHeader = 9;
constexpr uint8_t version = 1;
enum class Kind : uint8_t { Hello, Alive, Identity, Climate, Presence, Other };

struct Event {
  uint8_t mac[6] {};
  Kind kind = Kind::Other;
  char name[25] {};
  int16_t temperature10 = 0;
  uint16_t humidity10 = 0, distanceCm = 0;
  uint16_t frames = 0, bytes = 0;
  bool detected = false, hasDistance = false;
};

inline uint16_t little16(const uint8_t* p) { return uint16_t(p[0]) | (uint16_t(p[1]) << 8); }

// Compatible with espnow-sensors-node protocol v1; parse bytes without alignment assumptions.
inline bool decode(const uint8_t mac[6], const uint8_t* data, size_t length, Event& event) {
  if (!mac || !data || length < frameHeader || data[0] != version || data[8] > maxPayload ||
      length < frameHeader + data[8]) return false;
  event = {};
  std::memcpy(event.mac, mac, 6);
  if (data[1] == 1) { event.kind = Kind::Hello; return true; }
  if (data[1] != 4) return false;
  const uint8_t* payload = data + frameHeader;
  const size_t size = data[8];
  if (size < 4 || payload[0] != 0xb1 || payload[1] != version) return false;
  switch (payload[2]) {
    case 1:
      if (size != 28) return false;
      event.kind = Kind::Identity;
      std::memcpy(event.name, payload + 4, 24);
      event.name[24] = '\0';
      return true;
    case 2: {
      if (size != 8) return false;
      const uint16_t raw = little16(payload + 4);
      event.temperature10 = static_cast<int16_t>(raw < 0x8000 ? raw : int32_t(raw) - 65536);
      event.humidity10 = little16(payload + 6);
      event.kind = Kind::Climate;
      return true;
    }
    case 6:
      if (size != 4) return false;
      event.kind = Kind::Alive;
      return true;
    case 11:
      if (size != 12 && size != 14) return false;
      if (payload[4] > 1 || payload[5] > 1) return false;
      event.kind = Kind::Presence;
      event.detected = payload[4] != 0;
      event.hasDistance = payload[5] != 0;
      event.distanceCm = little16(payload + 6);
      event.frames = little16(payload + 8);
      event.bytes = little16(payload + 10);
      return true;
    default:
      event.kind = Kind::Other;
      return true;
  }
}

inline size_t heartbeat(uint8_t* out, size_t capacity, uint16_t sequence, uint32_t now) {
  static constexpr char beacon[] = "PIO_MASTER_V1";
  constexpr size_t size = frameHeader + sizeof(beacon) - 1;
  if (!out || capacity < size) return 0;
  out[0] = version; out[1] = 2;
  out[2] = sequence & 0xff; out[3] = sequence >> 8;
  for (unsigned i = 0; i < 4; ++i) out[4 + i] = now >> (8 * i);
  out[8] = sizeof(beacon) - 1;
  std::memcpy(out + frameHeader, beacon, sizeof(beacon) - 1);
  return size;
}

}  // namespace partner::sensor_protocol
