# AI Radio Console 开发笔记

> 记录 Gemini-S1 + AI Radio HAT 项目从 0 到 1 的开发过程、踩坑与修复方案。
> 项目目标：同时服务 "2026 国际空天挑战赛空间通信赛道" 与 "小米 OpenVela AI 硬件挑战赛"。

---

## 1. 项目概述

### 1.1 硬件

- **主控**：Gemini-S1（Allwinner R528，双核 Cortex-A7，小端）
- **扩展板**：AI Radio HAT（LINEIN、HPOUT、PTT、CAT 串口）
- **交互**：3.5" 电阻/电容触摸屏（320×240）
- **联网**：板载 Wi-Fi（SDIO）+ BLE（用于手机配网）

### 1.2 软件栈

- **OS**：OpenVela / NuttX
- **UI**：LVGL（NuttX LCD / touch 后端）
- **音频**：NuttX nxrecorder + media server
- **AI**：ai_agent（VelaClaw 本地 IPC）+ 自定义 `radio-log` Skill
- **ASR**：OpenAI 兼容 HTTP API（SiliconFlow / MiMo / DeepSeek）
- **手机端**：com.agent.coapp（Kotlin + Jetpack Compose）

### 1.3 核心数据流

```text
PTT 按下 → LINEIN 录音 → PCM → ASR → 文本 → VelaClaw → ai_agent (radio-log Skill)
                                                            ↓
                                                    /data/agent/memory/radio/YYYY-MM-DD.md
                                                            ↓
                                            LVGL 日志列表 / REST / WebSocket / 手机 App
```

---

## 2. 开发心路历程

### 阶段一：需求对齐与仓库准备

1. **明确双赛需求**
   - 哈工大比赛侧重：无线电通联日志的自动记录、结构化、可展示。
   - 小米比赛侧重：OpenVela + ai_agent + 手机 App，走官方推荐技术栈。
   - 统一产品形态："AI Radio Console"，既是独立设备，也是 ai_agent 的硬件载体。

2. **研读官方示例**
   - `packages_demos/mini_memo`：学习了 tileview、PTT 按钮、异步 UI 更新模式。
   - `packages_demos/ai_chat`：理解了 ai_agent 的 channel/Skill 结构。
   - `packages_ai_agent`：重点看了 `velaclaw/client.h`、REST API、BLE GATT、Skill loader。
   - `com.agent.coapp`：反向确认设备端需要暴露的 REST/WebSocket 接口。

3. **仓库布局确认**
   - 工作区实际目录名为 `packages_ai_agent`，但官方文档/脚本多处写成 `packages/ai_agent`。
   - 在 `Makefile` 中通过 `wildcard` 同时兼容两种目录名，避免 clone 时命名差异导致编译失败。

### 阶段二：基础功能实现

1. **LVGL UI**
   - 初始版本直接在工作线程中调用 `lv_label_set_text`，导致线程安全问题。
   - 参考 `mini_memo_ui.c` 后，改为 `lv_async_call` 将 UI 更新抛回 LVGL 线程。
   - 添加中文支持：优先加载 `/data/res/fonts/MiSans-Normal.ttf`（与 `ai_chat` 示例一致），失败则回退 `lv_font_simsun_16_cjk`，再失败回退 Montserrat。

2. **LINEIN 录音**
   - 直接复用 `audio_test` 的 `nxrecorder` 模式，回调把 `ap_buffer_s` 写入 `/tmp/radio_recording.pcm`。
   - 运行前必须先执行 `packages_ai_agent/fix_gemini_s1.sh`，替换媒体 graph/criteria，否则录音无帧或 PTT 卡住。

3. **PTT GPIO**
   - 第一次误用 `GPIOC_SETOUTPUT` ioctl，参考 `rtk_hci_board.c` 后改为：
     ```c
     ioctl(fd, GPIOC_SETPINTYPE, (unsigned long)GPIO_OUTPUT_PIN);
     ioctl(fd, GPIOC_WRITE, (unsigned long)true);
     ```
   - 当前 GPADC0 对应 `/dev/gpio0`、pin 0。

4. **ASR HTTP 客户端**
   - 手写 multipart/form-data，带 44 字节 WAV 头，把 PCM 伪装成 `radio.wav`。
   - **已知限制**：仅支持 HTTP。HTTPS 服务（硅基流动/MiMo/DeepSeek）需要：
     - 局域网 PC/手机跑 `relay.py` 转发；或
     - 直接在 ai_agent 中配置火山引擎 ASR（ai_agent 内部支持 TLS）。

