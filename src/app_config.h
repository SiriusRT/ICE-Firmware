#pragma once
// app_config.h — 引脚与硬件常量（drivers 层使用）

// ---- GPIO 引脚 ----
#define PIN_PELTIER_PWM 25   // 半导体制冷片 PWM（LEDC）
#define PIN_FAN_PWM     26   // 4针风扇 PWM（LEDC, 25kHz）
#define PIN_FAN_TACH    27   // 风扇 TACH（开漏，上拉，2脉冲/转）
#define PIN_ENC_A       32   // 旋钮相位 A
#define PIN_ENC_B       33   // 旋钮相位 B
#define PIN_ENC_BTN     34   // 旋钮按键（输入专用脚）
#define PIN_OLED_SDA    21
#define PIN_OLED_SCL    22
#define PIN_ONEWIRE     4    // DS18B20 总线（柜内×2、冷侧、热侧）
#define PIN_BOARD_NTC   36   // 主控板温度 NTC（输入专用 ADC）
#define PIN_POWER_LATCH 13   // 软关机锁存（预留）

// ---- LEDC ----
#define LEDC_CH_PELTIER 0
#define LEDC_CH_FAN     1
#define PELTIER_PWM_FREQ 15000u
#define FAN_PWM_FREQ     25000u
#define PWM_RES_BITS     10   // 0..1023

// ---- I2C ----
#define INA226_ADDR 0x40
#define INA226_SHUNT_MOHM 5     // 分流电阻 5mΩ
#define INA226_MAX_CURRENT_A 5.0f

// ---- 传感器 ----
#define ONE_WIRE_DEVICE_MAX 8
// DS18B20 总线索引顺序：0=柜内1, 1=柜内2, 2=冷侧, 3=热侧
#define IDX_CABINET1 0
#define IDX_CABINET2 1
#define IDX_COLD     2
#define IDX_HOT      3

// NTC（主控板）参数
#define NTC_PULLUP_OHM 10000.0f
#define NTC_R0_OHM     10000.0f
#define NTC_BETA       3950.0f
#define NTC_T0_K       298.15f

// ---- 设备 ----
#define APP_DEVICE_NAME "ICEBOX-S0001"

// ---- 供电识别模式（协议 §3.5）----
// 0=自动(按输入电压启发式) 1=强制PD 2=强制QC 3=强制DC12 4=强制USB5 5=强制未知
// 说明：自动模式无法区分 QC 12V 与 12V DC（均 ~12V），如需精确识别请加 D+/D- 检测硬件并改用强制模式。
#define SUPPLY_DETECT_MODE 0
