#pragma once

#include <cstring>
#include "Pcm16.h"

namespace partner {

// Fixed 3:2 linear interpolation for Speech API PCM (24 kHz) on the shared
// 16 kHz microphone/speaker bus. State survives arbitrary network chunks.
class Pcm24To16 : public IPcmSink {
 public:
  explicit Pcm24To16(IPcmSink& output) : output_(output) {}
  bool writePcm(const uint8_t* bytes, size_t size, std::string& error) override {
    if (size % 2) { error = "Unaligned PCM input"; return false; }
    for (size_t i = 0; i < size; i += 2) {
      std::memcpy(&group_[used_++], bytes + i, 2);
      if (used_ == 3) {
        const int16_t samples[] = {group_[0], static_cast<int16_t>(
            (static_cast<int32_t>(group_[1]) + group_[2]) / 2)};
        if (!output_.writePcm(reinterpret_cast<const uint8_t*>(samples), sizeof(samples), error)) return false;
        used_ = 0;
      }
    }
    return true;
  }
  bool finish(std::string& error) {
    if (!used_) return true;
    const size_t count = used_;
    used_ = 0;
    return output_.writePcm(reinterpret_cast<const uint8_t*>(group_), count * 2, error);
  }
 private:
  IPcmSink& output_;
  int16_t group_[3] {};
  size_t used_ = 0;
};

}  // namespace partner
