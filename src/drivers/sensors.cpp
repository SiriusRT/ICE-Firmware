// drivers/sensors.cpp — 温度 / INA226 电气量 / 风扇测速

#include "sensors.h"
#include <Arduino.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <limits.h>
#include <math.h>

#include "app_config.h"

SemaphoreHandle_t g_i2c_mutex = nullptr;

static OneWire oneWire(PIN_ONEWIRE);
static DallasTemperature ds(&oneWire);
static uint8_t g_device_count = 0;

// ---------- INA226 寄存器 ----------
#define INA_REG_CONFIG 0x00
#define INA_REG_SHUNT  0x01
#define INA_REG_BUS    0x02
#define INA_REG_POWER  0x03
#define INA_REG_CURRENT 0x04
#define INA_REG_CAL    0x05

static uint16_t ina_cal = 0;
static bool ina_ok = false;

static uint16_t ina_read16(uint8_t reg) {
  Wire.beginTransmission(INA226_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return 0;
  if (Wire.requestFrom((uint8_t)INA226_ADDR, (uint8_t)2) != 2) return 0;
  uint16_t v = (uint16_t)Wire.read() << 8;
  v |= Wire.read();
  return v;
}

static bool ina_write16(uint8_t reg, uint16_t val) {
  Wire.beginTransmission(INA226_ADDR);
  Wire.write(reg);
  Wire.write((uint8_t)(val >> 8));
  Wire.write((uint8_t)(val & 0xFF));
  return Wire.endTransmission() == 0;
}

static void ina_init() {
  // current LSB = Imax / 32768
  float cur_lsb = INA226_MAX_CURRENT_A / 32768.0f;
  ina_cal = (uint16_t)(0.00512f / (cur_lsb * (INA226_SHUNT_MOHM / 1000.0f)));
  ina_write16(INA_REG_CONFIG, 0x4127);   // 连续测量 16 次平均
  ina_write16(INA_REG_CAL, ina_cal);
  uint16_t mfr = ina_read16(0xFE);       // 制造商 ID 应为 0x5449
  ina_ok = (mfr == 0x5449);
}

// ---------- 风扇测速 ----------
static volatile uint32_t g_tach_count = 0;
static uint32_t g_tach_last_count = 0;
static uint32_t g_tach_last_ms = 0;

void Sensors::tach_isr() { g_tach_count++; }

uint16_t Sensors::read_rpm() {
  uint32_t now = millis();
  uint32_t cnt = g_tach_count;
  uint32_t dt = now - g_tach_last_ms;
  uint32_t dc = cnt - g_tach_last_count;
  g_tach_last_count = cnt;
  g_tach_last_ms = now;
  if (dt == 0) return current_rpm_;
  uint32_t rpm = dc * 30000UL / dt;  // 2 脉冲/转：rpm = dc/dt * 1000/2 * 60
  if (rpm > 20000) rpm = 0;          // 明显异常
  current_rpm_ = (uint16_t)rpm;
  return current_rpm_;
}

// ---------- 主控板 NTC ----------
float Sensors::read_board_ntc() {
  uint32_t mv = analogReadMilliVolts(PIN_BOARD_NTC);
  if (mv < 5) return NAN;
  float r = NTC_PULLUP_OHM * (3300.0f / (float)mv - 1.0f);
  if (r <= 0) return NAN;
  float t = 1.0f / (1.0f / NTC_T0_K + logf(r / NTC_R0_OHM) / NTC_BETA) - 273.15f;
  return t;
}

static int16_t to_10(float t) {
  if (isnan(t) || t < -55.0f || t > 125.0f) return INT16_MIN;
  return (int16_t)lroundf(t * 10.0f);
}

// ---------- 初始化 ----------
void Sensors::init() {
  if (!g_i2c_mutex) g_i2c_mutex = xSemaphoreCreateMutex();

  pinMode(PIN_FAN_TACH, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_FAN_TACH), Sensors::tach_isr, RISING);

  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  ina_init();

  analogSetPinAttenuation(PIN_BOARD_NTC, ADC_11db);

  ds.begin();
  g_device_count = ds.getDeviceCount();
  ds.setResolution(12);
}

// ---------- 周期采样 ----------
SensorReadings Sensors::update() {
  SensorReadings r;
  r.cabinet1_10 = INT16_MIN;
  r.cabinet2_10 = INT16_MIN;
  r.cold_side_10 = INT16_MIN;
  r.hot_side_10 = INT16_MIN;
  r.board_10 = INT16_MIN;
  r.voltage_cV = 0;
  r.current_cA = 0;
  r.power_w10 = 0;

  // 1) DS18B20：总线并发转换后逐一读取
  ds.requestTemperatures();
  delay(750);

  auto read_ds = [&](uint8_t idx, int16_t& out) {
    if (idx >= g_device_count) return;
    float t = ds.getTempCByIndex(idx);
    out = to_10(t);
  };
  read_ds(IDX_CABINET1, r.cabinet1_10);
  read_ds(IDX_CABINET2, r.cabinet2_10);
  read_ds(IDX_COLD, r.cold_side_10);
  read_ds(IDX_HOT, r.hot_side_10);

  r.cabinet_ok = (r.cabinet1_10 != INT16_MIN) || (r.cabinet2_10 != INT16_MIN);
  r.cold_ok = r.cold_side_10 != INT16_MIN;
  r.hot_ok = r.hot_side_10 != INT16_MIN;

  // 2) INA226
  if (xSemaphoreTake(g_i2c_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    if (ina_ok) {
      uint16_t bus = ina_read16(INA_REG_BUS);      // 1.25mV/LSB
      int16_t cur_raw = (int16_t)ina_read16(INA_REG_CURRENT);
      uint16_t pw = ina_read16(INA_REG_POWER);     // 25*currentLSB W/LSB
      float cur_lsb = INA226_MAX_CURRENT_A / 32768.0f;
      float v = bus * 1.25e-3f;
      float i = cur_raw * cur_lsb;
      float p = pw * 25.0f * cur_lsb;
      if (v > 0.05f && v < 60.0f) {
        r.voltage_cV = (uint16_t)lroundf(v * 100.0f);
        r.current_cA = (uint16_t)lroundf(fabsf(i) * 100.0f);
        r.power_w10 = (uint16_t)lroundf(p * 10.0f);
      }
    }
    xSemaphoreGive(g_i2c_mutex);
  }

  // 3) 主控板 NTC
  float b = read_board_ntc();
  r.board_10 = to_10(b);
  r.board_ok = r.board_10 != INT16_MIN;

  // 4) 总体有效性：热侧必须有效，且至少一路柜内有效
  r.valid = r.hot_ok && r.cabinet_ok;

  read_rpm();  // 更新当前转速缓存
  return r;
}

Sensors g_sensors;
