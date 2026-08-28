// test/test_core/test_main.cpp — core 层单元测试（原生环境运行，不依赖硬件）

#include <unity.h>
#include <cstring>

#include "codec.h"
#include "control.h"
#include "fan.h"
#include "state.h"

using namespace ice;

// ---------- CRC ----------
void test_crc_known_value() {
  // CRC16-MODBUS 标准校验向量：'123456789' → 0x4B37
  uint8_t data[] = "123456789";
  TEST_ASSERT_EQUAL_UINT16(0x4B37, crc16_modbus(data, 9));
}

// ---------- 组帧 ----------
void test_pack_telemetry() {
  Telemetry t;
  t.state_bits = 0x01;
  t.cabinet_temp_10 = 34;
  t.hot_side_temp_10 = 421;
  t.target_temp_10 = 50;
  t.power_w10 = 327;
  t.voltage_cV = 1987;
  t.current_cA = 172;
  t.fan_rpm = 3700;
  t.peltier_pwm = 60;
  t.alarm = 0;
  uint8_t buf[20];
  TEST_ASSERT_EQUAL(20, pack_telemetry(t, buf));
  TEST_ASSERT_EQUAL_UINT8(0xA5, buf[0]);
  TEST_ASSERT_EQUAL_UINT8(0x01, buf[1]);
  TEST_ASSERT_EQUAL_UINT16(34, get_le16(buf + 2));
  TEST_ASSERT_EQUAL_UINT16(421, get_le16(buf + 4));
  TEST_ASSERT_EQUAL_UINT16(3700, get_le16(buf + 14));
  TEST_ASSERT_EQUAL_UINT8(60, buf[16]);
  TEST_ASSERT_EQUAL_UINT16(crc16_modbus(buf, 18), get_le16(buf + 18));
}

void test_pack_telemetry_ext() {
  Telemetry t;
  t.cold_side_temp_10 = -120;
  t.cabinet2_temp_10 = 33;
  t.board_temp_10 = 320;
  t.fan_target_rpm = 1500;
  t.fan_loop_mode = 1;
  t.fan_duty = 45;
  t.cmd_ack = 3;
  t.uptime_s = 12345;
  uint8_t buf[20];
  TEST_ASSERT_EQUAL(20, pack_telemetry_ext(t, buf));
  TEST_ASSERT_EQUAL_UINT8(0xA7, buf[0]);
  TEST_ASSERT_EQUAL_UINT16(0xFF88, get_le16(buf + 1));  // -120
  TEST_ASSERT_EQUAL_UINT8(1, buf[9]);
  TEST_ASSERT_EQUAL_UINT8(45, buf[10]);
  TEST_ASSERT_EQUAL_UINT8(3, buf[11]);
  TEST_ASSERT_EQUAL_UINT16(12345, get_le16(buf + 12));
  TEST_ASSERT_EQUAL_UINT16(crc16_modbus(buf, 18), get_le16(buf + 18));
}

void test_pack_config() {
  Config c;
  c.target_temp_10 = 120;
  c.fan_loop_mode = 1;
  c.fan_curve[0] = {350, 800};
  uint8_t buf[32];
  TEST_ASSERT_EQUAL(32, pack_config(c, buf));
  TEST_ASSERT_EQUAL_UINT8(2, buf[0]);
  TEST_ASSERT_EQUAL_UINT16(120, get_le16(buf + 3));
  TEST_ASSERT_EQUAL_UINT8(0x04 | (1 << 4), buf[13]);  // 断点数4 + 闭环
  TEST_ASSERT_EQUAL_UINT16(350, get_le16(buf + 14));
  TEST_ASSERT_EQUAL_UINT16(800, get_le16(buf + 16));
  TEST_ASSERT_EQUAL_UINT16(crc16_modbus(buf, 30), get_le16(buf + 30));
}

void test_pack_device_info() {
  Config c;
  c.serial = 1;
  char s[24];
  size_t n = pack_device_info(c, s, sizeof(s));
  TEST_ASSERT_TRUE(n <= 20);
  TEST_ASSERT_EQUAL_STRING("ICEBOX v1.1 S0001", s);
}

