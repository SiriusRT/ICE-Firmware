#pragma once
// core/state.h — 框架无关的状态结构与常量（不依赖 Arduino / FreeRTOS）

#include <cstdint>
#include <cstddef>

namespace ice {

// ---- 数值范围（温度均为 0.1℃ 有符号定点）----
constexpr int16_t TARGET_MIN_10 = -200;      // -20.0℃
constexpr int16_t TARGET_MAX_10 = 200;       // +20.0℃
constexpr int16_t OVERTEMP_MIN_10 = 0;
constexpr int16_t OVERTEMP_MAX_10 = 1250;    // 125.0℃
constexpr int16_t OVERTEMP_DEFAULT_10 = 600; // 60.0℃
constexpr int16_t FAN_CURVE_TEMP_MIN_10 = -550;
constexpr int16_t FAN_CURVE_TEMP_MAX_10 = 1250;
constexpr uint16_t FAN_RPM_MAX = 4200;
constexpr uint8_t FAN_CURVE_POINTS = 4;
constexpr uint16_t FAN_RPM_MIN = 600;

constexpr int16_t KP_MIN = 0, KP_MAX = 10000;   // ×100
constexpr int16_t KI_MIN = 0, KI_MAX = 1000;
constexpr int16_t KD_MIN = 0, KD_MAX = 10000;

constexpr float V_UNDER_V = 10.5f;             // 欠压保护（V）
constexpr float V_OVER_V = 24.0f;              // 过压保护（V）
constexpr float POWER_LIMIT_W = 70.0f;         // 60W + 裕量
constexpr float USB_PD_VOLT_THRESHOLD = 14.0f; // ≥此电压视为 USB-PD(20V)，否则 12V DC

// ---- 折线断点（默认）----
struct FanCurvePoint {
  int16_t temp_10;   // 0.1℃
  uint16_t rpm;      // RPM
};

constexpr FanCurvePoint DEFAULT_CURVE[FAN_CURVE_POINTS] = {
    {350, 800}, {450, 1500}, {550, 2600}, {650, 4200}};

struct Config {
  uint8_t version = 2;
  uint8_t alarm = 0;
  bool cooling_enable = false;
  int16_t target_temp_10 = 50;             // 5.0℃
  int16_t overtemp_10 = OVERTEMP_DEFAULT_10;
  int16_t kp100 = 850;                     // Kp 8.5
  int16_t ki100 = 50;                      // Ki 0.5
  int16_t kd100 = 20;                      // Kd 0.2
  uint8_t fan_loop_mode = 0;               // 0=开环, 1=闭环
  FanCurvePoint fan_curve[FAN_CURVE_POINTS] = {
      {350, 800}, {450, 1500}, {550, 2600}, {650, 4200}};
  uint32_t serial = 1;
  // ---- 联网（远程）配置 ----
  char wifi_ssid[33] = "";
  char wifi_pass[65] = "";
  char mqtt_host[65] = "";
  uint16_t mqtt_port = 1883;
  char mqtt_user[33] = "";
  char mqtt_pass[33] = "";
};

// ---- 遥测状态位（协议 §3）----
enum StateBits : uint8_t {
  SB_COOLING = 0x01,
  SB_FAN = 0x02,
  SB_TRIP = 0x04,
  SB_ALARM = 0x08,
  SB_POWER_DC = 0x10,  // bit4: 1=12V DC, 0=USB-PD
  SB_KNOB = 0x20,
};

// ---- 报警码（协议 §3）----
enum AlarmCode : uint8_t {
  AL_NONE = 0,
  AL_OVERTEMP_TRIP = 1,
  AL_UNDERVOLT = 2,
  AL_OVERVOLT = 3,
  AL_SENSOR_FAULT = 4,
  AL_OVERPOWER = 5,
  AL_FAN_STALL = 6,
};

struct Telemetry {
  uint8_t state_bits = 0;
  int16_t cabinet_temp_10 = 0;
  int16_t hot_side_temp_10 = 0;
  int16_t target_temp_10 = 50;
  uint16_t power_w10 = 0;
  uint16_t voltage_cV = 0;   // 0.01V
  uint16_t current_cA = 0;   // 0.01A
  uint16_t fan_rpm = 0;
  uint8_t peltier_pwm = 0;
  uint8_t alarm = 0;
  // ---- FFB5 扩展 ----
  int16_t cold_side_temp_10 = 0;
  int16_t cabinet2_temp_10 = 0;
  int16_t board_temp_10 = 0;
  uint16_t fan_target_rpm = 0;
  uint8_t fan_loop_mode = 0;
  uint8_t fan_duty = 0;
  uint8_t cmd_ack = 0;
  uint16_t uptime_s = 0;
};

// ---- 工具 ----
inline int16_t clamp_s16(int16_t v, int16_t lo, int16_t hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}
inline uint16_t clamp_u16(uint16_t v, uint16_t lo, uint16_t hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}
inline uint8_t clamp_u8(uint8_t v, uint8_t lo, uint8_t hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

// 折线温度严格递增则有效
bool fan_curve_valid(const FanCurvePoint* pts, uint8_t n);

// 将所有字段钳位到合法范围（含折线校验，非法则复位默认折线）
void config_sanitize(Config& cfg);

}  // namespace ice
