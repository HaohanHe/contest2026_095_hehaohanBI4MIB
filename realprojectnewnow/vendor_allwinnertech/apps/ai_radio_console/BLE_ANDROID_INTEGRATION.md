# AI Radio Console — BLE 配网与 Android 伴侣 App 集成文档

> 本文为手机端开发预留接口，覆盖 BLE 配网流程、NUS 服务定义、REST/WebSocket API 及扩展命令预留点。
> 对应代码：
> - 设备端：`packages_ai_agent/src/infra/ble_gatt.c`、`packages_ai_agent/src/infra/ble_cmd_handler.c`
> - 电台控制台：`vendor_allwinnertech/apps/ai_radio_console/`
> - 手机端：`com.agent.coapp/`

---

## 1. 整体架构

```text
┌─────────────────┐      BLE (NUS)       ┌──────────────────────┐
│  Android 手机    │ ◄──────────────────► │   Gemini-S1 开发板    │
│  com.agent.coapp │                      │  ai_agent + ai_radio │
└─────────────────┘                      └──────────────────────┘
       │                                          │
       │  WiFi 配网完成后                         │
       │  通过 HTTP/WebSocket 通信                │
       ▼                                          ▼
  http://<board-ip>:28789                 /data/agent/...
  http://<board-ip>:28790
```

**关键端口约定：**

| 服务 | 端口 | 说明 |
|------|------|------|
| ai_agent 原生 API | 28789 | com.agent.coapp 默认连接，提供 config/skills/logs/chat |
| ai_radio_console 电台视图 | 28790 | 电台状态、实时日志、电台专属配置 |

> 遵循小米官方示例，**不重复实现 BLE**：`ai_agent` 已内置 NUS GATT 服务和 WiFi 配网命令处理。

---

## 2. BLE 配网流程

### 2.1 设备端启用条件

在 `menuconfig` 中开启：

```
Application Configuration → Packages → Vela AI Agent
  → Enable BLE GATT data channel (CONFIG_AI_AGENT_BLE_GATT=y)
  → Enable REST API endpoints (CONFIG_AI_AGENT_REST_API=y)
  → Enable WiFi support (CONFIG_AI_AGENT_WIFI=y)
```

启动时 `ai_agent` 自动注册 NUS 服务并开始广播：

```nsh
nsh> ai_agent
```

### 2.2 NUS 服务定义

使用标准 Nordic UART Service UUID：

| UUID | 方向 | 说明 |
|------|------|------|
| `6e400001-b5a3-f393-e0a9-e50e24dcca9e` | Service | Nordic UART Service |
| `6e400002-b5a3-f393-e0a9-e50e24dcca9e` | Phone → Device (RX) | 写入 JSON 命令 |
| `6e400003-b5a3-f393-e0a9-e50e24dcca9e` | Device → Phone (TX) | Notify 状态/结果 |

广播名称：`VelaClaw`

MTU：App 请求 512，实际以协商为准。单包 ATT payload = MTU - 3。

### 2.3 设备端已支持的 JSON 命令

`ble_cmd_handler_recv` 解析 RX 写入的 UTF-8 JSON：

```json
// 配置 WiFi（password 为空表示开放网络）
{"cmd":"wifi_config","ssid":"YOUR_SSID","password":"YOUR_PASSWORD"}

// 心跳
{"cmd":"ping"}

// 查询网络状态（返回 IP）
{"cmd":"status"}
```

设备端响应格式：

```json
{"status":"ok","msg":"wifi connected"}
{"status":"ok","msg":"pong"}
{"status":"ok","network":true,"ip":"192.168.1.123"}
{"status":"error","msg":"missing ssid"}
```

手机端收到 `"ip":"..."` 后保存到 DataStore，后续 REST/WebSocket 直接连该 IP。

### 2.4 手机端流程（com.agent.coapp）

