# DayNote 完整产品方案 v3

## 产品定位
Gemini-S1 上的"全天记忆设备"。自动录音 + AI 转写 + 记忆索引 + 每日高光。不是"按键录音玩具"，是真正的全天候个人记忆系统。

## 硬件能力边界（必须遵守）
- 128MB DDR3, Cortex-A7 @ 1.2GHz
- I2S 音频输入 (16kHz, 16bit, mono)
- 2.8" LCD (320x240) + resistive touch + LRADC 按键
- WiFi (RTL8733BS, 802.11 b/g/n)
- SD 卡 (YAFFS 文件系统)

## 核心功能（分层设计）

### Layer 1: 全天候自动录音
- **自动分段录音**：VAD 静音检测，静音 2 秒自动分段
- **每段最大 5 分钟**：避免内存溢出
- **全天候运行**：后台线程持续录音，不阻塞 UI
- **低功耗模式**：检测不到声音时降低采样率

### Layer 2: 记忆系统（Hippocampus）
- **HippocampusIndex**：每段录音的关键词索引（时间 + 关键词 + 摘要）
- **时间线**：按时间排序的录音列表
- **高光标记**：LLM 判断哪些是"今日高光"
- **搜索**：按关键词搜索历史录音
- **每日摘要**：DailyDigest，挑出今天最重要的 2-3 条

### Layer 3: AI Agent 集成
- **复用 packages/ai_agent/ 框架**：
  - `agent_loop.c` - ReAct 循环
  - `llm_router.c` - 多 LLM 后端（MiMo/Kimi/Qwen）
  - `message_bus.c` - 消息总线
  - `tool_registry.c` - 工具调用
- **DayNote Agent**：专门的 agent，处理录音 → ASR → 总结 → 记忆索引
- **对话模式**：用户可以通过 LRADC 按键与 agent 对话（"今天上午讨论了什么？"）

### Layer 4: 手机端同步
- **HTTP Server**：开发板端运行 HTTP 服务（8080 端口）
- **API 设计**：
  - GET /api/notes - 获取所有笔记
  - GET /api/notes/:id - 获取单条笔记
  - GET /api/notes/:id/audio - 下载 WAV 文件
  - POST /api/sync - 手机端上传数据到开发板
- **手机端**：以后再做，先确保 API 可用

## 文件结构

### 新增文件
```
app/ai_radio_console/src/
├── vad_detector.c          # VAD 静音检测（能量阈值法）
├── auto_recorder.c         # 自动分段录音（VAD + 分段 + 循环）
├── memory_index.c          # Hippocampus 记忆索引
├── daily_digest.c          # DailyDigest 每日摘要
├── daynote_agent.c         # DayNote AI Agent（ReAct 循环）
├── note_store.c            # 笔记存储（JSON 索引 + WAV）
├── http_server.c           # HTTP 服务（手机端同步）
└── ui_daynote.c            # 新 UI（时间线 + 关键词 + 高光）

app/ai_radio_console/include/
├── vad_detector.h
├── auto_recorder.h
├── memory_index.h
├── daily_digest.h
├── daynote_agent.h
├── note_store.h
├── http_server.h
└── ui_daynote.h
```

### 需要修改的文件
- `main.c` - 集成 auto_recorder + memory_index + daily_digest
- `Makefile` - 添加新源文件
- `Kconfig` - 更新描述

### 不需要修改的文件
- `siliconflow_client.c` - ASR + LLM API 已经完整
- `wav_encoder.c` - WAV 编码已经完整
- `config_store.c` - 配置存储已经完整
- `spacelog_settings.c` - WiFi/API 设置已经完整
- `wifi_auto_connect.c` - WiFi 连接已经完整

## 数据流

```
[全天候自动录音]
    ↓
[VAD 静音检测] → 静音 2s → 分段
    ↓
[保存 WAV] → /data/daynote/note_YYYYMMDD_HHMMSS.wav
    ↓
[调用 sf_client_transcribe_audio] → SiliconFlow ASR API
    ↓ HTTP multipart upload
[ASR 返回 text]
    ↓
[调用 sf_client_chat_completion] → SiliconFlow LLM API
    ↓ HTTP JSON
[LLM 返回 summary + keywords + is_highlight]
    ↓
[memory_index_add] → 更新 HippocampusIndex
    ↓
[UI 更新] → 时间线 + 关键词 + 高光

[每日定时]
    ↓
[daily_digest_generate] → 汇总今天的 highight 条目
    ↓
[LLM 生成每日摘要]
    ↓
[UI 显示] → 今日高光 + 关键词云

[HTTP 服务]
    ↓
[手机端 GET /api/notes] → JSON 笔记列表
[手机端 GET /api/notes/:id/audio] → WAV 文件下载
```