// ---------- 命令解析 ----------
void test_parse_set_target() {
  uint8_t f[6];
  f[0] = 0xA6;
  f[1] = CMD_SET_TARGET;
  put_le_s16(f + 2, 1234);
  put_le16(f + 4, crc16_modbus(f, 4));
  Command cmd;
  TEST_ASSERT_EQUAL(ACK_OK, parse_command(f, sizeof(f), cmd));
  TEST_ASSERT_EQUAL_UINT8(CMD_SET_TARGET, cmd.id);
  TEST_ASSERT_EQUAL_INT16(1234, cmd.target_temp_10);
}

void test_parse_set_fan_curve_valid() {
  uint8_t f[20];
  f[0] = 0xA6;
  f[1] = CMD_SET_FAN_CURVE;
  for (uint8_t i = 0; i < 4; ++i) {
    put_le_s16(f + 2 + i * 4, (int16_t)(350 + i * 100));
    put_le16(f + 4 + i * 4, (uint16_t)(800 + i * 700));
  }
  put_le16(f + 18, crc16_modbus(f, 18));
  Command cmd;
  TEST_ASSERT_EQUAL(ACK_OK, parse_command(f, sizeof(f), cmd));
  TEST_ASSERT_EQUAL_UINT16(1500, cmd.curve[1].rpm);
}

void test_parse_bad_crc() {
  uint8_t f[6] = {0xA6, 0x01, 0x00, 0x00, 0x12, 0x34};
  Command cmd;
  TEST_ASSERT_EQUAL(ACK_CRC, parse_command(f, sizeof(f), cmd));
}

void test_parse_unknown_cmd() {
  uint8_t f[5] = {0xA6, 0x55, 0, 0, 0};
  put_le16(f + 3, crc16_modbus(f, 3));
  Command cmd;
  TEST_ASSERT_EQUAL(ACK_UNKNOWN, parse_command(f, sizeof(f), cmd));
}

void test_parse_bad_len() {
  uint8_t f[5] = {0xA6, 0x01, 0x00, 0, 0};  // SET_TARGET 但只有 1 字节载荷
  put_le16(f + 3, crc16_modbus(f, 3));
  Command cmd;
  TEST_ASSERT_EQUAL(ACK_RANGE, parse_command(f, sizeof(f), cmd));
}

// ---------- 配置工具 ----------
void test_config_sanitize() {
  Config c;
  c.target_temp_10 = 9999;
  c.overtemp_10 = -50;
  c.kp100 = 12000;  // 超出上限 10000（int16 内）
  c.fan_loop_mode = 3;
  c.fan_curve[1].temp_10 = 200;  // 与断点0(350)倒序 → 非法
  config_sanitize(c);
  TEST_ASSERT_EQUAL_INT16(TARGET_MAX_10, c.target_temp_10);
  TEST_ASSERT_EQUAL_INT16(OVERTEMP_MIN_10, c.overtemp_10);
  TEST_ASSERT_EQUAL_INT16(KP_MAX, c.kp100);
  TEST_ASSERT_EQUAL_UINT8(1, c.fan_loop_mode);
  // 折线被复位为默认
  TEST_ASSERT_EQUAL_INT16(350, c.fan_curve[0].temp_10);
  TEST_ASSERT_EQUAL_UINT16(1500, c.fan_curve[1].rpm);
}

void test_fan_curve_valid() {
  FanCurvePoint pts[4] = {{350, 800}, {450, 1500}, {550, 2600}, {650, 4200}};
  TEST_ASSERT_TRUE(fan_curve_valid(pts, 4));
  pts[2].temp_10 = 440;  // 破坏递增
  TEST_ASSERT_FALSE(fan_curve_valid(pts, 4));
}

