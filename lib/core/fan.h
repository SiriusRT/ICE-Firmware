#pragma once
// core/fan.h — 风扇控制：折线插值 + 开环(占空比映射) / 闭环(PI 追转速)，框架无关

#include <cstdint>
#include "state.h"

namespace ice {

class FanController {
 public:
  void reset();

  // 热侧温度(0.1℃) → 目标转速（折线线性插值，超出末断点沿用末断点）
  uint16_t curve_lookup(int16_t hot_temp_10, const FanCurvePoint* curve, uint8_t n) const;

  // 开环：目标转速 → 占空比（线性映射 FAN_RPM_MIN..FAN_RPM_MAX → 下限..100）
  uint8_t duty_open(uint16_t target_rpm) const;

  // 闭环：目标转速 + 实测转速 → 占空比（PI 反馈）
  uint8_t duty_closed(uint16_t target_rpm, uint16_t measured_rpm, float dt);

  void set_mode(uint8_t mode) { mode_ = mode; }
  uint8_t mode() const { return mode_; }
  uint8_t current_duty() const { return duty_; }
  uint16_t current_target_rpm() const { return target_rpm_; }

  // 统一入口：按模式计算占空比
  uint8_t compute(uint16_t target_rpm, uint16_t measured_rpm, float dt);

 private:
  uint8_t mode_ = 0;          // 0=开环, 1=闭环
  uint8_t duty_ = 0;
  uint16_t target_rpm_ = 0;
  float integral_ = 0;
  float prev_err_ = 0;
};

}  // namespace ice
