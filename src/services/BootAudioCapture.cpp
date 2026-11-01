#include "BootAudioCapture.h"
#include "services/PartnerLog.h"
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <algorithm>
#include <cstring>

namespace partner {

void BootAudioCapture::start() {
  if (!armed_) return;
  mic_.samples = static_cast<int16_t*>(heap_caps_malloc(sampleCount * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  afe_.samples = static_cast<int16_t*>(heap_caps_malloc(sampleCount * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!mic_.samples || !afe_.samples) {
    cancel();
    partner::log.println("[capture] PSRAM allocation failed; no recording started");
    return;
  }
  running_.store(true);
  partner::log.println("[capture] START: first 30 seconds, mic before AFE + AFE output, PCM16 mono 16 kHz");
}

void BootAudioCapture::cancel() {
  running_.store(false);
  heap_caps_free(mic_.samples);
  heap_caps_free(afe_.samples);
  mic_.samples = afe_.samples = nullptr;
}

void BootAudioCapture::append(Track& track, const int16_t* samples, size_t count) {
  const size_t offset = track.count.load(std::memory_order_relaxed);
  if (offset == sampleCount || !samples || !count) return;
  if (!offset) track.startedAt = millis();
  const size_t length = std::min(count, sampleCount - offset);
  std::memcpy(track.samples + offset, samples, length * 2);
  if (offset + length == sampleCount) track.endedAt = millis();
  track.count.store(offset + length, std::memory_order_release);
}

void BootAudioCapture::microphone(const int16_t* samples, size_t count) {
  if (running_.load()) append(mic_, samples, count);
}
void BootAudioCapture::processed(const int16_t* samples, size_t count) {
  if (running_.load()) append(afe_, samples, count);
}
bool BootAudioCapture::complete() const {
  return running_.load() && !saved_ && mic_.count.load(std::memory_order_acquire) == sampleCount &&
         afe_.count.load(std::memory_order_acquire) == sampleCount;
}

bool BootAudioCapture::writeWav(fs::FS& filesystem, const char* path, const Track& track) {
  uint8_t header[44] {};
  auto put16 = [&header](size_t offset, uint16_t value) {
    header[offset] = value;
    header[offset + 1] = value >> 8;
  };
  auto put32 = [&header](size_t offset, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) header[offset + i] = value >> (i * 8);
  };
  std::memcpy(header, "RIFF", 4);
  put32(4, 36 + sampleCount * 2);
  std::memcpy(header + 8, "WAVEfmt ", 8);
  put32(16, 16);
  put16(20, 1);
  put16(22, 1);
  put32(24, sampleRate);
  put32(28, sampleRate * 2);
  put16(32, 2);
  put16(34, 16);
  std::memcpy(header + 36, "data", 4);
  put32(40, sampleCount * 2);
  const String temporary = String(path) + ".part";
  auto file = filesystem.open(temporary, FILE_WRITE);
  bool ok = file && file.write(header, sizeof(header)) == sizeof(header);
  const auto* bytes = reinterpret_cast<const uint8_t*>(track.samples);
  for (size_t offset = 0; ok && offset < sampleCount * 2;) {
    const size_t length = std::min(size_t(4096), sampleCount * 2 - offset);
    ok = file.write(bytes + offset, length) == length;
    offset += length;
    vTaskDelay(1);
  }
  file.close();
  if (ok) ok = filesystem.rename(temporary, path);
  if (!ok) filesystem.remove(temporary);
  partner::log.printf("[capture] %s %s, samples=%u, elapsed=%u ms\n", ok ? "saved" : "failed",
                path, static_cast<unsigned>(sampleCount), track.endedAt - track.startedAt);
  return ok;
}

bool BootAudioCapture::save(fs::FS& filesystem) {
  if (!complete()) return false;
  const bool micOk = writeWav(filesystem, micPath, mic_);
  const bool afeOk = writeWav(filesystem, afePath, afe_);
  saved_ = true;
  // Full tracks no longer write. The acquire above observes their final memcpy.
  cancel();
  return micOk && afeOk;
}

}  // namespace partner
