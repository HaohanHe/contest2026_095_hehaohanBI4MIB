# AI Radio Console

基于 Gemini-S1 (Allwinner R528) 开发板 + AI Radio HAT 的业余电台智能通联控制台。

## 功能

- **LVGL 触摸屏界面**：状态栏、最近转写、日志列表、按住录音按钮。
- **电台音频录制**：通过 LINEIN 采集电台 SPK 输出，16kHz 16bit 单声道 PCM。
- **PTT 控制**：通过 GPADC0 GPIO 经光耦隔离驱动电台 PTT。
- **ASR 识别**：默认使用 ai_agent 内置 `voice_channel`（推荐，走官方 mini_memo 路径，支持火山引擎 ASR）；也可切回 `nxrecorder` + 自定义 HTTP ASR 客户端（硅基流动 / MiMo / DeepSeek 等，需 HTTP 中继）。
- **AI Agent 日志**：通过 VelaClaw 调用 ai_agent，使用 `radio-log` Skill 生成结构化 QSO 日志。
- **com.agent.coapp 伴侣 App**：ai_agent 本身在端口 `28789` 提供完整的 REST/WebSocket API；本应用在端口 `28790` 额外暴露电台控制台视角（状态、日志）。

## 文件结构

| 文件 | 说明 |
|---|---|
| `main.c` | 应用入口，PTT 轮询，工作线程调度 |
| `radio_ui.c/h` | LVGL 主界面 |
| `audio_recorder.c/h` | LINEIN 录音（nxrecorder） |
| `ptt_control.c/h` | PTT GPIO 控制 |
| `asr_client.c/h` | HTTP ASR 客户端 |
| `radio_agent.c/h` | VelaClaw + radio-log Skill |
| `radio_server.c/h` | REST/WebSocket 服务器 |
| `radio-log.md` | ai_agent Skill 文件 |
| `Kconfig` | 构建配置选项 |
| `DEVNOTES.md` | 开发笔记与问题清单 |
| `BLE_ANDROID_INTEGRATION.md` | BLE 配网与 Android App 集成文档 |

## 硬件接线

### AI Radio HAT ↔ Gemini-S1

| AI Radio HAT | Gemini-S1 | 说明 |
|---|---|---|
| LINEIN_L | 顶部粉色 `LINEIN` | R528 音频输入 |
| LINEIN_GND | `LINEIN` 外壳/GND | 音频地 |
| HPOUT_L | 顶部绿色 `HPOUT` | R528 音频输出（可选） |
| HPOUT_GND | `HPOUT` 外壳/GND | 音频地 |
| PTT_IN | 右侧排针 `GPADC0` | PTT 控制输出 |
| PTT_GND | `GND` | 共地 |
| CAT-TX | 底部排针 `R_TX` | 电台 CAT 串口 TX（可选） |
| CAT-RX | 底部排针 `R_RX` | 电台 CAT 串口 RX（可选） |
| TWI2_SDA (PB9) | 左侧 `TWI2_SDA` | I2C 数据（可选） |
| TWI2_SCK (PB8) | 左侧 `TWI2_SCK` | I2C 时钟（可选） |
| 3.3V / GND | 对应 3.3V / GND | 供电 |

### 电台 ↔ AI Radio HAT

| 电台 | AI Radio HAT | 说明 |
|---|---|---|
| 耳机 SPK 输出 | J1 3.5mm 耳机座 | 电台音频输入到 R528 |
| MIC 输入 | J1 MIC 段 | R528 HPOUT 输出到电台（可选） |
| PTT 按键两端 | J4 PTT+ / PTT- | 由 GPADC0 控制 |
| CAT 串口 | J2 TX/RX/GND/VCC | 频率/模式读写（可选） |

## 构建

```bash
cd <openvela-project-root>

# 1. 拷贝 Gemini-S1 defconfig
cp packages_ai_agent/defconfigs/gemini-s1/gemini-s1_defconfig \
   vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/defconfig

# 2. 应用音频补丁
bash packages_ai_agent/fix_gemini_s1.sh

# 3. 开启 ai_radio_console（以及所需依赖）
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay menuconfig
#   → Application Configuration → AI Radio Console (Gemini-S1 HAT) 勾选
#   → 依赖 AUDIO、LVGL、media server、ai_agent 已包含在 defconfig

# 4. 编译
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/ \
    -e -Wno-error -j"$(nproc)"

# 5. 打包烧录
source envsetup.sh && lunch_nuttx && pack
```

## 运行

在 NSH 中启动：

```nsh
nsh> ai_radio_console
```

首次运行前，配置 ASR API 密钥：