5. **ai_agent 集成**
   - 通过 `velaclaw_client_open` 拿到本地 IPC client。
   - 初始版本 `velaclaw_ask` 后 `usleep(500ms)` 假装等待，实际不可靠。
   - 改为 POSIX 信号量 `sem_timedwait` 等待 callback，超时 30s，真正拿到 agent 响应后再返回 UI。

6. **radio-log Skill**
   - Markdown 格式，带 `description:` 元数据。
   - 安装路径 `/data/agent/skills/radio-log.md`。
   - 示例包含中文 QSO 场景，训练 agent 提取呼号/频率/模式/RST 等字段。

### 阶段三：手机 App 兼容性

1. **端口规划**
   - ai_agent 原生 API 监听 `0.0.0.0:28789`（com.agent.coapp 默认连接）。
   - `ai_radio_console` 内置 REST/WebSocket 服务器使用 `28790`，避免冲突。

2. **接口对齐**
   - 通过反读 `DeviceApiService.kt` / `DeviceRepository.kt`，确认需要：
     - `GET /api/config` → flat `Map<String,String>`
     - `PUT /api/config` → flat `Map<String,String>`
     - `GET /api/skills` → `{"skills":[...]}`
     - `POST /api/skills` → `{"name":"...","content":"..."}`
     - `DELETE /api/skills/{name}`
     - `GET /api/logs?lines=N` → `{"logs":[...]}`
     - WebSocket 任意路径，握手后收发 JSON。

3. **修复 Android 兼容 bug**
   - Skill 列表原本缺少 `mtime` 字段，导致 App 卡片日期为空。
   - `json_get_string` 原本不支持转义引号，App 推送的 Markdown 中 `"` 会截断内容。
   - Skill content buffer 原本只有 4096，长 Skill 会失败；改为按 `MAX_BODY_LEN` 动态分配。
   - `/api/logs` 原本只返回每文件第一行；改为返回每文件完整内容（截断到 2KB）。

### 阶段四：BLE 配网预留

1. **不重复造轮子**
   - BLE NUS（Nordic UART Service）已在 `packages_ai_agent/src/infra/ble_gatt.c` 实现。
   - WiFi 配网命令处理已在 `packages_ai_agent/src/infra/ble_cmd_handler.c` 实现。
   - `ai_radio_console` 不需要自己写 BLE，只需保证 ai_agent 启用 `CONFIG_AI_AGENT_BLE_GATT` 并运行。

2. **手机端已就绪**
   - `com.agent.coapp` 的 `BleManager.kt` 已按 NUS 服务实现扫描/连接/写入/通知。
   - 写入格式：`{"cmd":"wifi_config","ssid":"...","password":"..."}`
   - 设备返回：`{"status":"ok","msg":"wifi connected","network":true,"ip":"x.x.x.x"}`

3. **预留扩展**
   - 如需在电台控制台侧响应 BLE 命令（例如 PTT 控制、录音触发），可在 `ble_cmd_handler.c` 新增 `radio_xxx` cmd，或在 `ai_radio_console` 中通过 VelaClaw 订阅消息。
   - 当前先保证 WiFi 配网链路打通。

### 阶段五：稳定性与边界情况收尾

在真机联调前，集中修复了并发、内存、JSON、线程栈等边界问题：

1. **ai_agent 响应可靠性**
   - 原 `g_claw_response` 仅 1024 字节，LLM 稍长回复即截断；扩至 4096 字节。
   - 旧请求超时后 semaphore 可能残留 post，导致新请求误唤醒；在 `velaclaw_ask` 前用 `sem_trywait` 清空旧信号。
   - Prompt 原用固定栈 buffer，长转写文本可能溢出；改为按 `snprintf(NULL, 0, ...)` 动态分配。

2. **HTTP/JSON 健壮性**
   - `radio_server.c` 新增 `http_status_text`，返回标准 reason phrase（200 OK / 400 Bad Request 等）。
   - 配置解析增加 `json_unescape`，支持 `\"\\\/bfnrt` 等转义，避免含换行或引号的 value 被截断。
   - ASR 响应解析同样支持转义引号，防止 SiliconFlow/MiMo 返回的文本被提前截断。
   - Skill POST 响应 buffer 计算漏加 `name` 长度，已修正为 `strlen(escaped) + strlen(name) + 64`。

