// main.cpp — 固件入口：初始化 + 任务调度（RTOS）

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
#include "tasks.h"
#include "transport/ble.h"
#include "transport/mqtt.h"
#include "transport/wifi.h"

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("=== ICEBOX firmware v1.2 (BLE + WiFi/MQTT) ===");

  persist::init();
  app_init();
  g_actuators.init();
  g_sensors.init();
  g_input.init();
  g_display.init();
  wifi_init();
  mqtt_init();
  ble_init();

  xTaskCreatePinnedToCore(task_sensor, "sensor", 4096, nullptr, 5, nullptr, 1);
  xTaskCreatePinnedToCore(task_control, "control", 4096, nullptr, 8, nullptr, 1);
  xTaskCreatePinnedToCore(task_fan, "fan", 4096, nullptr, 7, nullptr, 1);
  xTaskCreatePinnedToCore(task_encoder, "encoder", 2048, nullptr, 6, nullptr, 1);
  xTaskCreatePinnedToCore(task_telemetry, "telemetry", 4096, nullptr, 4, nullptr, 0);
  xTaskCreatePinnedToCore(task_display, "display", 4096, nullptr, 2, nullptr, 0);
  xTaskCreatePinnedToCore(task_transport, "transport", 4096, nullptr, 4, nullptr, 0);
  xTaskCreatePinnedToCore(task_console, "console", 2048, nullptr, 1, nullptr, 0);

  Serial.println("[main] tasks started");
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
