// core/control.cpp — 温控状态机 + PID + 保护锁存，框架无关

#include "control.h"

namespace ice {

constexpr float FAN_STALL_RPM = 300.0f;
constexpr float FAN_STALL_WINDOW_S = 5.0f;

void ControlLoop::reset() {
  pid_.reset();
  trip_ = false;
  stall_accum_s_ = 0;
}

bool ControlLoop::clear_alarm() {
  // 仅当过热故障已消失才清除锁存。update() 每周期依据 hot_side_temp 维持 trip_，
  // 因此这里直接尝试解除，若仍超温，下一周期会再次置位。
  if (trip_) trip_ = false;
  stall_accum_s_ = 0;
  return !trip_;
}

void ControlLoop::update(const ControlInputs& in, float dt, ControlOutputs& out) {
  uint8_t alarm = AL_NONE;
  uint8_t bits = 0;

  // ---- 故障判定 ----
  bool overtemp = in.hot_side_temp >= in.overtemp;
  bool undervolt = in.voltage > 0.5f && in.voltage < V_UNDER_V;
  bool overvolt = in.voltage > V_OVER_V;
  bool overpower = in.power_w > POWER_LIMIT_W;
  bool sensor_fault = !in.sensors_ok;

  // 风扇堵转：制冷使能时转速过低持续 N 秒
  if (in.cooling_enable && in.fan_rpm > 0.1f && in.fan_rpm < FAN_STALL_RPM) {
    stall_accum_s_ += dt;
  } else {
    stall_accum_s_ = 0;
  }
  bool fan_stall = in.cooling_enable && stall_accum_s_ >= FAN_STALL_WINDOW_S;

  // 过热跳闸：锁存（即使后续温度回落也保持，直到清除命令且故障消失）
  if (overtemp) trip_ = true;

  if (trip_) {
    alarm = AL_OVERTEMP_TRIP;
  } else if (undervolt) {
    alarm = AL_UNDERVOLT;
  } else if (overvolt) {
    alarm = AL_OVERVOLT;
  } else if (sensor_fault) {
    alarm = AL_SENSOR_FAULT;
  } else if (overpower) {
    alarm = AL_OVERPOWER;
  } else if (fan_stall) {
    alarm = AL_FAN_STALL;
  }

  // ---- 制冷输出 ----
  bool cooling_active = in.cooling_enable && !trip_ && alarm == AL_NONE;
  uint8_t pwm = 0;
  if (cooling_active) {
    pid_.kp = in.kp;
    pid_.ki = in.ki;
    pid_.kd = in.kd;
    pwm = (uint8_t)pid_.update(in.target_temp, in.cabinet_temp, dt);
  } else {
    pid_.integral = 0;  // 停机时复位积分，防止 windup
    pid_.prev_err = 0;
  }

  // ---- 状态位 ----
  bits |= (cooling_active ? SB_COOLING : 0);
  bits |= (in.cooling_enable ? SB_FAN : 0);
  bits |= (trip_ ? SB_TRIP : 0);
  bits |= (alarm != AL_NONE ? SB_ALARM : 0);
  bits |= (in.usb_pd ? 0 : SB_POWER_DC);
  bits |= (in.knob_pressed ? SB_KNOB : 0);

  out.peltier_pwm = pwm;
  out.state_bits = bits;
  out.alarm = alarm;
}

}  // namespace ice
