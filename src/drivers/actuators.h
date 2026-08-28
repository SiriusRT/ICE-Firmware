#pragma once
// drivers/actuators.h — PWM 输出（半导体制冷片 + 4针风扇）

#include <cstdint>

class Actuators {
 public:
  void init();
  void set_peltier_pwm(uint8_t percent);  // 0..100
  void set_fan_pwm(uint8_t percent);      // 0..100
};

extern Actuators g_actuators;
