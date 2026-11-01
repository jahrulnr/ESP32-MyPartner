#include "domain/AudioSamples.h"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace partner;
int main() {
  for (const int16_t sample : {int16_t(-32768), int16_t(-1), int16_t(0), int16_t(1), int16_t(32767)})
    assert(i2s32ToPcm16(pcm16ToI2s32(sample), 1.0f) == sample);
  assert(i2s32ToPcm16(16384, 4.0f) == 1);
  assert(i2s32ToPcm16(-16384, 4.0f) == -1);
  assert(i2s32ToPcm16(INT32_MAX, 4.0f) == 32767);
  assert(i2s32ToPcm16(INT32_MIN, 4.0f) == -32768);
  assert(i2s32ToPcm16(pcm16ToI2s32(1200), 0.5f) == 600);
  assert(i2s32ToPcm16(INT32_MAX, 0.0f) == 0);
  assert(scalePcm16(-20000, 1.0f) == -20000);
  assert(scalePcm16(12000, 0.5f) == 6000);
  assert(scalePcm16(-12000, 0.5f) == -6000);
  assert(scalePcm16(20000, 2.0f) == 32767);
  assert(scalePcm16(-20000, 2.0f) == -32768);
  assert(scalePcm16(12345, 0.0f) == 0);
  assert(scalePcm16(1, 100000.0f) == 32767);
  // INMP441 24-bit samples are left aligned in the 32-bit I2S slot.
  uint32_t state = 0x12345678;
  for (unsigned i = 0; i < 100000; ++i) {
    state = state * 1664525u + 1013904223u;
    uint32_t bits = state & 0xffffff00u;
    int32_t raw;
    std::memcpy(&raw, &bits, sizeof(raw));
    const int64_t oldValue = static_cast<int64_t>(raw) * 4 / 65536;
    const int16_t expected = oldValue > 32767 ? 32767 : oldValue < -32768 ? -32768 : oldValue;
    assert(i2s32ToPcm16(raw, 4.0f) == expected);
  }
  std::cout << "audio samples: microphone and speaker volume, mute and saturation passed\n";
}