1. **扫描**：`BleManager.startScan()` 按 MAC 地址 `D0:C1:BF:B0:DF:F4` 过滤，同时显示有名称的 NUS 设备。
2. **连接**：`BluetoothDevice.connectGatt(..., TRANSPORT_LE)`。
3. **MTU 协商**：`gatt.requestMtu(512)`。
4. **服务发现**：使能 TX characteristic 的 notification（CCCD descriptor `0x2902`）。
5. **写入 WiFi 凭据**：通过 RX characteristic 写入 UTF-8 JSON。
6. **等待响应**：设备连接 WiFi 后返回 `"msg":"wifi connected"`，随后 App 发送 `{"cmd":"status"}` 获取 IP。
7. **保存 IP**：写入 DataStore 的 `device_ip`，默认端口 28789。

### 2.5 真机验证命令

板端启动 ai_agent 后，可用手机或 nRF Connect 测试：

```text
连接 NUS 服务 → 启用 TX notify → 写入 RX:
{"cmd":"wifi_config","ssid":"test","password":"12345678"}

收到 notify:
{"status":"ok","msg":"wifi connected"}
{"status":"ok","network":true,"ip":"192.168.x.x"}
```

---

## 3. REST API 接口

### 3.1 ai_agent 原生 API（端口 28789）

`packages_ai_agent/src/infra/api_handler.c` 实现，与 com.agent.coapp 默认兼容。

| 方法 | 路径 | 请求体 | 响应 | 说明 |
|------|------|--------|------|------|
| GET | `/api/config` | - | `Map<String,String>` | 配置表，密钥已 mask |
| PUT | `/api/config` | `Map<String,String>` | `{"ok":true}` | 更新配置；含 `****` 的 secret 不会被覆盖 |
| GET | `/api/skills` | - | `{"skills":[...]}` | 技能列表，含 `content` 预览 |
| POST | `/api/skills` | `{"name":"...","content":"..."}` | `{"ok":true}` | 推送/更新技能并热加载 |
| DELETE | `/api/skills/{name}` | - | `{"ok":true}` | 删除技能并热加载 |
| GET | `/api/logs` | `?lines=N` | `{"logs":[...]}` | 运行日志 |

WebSocket：`ws://<board-ip>:28789/ws/chat`

### 3.2 ai_radio_console 电台视图 API（端口 28790）

`radio_server.c` 实现，面向电台控制台场景。

| 方法 | 路径 | 请求体 | 响应 | 说明 |
|------|------|--------|------|------|
| GET | `/a2a/health` | - | `{"status":"ok"}` | 健康检查 |
| GET | `/api/config` | - | `Map<String,String>` | 电台控制台配置，secret mask |
| PUT | `/api/config` | `Map<String,String>` | `{"ok":true}` | 更新配置 |
| GET | `/api/skills` | - | `{"skills":[...]}` | 技能列表，`content:null`，含 `mtime` |
| GET | `/api/skills/{name}` | - | `{"name":"...","content":"..."}` | 读取完整技能 Markdown |
| POST | `/api/skills` | `{"name":"...","content":"..."}` | `{"ok":true}` | 推送/更新技能 |
| DELETE | `/api/skills/{name}` | - | `{"ok":true}` | 删除技能 |
| GET | `/api/logs` | `?lines=N` | `{"logs":["..."]}` | QSO 日志（每文件一条字符串） |

WebSocket：`ws://<board-ip>:28790/ws/chat`（任意路径识别 `Upgrade: websocket` 即握手）

电台控制台 WS 广播消息：

```json
{"type":"status","content":"recording"}
{"type":"status","content":"transcribing"}
{"type":"status","content":"thinking"}
{"type":"status","content":"idle"}
{"type":"log","timestamp":"14:30:00","content":"BD2AAA 这里是 BD2BBB ..."}
```

### 3.3 让 App 同时管理电台控制台

com.agent.coapp 默认端口是 28789。如需管理电台控制台：

**方案 A：App 支持多端口/设备切换**
在 `ConfigRepository` / `ConfigScreen` 中增加 `radio_console_port`（默认 28790），并提供切换入口。

**方案 B：把电台配置同步到 ai_agent config**
在 `ai_radio_console` 启动时，将常用配置（ASR URL/模型、PTT GPIO 等）写入 `/data/agent/config/config.json` 的扁平键：

```json
{
  "radio_asr_url": "http://...",
  "radio_asr_model": "iic/SenseVoiceSmall",
  "radio_ptt_gpio": "/dev/gpio0"
}
```

