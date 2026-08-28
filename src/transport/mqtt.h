#pragma once
// transport/mqtt.h — MQTT 客户端（远程骨架）
// 复用 BLE 协议 v1.1 的 A5/A6 数据帧作为 MQTT 载荷，云端/小程序可共用解析逻辑。
// Topic：icebox/S<sn>/{telemetry,telemetry_ext,cmd,config,config/get}

#include "state.h"

void mqtt_init();
bool mqtt_connect(const ice::Config& cfg);
void mqtt_disconnect();
bool mqtt_is_connected();
void mqtt_publish_telemetry(const ice::Telemetry& tel);
void mqtt_publish_config(const ice::Config& cfg);
void mqtt_loop();
