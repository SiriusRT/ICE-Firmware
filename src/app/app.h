#pragma once
// app/app.h — 应用共享状态（互斥保护）+ 命令分发
// 唯一持有 Config/Telemetry 的单点，任务间通过快照读写

#include <cstdint>
#include <cstddef>
#include "codec.h"
#include "state.h"
#include "drivers/sensors.h"

struct AppSnapshot {
  ice::Config cfg;
  ice::Telemetry tel;
  bool knob_active = false;
  bool sensors_ok = true;
};

void app_init();
void app_snapshot(AppSnapshot& out);

// ---- 任务写入 ----
void app_set_sensor(const SensorReadings& r);
void app_set_control(uint8_t pwm, uint8_t state_bits, uint8_t alarm);
void app_set_fan(uint8_t duty, uint16_t target_rpm, uint16_t rpm, uint8_t mode);
void app_knob_set_target(int16_t target_10);
void app_set_knob_active(bool active);
void app_set_cooling(bool on);
void app_set_fan_loop(uint8_t mode);

// ---- BLE 命令（按协议 v1.1 校验/钳位，回写 cmd_ack）----
void app_apply_command_frame(const uint8_t* data, size_t len);

// 清除报警请求（控制任务消费）
bool app_take_clear_alarm();

// 遥测变化标记（立即上报）
bool app_take_tel_dirty();

// 配置落盘控制
bool app_config_dirty();
void app_mark_config_clean();
uint32_t app_config_changed_ms();
