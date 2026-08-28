// transport/mqtt.cpp — MQTT 客户端（远程骨架）
// 数据帧复用 BLE 协议 v1.1：遥测 A5/A7 帧、命令 A6 帧、配置 32B 帧。

#include "mqtt.h"
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <cstdio>
#include <cstring>

#include "app/app.h"
#include "codec.h"

static WiFiClient wifi_client;
static PubSubClient client(wifi_client);
static char topic_base[24] = "icebox/S0001";
static bool config_requested = false;

static void build_topic_base(const ice::Config& cfg) {
  snprintf(topic_base, sizeof(topic_base), "icebox/S%04u", (unsigned)(cfg.serial % 10000));
}

static void on_message(char* topic, byte* payload, unsigned int len) {
  if (strstr(topic, "/cmd")) {
    // 与 BLE FFB2 相同处理：校验 + 钳位 + cmd_ack 回写
    app_apply_command_frame(payload, len);
  } else if (strstr(topic, "/config/get")) {
    config_requested = true;
  }
}

void mqtt_init() {
  client.setCallback(on_message);
  client.setKeepAlive(30);
  client.setSocketTimeout(10);
}

bool mqtt_connect(const ice::Config& cfg) {
  if (!WiFi.isConnected()) return false;
  build_topic_base(cfg);

  client.setServer(cfg.mqtt_host, cfg.mqtt_port);
  char cid[24];
  snprintf(cid, sizeof(cid), "icebox-%04u", (unsigned)(cfg.serial % 10000));

  bool ok;
  if (cfg.mqtt_user[0] != '\0') {
    ok = client.connect(cid, cfg.mqtt_user, cfg.mqtt_pass);
  } else {
    ok = client.connect(cid);
  }
  if (!ok) {
    Serial.printf("[mqtt] connect fail, rc=%d\n", client.state());
    return false;
  }

  char topic[64];
  snprintf(topic, sizeof(topic), "%s/cmd", topic_base);
  client.subscribe(topic);
  snprintf(topic, sizeof(topic), "%s/config/get", topic_base);
  client.subscribe(topic);
  Serial.printf("[mqtt] connected %s:%u, base=%s\n", cfg.mqtt_host, cfg.mqtt_port, topic_base);
  return true;
}

void mqtt_disconnect() {
  if (client.connected()) client.disconnect();
}

bool mqtt_is_connected() { return client.connected(); }

void mqtt_publish_telemetry(const ice::Telemetry& tel) {
  if (!client.connected()) return;
  uint8_t f1[20], f5[20];
  ice::pack_telemetry(tel, f1);
  ice::pack_telemetry_ext(tel, f5);
  char topic[64];
  snprintf(topic, sizeof(topic), "%s/telemetry", topic_base);
  client.publish(topic, f1, 20, false);
  snprintf(topic, sizeof(topic), "%s/telemetry_ext", topic_base);
  client.publish(topic, f5, 20, false);
}

void mqtt_publish_config(const ice::Config& cfg) {
  if (!client.connected()) return;
  uint8_t buf[32];
  ice::pack_config(cfg, buf);
  char topic[64];
  snprintf(topic, sizeof(topic), "%s/config", topic_base);
  client.publish(topic, buf, 32, false);
}

void mqtt_loop() {
  if (!client.connected()) return;
  client.loop();
  if (config_requested) {
    config_requested = false;
    AppSnapshot s;
    app_snapshot(s);
    mqtt_publish_config(s.cfg);
  }
}