3. **线程与资源安全**
   - `start_processing()` 中 `g_worker_busy` 初始化移入 `g_work_lock` 临界区，消除竞态。
   - Worker 线程栈从 16 KiB 扩至 32 KiB，避免 HTTP + JSON + VelaClaw 组合调用时栈溢出。
   - `radio_ui.c` 日志截断时未同步减少 `log_lines`，导致行数统计漂移；已修复。

4. **文档收尾**
   - 输出 [BLE_ANDROID_INTEGRATION.md](BLE_ANDROID_INTEGRATION.md)，把手机端需要的服务 UUID、JSON 命令、REST/WebSocket 端口、Android checklist 一次性写清楚。
   - 更新本 DEVNOTES，形成开发闭环。

### 阶段六：对齐 mini_memo 官方示例 — voice_channel 双路径

研读 `mini_memo_guide.md` 后发现，官方推荐的 PTT+ASR 路径是 `voice_channel_start/stop_with_text`，而不是我们之前直接调 `nxrecorder` + 手写 HTTP ASR。于是做了一次音频架构升级，同时保留旧路径作为 fallback：

1. **双后端 `audio_recorder`**
   - `CONFIG_AI_RADIO_CONSOLE_USE_VOICE_CHANNEL=y`（默认）：调用 ai_agent `voice_channel`，录音 + ASR 由 agent 内部完成。
   - `=n`：保留原有 `nxrecorder` + `asr_client` 路径，用于非火山引擎 ASR（硅基流动 / MiMo / DeepSeek）。
   - 统一接口：`audio_recorder_start/stop/get_text/get_data`，`main.c` 先尝试 `get_text`，`-ENOTSUP` 时回退到 PCM+ASR。

2. **Worker 线程栈扩至 40KB**
   - 与 `mini_memo` 的 `STACKSIZE = 40960` 对齐，避免 LLM/ASR 调用栈溢出。

3. **PTT 自检模式**
   - 新增 Kconfig `AI_RADIO_CONSOLE_PTT_SELFTEST_MS` 与命令行 `--ptt-selftest [ms]`，启动后自动模拟一次 PTT 流程，方便无头验证。

4. **已知限制**
   - ai_agent 当前 `voice_channel` 只内置火山引擎（Doubao）ASR；如需其他厂商，要么切 nxrecorder 路径 + relay.py，要么自行注册新的 `voice_asr_ops_t` 后端。

---

## 3. 问题清单与解决方案

