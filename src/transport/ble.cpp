// transport/ble.cpp — BLE GATT 服务器（NimBLE-Arduino 2.x），协议 v1.1

#include "ble.h"
#include <cstring>
#include <NimBLEDevice.h>

#include "app/app.h"
#include "app_config.h"

static const char* UUID_SVC = "0000FFB0-0000-1000-8000-00805F9B34FB";
static const char* UUID_TELEMETRY = "0000FFB1-0000-1000-8000-00805F9B34FB";
static const char* UUID_COMMAND = "0000FFB2-0000-1000-8000-00805F9B34FB";
static const char* UUID_CONFIG = "0000FFB3-0000-1000-8000-00805F9B34FB";
static const char* UUID_DEVICE_INFO = "0000FFB4-0000-1000-8000-00805F9B34FB";
static const char* UUID_EXT_TELEMETRY = "0000FFB5-0000-1000-8000-00805F9B34FB";

static NimBLECharacteristic* g_char_tele = nullptr;
static NimBLECharacteristic* g_char_ext = nullptr;
static volatile bool g_connected = false;

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* /*pServer*/, NimBLEConnInfo& /*connInfo*/) override {
    g_connected = true;
    Serial.println("[ble] connected");
  }
  void onDisconnect(NimBLEServer* /*pServer*/, NimBLEConnInfo& /*connInfo*/, int /*reason*/) override {
    g_connected = false;
    Serial.println("[ble] disconnected, restart advertising");
    NimBLEDevice::startAdvertising();
  }
};

class CharCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& /*connInfo*/) override {
    if (pChar->getUUID().equals(NimBLEUUID(UUID_COMMAND))) {
      const NimBLEAttValue& v = pChar->getValue();
      app_apply_command_frame(v.data(), v.size());
    }
  }

  void onRead(NimBLECharacteristic* pChar, NimBLEConnInfo& /*connInfo*/) override {
    if (pChar->getUUID().equals(NimBLEUUID(UUID_CONFIG))) {
      AppSnapshot s;
      app_snapshot(s);
      uint8_t buf[32];
      size_t n = ice::pack_config(s.cfg, buf);
      pChar->setValue(buf, n);
    } else if (pChar->getUUID().equals(NimBLEUUID(UUID_DEVICE_INFO))) {
      AppSnapshot s;
      app_snapshot(s);
      char info[24];
      ice::pack_device_info(s.cfg, info, sizeof(info));
      pChar->setValue((uint8_t*)info, (uint16_t)strlen(info));
    }
  }
};

void ble_init() {
  NimBLEDevice::init(APP_DEVICE_NAME);

  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  NimBLEService* svc = server->createService(UUID_SVC);

  g_char_tele =
      svc->createCharacteristic(UUID_TELEMETRY, NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ);
  svc->createCharacteristic(UUID_COMMAND, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR)
      ->setCallbacks(new CharCallbacks());
  svc->createCharacteristic(UUID_CONFIG, NIMBLE_PROPERTY::READ)->setCallbacks(new CharCallbacks());
  svc->createCharacteristic(UUID_DEVICE_INFO, NIMBLE_PROPERTY::READ)
      ->setCallbacks(new CharCallbacks());
  g_char_ext =
      svc->createCharacteristic(UUID_EXT_TELEMETRY, NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ);

  // 遥测特征提供初始值（0 字节亦可）
  uint8_t z[20] = {0};
  g_char_tele->setValue(z, sizeof(z));
  g_char_ext->setValue(z, sizeof(z));

  svc->start();

  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->setName(APP_DEVICE_NAME);
  adv->addServiceUUID(UUID_SVC);
  adv->start();

  Serial.printf("[ble] advertising as %s\n", APP_DEVICE_NAME);
}

bool ble_connected() { return g_connected; }

void ble_notify_frames(const uint8_t* ffb1, const uint8_t* ffb5) {
  if (!g_connected) return;
  g_char_tele->setValue(ffb1, 20);
  g_char_tele->notify();
  g_char_ext->setValue(ffb5, 20);
  g_char_ext->notify();
}
