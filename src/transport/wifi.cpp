// transport/wifi.cpp — WiFi STA 管理与自动重连（远程骨架）

#include "wifi.h"
#include <Arduino.h>
#include <WiFi.h>

void wifi_init() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
}

bool wifi_connect(const char* ssid, const char* pass, uint32_t timeout_ms) {
  if (!ssid || ssid[0] == '\0') return false;
  if (WiFi.isConnected()) return true;

  Serial.printf("[wifi] connecting to '%s' ...\n", ssid);
  WiFi.begin(ssid, pass ? pass : "");
  uint32_t t0 = millis();
  while (!WiFi.isConnected() && millis() - t0 < timeout_ms) {
    delay(100);
  }
  if (WiFi.isConnected()) {
    configTime(0, 0, "pool.ntp.org", "ntp.aliyun.com");  // SNTP 校时（远程时间戳用）
    Serial.printf("[wifi] connected, ip=%s rssi=%d\n", WiFi.localIP().toString().c_str(),
                  WiFi.RSSI());
  } else {
    Serial.printf("[wifi] connect failed\n");
  }
  return WiFi.isConnected();
}

void wifi_disconnect() {
  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_OFF);
  WiFi.mode(WIFI_STA);
}

bool wifi_is_connected() { return WiFi.isConnected(); }

String wifi_ip() { return WiFi.localIP().toString(); }
