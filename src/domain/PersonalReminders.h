#pragma once

#include <array>
#include <cstdint>
#include <cstring>

namespace partner {

struct PersonalReminder {
  uint32_t id = 0;
  int64_t dueEpoch = 0;
  char text[161] {};
};

// Owned by the network task. A pending reminder is removed only after speech succeeds.
class PersonalReminders {
 public:
  static constexpr size_t capacity = 8;
  const std::array<PersonalReminder, capacity>& all() const { return items_; }
  bool add(int64_t due, const char* text, uint32_t& id) {
    if (due <= 0 || !text || !*text || std::strlen(text) >= sizeof(PersonalReminder::text)) return false;
    for (auto& item : items_) if (!item.id) {
      item.id = nextId_++; if (!nextId_) nextId_ = 1;
      item.dueEpoch = due;
      std::memcpy(item.text, text, std::strlen(text) + 1);
      id = item.id;
      return true;
    }
    return false;
  }
  bool restore(const PersonalReminder& item) {
    if (!item.id || item.dueEpoch <= 0 || !item.text[0] || !std::memchr(item.text, 0, sizeof(item.text))) return false;
    for (const auto& slot : items_) if (slot.id == item.id) return false;
    for (auto& slot : items_) if (!slot.id) {
      slot = item;
      if (nextId_ <= item.id) nextId_ = item.id + 1;
      if (!nextId_) nextId_ = 1;
      return true;
    }
    return false;
  }
  bool remove(uint32_t id) {
    if (!id || id == inFlight_) return false;
    for (auto& item : items_) if (item.id == id) { item = {}; return true; }
    return false;
  }
  bool takeDue(int64_t now, bool present, PersonalReminder& result) {
    if (!present || inFlight_ || now <= 0) return false;
    PersonalReminder* earliest = nullptr;
    for (auto& item : items_) if (item.id && item.dueEpoch <= now && (!earliest || item.dueEpoch < earliest->dueEpoch)) earliest = &item;
    if (!earliest) return false;
    result = *earliest; inFlight_ = result.id; return true;
  }
  bool finish(uint32_t id, bool delivered) {
    if (!id || id != inFlight_) return false;
    inFlight_ = 0;
    return !delivered || remove(id);
  }
  uint32_t inFlight() const { return inFlight_; }
 private:
  std::array<PersonalReminder, capacity> items_ {};
  uint32_t nextId_ = 1;
  uint32_t inFlight_ = 0;
};

class PresenceTransitions {
 public:
  bool update(bool valid, bool detected) {
    if (!valid) { known_ = false; return false; }
    const bool arrived = known_ && !last_ && detected;
    known_ = true; last_ = detected;
    return arrived;
  }
 private:
  bool known_ = false;
  bool last_ = false;
};

}  // namespace partner
