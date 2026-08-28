// core/state.cpp — 框架无关状态工具

#include "state.h"

namespace ice {

bool fan_curve_valid(const FanCurvePoint* pts, uint8_t n) {
  if (!pts || n < 2) return false;
  int16_t prev = pts[0].temp_10;
  for (uint8_t i = 1; i < n; ++i) {
    if (pts[i].temp_10 <= prev) return false;
    prev = pts[i].temp_10;
  }
  return true;
}

void config_sanitize(Config& cfg) {
  cfg.target_temp_10 = clamp_s16(cfg.target_temp_10, TARGET_MIN_10, TARGET_MAX_10);
  cfg.overtemp_10 = clamp_s16(cfg.overtemp_10, OVERTEMP_MIN_10, OVERTEMP_MAX_10);
  cfg.kp100 = clamp_s16(cfg.kp100, KP_MIN, KP_MAX);
  cfg.ki100 = clamp_s16(cfg.ki100, KI_MIN, KI_MAX);
  cfg.kd100 = clamp_s16(cfg.kd100, KD_MIN, KD_MAX);
  cfg.fan_loop_mode = cfg.fan_loop_mode ? 1 : 0;

  bool ok = true;
  for (uint8_t i = 0; i < FAN_CURVE_POINTS; ++i) {
    cfg.fan_curve[i].temp_10 =
        clamp_s16(cfg.fan_curve[i].temp_10, FAN_CURVE_TEMP_MIN_10, FAN_CURVE_TEMP_MAX_10);
    cfg.fan_curve[i].rpm = clamp_u16(cfg.fan_curve[i].rpm, 0, FAN_RPM_MAX);
  }
  if (!fan_curve_valid(cfg.fan_curve, FAN_CURVE_POINTS)) {
    for (uint8_t i = 0; i < FAN_CURVE_POINTS; ++i) {
      cfg.fan_curve[i] = DEFAULT_CURVE[i];
    }
    (void)ok;
  }
}

}  // namespace ice