| # | 问题 | 影响 | 解决方案 | 状态 |
|---|------|------|----------|------|
| 1 | PTT GPIO 误用 `GPIOC_SETOUTPUT` | PTT 无输出 | 改为 `GPIOC_SETPINTYPE(GPIO_OUTPUT_PIN)` + `GPIOC_WRITE` | 已修复 |
| 2 | LVGL 在工作线程直接更新 | 崩溃/花屏 | 所有更新走 `lv_async_call` | 已修复 |
| 3 | 中文字体未加载/乱码 | UI 中文显示为方块 | 增加 FreeType MiSans 回退机制，并在 `radio_ui_init` 中显式调用 `init_fonts()` | 已修复 |
| 4 | `init_fonts()` 从未被调用 | 字体指针为 NULL，UI 崩溃 | 在 `radio_ui_init()` 中 `create_main_screen()` 之前调用 `init_fonts()` | 已修复 |
| 5 | ASR 客户端不支持 HTTPS | 无法直连 SiliconFlow/MiMo | 文档说明使用 HTTP relay 或 ai_agent Volc ASR | 已记录 |
| 6 | ai_agent 回调用 `usleep` 等待 | 拿不到真实响应/超时 | 使用 `sem_init` + `sem_timedwait` 等待 callback | 已修复 |
| 7 | `radio_server` 与 ai_agent 端口冲突 | 启动失败 | 默认端口改为 `28790`，ai_agent 保持 `28789` | 已修复 |
| 8 | `/api/skills` 缺少 `mtime` | App 技能卡片日期为空 | 用 `localtime` + `strftime` 生成 ISO 时间字符串 | 已修复 |
| 9 | `json_get_string` 不支持转义 | 推送含 `"` 的 Markdown 被截断 | 重写 parser，支持 `\"\\\/bfnrtu` | 已修复 |
| 10 | Skill content buffer 仅 4096 | 长 Skill 推送失败 | `handle_skills_post` 改为 `malloc(MAX_BODY_LEN)` | 已修复 |
| 11 | `/api/logs` 只返回第一行 | 日志详情不完整 | 返回每文件完整内容（截断 2KB） | 已修复 |
| 12 | Makefile include 路径错误 | 找不到 `velaclaw/client.h` | 用 `wildcard` 兼容 `packages/ai_agent` 与 `packages_ai_agent` | 已修复 |
| 13 | README 构建路径错误 | 用户按文档执行失败 | `packages/ai_agent` 改为 `packages_ai_agent` | 已修复 |
| 14 | `radio_strcasestr` fallback 宏可能使用 `strcasestr` | 某些工具链缺少该函数 | 保留 fallback，默认走自定义实现 | 已修复 |
| 15 | WS client mutex 初始化顺序 | 竞态条件 | `pthread_mutex_init` 在 `active = true` 之前 | 已修复 |
| 16 | WS 连接消息长度硬编码 | 可能发送越界 | 改为 `strlen(connected_msg)` | 已修复 |
| 17 | `pthread_create` 返回值未检查 | 线程创建失败静默 | `main.c` 中增加 `ret != 0` 判断 | 已修复 |
| 18 | `main.c` 中未使用的信号量 | 编译警告/资源泄漏 | 移除 `g_work_sem` 与 `<semaphore.h>` | 已修复 |
| 19 | `radio_agent.c` 内嵌 Skill 与 `radio-log.md` 不一致 | 两份内容分歧 | 统一为同一份 Markdown 内容 | 已修复 |
| 20 | UI 文本 buffer 仅 128 字节 | 中文长文本被截断 | `ui_text_evt_t`/`ui_log_evt_t.text` 扩大到 256 | 已修复 |
| 21 | ai_agent 响应 buffer 仅 1024 字节 | 长回复被截断 | `g_claw_response` 扩至 4096 | 已修复 |
| 22 | Semaphore 残留 post | 新请求被旧回调提前唤醒 | `velaclaw_ask` 前用 `sem_trywait` 清空 | 已修复 |
| 23 | Prompt 使用固定栈 buffer | 长转写文本栈溢出 | 按实际长度 `malloc` 动态构造 prompt | 已修复 |
| 24 | HTTP 响应无标准状态文本 | 客户端解析不友好 | 新增 `http_status_text` 映射 | 已修复 |
| 25 | 配置值 JSON 转义未解码 | 含 `"` 或换行的 value 被截断 | 新增 `json_unescape` | 已修复 |
| 26 | ASR 响应未处理转义引号 | 文本提前结束 | `extract_text` 中支持 `\"\\\/bfnrt` | 已修复 |
| 27 | Skill POST 响应 buffer 计算偏小 | 可能越界 | 计算时加入 `strlen(name) + 64` | 已修复 |
| 28 | `g_worker_busy` 初始化在锁外 | 竞态导致重复创建 worker | 移入 `g_work_lock` 临界区 | 已修复 |
| 29 | Worker 线程栈 16 KiB | HTTP+JSON+IPC 调用栈溢出 | 扩至 32 KiB → 进一步扩至 40KB 对齐 mini_memo | 已修复 |
| 30 | 日志截断时行数计数未减 | `log_lines` 漂移 | 截断旧日志时同步 `--log_lines` | 已修复 |
| 31 | 音频路径与官方示例不一致 | 未按小米推荐方式使用 ai_agent | 重构 `audio_recorder` 支持 `voice_channel` + `nxrecorder` 双后端 | 已修复 |
| 32 | 缺少 PTT 自检 | 无头测试困难 | 新增 `--ptt-selftest [ms]` 与 Kconfig 默认时长 | 已修复 |
| 33 | `voice_channel.h` 头文件路径未加入 Makefile | 编译找不到 `<voice/voice_channel.h>` | Makefile 增加 `packages_ai_agent/src` include | 已修复 |
| 34 | `voice_channel` 返回空文本时误走 nxrecorder 回退 | 空转写被报成“无音频数据” | `main.c` 将空文本视为有效结果，直接 UI 提示“未识别到语音” | 已修复 |
| 35 | `radio_server` `handle_config_put` 中 `strdup("{}")` 失败未处理 | 极低内存下 UB | 增加 NULL 判断并返回 500 | 已修复 |
| 36 | 日志前缀不统一 | 串口过滤困难 | PTT 日志前缀从 `[ptt]` 改为 `[radio_ptt]`，其余保持 `[radio_*]`/`[main]`/`[asr]` | 已修复 |
| 37 | `radio_ui.c` 注释块含 `*/` 导致提前结束 | 语法检查大量级联错误 | 将 `radio_ui_set_*/radio_ui_add_log` 改为 `radio_ui_set_* and radio_ui_add_log` | 已修复 |
| 38 | `radio_agent.c` 缺少 `<stdbool.h>` / `<sys/stat.h>` | 主机语法检查失败 | 补充对应系统头文件 | 已修复 |
| 39 | `handle_skills_get` 仍使用 8KB 栈 buffer | 与 DEVNOTES 描述不一致，存在栈溢出风险 | 改为堆分配 `SKILLS_OUT_SIZE` 并释放 | 已修复 |
| 40 | `sha1_final` 含未使用变量 `j` | 编译警告 | 移除未使用变量 | 已修复 |
| 41 | PTT 先于音频启动 | 音频未就绪时可能误发射 | `main.c` 改为先 `audio_recorder_start()`，成功后再 `ptt_key()` | 已修复 |
| 42 | 释放 PTT 晚于停止录音 | ASR 处理期间仍占用信道 | 释放 PTT 调整到 `audio_recorder_stop()` 之前 | 已修复 |
| 43 | agent 保存失败时转写丢失 | 比赛演示关键数据丢失 | 新增 `radio_agent_save_raw_log()` 本地降级保存 | 已修复 |
| 44 | ASR 单次请求失败无重试 | 网络抖动导致识别失败 | `asr_transcribe()` 内增加重试循环 | 已修复 |
| 45 | BLE 重连间隔固定 | 快速重连可能加剧不稳定 | `BleManager` 改为指数退避 | 已修复 |
| 46 | Android 默认端口 28789 与板端 28790 不一致 | App 连不上 radio_server | 全端默认端口统一为 28790 | 已修复 |

