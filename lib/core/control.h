#pragma once
// core/control.h — 温控状态机 + PID + 保护锁存，框架无关

#include <cstdint>
#include "state.h"

namespace ice {

// 增量式 PID
struct Pid {
  float kp = 0, ki = 0, kd = 0;
  float integral = 0;
  float prev_err = 0;
  bool inited = false;

  void reset(float kp_ = 0, float ki_ = 0, float kd_ = 0) {
    kp = kp_;
    ki = ki_;
    kd = kd_;
    integral = 0;
    prev_err = 0;
    inited = true;
  }

  // 制冷型 PID：输出正值 = “需要更多制冷”。
  // err = input - setpoint（测量高于目标 → 输出增大）。返回 0..100
  float update(float setpoint, float input, float dt) {
    if (!inited) {  // 仅初始化状态量，不清零外部设置的增益
      integral = 0;
      prev_err = 0;
      inited = true;
    }
    float err = input - setpoint;
    integral += err * dt;
    integral = integral > 400 ? 400 : (integral < -400 ? -400 : integral);
    float d = dt > 0 ? (err - prev_err) / dt : 0;
    prev_err = err;
    float out = kp * err + ki * integral + kd * d;
    return out > 100 ? 100 : (out < 0 ? 0 : out);
  }
};

struct ControlInputs {
  float cabinet_temp = 0;    // ℃（两路柜内取均值）
  float hot_side_temp = 0;   // ℃
  float voltage = 0;         // V
  float current = 0;         // A
  float power_w = 0;         // W
  float fan_rpm = 0;         // RPM
  float target_temp = 0;     // ℃
  float overtemp = 60.0f;    // ℃
  float kp = 8.5f, ki = 0.5f, kd = 0.2f;
  bool sensors_ok = true;
  bool cooling_enable = false;
  bool usb_pd = true;        // true=USB-PD, false=12V DC
  bool knob_pressed = false;
  uint8_t max_peltier_pwm = 100;  // 供电能力约束的制冷 PWM 上限（0..100）
};

struct ControlOutputs {
  uint8_t peltier_pwm = 0;   // 0..100
  uint8_t state_bits = 0;
  uint8_t alarm = AL_NONE;
};

class ControlLoop {
 public:
  void reset();
  // 每周期调用（dt 秒）。故障在 out.alarm/state_bits 中体现，过热跳闸为锁存。
  void update(const ControlInputs& in, float dt, ControlOutputs& out);
  bool trip_latched() const { return trip_; }
  // 清除跳闸锁存：故障已消除则清除并返回 true，否则保持
  bool clear_alarm();
  float pid_integral() const { return pid_.integral; }

 private:
  Pid pid_;
  bool trip_ = false;
  float stall_accum_s_ = 0;
};

}  // namespace ice
