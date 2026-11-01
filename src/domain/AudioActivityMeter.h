#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <algorithm>

namespace partner {

struct AudioActivity {
  uint16_t peak = 0;
  uint16_t rms = 0;
};

// Producer-owned 250 ms windows; report the strongest window over the last 2 s.
class AudioActivityMeter {
 public:
  void reset() {
    windows_ = {};
    next_ = count_ = 0;
    squares_ = peak_ = 0;
    startedAt_ = 0;
    started_ = false;
    last_ = {};
  }
  AudioActivity observe(const int16_t* samples, size_t count, uint32_t now) {
    if (!started_) { started_ = true; startedAt_ = now; }
    for (size_t i = 0; i < count; ++i) {
      const int value = samples[i];
      const unsigned magnitude = value < 0 ? -value : value;
      peak_ = std::max(peak_, magnitude);
      squares_ += static_cast<uint64_t>(magnitude) * magnitude;
    }
    count_ += count;
    if (now - startedAt_ < 250) return last_;
    windows_[next_] = {now, static_cast<uint16_t>(peak_),
                       static_cast<uint16_t>(count_ ? std::sqrt(static_cast<float>(squares_) / count_) : 0), true};
    next_ = (next_ + 1) % windows_.size();
    startedAt_ = now;
    peak_ = 0;
    squares_ = 0;
    count_ = 0;
    last_ = {};
    for (const auto& window : windows_) {
      if (!window.valid || now - window.at > 2000) continue;
      last_.peak = std::max(last_.peak, window.peak);
      last_.rms = std::max(last_.rms, window.rms);
    }
    return last_;
  }

 private:
  struct Window { uint32_t at = 0; uint16_t peak = 0; uint16_t rms = 0; bool valid = false; };
  std::array<Window, 8> windows_ {};
  size_t next_ = 0, count_ = 0;
  uint64_t squares_ = 0;
  unsigned peak_ = 0;
  uint32_t startedAt_ = 0;
  bool started_ = false;
  AudioActivity last_ {};
};

}  // namespace partner
