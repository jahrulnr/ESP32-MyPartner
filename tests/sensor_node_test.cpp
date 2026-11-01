#include "domain/SensorNodeProtocol.h"
#include "domain/SensorRegistry.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

using namespace partner;
using namespace partner::sensor_protocol;

static std::vector<uint8_t> state(std::initializer_list<uint8_t> payload) {
  std::vector<uint8_t> frame {1, 4, 1, 0, 0, 0, 0, 0, static_cast<uint8_t>(payload.size())};
  frame.insert(frame.end(), payload);
  return frame;
}

int main() {
  uint8_t bytes[64] {};
  assert(heartbeat(bytes, sizeof(bytes), 0x1234, 0x12345678) == 22);
  assert(bytes[0] == 1 && bytes[1] == 2 && bytes[2] == 0x34 && bytes[3] == 0x12);
  assert(bytes[4] == 0x78 && bytes[7] == 0x12 && bytes[8] == 13);
  assert(std::memcmp(bytes + 9, "PIO_MASTER_V1", 13) == 0);
  assert(heartbeat(bytes, 21, 0, 0) == 0);

  uint8_t mac[6] = {1, 2, 3, 4, 5, 6};
  Event event;
  auto climate = state({0xb1, 1, 2, 0, 0xf4, 0xff, 0x2b, 0x02});
  assert(decode(mac, climate.data(), climate.size(), event));
  assert(event.kind == Kind::Climate && event.temperature10 == -12 && event.humidity10 == 555);
  assert(!decode(mac, climate.data(), climate.size() - 1, event));
  climate[0] = 2;
  assert(!decode(mac, climate.data(), climate.size(), event));
  auto presence = state({0xb1, 1, 11, 0, 1, 1, 0x2c, 1, 3, 0, 20, 0, 2, 3});
  assert(decode(mac, presence.data(), presence.size(), event));
  assert(event.kind == Kind::Presence && event.detected && event.hasDistance && event.distanceCm == 300);
  assert(event.frames == 3 && event.bytes == 20);
  presence[8] = 12; presence.resize(21);  // Earlier node reports without two extension bytes.
  assert(decode(mac, presence.data(), presence.size(), event));
  presence[9 + 4] = 2;
  assert(!decode(mac, presence.data(), presence.size(), event));

  SensorRegistry registry;
  Event radar {};
  std::memcpy(radar.mac, mac, 6);
  radar.kind = Kind::Presence; radar.detected = true; radar.hasDistance = true; radar.distanceCm = 300; radar.frames = 1;
  assert(registry.accept(radar, 0xfffffff0u));
  auto snapshot = registry.snapshot(10);
  assert(snapshot.online && snapshot.hasPresence && snapshot.detected && snapshot.distanceCm == 300);
  Event missing = radar; missing.detected = false; missing.frames = 0;
  assert(registry.accept(missing, 11));
  assert(registry.snapshot(11).online && !registry.snapshot(11).hasPresence);
  assert(registry.accept(radar, 12));
  Event alive = radar; alive.kind = Kind::Alive;
  assert(registry.accept(alive, 44000));
  snapshot = registry.snapshot(45013);
  assert(snapshot.online && snapshot.hasPresence && !snapshot.hasClimate);
  assert(!registry.snapshot(90000).online);
  SensorRegistry ttl;
  Event weather = radar; weather.kind = Kind::Climate; weather.temperature10 = 320; weather.humidity10 = 550;
  constexpr uint32_t first = 0xfffffff0u;
  assert(ttl.accept(weather, first));
  assert(ttl.accept(radar, first + 1000));
  assert(ttl.accept(alive, first + 299999));
  snapshot = ttl.snapshot(first + 299999);
  assert(snapshot.online && snapshot.hasClimate && snapshot.hasPresence);
  assert(ttl.accept(alive, first + 300000));
  snapshot = ttl.snapshot(first + 300000);
  assert(snapshot.online && !snapshot.hasClimate && snapshot.hasPresence);
  snapshot = ttl.snapshot(first + 301000);
  assert(snapshot.online && !snapshot.hasClimate && !snapshot.hasPresence);
  for (unsigned i = 0; i < SensorRegistry::capacity; ++i) {
    alive.mac[0] = i + 10;
    assert(registry.accept(alive, 100000));
  }
  const auto nodes = registry.snapshots(100000);
  assert(nodes.count == SensorRegistry::capacity && nodes.nodes[0].online);
  assert(nodes.nodes[0].mac[0] == 10);
  alive.mac[0] = 99;
  assert(!registry.accept(alive, 100001));
  assert(registry.accept(alive, 145001));
  std::cout << "sensor node: v1 frames, signed climate, presence, stale data and rollover passed\n";
}
