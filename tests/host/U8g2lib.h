#pragma once

// Host wrapper for the actual U8g2 C rasterizer; no drawing operations are mocked.
#include <u8g2.h>

class U8G2 {
 public:
  U8G2() {
    u8g2_Setup_ssd1306_i2c_128x64_noname_f(&buffer_, U8G2_R0, noop, noop);
  }
  void clearBuffer() { u8g2_ClearBuffer(&buffer_); }
  void sendBuffer() { ++presented; }
  void setPowerSave(uint8_t value) { powerSave = value; ++powerChanges; }
  uint8_t* getBufferPtr() { return u8g2_GetBufferPtr(&buffer_); }
  void setMaxClipWindow() { u8g2_SetMaxClipWindow(&buffer_); }
  void setClipWindow(int x0, int y0, int x1, int y1) { u8g2_SetClipWindow(&buffer_, x0, y0, x1, y1); }
  void setDrawColor(int color) { u8g2_SetDrawColor(&buffer_, color); }
  void setFont(const uint8_t* font) { u8g2_SetFont(&buffer_, font); }
  int getStrWidth(const char* text) { return u8g2_GetStrWidth(&buffer_, text); }
  void drawStr(int x, int y, const char* text) { u8g2_DrawStr(&buffer_, x, y, text); }
  void drawPixel(int x, int y) { u8g2_DrawPixel(&buffer_, x, y); }
  void drawLine(int x0, int y0, int x1, int y1) { u8g2_DrawLine(&buffer_, x0, y0, x1, y1); }
  void drawHLine(int x, int y, int width) { u8g2_DrawHLine(&buffer_, x, y, width); }
  void drawBox(int x, int y, int width, int height) { u8g2_DrawBox(&buffer_, x, y, width, height); }
  void drawFrame(int x, int y, int width, int height) { u8g2_DrawFrame(&buffer_, x, y, width, height); }
  void drawRBox(int x, int y, int width, int height, int radius) { u8g2_DrawRBox(&buffer_, x, y, width, height, radius); }
  void drawTriangle(int x0, int y0, int x1, int y1, int x2, int y2) { u8g2_DrawTriangle(&buffer_, x0, y0, x1, y1, x2, y2); }
  const uint8_t* pixels() { return u8g2_GetBufferPtr(&buffer_); }
  unsigned presented = 0;
  unsigned powerChanges = 0;
  bool powerSave = false;

 private:
  static uint8_t noop(u8x8_t*, uint8_t, uint8_t, void*) { return 1; }
  u8g2_t buffer_ {};
};
