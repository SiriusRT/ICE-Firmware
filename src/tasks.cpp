// tasks.cpp — RTOS 任务实现

#include "tasks.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "app/app.h"
#include "app_config.h"
#include "drivers/actuators.h"
#include "drivers/display.h"
#include "drivers/input.h"
#include "drivers/persist.h"
#include "drivers/sensors.h"
#include "transport/ble.h"
#include "transport/mqtt.h"
#include "transport/wifi.h"
#include "codec.h"
#include "control.h"
#include "fan.h"
#include "state.h"
#include <math.h>
#include <cstring>
#include <cstdlib>

// ---------------- 传感器（1Hz）----------------
void task_sensor(void*) {
  for (;;) {
    SensorReadings r = g_sensors.update();
    app_set_sensor(r);
    vTaskDelay(pdMS_TO_TICKS(250));  // update() 已阻塞 ~800ms，合计约 1Hz
  }
}

// ---------------- 温控核心（10Hz）----------------
void task_control(void*) {
  ice::ControlLoop loop;
  loop.reset();
  for (;;) {
    AppSnapshot s;
    app_snapshot(s);

    // 柜内温度：两路取均值
    float c1 = s.tel.cabinet_temp_10 == INT16_MIN ? NAN : s.tel.cabinet_temp_10 / 10.0f;
    float c2 = s.tel.cabinet2_temp_10 == INT16_MIN ? NAN : s.tel.cabinet2_temp_10 / 10.0f;
    float cabinet = 0;
    int n = 0;
    if (!isnan(c1)) { cabinet += c1; ++n; }
    if (!isnan(c2)) { cabinet += c2; ++n; }
    cabinet = n ? cabinet / n : 0.0f;

    ice::ControlInputs in;
    in.cabinet_temp = cabinet;
    in.hot_side_temp = s.tel.hot_side_temp_10 == INT16_MIN ? 0 : s.tel.hot_side_temp_10 / 10.0f;
    in.voltage = s.tel.voltage_cV / 100.0f;
    in.current = s.tel.current_cA / 100.0f;
    in.power_w = s.tel.power_w10 / 10.0f;
    in.fan_rpm = s.tel.fan_rpm;
    in.target_temp = s.cfg.target_temp_10 / 10.0f;
    in.overtemp = s.cfg.overtemp_10 / 10.0f;
    in.kp = s.cfg.kp100 / 100.0f;
    in.ki = s.cfg.ki100 / 100.0f;
    in.kd = s.cfg.kd100 / 100.0f;
    in.sensors_ok = s.sensors_ok;
    in.cooling_enable = s.cfg.cooling_enable;
    in.usb_pd = in.voltage >= ice::USB_PD_VOLT_THRESHOLD;
    in.knob_pressed = s.knob_active;

    ice::ControlOutputs out;
    loop.update(in, 0.1f, out);

    g_actuators.set_peltier_pwm(out.peltier_pwm);
    app_set_control(out.peltier_pwm, out.state_bits, out.alarm);

    if (app_take_clear_alarm()) loop.clear_alarm();

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// ---------------- 风扇控制（10Hz）----------------
void task_fan(void*) {
  ice::FanController fan;
  fan.reset();
  for (;;) {
    AppSnapshot s;
    app_snapshot(s);
    fan.set_mode(s.cfg.fan_loop_mode);

    uint16_t target = fan.curve_lookup(s.tel.hot_side_temp_10, s.cfg.fan_curve,
                                       ice::FAN_CURVE_POINTS);
    uint16_t measured = g_sensors.read_rpm();
    uint8_t duty = fan.compute(target, measured, 0.1f);

    g_actuators.set_fan_pwm(duty);
    app_set_fan(duty, target, measured, s.cfg.fan_loop_mode);

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// ---------------- 旋钮（50Hz，丝滑调速）----------------
void task_encoder(void*) {
  for (;;) {
    int32_t d = g_input.read_delta();
    if (d != 0) {
      AppSnapshot s;
      app_snapshot(s);
      int32_t absd = d < 0 ? -d : d;
      int32_t unit = 1;  // 0.1℃/格
      if (absd >= 8) unit = 10;
      else if (absd >= 3) unit = 5;
      else if (absd >= 2) unit = 2;
      int32_t t = s.cfg.target_temp_10 + d * unit;
      t = t < ice::TARGET_MIN_10 ? ice::TARGET_MIN_10
                                 : (t > ice::TARGET_MAX_10 ? ice::TARGET_MAX_10 : t);
      app_knob_set_target((int16_t)t);
    }

    InputEvent ev = g_input.poll();
    app_set_knob_active(ev.knob_active);
    if (ev.long_pressed) {
      // 长按：离线切换风扇控制模式（开环/闭环）
      AppSnapshot s;
      app_snapshot(s);
      app_set_fan_loop(s.cfg.fan_loop_mode ? 0 : 1);
      Serial.printf("[knob] long press -> fan loop mode %d\n", s.cfg.fan_loop_mode ? 0 : 1);
    } else if (ev.pressed) {
      // 短按：离线开关制冷
      AppSnapshot s;
      app_snapshot(s);
      app_set_cooling(!s.cfg.cooling_enable);
      Serial.printf("[knob] short press -> cooling %d\n", !s.cfg.cooling_enable);
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// ---------------- 遥测上报（1Hz + 变化即报）----------------
void task_telemetry(void*) {
  uint32_t uptime_s = 0;
  uint32_t last_notify = 0;
  for (;;) {
    bool dirty = app_take_tel_dirty();
    uint32_t now = millis();

    if (now - last_notify >= 1000 || dirty) {
      AppSnapshot s;
      app_snapshot(s);
      s.tel.uptime_s = (uint16_t)uptime_s;
      ++uptime_s;

      uint8_t f1[20], f5[20];
      ice::pack_telemetry(s.tel, f1);
      ice::pack_telemetry_ext(s.tel, f5);
      ble_notify_frames(f1, f5);
      last_notify = now;
    }

    // 配置变更静默 2s 后落盘
    if (app_config_dirty() && (now - app_config_changed_ms() > 2000)) {
      AppSnapshot s;
      app_snapshot(s);
      persist::save(s.cfg);
      app_mark_config_clean();
      Serial.println("[nvs] config saved");
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// ---------------- 显示（2Hz）----------------
void task_display(void*) {
  for (;;) {
    AppSnapshot s;
    app_snapshot(s);
    g_display.render(s.cfg, s.tel, ble_connected());
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

// ---------------- WiFi/MQTT 远程（10Hz 管理 + 1Hz 遥测发布）----------------
void task_transport(void*) {
  uint32_t last_wifi_try = 0;
  uint32_t last_pub = 0;
  for (;;) {
    AppSnapshot s;
    app_snapshot(s);
    bool have_wifi = s.cfg.wifi_ssid[0] != '\0';

    if (have_wifi) {
      if (!wifi_is_connected() && millis() - last_wifi_try > 10000) {
        last_wifi_try = millis();
        wifi_connect(s.cfg.wifi_ssid, s.cfg.wifi_pass, 5000);
      }
    } else if (wifi_is_connected()) {
      wifi_disconnect();
    }

    if (wifi_is_connected() && s.cfg.mqtt_host[0] != '\0') {
      if (!mqtt_is_connected()) mqtt_connect(s.cfg);
      mqtt_loop();
      if (mqtt_is_connected() && millis() - last_pub >= 1000) {
        last_pub = millis();
        AppSnapshot s2;
        app_snapshot(s2);
        mqtt_publish_telemetry(s2.tel);
      }
    } else if (mqtt_is_connected()) {
      mqtt_disconnect();
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// ---------------- 串口配置命令 -----------------
static char console_buf[128];
static uint8_t console_len = 0;

static void console_exec(char* line) {
  char* cmd = strtok(line, " \t");
  if (!cmd) return;

  if (strcmp(cmd, "help") == 0) {
    Serial.println(F("commands:"));
    Serial.println(F("  wifi <ssid> <pass> | wifi off"));
    Serial.println(F("  mqtt <host> [port] | mqtt user <u> <p> | mqtt off"));
    Serial.println(F("  status | save"));
    return;
  }
  if (strcmp(cmd, "status") == 0) {
    AppSnapshot s;
    app_snapshot(s);
    Serial.printf("BLE:%s WiFi:%s(%s) MQTT:%s\n", ble_connected() ? "on" : "off",
                  wifi_is_connected() ? wifi_ip().c_str() : "off", s.cfg.wifi_ssid,
                  mqtt_is_connected() ? "on" : "off");
    Serial.printf("target %.1fC cooling=%d fan_loop=%d alarm=%d pwm=%d\n",
                  s.cfg.target_temp_10 / 10.0f, (int)s.cfg.cooling_enable, s.cfg.fan_loop_mode,
                  s.tel.alarm, s.tel.peltier_pwm);
    return;
  }
  if (strcmp(cmd, "wifi") == 0) {
    char* ssid = strtok(nullptr, " \t");
    if (!ssid) {
      Serial.println(F("usage: wifi <ssid> <pass> | wifi off"));
      return;
    }
    if (strcmp(ssid, "off") == 0) {
      app_set_wifi_config("", "");
      Serial.println(F("wifi disabled"));
    } else {
      char* pass = strtok(nullptr, " \t");
      app_set_wifi_config(ssid, pass ? pass : "");
      Serial.printf("wifi set: %s\n", ssid);
    }
    return;
  }
  if (strcmp(cmd, "mqtt") == 0) {
    char* a = strtok(nullptr, " \t");
    if (!a) {
      Serial.println(F("usage: mqtt <host> [port] | mqtt user <u> <p> | mqtt off"));
      return;
    }
    if (strcmp(a, "off") == 0) {
      app_set_mqtt_config("", 1883, "", "");
      Serial.println(F("mqtt disabled"));
    } else if (strcmp(a, "user") == 0) {
      char* u = strtok(nullptr, " \t");
      char* p = strtok(nullptr, " \t");
      AppSnapshot s;
      app_snapshot(s);
      app_set_mqtt_config(s.cfg.mqtt_host, s.cfg.mqtt_port, u ? u : "", p ? p : "");
      Serial.println(F("mqtt user set"));
    } else {
      char* port = strtok(nullptr, " \t");
      AppSnapshot s;
      app_snapshot(s);
      app_set_mqtt_config(a, port ? (uint16_t)atoi(port) : 1883, s.cfg.mqtt_user, s.cfg.mqtt_pass);
      Serial.printf("mqtt set: %s:%s\n", a, port ? port : "1883");
    }
    return;
  }
  if (strcmp(cmd, "save") == 0) {
    AppSnapshot s;
    app_snapshot(s);
    persist::save(s.cfg);
    app_mark_config_clean();
    Serial.println(F("saved"));
    return;
  }
  Serial.println(F("unknown cmd, type 'help'"));
}

void task_console(void*) {
  for (;;) {
    while (Serial.available()) {
      char c = (char)Serial.read();
      if (c == '\n' || c == '\r') {
        if (console_len > 0) {
          console_buf[console_len] = '\0';
          console_exec(console_buf);
          console_len = 0;
        }
      } else if (console_len < sizeof(console_buf) - 1) {
        console_buf[console_len++] = c;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
