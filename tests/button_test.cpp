#include "domain/Button.h"

#include <cassert>
#include <iostream>

using namespace partner;

int main() {
  Button button;
  assert(button.update(true, 100) == ButtonAction::None);
  assert(button.update(false, 110) == ButtonAction::None);  // Bounce.
  assert(button.update(true, 115) == ButtonAction::None);
  assert(button.update(true, 145) == ButtonAction::None);
  assert(button.update(false, 200) == ButtonAction::None);
  assert(button.update(false, 230) == ButtonAction::Click);
  assert(button.update(false, 240) == ButtonAction::None);
  assert(button.update(true, 300) == ButtonAction::None);
  assert(button.update(true, 330) == ButtonAction::None);
  assert(button.update(true, 1130) == ButtonAction::Hold);
  assert(button.update(true, 2000) == ButtonAction::None);
  assert(button.update(false, 2010) == ButtonAction::None);
  assert(button.update(false, 2040) == ButtonAction::None);  // Hold has no click on release.
  Button rollover;
  assert(rollover.update(true, UINT32_MAX - 40) == ButtonAction::None);
  assert(rollover.update(true, UINT32_MAX - 10) == ButtonAction::None);
  assert(rollover.update(true, 800) == ButtonAction::Hold);
  std::cout << "button: debounce, click, hold, release and millis rollover passed\n";
}
