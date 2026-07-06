# R528/OpenVela AI 无线电控制台工程化集成 Spec

## Why

项目已具备 SiliconFlow ASR/LLM 的核心调用能力，但要真正在 Gemini-S1（Allwinner R528 + openvela）上作为作品运行，还需要补齐联网、人机交互、音频输入等工程化能力。本 Spec 把与板级集成相关的多项改进统一规划，避免零散修补。

## What Changes

- **自动 WiFi 联网**：板子启动后自动读取 `/data/etc/WiFi/wapi.conf` 并连接 AP、获取 IP，确保 ASR/LLM 调用不依赖手动配网。
- **LVGL 状态显示页面**：新增一个 AI 电台窗口，实时显示当前频率/模式、ASR 识别文本、LLM 分析摘要、报警级别。
- **LRADC 按键输入**：接入 R528 的 LRADC 按键事件（`/dev/input/event1`），支持 PTT/菜单/确认等快捷操作。
- **I2S 音频采集**：从 I2S2 麦克风采集 16kHz PCM，替换当前模拟/占位音频源，使流式 ASR 真正可用。
- **代码规模统计**：在每次构建后输出各模块代码行数，作为项目复杂度与可维护性的可见性指标。

**BREAKING**：I2S 音频采集接入后，`asr_engine_feed_audio` 的输入源将从现有 8kHz 模拟源切换为 I2S 16kHz 真实麦克风数据，采样率处理逻辑需要同步调整。

## Impact

- Affected specs：AI 分析管线、ASR 实时识别、配置持久化。
- Affected code：
  - `app/ai_radio_console/src/agent_bridge.c`
  - `app/ai_radio_console/src/asr_engine.c`
  - `app/ai_radio_console/src/main.c`
  - 新增 `app/ai_radio_console/src/wifi_auto_connect.c`
  - 新增 `app/ai_radio_console/src/input_lradc.c`
  - 新增 `app/ai_radio_console/src/audio_i2s.c`
  - 新增 `app/ai_radio_console/src/ui_ai_radio.c`

## ADDED Requirements

### Requirement: 自动 WiFi 联网

The system SHALL ensure the AI console can reach SiliconFlow API after boot without manual interaction.

#### Scenario: 已配置 WiFi

- **GIVEN** `/data/etc/WiFi/wapi.conf` 存在且包含有效 SSID/密码
- **WHEN** 系统启动
- **THEN** 自动加载配置、扫描、连接、DHCP 获取 IP，并在失败时重试 3 次

#### Scenario: 未配置 WiFi

- **GIVEN** `/data/etc/WiFi/wapi.conf` 不存在
- **WHEN** 系统启动
- **THEN** 记录警告但继续运行，等待用户通过命令行配置

### Requirement: LVGL 状态显示页面

The system SHALL provide an on-screen view of AI radio status when a display is connected.

#### Scenario: 正常运行

- **GIVEN** LVGL 已初始化且显示可用
- **WHEN** ASR 产生 partial/final 文本或 LLM 产生分析结果
- **THEN** 对应文本在 AI 电台窗口中实时更新

#### Scenario: 报警触发

- **WHEN** LLM 分析结果 alert_level > NONE
- **THEN** 屏幕以红色背景或闪烁方式高亮报警信息

### Requirement: LRADC 按键输入

The system SHALL support physical button input via LRADC for hands-free control.

#### Scenario: PTT 按键

- **WHEN** 用户长按 PTT 按键超过 200ms
- **THEN** 控制台开始/停止音频采集与 ASR 识别

#### Scenario: 菜单按键

- **WHEN** 用户短按菜单键
- **THEN** 在 LVGL 界面中切换焦点或确认选项

### Requirement: I2S 音频采集

The system SHALL capture real microphone audio from I2S2 and feed it to the streaming ASR engine.

#### Scenario: 正常录音

- **GIVEN** I2S2 麦克风已连接并驱动正常
- **WHEN** ASR 引擎启动
- **THEN** 16kHz PCM 数据持续送入 `asr_engine_feed_audio`

#### Scenario: 采样率不一致

- **GIVEN** I2S 实际输出 8kHz
- **WHEN** 数据进入 ASR 引擎
- **THEN** 引擎应正确重采样到 16kHz，不产生语速异常

### Requirement: 代码规模统计

The system SHALL report code size metrics after each local build.

#### Scenario: 本地构建完成

- **WHEN** `make -f Makefile.local` 成功
- **THEN** 终端输出 src 目录下 .c/.h 文件行数统计与 ELF 文件大小

## MODIFIED Requirements

### Requirement: ASR 引擎输入源

ASR 引擎当前接收 8kHz 输入并上采样到 16kHz。接入 I2S 后，如果 I2S 直接提供 16kHz 数据，应移除上采样逻辑；如果仍提供 8kHz，应保持现有上采样。

- **WHEN** 检测到 `AUDIO_SAMPLE_RATE` 配置为 16000
- **THEN** `asr_engine_feed_audio` 直接将样本写入 VAD 缓冲区，不再调用 upsample

## REMOVED Requirements

无。
