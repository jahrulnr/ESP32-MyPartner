#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <utility>

namespace partner {

// A fresh direction for each successful reminder; history lives only in RAM.
class ReminderVariation {
 public:
  struct Choice { unsigned angle; float temperature; };
  static constexpr unsigned angleCount = 8;

  ReminderVariation(float minTemperature = 0.85f, float maxTemperature = 1.25f)
      : minTemperature_(minTemperature), maxTemperature_(maxTemperature) {}

  Choice choose(uint32_t random) const {
    unsigned angle = random % (hasLast_ ? angleCount - 1 : angleCount);
    if (hasLast_ && angle >= lastAngle_) ++angle;
    return {angle, minTemperature_ + (maxTemperature_ - minTemperature_) * ((random >> 8) % 101) / 100.0f};
  }

  static const char* direction(unsigned angle) {
    static constexpr const char* directions[] = {
        "sapaan akrab dan ringan", "pengamatan singkat tentang waktu",
        "ajakan jeda atau peregangan", "pertanyaan ringan untuk refleksi",
        "kalimat apresiasi yang sederhana", "pengingat minum air yang santai",
        "humor ringan yang natural", "pesan tenang tanpa ajakan"};
    return directions[angle % angleCount];
  }

  const std::array<std::string, 3>& recent() const { return recent_; }
  void accepted(Choice choice, const std::string& text) {
    lastAngle_ = choice.angle;
    hasLast_ = true;
    recent_[2] = std::move(recent_[1]);
    recent_[1] = std::move(recent_[0]);
    recent_[0] = text;
  }

 private:
  std::array<std::string, 3> recent_ {};
  unsigned lastAngle_ = 0;
  bool hasLast_ = false;
  float minTemperature_, maxTemperature_;
};

}  // namespace partner
