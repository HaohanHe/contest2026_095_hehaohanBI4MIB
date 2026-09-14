# DayNote 产品方案 v2

## 产品定位
Gemini-S1 上的"得到大脑"式语音笔记设备。按键录音 → AI 转写 + 总结 → 挑出今日高光。

## 硬件限制（必须遵守）
- 128MB DDR3, Cortex-A7 @ 1.2GHz, 无 NPU
- 2.8" SPI LCD (320x240),  resistive touch
- I2S 音频输入 (16kHz, 16bit, mono)
- LRADC 按键 (ENTER 录音, HOME 设置)
- WiFi RTL8733BS (802.11 b/g/n)
- SD 卡 (YAFFS)

## 不能做的（硬件限制）
- ❌ 本地 ASR 模型（Whisper 最小也要 ~100MB RAM）
- ❌ 本地 LLM（7B 模型需要 4GB+）
- ❌ 本地 TTS（语音合成模型太大）
- ❌ 流式 ASR（同时跑 AudioRecord + HTTPS 上传 + mbedtls 内存不够）
- ❌ 录音超过 5 分钟（内存 buffer 限制）

## 能做的（已验证）
- ✅ I2S 录音 (16kHz mono, 16bit PCM)
- ✅ WAV 编码（44 字节 header + PCM data）
- ✅ SiliconFlow ASR API（multipart/form-data 上传 WAV，返回文本）
- ✅ SiliconFlow LLM API（JSON chat completion，返回总结）
- ✅ WiFi 自动连接（wapi 命令）
- ✅ LVGL UI (320x240, 中文显示)
- ✅ 本地文件存储（/data/daynote/ WAV + JSON 索引）

## 数据流（完整 end-to-end）
```
[用户按 ENTER]
    ↓
[I2S 录音 5s~5min] → PCM buffer (malloc, max 5min = ~6MB)
    ↓
[用户再按 ENTER]
    ↓
[PCM → WAV] → /data/daynote/note_YYYYMMDD_HHMMSS.wav (~100-600KB)
    ↓
[调用 sf_client_transcribe_audio] → SiliconFlow ASR API
    ↓ HTTP multipart upload
[ASR 返回 text] → transcript
    ↓
[调用 sf_client_chat_completion] → SiliconFlow LLM API
    ↓ HTTP JSON
[LLM 返回 summary] → summary text
    ↓
[note_store_add] → 更新 JSON 索引
    ↓
[UI 显示] → transcript + summary + 今日高光
```

## 文件结构
```
/data/daynote/
├── note_20260801_093000.wav    ← 录音文件
├── note_20260801_143000.wav
├── note_20260801_180000.wav
└── index.json                   ← 笔记索引
```

index.json 格式：
```json
[
  {
    "id": 1722489000,
    "ts": "2026-08-01T09:30:00",
    "dur": 45,
    "highlight": true,
    "txt": "今天上午讨论了项目方案...",
    "sum": "上午确定了技术方向，下午开始实现",
    "wav": "/data/daynote/note_20260801_093000.wav"
  }
]
```

## UI 布局 (320x240)
```
┌─────────────────────────────┐
│ 09:30:00          3 notes  │ ← 26px topbar
├─────────────────────────────┤
│      ┌──────────────┐       │
│      │     REC      │       │ ← 40px record button
│      └──────────────┘       │
├─────────────────────────────┤
│ Last: 讨论了项目方案...      │ ← 20px title
├─────────────────────────────┤
│ 转写文本...                  │ ← 70px transcript
│                             │
├─────────────────────────────┤
│ 今日高光:                    │ ← 50px summary
│ • 确定了技术方向             │
│ • 下午开始实现               │
├─────────────────────────────┤
│ ENTER:Rec HOME(L):Settings  │ ← bottom hint
└─────────────────────────────┘
```

## 录音状态机
```
REC_IDLE → (ENTER pressed) → REC_RECORDING
REC_RECORDING → (ENTER pressed) → REC_PROCESSING
REC_PROCESSING → (ASR done) → REC_ANALYZING
REC_ANALYZING → (LLM done) → REC_IDLE
```

## 错误处理策略
1. **WiFi 未连接**: 保存 WAV + transcript(空) → 标记 pending → 后台重试上传
2. **ASR API 失败**: 显示 "ASR failed, saved locally" → 保留 WAV
3. **LLM API 失败**: 显示 transcript（无总结）
4. **存储空间不足**: 停止录音 → 显示 "Storage full"
5. **录音超时**: 5 分钟自动停止

## 与 Opedrgent 的对应关系
| Opedrgent (Android) | DayNote (Gemini-S1) | 说明 |
|---------------------|---------------------|------|
| AudioRecord + PCM | I2S audio_i2s.c | 不同硬件，相同 PCM 数据 |
| PCM → WAV | wav_encoder.c | 已有 |
| 流式 ASR (Sherpa-ONNX / MiMO / Android) | SiliconFlow 云 ASR | 只能用云 API |
| 文件 ASR (fallback) | SiliconFlow 云 ASR | 主要方式 |
| LLM chat (多后端) | SiliconFlow 云 LLM | 简化，只用 SiliconFlow |
| NoteDao + SQLite | note_store.c (JSON) | 简化存储 |
| HippocampusIndex | daily_summary.c | 简化，只做每日摘要 |
| InsightSproutEngine | LLM prompt | 简化，只做高光提取 |
| DailyDigestNotifier | daily_summary_generate | 简化，只做今日摘要 |
| MediaProjection (内录) | ❌ 不支持 | 硬件无此功能 |
| 声纹识别 | ❌ 不支持 | 硬件限制 |
| TTS 播放 | ❌ 不支持 | 无音频输出驱动 |

## 需要新增的文件
1. `note_store.c` / `note_store.h` - 笔记存储（JSON 索引 + WAV 文件）
2. `daily_summary.c` / `daily_summary.h` - 每日摘要生成
3. `ui_daynote.c` / `ui_daynote.h` - DayNote UI（替换 radio UI）

## 需要修改的文件
1. `main.c` - 集成 note_store + daily_summary，修改状态机
2. `Makefile` - 添加新源文件
3. `Kconfig` - 更新描述

## 不需要修改的文件
- `siliconflow_client.c` - ASR + LLM API 已经完整
- `audio_i2s.c` - 录音已经完整
- `wav_encoder.c` - WAV 编码已经完整
- `config_store.c` - 配置存储已经完整
- `spacelog_settings.c` - WiFi/API 设置已经完整
- `wifi_auto_connect.c` - WiFi 连接已经完整
