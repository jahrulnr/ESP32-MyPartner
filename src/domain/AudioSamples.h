#pragma once
#include <cstdint>

namespace partner {
inline int32_t pcm16ToI2s32(int16_t sample) { return static_cast<int32_t>(sample) * 65536; }
inline int16_t clampPcm16(float sample) {
  if (sample >= 32767.0f) return 32767;
  if (sample <= -32768.0f) return -32768;
  return static_cast<int16_t>(sample);
}
inline int16_t scalePcm16(int16_t sample, float volume) {
  return clampPcm16(static_cast<float>(sample) * volume);
}
inline int16_t i2s32ToPcm16(int32_t sample, float volume) {
  return clampPcm16((static_cast<float>(sample) / 65536.0f) * volume);
}
}  // namespace partner