```nsh
nsh> mkdir -p /data/agent/config
nsh> echo "sk-xxxxxxxx" > /data/agent/config/asr_key.txt
```

> 或设置环境变量：

```nsh
nsh> export ASR_API_KEY=sk-xxxxxxxx
nsh> ai_radio_console
```

### PTT 自检

无需手动按屏幕/物理 PTT，启动后自动模拟一次指定时长的 PTT 流程，方便无头测试：

```nsh
# 命令行指定时长（ms）
nsh> ai_radio_console --ptt-selftest 1500

# 或在 menuconfig 中设置默认值
# AI_RADIO_CONSOLE_PTT_SELFTEST_MS = 1500
```

启动 ai_agent（用于日志保存）：

```nsh
nsh> ai_agent
vela> set_llm <preset> <api_key>
```

## 配置项

`menuconfig` 路径：`Application Configuration → AI Radio Console`

| 配置项 | 默认值 | 说明 |
|---|---|---|
| `AI_RADIO_CONSOLE_USE_VOICE_CHANNEL` | `y` | 使用 ai_agent `voice_channel` 处理 PTT+ASR（推荐） |
| `AI_RADIO_CONSOLE_PTT_GPIO` | `/dev/gpio0` | PTT GPIO 设备路径 |
| `AI_RADIO_CONSOLE_PTT_PIN` | `0` | PTT GPIO 引脚号 |
| `AI_RADIO_CONSOLE_PTT_SELFTEST_MS` | `0` | PTT 自检按住时长（ms），`0` 关闭 |
| `AI_RADIO_CONSOLE_AUDIO_DEV` | `/dev/audio/pcm0c` | 音频捕获设备（仅 nxrecorder 路径） |
| `AI_RADIO_CONSOLE_ASR_URL` | `https://api.siliconflow.cn/v1/audio/transcriptions` | ASR 端点（仅 nxrecorder 路径） |
| `AI_RADIO_CONSOLE_ASR_MODEL` | `iic/SenseVoiceSmall` | ASR 模型（仅 nxrecorder 路径） |
| `AI_RADIO_CONSOLE_SERVER` | `y` | 是否启用内置 REST/WebSocket 服务器 |
| `AI_RADIO_CONSOLE_SERVER_PORT` | `28790` | 内置服务器端口（避开 ai_agent 的 28789）|

## 音频后端选择

### 方案 A：voice_channel（默认，官方推荐）

与 `mini_memo` 一致，直接调用 ai_agent 的 `voice_channel_start()` / `voice_channel_stop_with_text()`。优点：

- 代码简洁，不重复造轮子
- ai_agent 内部处理录音、ASR、HTTPS、音频图补丁
- 配置走 ai_agent `/data/agent/config/config.json`，手机 App 也能统一读写

限制：当前 ai_agent 的 `voice_channel` 只内置了**火山引擎（Doubao）ASR**。需要把火山引擎凭证写入 ai_agent 配置：

```nsh
nsh> cat > /data/agent/config/config.json <<'EOF'
{
  "volc_appkey": "your-appkey",
  "volc_token": "your-token",
  "volc_asr_cluster": "volcengine_streaming_common"
}
EOF
```

启动顺序：

```nsh
nsh> ai_agent &
nsh> ai_radio_console
```

### 方案 B：nxrecorder + 自定义 ASR（legacy）

在 `menuconfig` 中关闭 `AI_RADIO_CONSOLE_USE_VOICE_CHANNEL`，则使用原有路径：

- `nxrecorder` 录制 PCM 到 `/tmp/radio_recording.pcm`
- 自定义 HTTP 客户端伪装 WAV 后调用 OpenAI 兼容 ASR
- 支持硅基流动 / MiMo / DeepSeek 等

限制：板端客户端只支持 HTTP。若服务端要求 HTTPS，需运行 `scripts/relay.py` 做 HTTP→HTTPS 转发。

启动顺序：

```nsh
nsh> echo "sk-xxxx" > /data/agent/config/asr_key.txt
nsh> ai_agent &
nsh> ai_radio_console
```

## REST / WebSocket API

> **架构说明**：ai_agent 本身已在 `0.0.0.0:28789` 提供完整的 `com.agent.coapp` API（配置、技能、日志、对话）。
> 为避免端口冲突，`ai_radio_console` 的内置服务器默认监听 **`28790`**，提供电台控制台视角。
> 如果你只需要 com.agent.coapp，直接连 `28789` 即可；如需电台实时状态/日志，可连 `28790`。

### REST（端口 28790）