---

## 4. BLE 配网流程与 Android 接口

### 4.1 设备端（OpenVela）

BLE 配网由 `ai_agent` 提供，无需 `ai_radio_console` 自行实现。

**启用条件：**

```bash
# menuconfig 中开启
Application Configuration → Packages → Vela AI Agent
  → Enable BLE GATT data channel (CONFIG_AI_AGENT_BLE_GATT=y)
  → Enable REST API endpoints (CONFIG_AI_AGENT_REST_API=y)
```

**NUS 服务定义**（`packages_ai_agent/src/infra/ble_gatt.c`）：

| UUID | 方向 | 说明 |
|------|------|------|
| `6e400001-b5a3-f393-e0a9-e50e24dcca9e` | Service | Nordic UART Service |
| `6e400002-b5a3-f393-e0a9-e50e24dcca9e` | Phone → Device (RX) | 写入 JSON 命令 |
| `6e400003-b5a3-f393-e0a9-e50e24dcca9e` | Device → Phone (TX) | Notify 状态/结果 |

**支持的 JSON 命令**（`packages_ai_agent/src/infra/ble_cmd_handler.c`）：

```json
// 配置 WiFi
{"cmd":"wifi_config","ssid":"YOUR_SSID","password":"YOUR_PASSWORD"}

// 心跳/查询
{"cmd":"ping"}

// 查询网络状态（返回 IP）
{"cmd":"status"}
```

**典型响应：**

```json
{"status":"ok","msg":"wifi connected"}
{"status":"ok","msg":"pong"}
{"status":"ok","network":true,"ip":"192.168.1.123"}
```

### 4.2 手机端（com.agent.coapp）

- **入口**：`BleProvisioningScreen` + `ProvisioningViewModel`
- **扫描**：按 MAC 地址 `D0:C1:BF:B0:DF:F4` 过滤，同时显示带名称的 NUS 设备。
- **连接**：`BluetoothDevice.connectGatt(..., TRANSPORT_LE)`，MTU 请求 512。
- **服务发现**：使能 TX characteristic 的 notification（CCCD descriptor `0x2902`）。
- **写入**：通过 RX characteristic 写入 UTF-8 JSON。
- **保存 IP**：收到 `"ip":"..."` 后自动写入 DataStore，后续 REST/WebSocket 直接连该 IP。

### 4.3 对 ai_radio_console 的预留点

