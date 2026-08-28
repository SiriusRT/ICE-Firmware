#pragma once
// drivers/display.h — OLED 显示（U8g2 SSD1306 128x64）

#include "state.h"

class Display {
 public:
  void init();
  void render(const ice::Config& cfg, const ice::Telemetry& tel, bool ble_connected);
};

extern Display g_display;
