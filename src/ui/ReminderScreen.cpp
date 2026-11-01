#include "ReminderScreen.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace partner {

void ReminderScreen::begin() {
  face_.CenterY = 31;
  face_.RandomBehavior = false;
  face_.RandomLook = false;
  face_.LeftEye.ApplyPreset(Preset_Normal);
  face_.RightEye.ApplyPreset(Preset_Normal);
  face_.Expression.GoTo_Normal();
  Transformation pose;
  pose.ScaleX = 0.8f;
  pose.ScaleY = 0.65f;
  face_.LeftEye.Transformation.SetDestin(pose);
  face_.RightEye.Transformation.SetDestin(pose);
}

void ReminderScreen::expression(const DeviceSnapshot& state, uint32_t nowMs) {
  if (state.arrivalSequence != seenArrival_) {
    seenArrival_ = state.arrivalSequence; arrivalAtMs_ = nowMs; arrivalActive_ = true;
  }
  if (arrivalActive_ && nowMs - arrivalAtMs_ >= 4000) arrivalActive_ = false;
  if (state.phase == ReminderPhase::Idle && state.microphoneLevel >= 20) {
    heardAtMs_ = nowMs; heardActive_ = true;
  }
  if (heardActive_ && nowMs - heardAtMs_ >= 1200) heardActive_ = false;
  int desired = static_cast<int>(state.phase);
  if (state.expression != Expression::Auto) desired = 100 + static_cast<int>(state.expression);
  else if (state.phase == ReminderPhase::Idle && arrivalActive_) desired = 5;
  else if (state.phase == ReminderPhase::Idle && heardActive_) desired = 6;
  if (desired == expression_) return;
  expression_ = desired;
  switch (desired) {
    case 1: face_.Expression.GoTo_Focused(); break;
    case 2: face_.Expression.GoTo_Surprised(); break;
    case 3: case 5: face_.Expression.GoTo_Happy(); break;
    case 4: face_.Expression.GoTo_Worried(); break;
    case 6: face_.Expression.GoTo_Surprised(); break;
    case 101: face_.Expression.GoTo_Normal(); break;
    case 102: face_.Expression.GoTo_Happy(); break;
    case 103: face_.Expression.GoTo_Sad(); break;
    case 104: face_.Expression.GoTo_Worried(); break;
    case 105: face_.Expression.GoTo_Focused(); break;
    case 106: face_.Expression.GoTo_Surprised(); break;
    case 107: face_.Expression.GoTo_Angry(); break;
    case 108: face_.Expression.GoTo_Sleepy(); break;
    default: face_.Expression.GoTo_Normal(); break;
  }
}

// Wi-Fi symbol follows the supplied Lopaka icon reference.
void ReminderScreen::wifiIcon(bool connected) {
  display_.drawPixel(6, 9);
  display_.drawLine(3, 6, 6, 4);
  display_.drawLine(6, 4, 9, 6);
  display_.drawLine(0, 3, 6, 0);
  display_.drawLine(6, 0, 12, 3);
  if (!connected) display_.drawLine(1, 1, 11, 10);
}

void ReminderScreen::header(const DeviceSnapshot& state) {
  wifiIcon(state.connected);
  display_.setFont(u8g2_font_5x7_tf);
  if (state.sensor.online && state.sensor.hasClimate) {
    char humidity[10];
    snprintf(humidity, sizeof(humidity), "H:%u%%", (state.sensor.humidity10 + 5) / 10);
    display_.drawStr(17, 8, humidity);
  }
  if (state.sensor.online && state.sensor.hasPresence) {
    char distance[10] = "-";
    if (state.sensor.detected && state.sensor.hasDistance)
      snprintf(distance, sizeof(distance), "%ucm", state.sensor.distanceCm);
    display_.drawStr(55, 8, distance);
  }
  char time[16] = "--:--";
  if (state.time.valid) snprintf(time, sizeof(time), "%02d:%02d", state.time.hour, state.time.minute);
  display_.drawStr(126 - display_.getStrWidth(time), 8, time);
  display_.drawHLine(0, 12, 128);
}

void ReminderScreen::footer(const DeviceSnapshot& state) {
  display_.drawHLine(0, 51, 128);
  display_.setFont(u8g2_font_5x7_tf);
  display_.drawStr(1, 62, "MIC");
  const unsigned level = state.microphoneLevel > 100 ? 100 : state.microphoneLevel;
  display_.drawFrame(23, 55, 77, 8);
  if (level) display_.drawBox(25, 57, level * 73 / 100, 4);
  char text[8];
  snprintf(text, sizeof(text), "%u%%", level);
  display_.drawStr(126 - display_.getStrWidth(text), 62, text);
}