App 通过 28789 读取/修改，再通过 28790 触发录音/监听状态。

---

## 4. 扩展 BLE 命令预留点

后续手机端如需远程控制电台（PTT、录音、静音等），推荐在 `packages_ai_agent/src/infra/ble_cmd_handler.c` 中扩展，复用现有 BLE 通道，不新增服务。

### 4.1 建议新增命令

```json
// 远程按下 PTT
{"cmd":"radio_ptt_key"}

// 远程松开 PTT
{"cmd":"radio_ptt_unkey"}

// 查询电台状态
{"cmd":"radio_status"}

// 设置 ASR 配置（通过 ai_agent config 同步）
{"cmd":"radio_config","asr_url":"...","asr_model":"..."}
```

### 4.2 设备端最小修改示例

在 `ble_cmd_handler.c` 的 `ble_cmd_handler_recv` 中增加分支：

```c
static void handle_radio_ptt_key(void)
{
    /* 调用 ai_radio_console 提供的接口或写 config 标记 */
    send_response("{\"status\":\"ok\",\"msg\":\"ptt keyed\"}");
}

/* ... */
if (strcmp(cmd, "radio_ptt_key") == 0) {
    handle_radio_ptt_key();
}
```

若需与 `ai_radio_console` 进程交互，可通过以下方式：

1. **VelaClaw 消息总线**：`ai_radio_console` 订阅自定义 topic，BLE handler 发布事件。
2. **Unix domain socket / 命名 FIFO**：进程间轻量通信。
3. **共享配置文件 + 信号**：简单场景下够用。

---

## 5. Android 端开发 checklist

- [ ] 确认 `BleManager.kt` 中 `TARGET_DEVICE_ADDRESS` 与 Gemini-S1 实际蓝牙 MAC 一致。
- [ ] 如需兼容多台设备，移除 MAC 硬编码，改为按 NUS service UUID + 设备名称前缀扫描。
- [ ] 配网成功后保存 `device_ip` 与 `radio_console_port`（28790）到 DataStore。
- [ ] 在 `ConfigScreen` 增加电台控制台专属配置项：ASR URL/模型/密钥、PTT GPIO。
- [ ] 在 `ChatScreen` 或新增 `RadioScreen` 中：
  - 显示当前电台状态（recording/transcribing/idle）。
  - 提供“按住说话”虚拟 PTT 按钮（通过 BLE `radio_ptt_key/unkey` 或 REST 触发）。
  - 展示 QSO 日志列表（GET `/api/logs`）。
- [ ] WebSocket 同时监听 28789（ai_agent 对话）和 28790（电台状态/日志），或二选一。
- [ ] 考虑把 `radio-log.md` 作为默认技能从 App 推送到设备（首次连接时）。

---

## 6. 常见问题

| 现象 | 原因/排查 |
|------|-----------|
| 手机扫描不到设备 | 确认 ai_agent 已启动、`CONFIG_AI_AGENT_BLE_GATT=y`、蓝牙天线已接 |
| 连接后无 NUS 服务 | 清除手机蓝牙缓存，或用 `refreshGattCache` 反射刷新 |
| 写入 WiFi 后无响应 | 确认 `CONFIG_AI_AGENT_WIFI=y`、wapi 工具可用、SSID/password 正确 |
| 配网成功但 App 连不上 REST | 确认手机与板子在同一 WiFi；防火墙未拦截 28789/28790 |
| 28790 端口无服务 | 确认 `ai_radio_console` 已启动且 `CONFIG_AI_RADIO_CONSOLE_SERVER=y` |

---

## 7. 参考文件

- 设备端 BLE：`packages_ai_agent/src/infra/ble_gatt.c`、`ble_cmd_handler.c`
- 设备端网络：`packages_ai_agent/src/infra/network_manager.c/h`
- 电台控制台：`vendor_allwinnertech/apps/ai_radio_console/radio_server.c`
- Android App：`com.agent.coapp/app/src/main/java/com/agent/coapp/ble/BleManager.kt`
- Android API：`com.agent.coapp/app/src/main/java/com/agent/coapp/network/DeviceApiService.kt`
