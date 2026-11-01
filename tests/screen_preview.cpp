#include "ui/ReminderScreen.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <string>

using namespace partner;

static U8G2 display;
static ReminderScreen screen(display);

static void capture(const DeviceSnapshot& state, const char* name, const std::string& directory) {
  for (unsigned i = 0; i < 30; ++i) {
    hostMillis += 33;
    screen.render(state, hostMillis);
  }
  FILE* file = std::fopen((directory + "/" + name + ".pbm").c_str(), "wb");
  assert(file);
  std::fprintf(file, "P1\n128 64\n");
  const auto* pixels = display.pixels();
  for (unsigned y = 0; y < 64; ++y) {
    for (unsigned x = 0; x < 128; ++x) {
      const bool on = !display.powerSave && ((pixels[x + (y / 8) * 128] >> (y % 8)) & 1);
      std::fprintf(file, "%u ", on ? 0 : 1);  // PBM uses 0 for white.
    }
    std::fprintf(file, "\n");
  }
  std::fclose(file);
}

int main(int argc, char** argv) {
  assert(argc == 2);
  screen.begin();
  DeviceSnapshot state;
  capture(state, "offline", argv[1]);
  state.connected = true;
  state.microphoneLevel = 5;
  capture(state, "syncing", argv[1]);
  state.time = {1793498400, 2026, 11, 1, 9, 0, 0, true};
  state.microphoneLevel = 30;
  capture(state, "idle", argv[1]);
  state.phase = ReminderPhase::Generating;
  state.microphoneLevel = 45;
  capture(state, "generating", argv[1]);
  state.phase = ReminderPhase::Synthesizing;
  state.microphoneLevel = 65;
  capture(state, "synthesizing", argv[1]);
  state.phase = ReminderPhase::Speaking;
  state.microphoneLevel = 80;
  capture(state, "speaking", argv[1]);
  state.phase = ReminderPhase::Error;
  state.microphoneLevel = 95;
  capture(state, "error", argv[1]);
  state.phase = ReminderPhase::Idle;
  state.microphoneLevel = 100;
  screen.togglePage();
  capture(state, "clock", argv[1]);
  screen.togglePage();
  capture(state, "sensor-offline", argv[1]);
  state.sensor.online = state.sensor.hasClimate = state.sensor.hasPresence = true;
  state.sensor.temperature10 = -123;
  state.sensor.humidity10 = 745;
  state.sensor.detected = state.sensor.hasDistance = true;
  state.sensor.distanceCm = 123;
  std::snprintf(state.sensor.name, sizeof(state.sensor.name), "pio-sensors-node-c3");
  capture(state, "sensor-present", argv[1]);
  state.sensor.hasDistance = false;
  capture(state, "sensor-no-distance", argv[1]);
  state.sensor.hasDistance = true;
  state.sensor.hasClimate = state.sensor.hasPresence = false;
  capture(state, "sensor-stale", argv[1]);
  state.sensor.hasClimate = state.sensor.hasPresence = true;
  state.sensor.detected = false;
  capture(state, "sensor-clear", argv[1]);
  assert(display.presented == 360 && display.powerSave && display.powerChanges == 1);
  state.sensor.hasPresence = false;
  screen.render(state, hostMillis);
  assert(!display.powerSave && display.powerChanges == 2);
  state.sensor.hasPresence = true;
  screen.render(state, hostMillis);
  assert(display.powerSave && display.powerChanges == 3);
  state.sensor.online = false;
  screen.render(state, hostMillis);
  assert(!display.powerSave && display.powerChanges == 4);
  state.sensor.online = state.sensor.detected = true;
  screen.render(state, hostMillis);
  assert(!display.powerSave && display.powerChanges == 4);

  // Compare a static sensor page before and after its one-pixel translation.
  screen.render(state, 0);
  uint8_t original[1024];
  std::copy(display.pixels(), display.pixels() + sizeof(original), original);
  screen.render(state, 60000);
  const auto* shifted = display.pixels();
  for (unsigned page = 0; page < 8; ++page) {
    assert(shifted[page * 128] == 0);
    for (unsigned x = 1; x < 128; ++x)
      assert(shifted[page * 128 + x] == original[page * 128 + x - 1]);
  }
  screen.render(state, 120000);
  shifted = display.pixels();
  for (unsigned page = 0; page < 8; ++page) {
    assert(shifted[page * 128] == 0);
    for (unsigned x = 1; x < 128; ++x) {
      const uint8_t expected = (original[page * 128 + x - 1] << 1) |
          (page ? original[(page - 1) * 128 + x - 1] >> 7 : 0);
      assert(shifted[page * 128 + x] == expected);
    }
  }
  screen.render(state, 360000);
  shifted = display.pixels();
  for (unsigned page = 0; page < 8; ++page) {
    assert(shifted[page * 128 + 127] == 0);
    for (unsigned x = 0; x < 127; ++x) {
      const uint8_t expected = (original[page * 128 + x + 1] >> 1) |
          (page < 7 ? original[(page + 1) * 128 + x + 1] << 7 : 0);
      assert(shifted[page * 128 + x] == expected);
    }
  }
  screen.togglePage();  // Face page
  state.phase = ReminderPhase::Idle;
  state.sensor.online = state.sensor.hasPresence = state.sensor.detected = true;
  state.microphoneLevel = 0;
  state.arrivalSequence = 1;
  capture(state, "arrival", argv[1]);
  hostMillis += 5000;
  state.microphoneLevel = 35;
  capture(state, "sound", argv[1]);
  state.microphoneLevel = 0;
  state.expression = Expression::Happy;
  capture(state, "mcp-happy", argv[1]);
  state.expression = Expression::Sad;
  capture(state, "mcp-sad", argv[1]);
  std::puts("OLED: presence power transitions and complete-frame pixel shifts passed");
}
