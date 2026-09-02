#pragma once
// core/supply.h — 供电协议识别与最大制冷功率判定，框架无关
// 协议 §3.5 FFB5：supply_protocol（偏移 14）+ supply_max_power（偏移 16–17）

#include <cstdint>
#include "state.h"

namespace ice {

// 供电识别模式（由 app_config.h SUPPLY_DETECT_MODE 配置）
enum SupplyDetectMode : uint8_t {
  SD_AUTO = 0,  // 依据实测输入电压启发式识别
  SD_PD = 1,    // 强制 USB-PD
  SD_QC = 2,    // 强制 QC
  SD_DC12 = 3,  // 强制 12V DC
  SD_USB5 = 4,  // 强制 USB 5V
  SD_UNKNOWN = 5,
};

struct SupplyInfo {
  uint8_t protocol = SP_UNKNOWN;
  float max_power_w = 0.0f;
};

// 依据输入电压（V）与识别模式判定供电类型与最大制冷功率（W）。
// 说明：电压启发式无法完全区分 QC 12V 与 12V DC（两者均 ~12V），
//       若需精确识别，应增加 D+/D- 检测等硬件信号并配合强制模式。
SupplyInfo detect_supply(float voltage_v, uint8_t mode);

// 依据允许的最大功率（W）计算制冷 PWM 占空比上限（0..100）。
// 假设制冷片功率近似与占空比线性（平均功率 = 占空比 × 全功率）。
uint8_t pwm_limit_from_power(float max_power_w);

}  // namespace ice