void ReminderScreen::clock(const LocalTime& time) {
  char hours[8] = "--:--";
  if (time.valid) snprintf(hours, sizeof(hours), "%02d:%02d", time.hour, time.minute);
  display_.setFont(u8g2_font_logisoso20_tn);
  display_.drawStr((128 - display_.getStrWidth(hours)) / 2, 37, hours);
  display_.setFont(u8g2_font_5x7_tf);
  char date[24] = "Menunggu NTP";
  if (time.valid) snprintf(date, sizeof(date), "%02d/%02d/%04d", time.day, time.month, time.year);
  display_.drawStr((128 - display_.getStrWidth(date)) / 2, 48, date);
}

void ReminderScreen::sensors(const SensorSnapshot& sensor) {
  display_.setFont(u8g2_font_5x7_tf);
  if (!sensor.online) {
    display_.drawStr(1, 27, "SENSOR: MENUNGGU");
    display_.drawStr(1, 41, "ESP-NOW");
    return;
  }
  char line[32];
  snprintf(line, sizeof(line), "NODE: %.18s", sensor.name[0] ? sensor.name : "ONLINE");
  display_.drawStr(1, 22, line);
  if (sensor.hasClimate) {
    const int temperature = sensor.temperature10;
    const unsigned magnitude = static_cast<unsigned>(std::abs(temperature));
    snprintf(line, sizeof(line), "T:%s%u.%uC  H:%u.%u%%", temperature < 0 ? "-" : "",
             magnitude / 10, magnitude % 10, sensor.humidity10 / 10, sensor.humidity10 % 10);
    display_.drawStr(1, 34, line);
  } else {
    display_.drawStr(1, 34, "T:--.-C  H:--.-%");
  }
  if (!sensor.hasPresence) std::snprintf(line, sizeof(line), "HADIR: --");
  else if (!sensor.detected) std::snprintf(line, sizeof(line), "HADIR: TIDAK");
  else if (sensor.hasDistance) std::snprintf(line, sizeof(line), "HADIR: YA %ucm", sensor.distanceCm);
  else std::snprintf(line, sizeof(line), "HADIR: YA");
  display_.drawStr(1, 46, line);
}

void ReminderScreen::shiftPixels(uint32_t nowMs) {
  // Move the complete framebuffer by up to one pixel once per minute.
  // This covers static header/footer pixels as well as the animated face.
  static constexpr int8_t offsets[][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1},
                                         {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}};
  const auto& offset = offsets[(nowMs / 60000) % 9];
  uint8_t* pixels = display_.getBufferPtr();
  if (offset[0]) {
    for (unsigned page = 0; page < 8; ++page) {
      uint8_t* row = pixels + page * 128;
      if (offset[0] > 0) { std::memmove(row + 1, row, 127); row[0] = 0; }
      else { std::memmove(row, row + 1, 127); row[127] = 0; }
    }
  }
  if (offset[1] > 0) {
    for (int page = 7; page >= 0; --page)
      for (int x = 0; x < 128; ++x)
        pixels[page * 128 + x] = (pixels[page * 128 + x] << 1) |
            (page ? pixels[(page - 1) * 128 + x] >> 7 : 0);
  } else if (offset[1] < 0) {
    for (int page = 0; page < 8; ++page)
      for (int x = 0; x < 128; ++x)
        pixels[page * 128 + x] = (pixels[page * 128 + x] >> 1) |
            (page < 7 ? pixels[(page + 1) * 128 + x] << 7 : 0);
  }
}

void ReminderScreen::render(const DeviceSnapshot& state, uint32_t nowMs) {
  const bool shouldPowerOff = state.sensor.online && state.sensor.hasPresence && !state.sensor.detected;
  if (shouldPowerOff != poweredOff_) {
    poweredOff_ = shouldPowerOff;
    display_.setPowerSave(poweredOff_ ? 1 : 0);
  }
  if (poweredOff_) return;
  display_.clearBuffer();
  display_.setMaxClipWindow();
  display_.setDrawColor(1);
  header(state);
  footer(state);
  expression(state, nowMs);
  if (page_ == Page::Clock) {
    clock(state.time);
  } else if (page_ == Page::Sensors) {
    sensors(state.sensor);
  } else {
    if (nowMs - lookAtMs_ >= 3000) {
      lookAtMs_ = nowMs;
      Transformation pose;
      pose.ScaleX = 0.8f;
      pose.ScaleY = 0.65f;
      static constexpr int8_t looks[] = {0, -3, 1, 3, -1};
      pose.MoveX = looks[(nowMs / 3000) % 5];
      face_.LeftEye.Transformation.SetDestin(pose);
      face_.RightEye.Transformation.SetDestin(pose);
      face_.LeftEye.Transformation.Animation.Restart();
      face_.RightEye.Transformation.Animation.Restart();
    }
    if (state.phase == ReminderPhase::Speaking) {
      display_.drawRBox(59, 46, 10, (nowMs / 140) % 2 ? 2 : 4, 1);
    }
    // Clip the face and defer presentation until the full frame is shifted.
    display_.setClipWindow(0, 14, 128, 51);
    face_.Update(false);
    display_.setMaxClipWindow();
  }
  shiftPixels(nowMs);
  display_.sendBuffer();
}

}  // namespace partner