```bash
BASE=http://<board-ip>:28790

# 健康检查
curl $BASE/a2a/health

# 读取电台控制台配置
curl $BASE/api/config

# 更新配置（只会覆盖存在的键，不会用 masked 值覆盖密钥）
curl -X PUT $BASE/api/config \
  -H "Content-Type: application/json" \
  -d '{"asr_api_key":"sk-xxxx","asr_model":"iic/SenseVoiceSmall"}'

# 列出技能（含 content 预览）
curl $BASE/api/skills

# 推送技能
curl -X POST $BASE/api/skills \
  -H "Content-Type: application/json" \
  -d '{"name":"radio-log","content":"# Radio Log\n..."}'

# 删除技能
curl -X DELETE $BASE/api/skills/radio-log

# 读取 QSO 日志
curl $BASE/api/logs
```

### ai_agent 原生 API（端口 28789）

```bash
BASE=http://<board-ip>:28789

# 读取 ai_agent 配置（LLM / ASR / TTS）
curl $BASE/api/config

# 推送配置
curl -X PUT $BASE/api/config \
  -H "Content-Type: application/json" \
  -d '{"llm_api_key":"sk-xxxx","model":"mimo-v2.5-pro"}'

# 列出技能
curl $BASE/api/skills

# 推送技能
curl -X POST $BASE/api/skills \
  -H "Content-Type: application/json" \
  -d '{"name":"radio-log","content":"# Radio Log\n..."}'

# 删除技能
curl -X DELETE $BASE/api/skills/radio-log

# 读取运行日志
curl $BASE/api/logs
```

### WebSocket

- **ai_agent 对话**：`ws://<board-ip>:28789`（与 com.agent.coapp 一致）
- **电台控制台状态**：`ws://<board-ip>:28790`（任意路径，识别 `Upgrade: websocket` 即握手）

电台控制台 WS 收到状态：

```json
{"type":"status","content":"recording"}
{"type":"status","content":"transcribing"}
{"type":"status","content":"thinking"}
{"type":"status","content":"idle"}
```

收到日志：

```json
{"type":"log","timestamp":"14:30:00","content":"BD2AAA 这里是 BD2BBB ..."}
```

## 演示脚本

```bash
#!/bin/sh
# demo.sh - 在板端测试电台控制台 REST API（默认 28790）

IP=127.0.0.1
PORT=28790

echo "== 健康检查 =="
curl -s http://$IP:$PORT/a2a/health | head -c 500

echo ""
echo "== 读取配置 =="
curl -s http://$IP:$PORT/api/config | head -c 500

echo ""
echo "== 列出技能 =="
curl -s http://$IP:$PORT/api/skills | head -c 500

echo ""
echo "== 读取日志 =="
curl -s http://$IP:$PORT/api/logs | head -c 500
```

## radio-log Skill

`radio-log` Skill 会在 `radio_agent_init()` 时自动安装到 `/data/agent/skills/radio-log.md`。它指示 ai_agent 从转写文本中提取：

- 时间
- 本台呼号 / 对方呼号
- 频率、模式
- 信号报告 RST
- 操作员姓名、位置
- 备注

并保存到 `/data/agent/memory/radio/YYYY-MM-DD.md`。

## HTTPS 说明

板端最小 ASR 客户端只支持 HTTP。若使用硅基流动 / MiMo / DeepSeek 等 HTTPS 服务，建议：

1. **HTTP 中继**：在局域网 PC/手机运行 `relay.py` 转发到 HTTPS 服务。
2. **ai_agent Volc ASR**：在 ai_agent 中配置火山引擎 ASR，由 ai_agent 处理语音转写。

## 故障排除

| 现象 | 排查 |
|---|---|
| UI 不显示 | 确认 `CONFIG_LV_USE_NUTTX_LCD` 与 `/dev/lcd0`；检查触摸屏配置 |
| 录音失败 | 确认 `/dev/audio/pcm0c` 存在；检查 fix_gemini_s1.sh 是否已应用 |
| ASR 失败 | 检查 `asr_key.txt` 或 `ASR_API_KEY`；确认网络可达；若使用 HTTPS 需中继 |
| PTT 无反应 | 检查 GPADC0 引脚定义；确认 GPIO 设备路径正确 |
| App 连不上 | com.agent.coapp 走 ai_agent 的 `28789`；电台控制台 WS 走 `28790`。确认对应端口监听且在同一 WiFi |

## 相关文档

- [DEVNOTES.md](DEVNOTES.md)：开发心路历程、问题清单与修复记录。
- [BLE_ANDROID_INTEGRATION.md](BLE_ANDROID_INTEGRATION.md)：BLE 配网流程、NUS 服务 UUID、REST/WebSocket 端口说明，以及 Android 端开发 checklist。

## 许可证

Apache-2.0，与 OpenVela / NuttX 保持一致。
