// core/supply.cpp — 供电识别与制冷功率约束实现，框架无关

#include "supply.h"

namespace ice {

SupplyInfo detect_supply(float voltage_v, uint8_t mode) {
  SupplyInfo si;
  switch (mode) {
    case SD_PD:
      si.protocol = SP_PD;
      si.max_power_w = SUPPLY_PD_MAX_W;
      return si;
    case SD_QC:
      si.protocol = SP_QC;
      si.max_power_w = SUPPLY_QC_MAX_W;
      return si;
    case SD_DC12:
      si.protocol = SP_DC12;
      si.max_power_w = SUPPLY_DC12_MAX_W;
      return si;
    case SD_USB5:
      si.protocol = SP_USB5;
      si.max_power_w = SUPPLY_USB5_MAX_W;
      return si;
    case SD_UNKNOWN:
      si.protocol = SP_UNKNOWN;
      si.max_power_w = 0.0f;
      return si;
    default:
      break;  // SD_AUTO
  }

  // AUTO：电压启发式
  if (voltage_v < 0.5f) {
    si.protocol = SP_UNKNOWN;
    si.max_power_w = 0.0f;
  } else if (voltage_v >= USB_PD_VOLT_THRESHOLD) {
    si.protocol = SP_PD;
    si.max_power_w = SUPPLY_PD_MAX_W;
  } else if (voltage_v >= 11.0f) {
    si.protocol = SP_DC12;  // 12V DC 点烟器
    si.max_power_w = SUPPLY_DC12_MAX_W;
  } else if (voltage_v >= 6.0f) {
    si.protocol = SP_QC;  // QC 协商 9V/12V
    si.max_power_w = SUPPLY_QC_MAX_W;
  } else if (voltage_v >= 4.5f) {
    si.protocol = SP_USB5;
    si.max_power_w = SUPPLY_USB5_MAX_W;
  } else {
    si.protocol = SP_UNKNOWN;
    si.max_power_w = 0.0f;
  }
  return si;
}

uint8_t pwm_limit_from_power(float max_power_w) {
  if (max_power_w <= 0.0f) return 0;
  float limit = 100.0f * max_power_w / PELTIER_FULL_POWER_W;
  if (limit > 100.0f) limit = 100.0f;
  return (uint8_t)limit;
}

}  // namespace ice
