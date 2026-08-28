#pragma once
// drivers/input.h — 数字旋钮（EC11 正交编码器）+ 按键

#include <Arduino.h>
#include <cstdint>

struct InputEvent {
  bool pressed = false;      // 本次轮询检测到一次短按
  bool long_pressed = false; // 长按（≥1s）
  bool knob_active = false;  // 按键当前处于按下（含 400ms 锁存，供遥测状态位）
};

class Input {
 public:
  void init();
  // 返回自上次调用以来累计的旋钮格数（带符号），并清零
  int32_t read_delta();
  // 轮询按键状态（建议 20ms 周期）
  InputEvent poll();

  static void IRAM_ATTR enc_isr();
};

extern Input g_input;
