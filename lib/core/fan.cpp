// core/fan.cpp — 风扇控制实现，框架无关

#include "fan.h"

namespace ice {

constexpr uint8_t FAN_DUTY_MIN = 20;       // 4针风扇启动占空比下限（可校准）
constexpr float FAN_PI_KP = 0.020f;        // %/RPM
constexpr float FAN_PI_KI = 0.004f;

void FanController::reset() {
  mode_ = 0;
  duty_ = 0;
  target_rpm_ = 0;
  integral_ = 0;
  prev_err_ = 0;
}

uint16_t FanController::curve_lookup(int16_t hot_temp_10, const FanCurvePoint* curve,
                                     uint8_t n) const {
  if (!curve || n == 0) return 0;
  if (n == 1) return curve[0].rpm;

  if (hot_temp_10 <= curve[0].temp_10) return curve[0].rpm;
  if (hot_temp_10 >= curve[n - 1].temp_10) return curve[n - 1].rpm;

  for (uint8_t i = 1; i < n; ++i) {
    if (hot_temp_10 <= curve[i].temp_10) {
      int32_t t0 = curve[i - 1].temp_10;
      int32_t t1 = curve[i].temp_10;
      int32_t r0 = curve[i - 1].rpm;
      int32_t r1 = curve[i].rpm;
      if (t1 <= t0) return r1;  // 防御：非法折线
      int32_t rpm = r0 + (r1 - r0) * (hot_temp_10 - t0) / (t1 - t0);
      return (uint16_t)(rpm < 0 ? 0 : rpm);
    }
  }
  return curve[n - 1].rpm;
}

uint8_t FanController::duty_open(uint16_t target_rpm) const {
  if (target_rpm <= FAN_RPM_MIN) return FAN_DUTY_MIN;
  if (target_rpm >= FAN_RPM_MAX) return 100;
  uint16_t span = FAN_RPM_MAX - FAN_RPM_MIN;
  uint8_t d = FAN_DUTY_MIN + (uint8_t)((target_rpm - FAN_RPM_MIN) * (100 - FAN_DUTY_MIN) / span);
  return d > 100 ? 100 : d;
}

uint8_t FanController::duty_closed(uint16_t target_rpm, uint16_t measured_rpm, float dt) {
  float err = (float)target_rpm - (float)measured_rpm;
  integral_ += err * dt;
  integral_ = integral_ > 400 ? 400 : (integral_ < -400 ? -400 : integral_);
  float d = duty_ + FAN_PI_KP * err + FAN_PI_KI * integral_;
  if (d < FAN_DUTY_MIN) {
    d = FAN_DUTY_MIN;
    integral_ -= err * dt * 0.5f;  // 抗饱和
  } else if (d > 100) {
    d = 100;
    integral_ -= err * dt * 0.5f;
  }
  prev_err_ = err;
  return (uint8_t)d;
}

uint8_t FanController::compute(uint16_t target_rpm, uint16_t measured_rpm, float dt) {
  target_rpm_ = target_rpm;
  if (mode_) {
    duty_ = duty_closed(target_rpm, measured_rpm, dt);
  } else {
    duty_ = duty_open(target_rpm);
    integral_ = 0;
  }
  return duty_;
}

}  // namespace ice
