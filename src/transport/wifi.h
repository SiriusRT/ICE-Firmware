#pragma once
// transport/wifi.h — WiFi STA 管理与自动重连（远程骨架）

#include <cstdint>
#include <WString.h>

void wifi_init();
bool wifi_connect(const char* ssid, const char* pass, uint32_t timeout_ms);
void wifi_disconnect();
bool wifi_is_connected();
String wifi_ip();
