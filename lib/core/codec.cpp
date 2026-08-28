// core/codec.cpp — ICEBOX BLE 协议编解码实现（v1.1），框架无关

#include "codec.h"
#include <cstdio>
#include <cstring>

namespace ice {

uint16_t crc16_modbus(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (int b = 0; b < 8; ++b) {
      if (crc & 0x0001)
        crc = (crc >> 1) ^ 0xA001;
      else
        crc >>= 1;
    }
  }
  return crc;
}

void put_le16(uint8_t* p, uint16_t v) {
  p[0] = (uint8_t)(v & 0xFF);
  p[1] = (uint8_t)((v >> 8) & 0xFF);
}

void put_le_s16(uint8_t* p, int16_t v) { put_le16(p, (uint16_t)v); }

uint16_t get_le16(const uint8_t* p) { return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }

size_t pack_telemetry(const Telemetry& t, uint8_t* out) {
  out[0] = HEADER_TELEMETRY;
  out[1] = t.state_bits;
  put_le_s16(out + 2, t.cabinet_temp_10);
  put_le_s16(out + 4, t.hot_side_temp_10);
  put_le_s16(out + 6, t.target_temp_10);
  put_le16(out + 8, t.power_w10);
  put_le16(out + 10, t.voltage_cV);
  put_le16(out + 12, t.current_cA);
  put_le16(out + 14, t.fan_rpm);
  out[16] = t.peltier_pwm;
  out[17] = t.alarm;
  put_le16(out + 18, crc16_modbus(out, 18));
  return 20;
}

size_t pack_telemetry_ext(const Telemetry& t, uint8_t* out) {
  out[0] = HEADER_EXT_TELEMETRY;
  put_le_s16(out + 1, t.cold_side_temp_10);
  put_le_s16(out + 3, t.cabinet2_temp_10);
  put_le_s16(out + 5, t.board_temp_10);
  put_le16(out + 7, t.fan_target_rpm);
  out[9] = t.fan_loop_mode;
  out[10] = t.fan_duty;
  out[11] = t.cmd_ack;
  put_le16(out + 12, t.uptime_s);
  out[14] = 0;
  out[15] = 0;
  out[16] = 0;
  out[17] = 0;
  put_le16(out + 18, crc16_modbus(out, 18));
  return 20;
}

size_t pack_config(const Config& c, uint8_t* out) {
  out[0] = c.version;
  out[1] = c.alarm;
  out[2] = c.cooling_enable ? 1 : 0;
  put_le_s16(out + 3, c.target_temp_10);
  put_le_s16(out + 5, c.overtemp_10);
  put_le_s16(out + 7, c.kp100);
  put_le_s16(out + 9, c.ki100);
  put_le_s16(out + 11, c.kd100);
  out[13] = FAN_CURVE_POINTS | ((c.fan_loop_mode & 0x01) << 4);
  for (uint8_t i = 0; i < FAN_CURVE_POINTS; ++i) {
    put_le_s16(out + 14 + i * 4, c.fan_curve[i].temp_10);
    put_le16(out + 16 + i * 4, c.fan_curve[i].rpm);
  }
  put_le16(out + 30, crc16_modbus(out, 30));
  return 32;
}

size_t pack_device_info(const Config& c, char* out, size_t max_len) {
  // 格式：ICEBOX v1.1 S%04u  （17 字节 + NUL，≤20）
  snprintf(out, max_len, "ICEBOX v1.1 S%04u", (unsigned)(c.serial % 10000));
  return strnlen(out, max_len);
}

uint8_t parse_command(const uint8_t* data, size_t len, Command& cmd) {
  if (!data || len < 4) return ACK_CRC;
  if (data[0] != HEADER_COMMAND) return ACK_CRC;
  if (crc16_modbus(data, len - 2) != get_le16(data + len - 2)) return ACK_CRC;

  cmd = Command();
  cmd.id = data[1];
  const uint8_t* pl = data + 2;
  size_t plen = len - 4;

  switch (cmd.id) {
    case CMD_SET_TARGET:
      if (plen != 2) return ACK_RANGE;
      cmd.target_temp_10 = (int16_t)get_le16(pl);
      break;
    case CMD_SET_PID:
      if (plen != 6) return ACK_RANGE;
      cmd.kp100 = (int16_t)get_le16(pl);
      cmd.ki100 = (int16_t)get_le16(pl + 2);
      cmd.kd100 = (int16_t)get_le16(pl + 4);
      break;
    case CMD_SET_FAN_CURVE:
      if (plen != (size_t)FAN_CURVE_POINTS * 4) return ACK_RANGE;
      for (uint8_t i = 0; i < FAN_CURVE_POINTS; ++i) {
        cmd.curve[i].temp_10 = (int16_t)get_le16(pl + i * 4);
        cmd.curve[i].rpm = get_le16(pl + i * 4 + 2);
      }
      break;
    case CMD_CLEAR_ALARM:
      if (plen != 0) return ACK_RANGE;
      break;
    case CMD_SET_COOLING:
      if (plen != 1) return ACK_RANGE;
      cmd.on = pl[0];
      break;
    case CMD_SET_OVERTEMP:
      if (plen != 2) return ACK_RANGE;
      cmd.overtemp_10 = (int16_t)get_le16(pl);
      break;
    case CMD_SET_FAN_LOOP:
      if (plen != 1) return ACK_RANGE;
      cmd.fan_loop_mode = pl[0];
      break;
    default:
      return ACK_UNKNOWN;
  }
  return ACK_OK;
}

}  // namespace ice
