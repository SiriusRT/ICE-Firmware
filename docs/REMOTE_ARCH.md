# ICEBOX 远程控制架构（WiFi / MQTT 骨架）v0.1

> 本文档描述固件侧已实现的 WiFi + MQTT 远程骨架，以及未来接入微信小程序远程控制的路径。
> 配套固件：`ICE-Firmware`；协议见 `docs/BLE_PROTOCOL.md`（v1.1）。

---

## 1. 总体设计原则

**一套协议帧，双通道复用。** BLE 与 MQTT 传输的载荷完全相同：

| 内容 | BLE 特征 | MQTT Topic | 载荷 |
|---|---|---|---|
| 实时遥测 | FFB1 | `icebox/S<sn>/telemetry` | `A5` 帧（20B） |
| 扩展遥测 | FFB5 | `icebox/S<sn>/telemetry_ext` | `A7` 帧（20B） |
| 控制命令 | FFB2 | `icebox/S<sn>/cmd` | `A6` 帧（≤20B） |
| 配置读取 | FFB3 | `icebox/S<sn>/config/get` → `.../config` | 32B 配置帧 |
| 设备信息 | FFB4 | `.../info`（后续） | ASCII |

因此云端/小程序对两种通道**共用同一套解析函数**（小程序侧 `parseTelemetry/parseExtendedTelemetry/parseConfig` 已就绪）。

Topic 基名：`icebox/S<sn>`，`<sn>` 为 4 位序列号（如 `S0001`）。

---

## 2. 固件侧已实现（骨架）

### 2.1 模块

```
src/transport/wifi.h/.cpp   WiFi STA + 自动重连 + SNTP 校时
src/transport/mqtt.h/.cpp   MQTT 客户端（PubSubClient）
src/tasks.cpp → task_transport  连接管理 + 1Hz 遥测发布
src/tasks.cpp → task_console    串口配置命令
src/app/app.cpp             app_set_wifi_config / app_set_mqtt_config（落盘 NVS）
lib/core/state.h            Config 新增 wifi_ssid/pass、mqtt_host/port/user/pass 字段
```

### 2.2 连接状态机（task_transport，100ms 周期）

```
config.wifi_ssid 非空 ? ─否─→ 断开 WiFi
       │是
       ├─ WiFi 未连接 & 距上次尝试>10s → wifi_connect（超时5s，自动重连）
       └─ WiFi 已连接 & mqtt_host 非空
              ├─ MQTT 未连接 → mqtt_connect（订阅 cmd、config/get）
              └─ MQTT 已连接 → mqtt_loop() + 1s 一次 publish telemetry/telemetry_ext
```

- MQTT 收到 `.../cmd` 载荷 → `app_apply_command_frame()`（**与 BLE FFB2 完全同一函数**，含校验/钳位/cmd_ack 回写）。
- MQTT 收到 `.../config/get` → 回发 32B 配置帧到 `.../config`。
- 断线自动重连（WiFi 重连 10s 节流，MQTT 每次状态机周期尝试）。

### 2.3 串口配置命令（115200，UART0）

```
wifi <ssid> <pass>      # 配置并连接 WiFi（空 pass 可省略）
wifi off                # 关闭 WiFi
mqtt <host> [port]      # 配置 MQTT 服务器（默认 1883）
mqtt user <u> <p>       # 设置 MQTT 账号
mqtt off                # 关闭 MQTT
status                  # 查看 BLE/WiFi/MQTT 状态与关键参数
save                    # 立即落盘（配置变更后 2s 也会自动保存）
```

所有配置保存于 NVS，重启自动生效。

---

## 3. 云侧 / 小程序接入路径（后续开发）

### 方案 A（推荐）：腾讯云 IoT Explorer（微信生态原生）
```
ESP32 ──MQTT/TLS──→ 腾讯云 IoT ──小程序 SDK/云函数──→ 微信小程序
```
- 设备以 一机一密/证书 接入；小程序通过「腾讯连连」或自建云函数读遥测、下发命令。
- 固件侧改动：`mqtt.cpp` 改为 TLS + 设备密钥（PubSubClient 需换 `WiFiClientSecure`，或改用 esp-mqtt）。

### 方案 B：自建后端 + 公共 MQTT Broker
```
ESP32 ──MQTT──→ EMQX/EMQ Cloud ──后端服务──→ 微信小程序(wx.request / WSS)
```
- 小程序不能直连裸 TCP MQTT，需经后端转发或 `mqtt.js` over WSS（须在公众平台配置 wss 合法域名）。

### 小程序侧改动（通用，两种方案都需要）
1. 把 `ble.js` 中的 `parseTelemetry/parseExtendedTelemetry/parseConfig` 抽到共享 `protocol.js`。
2. 新增 `services/mqtt.js`（或云函数封装），主控页加「本地蓝牙 / 远程」切换。
3. 云端订阅 `icebox/S<sn>/telemetry(_ext)`，命令写 `.../cmd`（A6 帧），配置走 `.../config/get` / `.../config`。

---

## 4. 安全与可靠性（上线前必做）

| 项 | 现状 | 建议 |
|---|---|---|
| MQTT 传输加密 | 明文 TCP | 生产环境启用 TLS（设备证书） |
| 命令鉴权 | 无 | 设备绑定 SN + 密钥，云端下发前校验；仅所有者可控 |
| QoS | 0 | 命令建议 QoS1；遥测可 0 |
| WiFi 凭据 | NVS 明文 | 出厂加密或首次配网后清空 |
| 断网恢复 | 已实现自动重连 | 加看门狗与上报状态位 |

---

## 5. 验证方法（无云也可自测）

1. 烧录后串口输入 `wifi <你的SSID> <密码>` → 看到 `connected, ip=...`。
2. 起一个本地 MQTT broker（如 `mosquitto`）或公共测试 broker，串口 `mqtt <host>`。
3. 用 `mosquitto_sub` 订阅 `icebox/S0001/#`，应每 1s 收到两帧遥测（帧头 `A5`/`A7`）。
4. `mosquitto_pub -t icebox/S0001/cmd -m <A6帧>`（可用小程序 `_buildCommand` 构造）即可远程控制，行为与 BLE 完全一致。
