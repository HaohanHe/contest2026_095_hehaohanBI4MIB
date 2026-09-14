# Agent Handoff — DayNote for Gemini-S1

> Last updated: 2026-08-01
> Purpose: Complete context for next AI agent to continue work

---

## Goal

Build DayNote — a full-day voice memory device for Gemini-S1 (R528) board.
Product concept: "得到大脑" style voice notes — record throughout the day → ASR → AI summary → highlight extraction.

Based on Xiaomi's openvela AI Agent framework (`packages/ai_agent/`).

## Board & Hardware

- **Board**: Gemini-S1 (Allwinner R528 dual-core Cortex-A7, 128MB DDR3)
- **Screen**: CL28CK31-18A (2.8" IPS resistive touch, controller IC = ILI9341, SPI)
- **OS**: openvela (NuttX RTOS)
- **SPI pins**: CS=PD10, SCK=PD11, MOSI=PD12, MISO=PD13, DC=PD14, Reset=PD19, Backlight=PD20
- **Touch**: Resistive (TPADC driver), NOT capacitive (GT911)
- **Audio**: I2S input (16kHz, 16bit, mono)
- **Buttons**: LRADC (ENTER = record, HOME long press = settings)

## Repository Structure

```
ontest2026_095_hehaohanBI4MIB/     # Contest repo (this directory)
├── app/ai_radio_console/           # DayNote app (source of truth)
│   ├── src/
│   │   ├── main.c                  # App entry, audio thread, button handler
│   │   ├── ui_daynote.c            # DayNote UI (320x240)
│   │   ├── vad_detector.c          # VAD静音检测（能量阈值法）
│   │   ├── auto_recorder.c         # 自动分段录音（后台线程+VAD）
│   │   ├── note_store.c            # 笔记存储（JSON索引+WAV文件）
│   │   ├── memory_index.c          # HippocampusIndex关键词索引
│   │   ├── daily_digest.c          # DailyDigest每日摘要
│   │   ├── http_sync.c             # HTTP同步服务（手机端API）
│   │   ├── siliconflow_client.c    # SiliconFlow ASR+LLM API
│   │   ├── wav_encoder.c           # WAV编码
│   │   ├── audio_i2s.c             # I2S音频采集
│   │   ├── config_store.c          # 配置存储
│   │   ├── spacelog_settings.c     # WiFi/API设置
│   │   └── wifi_auto_connect.c     # WiFi自动连接
│   ├── include/                    # 头文件
│   └── Makefile                    # 构建规则
├── hardware/ai_radio_hat/          # PCB设计（KiCad）
├── agent.md                        # THIS FILE
├── DAYNOTE_PLAN_v4.md              # 产品方案v4
└── gemini-s1_daynote_v2.img        # 最新固件（27MB）

/home/bi4mib/openvela-build/ -> 符号链接 -> /run/media/bi4mib/新加卷/ontest2026_095_hehaohanBI4MIB/openvela-build/
├── nuttx/                          # NuttX内核
├── packages/ai_agent/              # 小米AI Agent框架（C语言）
├── packages/demos/contest2026_095_ai_radio_console/  # App部署副本
└── vendor/allwinnertech/           # Allwinner R528 BSP
```

**重要：编译树在西数500G HDD上！**
- 符号链接：`/home/bi4mib/openvela-build/` → `/run/media/bi4mib/新加卷/ontest2026_095_hehaohanBI4MIB/openvela-build/`
- 所有修改必须在 **ontest2026_095_hehaohanBI4MIB/** 下进行，然后同步到 `openvela-build/`
- 台式机/其他电脑插上西数硬盘即可继续开发

## Current Status (2026-08-02 DayNote v2)

### Working Firmware
- **gemini-s1_daynote_v2.img** (27MB): 最新固件，修复音频管线、UI集成、defconfig配置。

### Build System
- 编译命令：`./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay -j1`
- pack命令：`cd vendor/allwinnertech/lichee && bash -c 'source envsetup.sh && lunch_nuttx 2 && pack'`
- 编译产物：`nuttx.bin` ~5MB，`vela.bin` ~5MB，打包后 ~27MB

### DayNote 数据流
```
[VAD检测到语音]
    ↓
[auto_recorder后台线程录音]
    ↓
[静音2s自动分段]
    ↓
[保存WAV] → /data/daynote/audio/note_YYYYMMDD_HHMMSS.wav
[调用sf_client_transcribe_audio] → SiliconFlow ASR API
    ↓ HTTP multipart upload
[ASR返回text]
    ↓
[调用sf_client_chat_completion] → SiliconFlow LLM API
    ↓ HTTP JSON
[LLM返回summary + keywords + is_highlight]
    ↓
[note_store_add] → 更新JSON索引
[memory_index_add] → 更新HippocampusIndex
[daily_digest_refresh] → 刷新每日摘要
[UI更新] → 时间线 + 关键词 + 高光
```

### 新增模块（本次会话）
| 文件 | 功能 |
|------|------|
| `vad_detector.c/h` | VAD静音检测（RMS能量阈值法，20ms帧） |
| `auto_recorder.c/h` | 自动分段录音（后台线程，VAD驱动，最大5分钟/段） |
| `note_store.c/h` | 笔记存储（JSON索引 + WAV文件） |
| `memory_index.c/h` | HippocampusIndex关键词索引 |
| `daily_digest.c/h` | DailyDigest每日摘要（LLM生成） |
| `http_sync.c/h` | HTTP同步服务（手机端API框架） |
| `ui_daynote.c/h` | DayNote UI（320x240适配） |

## 硬件能力边界（必须遵守）

### 能做的
- ✅ I2S录音 (16kHz mono, 16bit PCM)
- ✅ WAV编码
- ✅ SiliconFlow云ASR/LLM API
- ✅ WiFi自动连接
- ✅ LVGL UI (320x240, 中文显示)
- ✅ 本地文件存储（/data/daynote/ WAV + JSON索引）

### 不能做的（硬件限制）
- ❌ 本地ASR模型（Whisper最小也要~100MB RAM）
- ❌ 本地LLM（7B模型需要4GB+）
- ❌ 本地TTS
- ❌ 流式ASR（同时跑AudioRecord + HTTPS上传 + mbedtls内存不够）
- ❌ 录音超过5分钟（内存buffer限制）

## 与小米AI Agent框架的集成

### 已有能力（直接复用）
| 能力 | 文件 | 说明 |
|------|------|------|
| ReAct循环 | agent_loop.c | 已有，处理用户query |
| 多LLM路由 | llm_router.c | 支持MiMo/Kimi/Qwen/DeepSeek |
| LLM代理 | llm_proxy.c | 已有，支持tools格式 |
| 消息总线 | message_bus.c | 已有，inbound/outbound双队列 |
| 长期记忆 | memory_store.c | 已有，读写MEMORY.md + daily笔记 |
| 会话管理 | session_mgr.c | 已有，JSONL格式 |
| 工具注册 | tool_registry.c | 已有，30+工具 |
| 语音通道 | voice_channel.c | 已有，PTT录音 + TTS播放 |
| ASR抽象层 | voice_asr.c | 已有，支持streaming/batch |
| 音频采集 | audio_capture.c | 已有，16kHz/16bit/mono |
| LVGL UI | lvgl_ui_channel.c | 已有，但适配466x466手表屏 |

### 已有Skills（直接复用或修改）
| Skill | 文件 | 复用方式 |
|-------|------|----------|
| note-taker | agent_skills/note-taker.md | 直接复用，写daily笔记 |
| daily-briefing | agent_skills/daily-briefing.md | 直接复用，每日摘要 |
| voice-memo | agent_skills/voice-memo.md | 修改为自动录音模式 |

### 集成方式
DayNote 作为 ai_agent 的 background service + Skill：
- 通过 message_bus 通信
- 复用 llm_proxy 进行 ASR/LLM 调用
- 复用 memory_store 进行笔记存储
- 新增 DayNote slash 命令（/daynote, /summary, /search）

## 文件修改清单

### 需要修改的文件
1. `app/ai_radio_console/src/main.c` - 集成auto_recorder + memory_index + daily_digest
2. `app/ai_radio_console/Makefile` - 添加新源文件
3. `app/ai_radio_console/Kconfig` - 更新描述

### 不需要修改的文件
- `siliconflow_client.c` - ASR + LLM API 已经完整
- `audio_i2s.c` - 录音已经完整
- `wav_encoder.c` - WAV编码已经完整
- `config_store.c` - 配置存储已经完整
- `spacelog_settings.c` - WiFi/API设置已经完整
- `wifi_auto_connect.c` - WiFi连接已经完整

## 实现优先级

### P0（必须）
1. ✅ VAD静音检测
2. ✅ 自动分段录音
3. ✅ HippocampusIndex记忆索引
4. ✅ DailyDigest每日摘要
5. ✅ HTTP同步服务基础API

### P1（重要）
6. 集成ai_agent框架（message_bus + agent_loop + slash命令）
7. 手机端同步API
8. UI时间线视图

### P2（锦上添花）
9. 关键词搜索
10. 高光标记
11. 与小米AI Agent框架深度集成
12. 手机端App

## 关键文件路径

| 文件 | 用途 |
|------|------|
| `app/ai_radio_console/src/main.c` | App主入口 |
| `app/ai_radio_console/src/ui_daynote.c` | DayNote UI |
| `app/ai_radio_console/src/auto_recorder.c` | 自动录音 |
| `app/ai_radio_console/src/vad_detector.c` | VAD检测 |
| `app/ai_radio_console/src/memory_index.c` | 记忆索引 |
| `app/ai_radio_console/src/daily_digest.c` | 每日摘要 |
| `app/ai_radio_console/src/http_sync.c` | HTTP服务 |
| `app/ai_radio_console/Makefile` | 构建规则 |
| `vendor/.../nsh_minidisplay/defconfig` | 主defconfig |
| `packages/ai_agent/` | 小米AI Agent框架 |
| `gemini-s1_daynote_v2.img` | 最新固件 |

## 约束条件

- **必须使用nsh_minidisplay**（不是nsh，那是7寸MIPI屏用的）
- **必须遵循官方openvela模式**（AI Agent、uORB、WAPI、media server）
- **屏幕是电阻屏** — 不要启用GT911（电容屏）
- **编译需要用-j1** — 机器14GB RAM，更高并行度会导致OOM
- **LTO保持禁用** — 导致跨模块符号解析失败
- **clean rebuild后需要修复archive order bug** — 使用fix_archive_order.sh
- **不能跑本地模型** — 128MB RAM跑不了Whisper/LLM，只能用SiliconFlow云API
- **不能做流式ASR** — 同时跑I2S录音 + HTTPS上传 + mbedtls内存不够

## 给下一任的Notes

1. 编译树已搬到西数500G HDD（sdb1），路径：`/run/media/bi4mib/新加卷/ontest2026_095_hehaohanBI4MIB/openvela-build/`
2. 符号链接：`/home/bi4mib/openvela-build/` → 西数HDD真实路径
3. contest repo也在同一块HDD：`/run/media/bi4mib/新加卷/ontest2026_095_hehaohanBI4MIB/`
4. 拔了西数硬盘插别的电脑就能继续开发
5. 国际空间通信挑战赛已结束，不再需要无线电相关功能
6. 产品定位已从"AI Radio Console"转为"DayNote全天语音记忆设备"
7. 手机端同步API已设计但未实现（http_sync.c是框架）
8. ai_agent框架集成已设计但未深度实现（message_bus + agent_loop + slash命令）