## HippocampusIndex 数据结构

```c
typedef struct {
    int      id;
    time_t   timestamp;
    char     ts_str[32];
    int      duration_sec;
    char     wav_path[256];
    char    *transcript;
    char    *summary;
    char     keywords[128];      // "会议,项目,决策"
    bool     highlight;          // 是否今日高光
    float    importance;         // 重要性 0.0-1.0
} note_t;
```

## VAD 算法（能量阈值法）

```c
// 计算短时能量
float vad_compute_energy(const int16_t *samples, int n);

// VAD 状态机
typedef enum {
    VAD_SILENCE,      // 静音
    VAD_SPEECH_START, // 检测到语音开始
    VAD_SPEECH,       // 语音中
    VAD_SPEECH_END    // 语音结束（静音超过阈值）
} vad_state_t;

// 自动分段录音
int auto_recorder_start(void);      // 启动后台录音线程
int auto_recorder_stop(void);       // 停止录音
int auto_recorder_get_segment(void); // 获取已完成的录音段
```

## DailyDigest 算法

```c
// 每日摘要生成
int daily_digest_generate(void);

// 高光提取（从今天的录音中挑出最重要的 2-3 条）
int daily_digest_extract_highlights(note_t **highlights, int max_count);

// 关键词云（从今天的录音中提取高频关键词）
int daily_digest_get_keywords(char *buf, size_t max_len);
```

## DayNote Agent（ReAct 循环）

```c
// Agent 主循环
int daynote_agent_process_query(const char *query, char *response, size_t max_len);

// 支持的 query 示例：
// - "今天上午讨论了什么？"
// - "今天最重要的 3 条记录"
// - "搜索: 项目"
// - "总结今天"
```

## HTTP 服务 API

```
GET  /api/notes              - 获取所有笔记（JSON 数组）
GET  /api/notes/:id          - 获取单条笔记（JSON）
GET  /api/notes/:id/audio    - 下载 WAV 文件
GET  /api/daily/:date        - 获取某天的摘要
POST /api/sync               - 手机端上传数据（JSON）
GET  /api/status             - 设备状态（WiFi/存储/电量）
```

## 与小米 AI Agent 框架的集成

### 方案 A：直接集成（推荐）
将 DayNote 作为 ai_agent 的一个 Skill：
```json
{
  "name": "daynote",
  "description": "全天语音笔记",
  "tools": [
    "daynote.record",
    "daynote.stop",
    "daynote.search",
    "daynote.daily_summary",
    "daynote.highlight"
  ]
}
```

### 方案 B：独立运行 + 消息总线
DayNote 独立运行，通过 message_bus 与 ai_agent 通信：
```c
// DayNote → ai_agent
message_bus_publish("daynote.new_recording", note_json);

// ai_agent → DayNote
message_bus_subscribe("daynote.query", daynote_agent_handle_query);
```

## 实现优先级

### P0（必须）
1. VAD 静音检测
2. 自动分段录音
3. HippocampusIndex 记忆索引
4. DailyDigest 每日摘要
5. HTTP 服务基础 API

### P1（重要）
6. DayNote Agent（ReAct 循环）
7. 手机端同步 API
8. UI 时间线视图

### P2（锦上添花）
9. 关键词搜索
10. 高光标记
11. 与小米 AI Agent 框架集成
12. 手机端 App

## 技术细节

### VAD 实现
- 短时能量计算（RMS）
- 能量阈值自适应（基于背景噪声）
- 静音持续时间阈值（2s）
- 最小语音时长阈值（0.5s）

### 内存管理
- 每段录音最大 5 分钟 = 5 * 60 * 16kHz * 2 bytes = 9.6MB
- 使用 malloc + realloc 动态 buffer
- 录音完成后立即释放 buffer
- HippocampusIndex 使用 SQLite（内存友好）

### WiFi 优化
- WiFi 保持连接（wapi renew 每 30s）
- ASR/LLM API 调用失败自动重试（3 次）
- 存储满时自动删除最早的录音

### 电源管理
- 检测到声音才启动录音
- 静音 5 分钟进入低功耗模式
- LRADC 按键唤醒

## 手机端同步（以后做）
- 手机端 App（Flutter/Compose）
- WiFi 发现（mDNS）
- 自动同步（后台服务）
- 通知推送（新录音完成）
