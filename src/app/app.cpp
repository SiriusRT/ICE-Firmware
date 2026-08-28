// app/app.cpp — 应用共享状态与命令分发

#include "app.h"
#include <Arduino.h>
#include <limits.h>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "drivers/persist.h"

static SemaphoreHandle_t g_mutex = nullptr;
static ice::Config g_cfg;
static ice::Telemetry g_tel;
static volatile bool g_tel_dirty = false;
static volatile bool g_cfg_dirty = false;
static volatile uint32_t g_cfg_changed_ms = 0;
static volatile bool g_clear_alarm_pending = false;
static volatile bool g_knob_active = false;
static volatile bool g_sensors_ok = true;

static void lock() {
  if (g_mutex) xSemaphoreTake(g_mutex, portMAX_DELAY);
}
static void unlock() { xSemaphoreGive(g_mutex); }

void app_init() {
  if (!g_mutex) g_mutex = xSemaphoreCreateMutex();
  persist::load(g_cfg);
  ice::config_sanitize(g_cfg);
  g_tel.target_temp_10 = g_cfg.target_temp_10;
  g_tel.fan_loop_mode = g_cfg.fan_loop_mode;
  g_tel.cabinet_temp_10 = INT16_MIN;
  g_tel.hot_side_temp_10 = INT16_MIN;
}

void app_snapshot(AppSnapshot& out) {
  lock();
  out.cfg = g_cfg;
  out.tel = g_tel;
  out.knob_active = g_knob_active;
  out.sensors_ok = g_sensors_ok;
  unlock();
}

void app_set_sensor(const SensorReadings& r) {
  lock();
  g_tel.cabinet_temp_10 = r.cabinet1_10;
  g_tel.cabinet2_temp_10 = r.cabinet2_10;
  g_tel.cold_side_temp_10 = r.cold_side_10;
  g_tel.hot_side_temp_10 = r.hot_side_10;
  g_tel.board_temp_10 = r.board_10;
  g_tel.voltage_cV = r.voltage_cV;
  g_tel.current_cA = r.current_cA;
  g_tel.power_w10 = r.power_w10;
  g_sensors_ok = r.valid;
  g_tel_dirty = true;
  unlock();
}

void app_set_control(uint8_t pwm, uint8_t state_bits, uint8_t alarm) {
  lock();
  if (g_tel.peltier_pwm != pwm || g_tel.state_bits != state_bits || g_tel.alarm != alarm) {
    g_tel.peltier_pwm = pwm;
    g_tel.state_bits = state_bits;
    g_tel.alarm = alarm;
    g_cfg.alarm = alarm;  // 配置帧里的当前报警码保持同步
    g_tel_dirty = true;
  }
  unlock();
}

void app_set_fan(uint8_t duty, uint16_t target_rpm, uint16_t rpm, uint8_t mode) {
  lock();
  if (g_tel.fan_duty != duty || g_tel.fan_target_rpm != target_rpm || g_tel.fan_rpm != rpm ||
      g_tel.fan_loop_mode != mode) {
    g_tel.fan_duty = duty;
    g_tel.fan_target_rpm = target_rpm;
    g_tel.fan_rpm = rpm;
    g_tel.fan_loop_mode = mode;
    g_tel_dirty = true;
  }
  unlock();
}

void app_knob_set_target(int16_t target_10) {
  lock();
  int16_t t = ice::clamp_s16(target_10, ice::TARGET_MIN_10, ice::TARGET_MAX_10);
  if (g_cfg.target_temp_10 != t) {
    g_cfg.target_temp_10 = t;
    g_tel.target_temp_10 = t;
    g_cfg_dirty = true;
    g_cfg_changed_ms = (uint32_t)millis();
    g_tel_dirty = true;
  }
  unlock();
}

void app_set_knob_active(bool active) {
  lock();
  g_knob_active = active;
  unlock();
}

void app_set_cooling(bool on) {
  lock();
  if (g_cfg.cooling_enable != on) {
    g_cfg.cooling_enable = on;
    g_cfg_dirty = true;
    g_cfg_changed_ms = (uint32_t)millis();
    g_tel_dirty = true;
  }
  unlock();
}

void app_set_fan_loop(uint8_t mode) {
  lock();
  uint8_t m = mode ? 1 : 0;
  if (g_cfg.fan_loop_mode != m) {
    g_cfg.fan_loop_mode = m;
    g_tel.fan_loop_mode = m;
    g_cfg_dirty = true;
    g_cfg_changed_ms = (uint32_t)millis();
    g_tel_dirty = true;
  }
  unlock();
}

void app_set_wifi_config(const char* ssid, const char* pass) {
  lock();
  strncpy(g_cfg.wifi_ssid, ssid ? ssid : "", sizeof(g_cfg.wifi_ssid) - 1);
  g_cfg.wifi_ssid[sizeof(g_cfg.wifi_ssid) - 1] = '\0';
  strncpy(g_cfg.wifi_pass, pass ? pass : "", sizeof(g_cfg.wifi_pass) - 1);
  g_cfg.wifi_pass[sizeof(g_cfg.wifi_pass) - 1] = '\0';
  g_cfg_dirty = true;
  g_cfg_changed_ms = (uint32_t)millis();
  unlock();
}

