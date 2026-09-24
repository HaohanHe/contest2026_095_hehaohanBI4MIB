# DayNote —— 一台开口就能记事的 AI 电台终端

参赛队伍：hehaohanBI4MIB（何浩瀚，呼号 BI4MIB）
赛道：AI 硬件产品创新
运行平台：openvela（NuttX）on Gemini-S1，主控 Allwinner R528

## 这东西是干嘛的

在地里干活、野外架台、蹲在电台桌前的时候，手基本没空——握锄头、拧旋钮、捏话筒，
想记点什么只能靠脑子，回头就忘。DayNote 就是冲着这个来的：对着它把话说出来，
它自己录音、转成文字、分出类型、写好摘要，存成一条以后能翻、能搜的笔记。

它同时是个业余电台的网络终端：接一块自制的 AI Radio HAT，就能跑 MMDVM/DMR
数字模式，通话记录同样落到本地。两件事共用一套语音链路和一块 320×240 的屏。

## 设计上的几个取舍

- **端侧优先，网络是加分项。** 录音、VAD、本地分类、检索这些没网也得能用；
  ASR 和 LLM 放在云端，但请求走的是 OpenAI 兼容接口，换服务只改配置不改代码。
- **站在官方框架上写，不重造轮子。** 应用基于 openvela 的 ai_agent / mini_memo
  重构，LLM 路由、语音通道、消息总线都用框架的，自己只写业务层；nuttx、packages
  这些生产仓不动。
- **交互按小屏来。** 没有键盘，所有操作落在五个可滑动的页面和一个 PTT 按键上，
  信息以卡片呈现，一屏看完一条。

## 实际实现了什么

**语音记录**
- 两种触发：按住 PTT 录音；或后台 VAD 自动分段（RMS 能量阈值，约 60ms 语音起录、
  1.5s 静音停止，单段 0.5–30s），两条路互斥、共用麦克风。
- 16kHz/16bit/单声道采样，自动加 WAV 头。
- ASR：默认 SiliconFlow 的 `FunAudioLLM/SenseVoiceSmall`，multipart 直传；
  也可换成任何 OpenAI 兼容的 `/v1/audio/transcriptions` 端点。
- 理解：转录文本交给 LLM，输出类型（memo/todo/schedule）、短标题、摘要、关键词，
  日程类给出提醒时间；LLM 不通时用本地关键词规则兜底，记录不丢。

**界面与检索**
- LVGL 五页 tile：Home（时间线 + 统计）、Voice（录音）、Review（检索回看）、
  Settings、DMR（热点状态与通话记录）。
- FreeType 运行时加载 TTF 中文字体；笔记按时间线和卡片展示。
- 关键词 / 时间 / 类型检索全部在本机完成，数据不出设备。

**电台部分**
- MMDVM 网络客户端：连接 master、心跳、DMR 帧解析、当前通话状态，通话记录以 JSON
  持久化，重启不丢。
- BLE 配网与命令通道：Wi-Fi 配置、LLM/ASR/MMDVM 参数都从蓝牙下发，无需接键盘。

## 代码结构

```text
app/ai_radio_console/
├── daynote_main.c      入口、主循环、BLE 命令处理
├── daynote_core.c      笔记存储、本地分类、LLM 调用、VAD 线程
├── daynote_ui.c        五页 LVGL 界面
├── daynote_asr.c       SiliconFlow ASR 后端（TLS + multipart）
├── daynote_mmdvm.c     MMDVM 客户端、DMR 解析、通话记录
├── daynote_*.h
├── agent_skills/       三个技能的规格文件（见下）
├── Kconfig / Makefile / CMakeLists.txt
└── subset_font.py      字库裁剪工具
quickapp/ai_radio/      配套快应用（手机/手表端联动）
board/contest_board/    Gemini-S1 板级配置与 defconfig
hardware/ai_radio_hat/  扩展板工程（KiCad / 嘉立创 EDA）
logs/HaohanHe/          AI Coding 日志
```

## Agent Skills

三个技能以 Markdown 规格放在 `app/ai_radio_console/agent_skills/`，供 ai_agent
框架按意图加载，内容包含触发条件、调用步骤、降级策略和示例。

