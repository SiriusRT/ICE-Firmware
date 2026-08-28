#pragma once
// core/codec.h — ICEBOX BLE 协议编解码（v1.1），框架无关

#include <cstdint>
#include <cstddef>
#include "state.h"

namespace ice {

constexpr uint8_t HEADER_TELEMETRY = 0xA5;
constexpr uint8_t HEADER_COMMAND = 0xA6;
constexpr uint8_t HEADER_EXT_TELEMETRY = 0xA7;

// 命令 ID（协议 §4）
enum CmdId : uint8_t {
  CMD_SET_TARGET = 0x01,
  CMD_SET_PID = 0x02,
  CMD_SET_FAN_CURVE = 0x03,
  CMD_CLEAR_ALARM = 0x04,
  CMD_SET_COOLING = 0x05,
  CMD_SET_OVERTEMP = 0x06,
  CMD_SET_FAN_LOOP = 0x07,
};

// 命令应答码（协议 §3.5 FFB5.cmd_ack）
enum CmdAck : uint8_t {
  ACK_OK = 0,
  ACK_CRC = 1,
  ACK_RANGE = 2,
  ACK_CURVE = 3,
  ACK_UNKNOWN = 4,
};

// 解析后的命令
struct Command {
  uint8_t id = 0;
  int16_t target_temp_10 = 0;
  int16_t kp100 = 0, ki100 = 0, kd100 = 0;
  FanCurvePoint curve[FAN_CURVE_POINTS] = {};
  uint8_t on = 0;
  int16_t overtemp_10 = 0;
  uint8_t fan_loop_mode = 0;
};

uint16_t crc16_modbus(const uint8_t* data, size_t len);
void put_le16(uint8_t* p, uint16_t v);
void put_le_s16(uint8_t* p, int16_t v);
uint16_t get_le16(const uint8_t* p);

// 组帧，返回帧长
size_t pack_telemetry(const Telemetry& t, uint8_t* out);       // 20B
size_t pack_telemetry_ext(const Telemetry& t, uint8_t* out);   // 20B
size_t pack_config(const Config& c, uint8_t* out);             // 32B
size_t pack_device_info(const Config& c, char* out, size_t max_len);  // ≤20B

// 解析命令帧（[A6][id][payload][crc lo][crc hi]）。
// 返回 0(ACK_OK)/ACK_CRC/ACK_RANGE/ACK_UNKNOWN；结构合法后 cmd 填充载荷。
uint8_t parse_command(const uint8_t* data, size_t len, Command& cmd);

}  // namespace ice
