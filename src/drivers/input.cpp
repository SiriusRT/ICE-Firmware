// drivers/input.cpp — EC11 正交编码器（ISR 状态表解码）+ 按键（任务轮询）

#include "input.h"
#include <Arduino.h>
#include "app_config.h"

static volatile int32_t enc_delta = 0;
static volatile uint8_t enc_state = 0;
static volatile uint32_t knob_active_until = 0;

void IRAM_ATTR Input::enc_isr() {
  uint8_t a = (uint8_t)digitalRead(PIN_ENC_A);
  uint8_t b = (uint8_t)digitalRead(PIN_ENC_B);
  uint8_t s = (uint8_t)((a << 1) | b);
  // 状态表正交解码（X4）
  static const int8_t table[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
  enc_delta += table[(enc_state << 2) | s];
  enc_state = s;
}

void Input::init() {
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_ENC_BTN, INPUT_PULLUP);
  enc_state = ((uint8_t)digitalRead(PIN_ENC_A) << 1) | (uint8_t)digitalRead(PIN_ENC_B);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), Input::enc_isr, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_B), Input::enc_isr, CHANGE);
}

int32_t Input::read_delta() {
  noInterrupts();
  int32_t d = enc_delta;
  enc_delta = 0;
  interrupts();
  return d;
}

InputEvent Input::poll() {
  InputEvent ev;
  uint32_t now = millis();

  bool down = digitalRead(PIN_ENC_BTN) == LOW;
  static bool prev_down = false;
  static uint32_t press_start = 0;

  if (down && !prev_down) {
    press_start = now;
    knob_active_until = now + 400;  // 锁存 400ms，保证 1Hz 遥测能捕获
  }
  if (!down && prev_down) {
    uint32_t dur = now - press_start;
    if (dur >= 1000) {
      ev.long_pressed = true;
    } else if (dur >= 30) {
      ev.pressed = true;
    }
  }
  prev_down = down;

  ev.knob_active = (now < knob_active_until) || down;
  return ev;
}

Input g_input;
