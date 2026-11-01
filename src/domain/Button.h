#pragma once

#include <cstdint>

namespace partner {

enum class ButtonAction { None, Click, Hold };

class Button {
 public:
  ButtonAction update(bool pressed, uint32_t nowMs) {
    if (pressed != raw_) {
      raw_ = pressed;
      changedAt_ = nowMs;
    }
    if (nowMs - changedAt_ >= 30 && stable_ != raw_) {
      stable_ = raw_;
      if (stable_) {
        pressedAt_ = nowMs;
        held_ = false;
      } else if (!held_) {
        return ButtonAction::Click;
      }
    }
    if (stable_ && !held_ && nowMs - pressedAt_ >= 800) {
      held_ = true;
      return ButtonAction::Hold;
    }
    return ButtonAction::None;
  }

 private:
  bool raw_ = false;
  bool stable_ = false;
  bool held_ = false;
  uint32_t changedAt_ = 0;
  uint32_t pressedAt_ = 0;
};

}  // namespace partner