void app_set_mqtt_config(const char* host, uint16_t port, const char* user, const char* pass) {
  lock();
  strncpy(g_cfg.mqtt_host, host ? host : "", sizeof(g_cfg.mqtt_host) - 1);
  g_cfg.mqtt_host[sizeof(g_cfg.mqtt_host) - 1] = '\0';
  g_cfg.mqtt_port = port ? port : 1883;
  strncpy(g_cfg.mqtt_user, user ? user : "", sizeof(g_cfg.mqtt_user) - 1);
  g_cfg.mqtt_user[sizeof(g_cfg.mqtt_user) - 1] = '\0';
  strncpy(g_cfg.mqtt_pass, pass ? pass : "", sizeof(g_cfg.mqtt_pass) - 1);
  g_cfg.mqtt_pass[sizeof(g_cfg.mqtt_pass) - 1] = '\0';
  g_cfg_dirty = true;
  g_cfg_changed_ms = (uint32_t)millis();
  unlock();
}

// 应用一条 BLE 命令帧：结构校验 + 语义钳位，回写 cmd_ack
void app_apply_command_frame(const uint8_t* data, size_t len) {
  ice::Command cmd;
  uint8_t ack = ice::parse_command(data, len, cmd);
  if (ack != ice::ACK_OK) {
    lock();
    g_tel.cmd_ack = ack;
    g_tel_dirty = true;
    unlock();
    return;
  }

  bool changed = false;
  uint8_t cmd_ack = ice::ACK_OK;

  switch (cmd.id) {
    case ice::CMD_SET_TARGET: {
      int16_t t = ice::clamp_s16(cmd.target_temp_10, ice::TARGET_MIN_10, ice::TARGET_MAX_10);
      if (t != cmd.target_temp_10) cmd_ack = ice::ACK_RANGE;
      if (g_cfg.target_temp_10 != t) {
        g_cfg.target_temp_10 = t;
        g_tel.target_temp_10 = t;
        changed = true;
      }
      break;
    }
    case ice::CMD_SET_PID: {
      int16_t kp = ice::clamp_s16(cmd.kp100, ice::KP_MIN, ice::KP_MAX);
      int16_t ki = ice::clamp_s16(cmd.ki100, ice::KI_MIN, ice::KI_MAX);
      int16_t kd = ice::clamp_s16(cmd.kd100, ice::KD_MIN, ice::KD_MAX);
      if (kp != cmd.kp100 || ki != cmd.ki100 || kd != cmd.kd100) cmd_ack = ice::ACK_RANGE;
      g_cfg.kp100 = kp;
      g_cfg.ki100 = ki;
      g_cfg.kd100 = kd;
      changed = true;
      break;
    }
    case ice::CMD_SET_FAN_CURVE: {
      if (!ice::fan_curve_valid(cmd.curve, ice::FAN_CURVE_POINTS)) {
        cmd_ack = ice::ACK_CURVE;  // 拒绝并保留旧配置
        break;
      }
      for (uint8_t i = 0; i < ice::FAN_CURVE_POINTS; ++i) {
        g_cfg.fan_curve[i] = cmd.curve[i];
      }
      changed = true;
      break;
    }
    case ice::CMD_CLEAR_ALARM:
      g_clear_alarm_pending = true;
      break;
    case ice::CMD_SET_COOLING:
      if (cmd.on > 1) {
        cmd_ack = ice::ACK_RANGE;
      } else if (g_cfg.cooling_enable != (cmd.on == 1)) {
        g_cfg.cooling_enable = cmd.on == 1;
        changed = true;
      }
      break;
    case ice::CMD_SET_OVERTEMP: {
      int16_t t = ice::clamp_s16(cmd.overtemp_10, ice::OVERTEMP_MIN_10, ice::OVERTEMP_MAX_10);
      if (t != cmd.overtemp_10) cmd_ack = ice::ACK_RANGE;
      if (g_cfg.overtemp_10 != t) {
        g_cfg.overtemp_10 = t;
        changed = true;
      }
      break;
    }
    case ice::CMD_SET_FAN_LOOP:
      if (cmd.fan_loop_mode > 1) {
        cmd_ack = ice::ACK_RANGE;
      } else if (g_cfg.fan_loop_mode != cmd.fan_loop_mode) {
        g_cfg.fan_loop_mode = cmd.fan_loop_mode;
        g_tel.fan_loop_mode = cmd.fan_loop_mode;
        changed = true;
      }
      break;
    default:
      cmd_ack = ice::ACK_UNKNOWN;
      break;
  }

  lock();
  g_tel.cmd_ack = cmd_ack;
  if (changed) {
    g_cfg_dirty = true;
    g_cfg_changed_ms = (uint32_t)millis();
  }
  g_tel_dirty = true;
  unlock();
}

bool app_take_clear_alarm() {
  bool p = g_clear_alarm_pending;
  g_clear_alarm_pending = false;
  return p;
}

bool app_take_tel_dirty() {
  bool d = g_tel_dirty;
  g_tel_dirty = false;
  return d;
}

bool app_config_dirty() { return g_cfg_dirty; }
void app_mark_config_clean() { g_cfg_dirty = false; }
uint32_t app_config_changed_ms() { return g_cfg_changed_ms; }
