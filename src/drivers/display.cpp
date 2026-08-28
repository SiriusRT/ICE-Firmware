// drivers/display.cpp — OLED 显示（U8g2 SSD1306 128x64 HW_I2C）

#include "display.h"
#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <cstdio>

#include "app_config.h"
#include "sensors.h"

static U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, PIN_OLED_SCL, PIN_OLED_SDA);

void Display::init() {
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  u8g2.begin();
}

void Display::render(const ice::Config& cfg, const ice::Telemetry& tel, bool ble_connected) {
  char buf[24];

  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_6x12_tf);
    u8g2.setCursor(0, 12);

    // 第 1 行：目标 / 柜内
    snprintf(buf, sizeof(buf), "Tgt %5.1fC  In %5.1fC", cfg.target_temp_10 / 10.0f,
             tel.cabinet_temp_10 / 10.0f);
    u8g2.drawStr(0, 12, buf);

    // 第 2 行：热侧 / 功率
    snprintf(buf, sizeof(buf), "Hot %5.1fC  %5.1fW", tel.hot_side_temp_10 / 10.0f,
             tel.power_w10 / 10.0f);
    u8g2.drawStr(0, 25, buf);

    // 第 3 行：风扇
    snprintf(buf, sizeof(buf), "Fan %s %3u%% %5u", tel.fan_loop_mode ? "CL" : "OL", tel.fan_duty,
             tel.fan_rpm);
    u8g2.drawStr(0, 38, buf);

    // 第 4 行：状态
    if (tel.alarm != ice::AL_NONE) {
      snprintf(buf, sizeof(buf), "ALARM %d", tel.alarm);
    } else if (ble_connected) {
      snprintf(buf, sizeof(buf), "BLE OK %5.1fC", tel.target_temp_10 / 10.0f);
    } else {
      snprintf(buf, sizeof(buf), "Standby BLE--");
    }
    u8g2.drawStr(0, 51, buf);

    if (xSemaphoreTake(g_i2c_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
      u8g2.sendBuffer();
      xSemaphoreGive(g_i2c_mutex);
    }
  } while (u8g2.nextPage());
}

Display g_display;