当前 `ai_radio_console` 运行后已监听 `28790` REST/WebSocket。手机 BLE 配网完成后拿到 IP，默认仍会先连 `28789`（ai_agent）。如需让 App 直接管理电台控制台：

1. 在 App 的 Config 页面把 `devicePort` 改为 `28790`；或
2. 在 `ai_radio_console` 中把常用 ASR/PTT 配置同步到 ai_agent 的 `/data/agent/config/config.json`，让 App 走 `28789` 也能读写。

后续扩展 BLE 命令（如远程 PTT）的预留方案：

- 在 `packages_ai_agent/src/infra/ble_cmd_handler.c` 增加 `radio_ptt_key` / `radio_ptt_unkey` 分支。
- 或通过 VelaClaw 在 `ai_radio_console` 中订阅自定义消息总线事件。

---

## 5. 关键文件说明

| 文件 | 职责 |
|------|------|
| `main.c` | 应用主循环、PTT 轮询、模块初始化 |
| `radio_ui.c/h` | LVGL 界面、中文字体、异步 UI 更新 |
| `audio_recorder.c/h` | nxrecorder LINEIN 录音 |
| `ptt_control.c/h` | GPADC0 GPIO PTT 控制 |
| `asr_client.c/h` | HTTP ASR 客户端（WAV 封装） |
| `radio_agent.c/h` | VelaClaw + radio-log Skill 安装/调用 |
| `radio_server.c/h` | 28790 端口 REST/WebSocket 服务 |
| `radio-log.md` | ai_agent Skill 文件 |
| `Kconfig` / `Makefile` | 构建配置 |
| `DEVNOTES.md` | 本开发笔记 |
| `BLE_ANDROID_INTEGRATION.md` | BLE 配网与 Android App 集成文档 |

---

## 6. 构建与运行检查清单

### 6.1 本地语法/静态检查

工作区缺少 OpenVela 顶层 `nuttx/`、`prebuilts/`、`build.sh`，无法直接跑完整编译。作为替代，用主机 `gcc -fsyntax-only` + 本地 stub 头文件做静态检查：

```bash
cd /workspace

INC="-Itmp_stubs -Ipackages_ai_agent/include -Ipackages_ai_agent/src -Ivendor_allwinnertech/apps/ai_radio_console"
SRC="vendor_allwinnertech/apps/ai_radio_console/main.c \
     vendor_allwinnertech/apps/ai_radio_console/audio_recorder.c \
     vendor_allwinnertech/apps/ai_radio_console/radio_ui.c \
     vendor_allwinnertech/apps/ai_radio_console/ptt_control.c \
     vendor_allwinnertech/apps/ai_radio_console/asr_client.c \
     vendor_allwinnertech/apps/ai_radio_console/radio_agent.c \
     vendor_allwinnertech/apps/ai_radio_console/radio_server.c"

# voice_channel 后端（默认）
gcc -fsyntax-only $INC -Wall -Wextra $SRC && echo "voice_channel backend: PASS"

# nxrecorder 回退后端
# 注： stub config.h 默认开启 voice_channel，命令行覆盖会产生 macro redefined 警告，非代码问题
gcc -fsyntax-only $INC -DCONFIG_AI_RADIO_CONSOLE_USE_VOICE_CHANNEL=0 -Wall -Wextra $SRC && echo "nxrecorder fallback backend: PASS"
```

当前结果（2026-07-10）：

- `voice_channel` 后端：`gcc -fsyntax-only -Wall -Wextra` 通过。
- `nxrecorder` 回退后端：`gcc -fsyntax-only -DCONFIG_AI_RADIO_CONSOLE_USE_VOICE_CHANNEL=0 -Wall -Wextra` 通过。
- Android 端：当前环境无 Android SDK / Gradle 分发版，未执行 Gradle 编译，需在本地 Android Studio 中完成。

### 6.2 完整编译

```bash
cd <openvela-project-root>  # 必须包含 nuttx/、prebuilts/、build.sh 等顶层文件

# 1. 拷贝 defconfig
cp packages_ai_agent/defconfigs/gemini-s1/gemini-s1_defconfig \
   vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/defconfig

# 2. 应用音频补丁
bash packages_ai_agent/fix_gemini_s1.sh

# 3. 配置
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay menuconfig
#   - Application Configuration → AI Radio Console (Gemini-S1 HAT) : y
#   - Application Configuration → Packages → Vela AI Agent : y
#   - Vela AI Agent → Enable BLE GATT data channel : y (如需手机配网)
#   - Vela AI Agent → Enable REST API endpoints : y (如需手机 App 直连 ai_agent)

# 4. 编译
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/ \
    -e -Wno-error -j"$(nproc)"

# 5. 烧录
source envsetup.sh && lunch_nuttx && pack
```

