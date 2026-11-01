#pragma once

#include <Face.h>
#include <U8g2lib.h>

#include "domain/DeviceSnapshot.h"

namespace partner {

class ReminderScreen {
 public:
  explicit ReminderScreen(U8G2& display) : display_(display), face_(&display, 128, 64, 36) {}
  void begin();
  void togglePage() { page_ = static_cast<Page>((static_cast<unsigned>(page_) + 1) % 3); }
  void setPage(Screen screen) { page_ = static_cast<Page>(screen); }
  void render(const DeviceSnapshot& state, uint32_t nowMs);
  bool poweredOff() const { return poweredOff_; }

 private:
  void header(const DeviceSnapshot& state);
  void footer(const DeviceSnapshot& state);
  void clock(const LocalTime& time);
  void sensors(const SensorSnapshot& sensor);
  void expression(const DeviceSnapshot& state, uint32_t nowMs);
  void wifiIcon(bool connected);
  void shiftPixels(uint32_t nowMs);

  U8G2& display_;
  Face face_;
  enum class Page : uint8_t { Face, Clock, Sensors };
  Page page_ = Page::Face;
  int expression_ = -1;
  uint32_t seenArrival_ = 0, arrivalAtMs_ = 0, heardAtMs_ = 0;
  bool arrivalActive_ = false, heardActive_ = false;
  uint32_t lookAtMs_ = 0;
  bool poweredOff_ = false;
};

}  // namespace partner
