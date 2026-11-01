#include "domain/AudioActivityMeter.h"
#include <cassert>
#include <cstdint>
#include <iostream>
using namespace partner;
int main() {
  AudioActivityMeter meter;
  const int16_t speech[] = {3000, -3000, 3000, -3000};
  const int16_t silence[] = {0, 0, 0, 0};
  assert(meter.observe(speech, 4, 0).peak == 0);
  auto level = meter.observe(silence, 4, 250);
  assert(level.peak == 3000 && level.rms >= 2120 && level.rms <= 2122);
  level = meter.observe(silence, 4, 2000);
  assert(level.peak == 3000);
  level = meter.observe(silence, 4, 2251);
  assert(level.peak == 0 && level.rms == 0);
  meter.observe(speech, 4, 2300);
  meter.reset();  // Own playback must not boost a later background candidate.
  meter.observe(silence, 4, 2400);
  level = meter.observe(silence, 4, 2650);
  assert(level.peak == 0 && level.rms == 0);
  AudioActivityMeter rollover;
  rollover.observe(speech, 4, UINT32_MAX - 100);
  level = rollover.observe(speech, 4, 200);
  assert(level.peak == 3000 && level.rms == 3000);
  std::cout << "audio activity: window RMS, expiry and millis rollover passed\n";
}