| 技能 | 什么时候触发 | 做什么 |
|---|---|---|
| daynote-record | "记录一下"、按 PTT、VAD 分段 | 录音 → ASR → LLM 分类摘要 → 写入 memos.json；断网走本地规则 |
| daynote-search | "找一下关于 X 的记录"、Review 页检索 | 按关键词/时间/类型端侧检索，卡片展示 |
| daynote-summary | "总结一下今天"、周期回顾（默认 4 小时） | 聚合当天笔记，输出高光、待办、关键词与日报 |

## 编译与烧录

拉取整套工程，本仓 manifest 会把各目录软链进编译树：

```bash
repo init -u https://github.com/open-vela/contest2026_095_hehaohanBI4MIB \
  -b dev-ai-contest-2026 -m contest2026_095_hehaohanBI4MIB.xml
repo sync -c -j8
```

在 openvela 工作区根目录（本仓的上一级）构建：

```bash
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay -j8
cd vendor/allwinnertech/lichee && source envsetup.sh && lunch_nuttx && pack
```

应用由 Kconfig 开关 `LVX_USE_DEMO_CONTEST2026_095_AI_RADIO_CONSOLE` 控制，
依赖 `EXAMPLES_AI_AGENT_VELA` 或 `VELACLAW_DAEMON`，自动选择 cJSON；
板级能力（BLE、音频栈、FreeType、mbedTLS、libuv、网络等）已在
`board/contest_board/configs/nsh/defconfig` 打开。

## 首次运行：通过 BLE 下发配置

连上设备的 BLE 后，以 JSON 发命令，参数会落到本地 config store，重启保留。

LLM / ASR：

```json
{
  "cmd": "llm_config",
  "api_key": "sk-你的大模型密钥",
  "asr_api_key": "sk-你的ASR密钥",
  "host": "api.siliconflow.cn",
  "path": "/v1/chat/completions",
  "model": "Qwen/Qwen2.5-7B-Instruct"
}
```

MMDVM：

```json
{
  "cmd": "mmdvm_config",
  "dmr_id": 460xxxxxx,
  "callsign": "BI4MIB",
  "master": "master 地址",
  "password": "密码"
}
```

字体：把 TTF（建议 5MB 以内，如思源黑体）放到 `/data/daynote/font.ttf`，
启动时自动加载。

## 数据存储

- 笔记：`/data/daynote/memos.json`，每条含
  id / type / content / transcript / summary / keywords / timestamp / duration_ms / is_read；
  上限 100 条，自动淘汰最旧的已读条目。
- 通话记录：MMDVM 模块单独以 JSON 保存。
- 数据目录可在 Kconfig 的 `DAYNOTE_DATA_DIR` 修改。

## AI Radio HAT

80mm×50mm 双层扩展板（KiCad 7 与嘉立创 EDA 双工程）：电台 LINEIN 进 R528 供 ASR、
R528 HPOUT 出至电台 MIC 做数字语音、光耦隔离 PTT、CAT 串口、I2C 传感器扩展。
工程文件、原理图、BOM 与接线表见 `hardware/ai_radio_hat/`。

## 验证状态与已知问题

如实说明，不把没验证的说成已完成：

1. 应用侧代码（约 5,000 行）功能闭环，VAD、ASR、MMDVM 均为真实实现而非桩；
   但完整的干净环境编译还依赖配套的 ai_agent / velaclaw 组件，从 `repo sync` 到
   出固件的全链路尚需在 Linux 环境复跑确认。
2. VAD 的 RMS 阈值（当前 500）是经验值，不同麦克风增益下要实机标定。
3. 电阻屏坐标转换需要实机校准。
4. AI Radio HAT 已完成原理图与 BOM，PCB 布局布线和打样是下一步。
5. 获奖后按要求把作品 PR 到 openvela 上游对应仓库，走标准 PR + CI。

## AI Coding 日志

开发期间与 AI 工具的对话记录在 `logs/HaohanHe/`，含会话清单 `manifest.json`。
本次主要在 Trae（MiMo-V2.5）中开发，不在官方日志归集工具支持列表内，
无法导出 JSONL，故提交 Markdown 原始全文，说明见该目录。

