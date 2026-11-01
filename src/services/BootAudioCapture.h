#pragma once

#include <FS.h>
#include <atomic>

namespace partner {

// Each producer writes its own PSRAM track; flash is touched only after both fill.
class BootAudioCapture {
 public:
  static constexpr size_t sampleRate = 16000;
  static constexpr size_t seconds = 30;
  static constexpr size_t sampleCount = sampleRate * seconds;
  static constexpr const char* micPath = "/mic-boot.wav";
  static constexpr const char* afePath = "/afe-boot.wav";
  void arm() { armed_ = true; }
  void start();
  // Call only before producer tasks start, or after both tracks are full.
  void cancel();
  void microphone(const int16_t* samples, size_t count);
  void processed(const int16_t* samples, size_t count);
  bool complete() const;
  bool save(fs::FS& filesystem);

 private:
  struct Track {
    int16_t* samples = nullptr;
    std::atomic<size_t> count {0};
    uint32_t startedAt = 0;
    uint32_t endedAt = 0;
  };
  static void append(Track& track, const int16_t* samples, size_t count);
  static bool writeWav(fs::FS& filesystem, const char* path, const Track& track);
  bool armed_ = false;
  std::atomic<bool> running_ {false};
  bool saved_ = false;
  Track mic_;
  Track afe_;
};

}  // namespace partner
