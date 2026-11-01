#pragma once
#include <cstdint>

namespace partner {

class GreetingGate {
 public:
  static constexpr float minimumConfidence = 0.35f;
  explicit GreetingGate(uint16_t minimumMicRms) : minimumMicRms_(minimumMicRms) {}
  bool accept(uint32_t now, bool playbackSuppressed, float confidence, uint16_t micRms) {
    if (!(confidence >= minimumConfidence) || playbackSuppressed ||
        micRms < minimumMicRms_ ||
        (accepted_ && now - last_ < 1200)) return false;
    accepted_ = true;
    last_ = now;
    return true;
  }
 private:
  uint16_t minimumMicRms_;
  bool accepted_ = false;
  uint32_t last_ = 0;
};

}  // namespace partner
