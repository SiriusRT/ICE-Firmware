// drivers/persist.cpp — 配置持久化（NVS / Preferences）

#include "persist.h"
#include <Preferences.h>
#include <cstdio>

static Preferences prefs;
static const char* NS = "icebox";

namespace persist {

void init() { prefs.begin(NS, false); }

void load(ice::Config& cfg) {
  bool ok = prefs.getBool("ok", false);
  if (!ok) return;  // 首次上电，使用默认值
  cfg.target_temp_10 = (int16_t)prefs.getShort("tgt", cfg.target_temp_10);
  cfg.overtemp_10 = (int16_t)prefs.getShort("ovt", cfg.overtemp_10);
  cfg.kp100 = (int16_t)prefs.getShort("kp", cfg.kp100);
  cfg.ki100 = (int16_t)prefs.getShort("ki", cfg.ki100);
  cfg.kd100 = (int16_t)prefs.getShort("kd", cfg.kd100);
  cfg.cooling_enable = prefs.getBool("cng", cfg.cooling_enable);
  cfg.fan_loop_mode = (uint8_t)prefs.getChar("fan", cfg.fan_loop_mode);
  cfg.serial = prefs.getUInt("sn", cfg.serial);
  char key[5];
  for (uint8_t i = 0; i < ice::FAN_CURVE_POINTS; ++i) {
    snprintf(key, sizeof(key), "c%ut", i);
    cfg.fan_curve[i].temp_10 = (int16_t)prefs.getShort(key, cfg.fan_curve[i].temp_10);
    snprintf(key, sizeof(key), "c%ur", i);
    cfg.fan_curve[i].rpm = (uint16_t)prefs.getUShort(key, cfg.fan_curve[i].rpm);
  }
  ice::config_sanitize(cfg);
  prefs.getString("ws", cfg.wifi_ssid, sizeof(cfg.wifi_ssid));
  prefs.getString("wp", cfg.wifi_pass, sizeof(cfg.wifi_pass));
  prefs.getString("mh", cfg.mqtt_host, sizeof(cfg.mqtt_host));
  cfg.mqtt_port = prefs.getUShort("mp", cfg.mqtt_port);
  prefs.getString("mu", cfg.mqtt_user, sizeof(cfg.mqtt_user));
  prefs.getString("mpw", cfg.mqtt_pass, sizeof(cfg.mqtt_pass));
}

void save(const ice::Config& cfg) {
  prefs.putBool("ok", true);
  prefs.putShort("tgt", cfg.target_temp_10);
  prefs.putShort("ovt", cfg.overtemp_10);
  prefs.putShort("kp", cfg.kp100);
  prefs.putShort("ki", cfg.ki100);
  prefs.putShort("kd", cfg.kd100);
  prefs.putBool("cng", cfg.cooling_enable);
  prefs.putChar("fan", cfg.fan_loop_mode);
  prefs.putUInt("sn", cfg.serial);
  char key[5];
  for (uint8_t i = 0; i < ice::FAN_CURVE_POINTS; ++i) {
    snprintf(key, sizeof(key), "c%ut", i);
    prefs.putShort(key, cfg.fan_curve[i].temp_10);
    snprintf(key, sizeof(key), "c%ur", i);
    prefs.putUShort(key, cfg.fan_curve[i].rpm);
  }
  prefs.putString("ws", cfg.wifi_ssid);
  prefs.putString("wp", cfg.wifi_pass);
  prefs.putString("mh", cfg.mqtt_host);
  prefs.putUShort("mp", cfg.mqtt_port);
  prefs.putString("mu", cfg.mqtt_user);
  prefs.putString("mpw", cfg.mqtt_pass);
}

}  // namespace persist
