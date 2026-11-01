#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace partner {

class IPcmSink {
 public:
  virtual ~IPcmSink() = default;
  virtual bool writePcm(const uint8_t* bytes, size_t size, std::string& error) = 0;
};

// HTTP chunks can split a 16-bit sample at any byte, including a zero byte.
class Pcm16Assembler {
 public:
  explicit Pcm16Assembler(IPcmSink& sink) : sink_(sink) {}

  bool write(const uint8_t* bytes, size_t size, std::string& error) {
    if (size == 0) return true;
    if (hasLowByte_) {
      const uint8_t sample[] = {lowByte_, *bytes++};
      --size;
      if (!sink_.writePcm(sample, sizeof(sample), error)) return false;
      hasLowByte_ = false;
    }
    const size_t aligned = size & ~size_t(1);
    if (aligned && !sink_.writePcm(bytes, aligned, error)) return false;
    if (size != aligned) {
      lowByte_ = bytes[aligned];
      hasLowByte_ = true;
    }
    return true;
  }

  bool complete() const { return !hasLowByte_; }

 private:
  IPcmSink& sink_;
  bool hasLowByte_ = false;
  uint8_t lowByte_ = 0;
};

}  // namespace partner