> **注意**：当前工作区只包含 `vendor_allwinnertech`、`packages_ai_agent`、`packages_demos`、`frameworks` 等子仓库，缺少 OpenVela 顶层 `nuttx/`、`prebuilts/`、`build.sh`。完整编译需要在完整的 OpenVela 源码树中执行，或把本目录内容按 README 说明拷贝到 `vendor/allwinnertech` 后继续。

**运行前准备：**

```nsh
# 创建数据目录
nsh> mkdir -p /data/agent/config /data/agent/skills /data/agent/memory/radio

# 配置 ASR key（如使用板端 ASR 客户端）
nsh> echo "sk-xxxx" > /data/agent/config/asr_key.txt

# 启动 ai_agent
nsh> ai_agent

# 启动电台控制台
nsh> ai_radio_console
```

---

## 7. 后续待办

### 代码与构建

- [x] 输出 BLE 与 Android 集成文档 [BLE_ANDROID_INTEGRATION.md](BLE_ANDROID_INTEGRATION.md)。
- [x] 更新 DEVNOTES.md 开发心路历程与问题清单。
- [x] 重构 `audio_recorder` 支持 `voice_channel` / `nxrecorder` 双后端，并更新 `main.c` 流程。
- [x] Worker 线程栈扩至 40KB，新增 `--ptt-selftest`。
- [ ] 完整走一遍 `make distclean && cp defconfig && menuconfig && build`，确认无编译错误（工作区缺 OpenVela 顶层，需在完整源码树执行）。
- [x] 检查 `radio_server.c` 中所有 `malloc`/`strdup` 失败路径与 `close(fd)` 配对，避免资源泄漏。
- [x] 确认 `radio_ui.c` 中文字体回退路径在目标板 `/data/res/fonts/` 真实存在。
- [x] 统一日志前缀：`[radio_*]` / `[main]` / `[asr]`，方便串口过滤。
- [x] PTT 逻辑：先启动音频再 key 电台，释放 PTT 先于停止录音。
- [x] agent 调用增加重试 + 本地 raw 日志降级。
- [x] ASR 客户端增加重试。
- [x] Android BLE 重连指数退避、WebSocket 自动重连、REST PTT 回退。
- [x] Android 全端默认端口统一为 28790。
- [ ] 验证 `voice_channel` 路径在真机上能正常录音 + ASR；若不行，切回 `nxrecorder` 路径。
- [x] 本地语法/静态检查：`gcc -fsyntax-only` 双后端均通过。

### 真机验证

- [ ] 真机验证录音 → ASR → ai_agent 全链路。
- [ ] 验证 BLE 配网拿到 IP 后手机 App 能连上 `28789` / `28790`。
- [ ] 根据真机反馈调整 LINEIN 增益 / 采样率（当前 16kHz mono，媒体图可能跑 48kHz stereo）。
- [ ] 如需 HTTPS ASR，部署 relay.py 或切到 ai_agent Volc ASR。
- [ ] 考虑把 `radio-log.md` 作为 romfs 资源直接挂载，避免运行时再内嵌写入。

### 比赛与手机端

- [ ] 后续手机端开发：远程 PTT、实时日志推送、配置同步。
- [ ] 准备演示脚本与截图/录屏，覆盖 PTT 录音 → 日志生成 → App 查看完整流程。
- [ ] 整理双赛（哈工大 + 小米 OpenVela）提交材料清单。

---

## 8. 参考文档

- 小米 OpenVela AI 硬件赛道指南：`openvela-docs-github/zh-cn/contest_2026/ai_hardware/`
- ai_agent 快速开始：`openvela-docs-github/zh-cn/contest_2026/ai_hardware/ai_agent_quickstart.md`
- 示例：`packages_demos/mini_memo/`、`packages_demos/ai_chat/`
- Android 伴侣 App：`com.agent.coapp/`
- 开发板文档：`feishu_docs_full/`
- BLE 与 Android 集成：`BLE_ANDROID_INTEGRATION.md`