// ---------- 风扇 ----------
void test_fan_curve_lookup() {
  FanCurvePoint pts[4] = {{350, 800}, {450, 1500}, {550, 2600}, {650, 4200}};
  FanController f;
  TEST_ASSERT_EQUAL_UINT16(800, f.curve_lookup(300, pts, 4));
  TEST_ASSERT_EQUAL_UINT16(800, f.curve_lookup(350, pts, 4));
  TEST_ASSERT_EQUAL_UINT16(1500, f.curve_lookup(450, pts, 4));
  // 400 → 800 + 700*(50/100) = 1150
  TEST_ASSERT_EQUAL_UINT16(1150, f.curve_lookup(400, pts, 4));
  TEST_ASSERT_EQUAL_UINT16(4200, f.curve_lookup(700, pts, 4));
}

void test_fan_open_duty() {
  FanController f;
  f.set_mode(0);
  TEST_ASSERT_EQUAL_UINT8(20, f.duty_open(600));
  TEST_ASSERT_EQUAL_UINT8(100, f.duty_open(4200));
  TEST_ASSERT_TRUE(f.duty_open(2000) > 20 && f.duty_open(2000) < 100);
}

void test_fan_closed_loop_stable() {
  FanController f;
  f.set_mode(1);
  // 目标 2000，实测从 800 逐步逼近
  uint16_t rpm = 800;
  for (int i = 0; i < 200 && rpm < 2000; ++i) {
    uint8_t d = f.compute(2000, rpm, 0.1f);
    rpm += (uint16_t)((d - 20) * 40 / 80);  // 简化风扇响应
  }
  TEST_ASSERT_TRUE(rpm >= 1900);
  TEST_ASSERT_UINT8_WITHIN(20, 2000, rpm);
}

void test_control_heats_toward_target() {
  ControlLoop loop;
  ControlInputs in;
  in.cooling_enable = true;
  in.target_temp = 5.0f;
  in.cabinet_temp = 12.0f;
  in.hot_side_temp = 30.0f;
  in.overtemp = 60.0f;
  in.kp = 8.5f;
  in.ki = 0.5f;
  in.kd = 0.2f;
  ControlOutputs out;
  loop.update(in, 0.1f, out);
  TEST_ASSERT_TRUE(out.peltier_pwm > 0);  // 高于目标 → 制冷
}

void test_control_overtemp_trips() {
  ControlLoop loop;
  ControlInputs in;
  in.cooling_enable = true;
  in.hot_side_temp = 65.0f;
  in.overtemp = 60.0f;
  ControlOutputs out;
  loop.update(in, 0.1f, out);
  TEST_ASSERT_EQUAL_UINT8(AL_OVERTEMP_TRIP, out.alarm);
  TEST_ASSERT_EQUAL_UINT8(0, out.peltier_pwm);
  TEST_ASSERT_TRUE(out.state_bits & SB_TRIP);
  // 温度回落，锁存保持
  in.hot_side_temp = 40.0f;
  loop.update(in, 0.1f, out);
  TEST_ASSERT_EQUAL_UINT8(AL_OVERTEMP_TRIP, out.alarm);
  // 清除后恢复
  loop.clear_alarm();
  loop.update(in, 0.1f, out);
  TEST_ASSERT_EQUAL_UINT8(AL_NONE, out.alarm);
}

int runUnityTests() {
  UNITY_BEGIN();
  RUN_TEST(test_crc_known_value);
  RUN_TEST(test_pack_telemetry);
  RUN_TEST(test_pack_telemetry_ext);
  RUN_TEST(test_pack_config);
  RUN_TEST(test_pack_device_info);
  RUN_TEST(test_parse_set_target);
  RUN_TEST(test_parse_set_fan_curve_valid);
  RUN_TEST(test_parse_bad_crc);
  RUN_TEST(test_parse_unknown_cmd);
  RUN_TEST(test_parse_bad_len);
  RUN_TEST(test_config_sanitize);
  RUN_TEST(test_fan_curve_valid);
  RUN_TEST(test_fan_curve_lookup);
  RUN_TEST(test_fan_open_duty);
  RUN_TEST(test_fan_closed_loop_stable);
  RUN_TEST(test_control_heats_toward_target);
  RUN_TEST(test_control_overtemp_trips);
  return UNITY_END();
}

#ifdef ARDUINO
#include <Arduino.h>
void setup() { delay(1000); runUnityTests(); }
void loop() {}
#else
int main() { return runUnityTests(); }
#endif
