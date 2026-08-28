// drivers/actuators.cpp — PWM 输出（半导体制冷片 + 4针风扇，LEDC）

#include "actuators.h"
#include <Arduino.h>
#include "app_config.h"

void Actuators::init() {
  ledcSetup(LEDC_CH_PELTIER, PELTIER_PWM_FREQ, PWM_RES_BITS);
  ledcAttachPin(PIN_PELTIER_PWM, LEDC_CH_PELTIER);
  ledcSetup(LEDC_CH_FAN, FAN_PWM_FREQ, PWM_RES_BITS);
  ledcAttachPin(PIN_FAN_PWM, LEDC_CH_FAN);
  set_peltier_pwm(0);
  set_fan_pwm(0);
}

void Actuators::set_peltier_pwm(uint8_t percent) {
  if (percent > 100) percent = 100;
  ledcWrite(LEDC_CH_PELTIER, ((uint32_t)percent * ((1u << PWM_RES_BITS) - 1)) / 100u);
}

void Actuators::set_fan_pwm(uint8_t percent) {
  if (percent > 100) percent = 100;
  ledcWrite(LEDC_CH_FAN, ((uint32_t)percent * ((1u << PWM_RES_BITS) - 1)) / 100u);
}

Actuators g_actuators;
