#pragma once
// transport/ble.h — BLE GATT 服务（NimBLE-Arduino 2.x），实现协议 v1.1

#include <cstdint>

void ble_init();
bool ble_connected();
// 通知 FFB1 + FFB5（各 20 字节帧，已含 CRC）
void ble_notify_frames(const uint8_t* ffb1, const uint8_t* ffb5);
