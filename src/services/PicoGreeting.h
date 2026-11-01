#pragma once

#include "I2sAudio.h"

namespace partner {

// Synthesis is a boot-only preparation step. The recognition path only plays
// this ready-to-use 16 kHz PCM clip, with no network or TTS invocation.
class PicoGreeting {
 public:
  bool prepare(std::string& error);
  bool play(I2sAudio& audio, std::string& error);
  void clear();
 private:
  static void receive(int16_t* samples, unsigned count);
  static void idle();
  static void failed();
  static PicoGreeting* preparing_;
  static constexpr size_t capacity_ = 32000;
  int16_t* samples_ = nullptr;
  size_t count_ = 0;
  bool failed_ = false;
  SemaphoreHandle_t complete_ = nullptr;
};

}  // namespace partner
