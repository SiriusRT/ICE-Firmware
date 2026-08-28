#pragma once
// drivers/sensors.h — 温度(DS18B20×4 + NTC) / 电压电流功率(INA226) / 风扇测速(TACH)

#include <Arduino.h>
#include <cstdint>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// I2C 总线互斥（sensors 与 display 共用 Wire）
extern SemaphoreHandle_t g_i2c_mutex;

struct SensorReadings {
  // 温度 0.1℃ 定点；无效时为 INT16_MIN
  int16_t cabinet1_10;
  int16_t cabinet2_10;
  int16_t cold_side_10;
  int16_t hot_side_10;
  int16_t board_10;
  // 电气量
  uint16_t voltage_cV;   // 0.01V
  uint16_t current_cA;   // 0.01A
  uint16_t power_w10;    // 0.1W
  bool valid;            // 关键传感器总体有效
  bool cabinet_ok, cold_ok, hot_ok, board_ok;
};

class Sensors {
 public:
  void init();
  // 阻塞约 800ms（DS18B20 转换），返回最新读数
  SensorReadings update();

  // 风扇转速（2 脉冲/转），滑动窗口，可高频调用
  uint16_t read_rpm();

  static void IRAM_ATTR tach_isr();

 private:
  float read_board_ntc();
  uint16_t current_rpm_ = 0;
};

// 单例
extern Sensors g_sensors;
