# DayNote 基于 ai_agent 框架的实现方案 v4

## 核心思路
DayNote 不是独立应用，是 **ai_agent 框架的扩展 Skill + 后台服务**。
复用 ai_agent 已有的：LLM 路由、ASR、记忆系统、消息总线、LVGL UI 通道。
新增：VAD 自动分段录音、HippocampusIndex 关键词索引、DailyDigest、HTTP 同步服务。

## 已有能力（ai_agent 框架直接复用）

| 能力 | 文件 | 说明 |
|------|------|------|
| ReAct 循环 | agent_loop.c | 已有，处理用户 query |
| 多 LLM 后端 | llm_router.c | 支持 MiMo/Kimi/Qwen/DeepSeek |
| LLM HTTP 代理 | llm_proxy.c | 已有，支持 tools 格式 |
| 消息总线 | message_bus.c | 已有，inbound/outbound 双队列 |
| 长期记忆 | memory_store.c | 已有，读写 MEMORY.md + daily 笔记 |
| 会话管理 | session_mgr.c | 已有，JSONL 格式 |
| 工具注册 | tool_registry.c | 已有，30+ 工具 |
| 语音通道 | voice_channel.c | 已有，PTT 录音 + TTS 播放 |
| ASR 抽象层 | voice_asr.c | 已有，支持 streaming/batch |
| 音频采集 | audio_capture.c | 已有，16kHz/16bit/mono |
| LVGL UI | lvgl_ui_channel.c | 已有，但适配 466x466 手表屏 |

## 已有 Skills（直接复用或修改）

| Skill | 文件 | 复用方式 |
|-------|------|----------|
| note-taker | agent_skills/note-taker.md | 直接复用，写 daily 笔记 |
| daily-briefing | agent_skills/daily-briefing.md | 直接复用，每日摘要 |
| voice-memo | agent_skills/voice-memo.md | 修改为自动录音模式 |

## 需要新增的模块

### 1. vad_detector.c/h - VAD 静音检测
- 短时能量计算（RMS）
- 自适应阈值（基于背景噪声）
- 状态机：SILENCE → SPEECH_START → SPEECH → SPEECH_END

### 2. auto_recorder.c/h - 自动分段录音
- 后台线程持续录音
- VAD 驱动自动分段
- 每段最大 5 分钟
- 完成后自动触发 ASR → LLM → 记忆索引

### 3. memory_index.c/h - HippocampusIndex 扩展
- 在 memory_store.c 基础上增加：
  - 关键词提取（从 transcript 提取）
  - 时间线索引
  - 高光标记（is_highlight）
  - 搜索功能（按关键词/时间/重要性）

### 4. daily_digest.c/h - DailyDigest 扩展
- 在 daily-briefing skill 基础上增加：
  - 高光提取（从今天的录音中挑出最重要的 2-3 条）
  - 关键词云
  - 每日总结

### 5. http_sync.c/h - HTTP 同步服务
- 轻量 HTTP server（8080 端口）
- RESTful API：
  - GET /api/notes - 笔记列表
  - GET /api/notes/:id - 单条笔记
  - GET /api/notes/:id/audio - WAV 下载
  - GET /api/daily/:date - 某天摘要
  - GET /api/status - 设备状态

### 6. ui_daynote.c/h - DayNote UI（替换 lvgl_ui_channel.c）
- 适配 320x240 屏幕
- 时间线视图
- 录音状态指示
- 今日高光显示
- 搜索界面

## 数据流（基于 ai_agent 框架）

```
[auto_recorder 后台线程]
    ↓
[VAD 静音检测] → 静音 2s → 分段
    ↓
[保存 WAV] → /data/agent/memory/audio/note_YYYYMMDD_HHMMSS.wav
[写入 daily 笔记] → /data/agent/memory/daily/YYYY-MM-DD.md
    ↓
[调用 voice_asr] → SiliconFlow ASR API
    ↓ HTTP multipart upload
[ASR 返回 text]
    ↓
[调用 llm_proxy] → SiliconFlow LLM API
    ↓ HTTP JSON
[LLM 返回 summary + keywords + is_highlight]
    ↓
[memory_store_append_today] → 追加到 daily 笔记
[memory_index_add] → 更新 HippocampusIndex
[message_bus_publish] → "daynote.new_recording" → LVGL UI 更新
```

## 与 ai_agent 的集成点

### 1. 作为 Background Service
DayNote 后台录音线程作为 ai_agent 的 background task 运行：
```c
// agent_main.c 初始化阶段
int ret = auto_recorder_start();
if (ret == 0) {
    printf("[DayNote] Auto recorder started\n");
}
```

### 2. 通过 message_bus 通信
```c
// auto_recorder.c → LVGL UI
message_bus_publish("daynote.recording_start", NULL);
message_bus_publish("daynote.recording_stop", NULL);
message_bus_publish("daynote.new_recording", note_json);

// LVGL UI → auto_recorder
message_bus_subscribe("daynote.start", auto_recorder_start);
message_bus_subscribe("daynote.stop", auto_recorder_stop);
message_bus_subscribe("daynote.search", daynote_agent_search);
```

### 3. 作为 Skill 扩展
新增 `daynote` skill：
```markdown
# DayNote Skill
## When to use
用户说"记录"、"开始录音"、"今天讨论了什么"、"总结今天"

## How to use
- 记录: auto_recorder_start()
- 停止: auto_recorder_stop()
- 搜索: memory_index_search(query)
- 每日摘要: daily_digest_generate()
```

### 4. 复用 llm_router
DayNote 的 LLM 调用直接走 llm_proxy：
```c
// 不需要自己实现 HTTP 客户端
char response[2048];
llm_chat("你是个人助理，帮助总结语音笔记。", transcript, response, sizeof(response));
```

### 5. 复用 memory_store
DayNote 的笔记存储基于 memory_store：
```c
// 直接调用已有 API
memory_store_append_today("note", timestamp, content);
memory_read_recent(7); // 读取最近 7 天
```

## 文件修改清单

### 需要修改的 ai_agent 文件
1. `src/agent_main.c` - 添加 DayNote 初始化
2. `src/core/agent_loop.c` - 添加 daynote 相关 slash 命令（/daynote, /summary, /search）
3. `src/ui/lvgl_ui_channel.c` - 适配 320x240 屏幕，添加 DayNote UI
4. `include/agent_config.h` - 添加 DayNote 配置选项

### 需要新增的文件（在 ai_radio_console app 下）
1. `src/vad_detector.c/h`
2. `src/auto_recorder.c/h`
3. `src/memory_index.c/h`
4. `src/daily_digest.c/h`
5. `src/http_sync.c/h`
6. `src/ui_daynote.c/h`（或直接修改 lvgl_ui_channel.c）

### 需要新增的 Skill
1. `agent_skills/daynote-record.md`
2. `agent_skills/daynote-search.md`
3. `agent_skills/daynote-summary.md`

## 实现步骤

### Step 1: 集成 auto_recorder 到 ai_agent
1. 修改 agent_main.c，添加 auto_recorder 初始化
2. 实现 vad_detector.c
3. 实现 auto_recorder.c（后台线程 + VAD + 分段）
4. 测试：自动录音 → WAV 文件

### Step 2: 集成 ASR + LLM
1. 复用 voice_asr.c 和 llm_proxy.c
2. auto_recorder 完成后自动调用 ASR → LLM
3. 测试：录音 → ASR → LLM 总结

### Step 3: 扩展记忆系统
1. 实现 memory_index.c（关键词提取 + 时间线 + 搜索）
2. 复用 memory_store.c（daily 笔记追加）
3. 测试：记忆索引 → 搜索 → 每日摘要

### Step 4: DayNote UI
1. 修改 lvgl_ui_channel.c 适配 320x240
2. 添加时间线视图
3. 添加搜索界面
4. 测试：UI 显示 → 录音状态 → 搜索结果

### Step 5: HTTP 同步服务
1. 实现 http_sync.c（轻量 HTTP server）
2. 实现 RESTful API
3. 测试：手机端获取笔记列表 + 下载 WAV

### Step 6: DayNote Agent
1. 新增 daynote slash 命令（/daynote, /summary, /search）
2. 集成到 agent_loop.c
3. 测试：语音/按键 query → Agent 响应

## 关键代码示例

### auto_recorder.c 核心逻辑
```c
static void *auto_recorder_thread(void *arg)
{
    while (g_running) {
        // 1. 等待 VAD 检测到语音
        vad_state_t state = vad_wait_for_speech();
        if (state != VAD_SPEECH_START) continue;

        // 2. 开始录音
        audio_start_capture();
        g_recording = true;

        // 3. VAD 驱动分段
        while (g_recording) {
            int16_t buf[1024];
            int n = audio_read(buf, sizeof(buf));
            if (n <= 0) break;

            // 计算能量
            float energy = vad_compute_energy(buf, n / 2);
            vad_state_t state = vad_update(energy);

            // 静音 2s → 分段
            if (state == VAD_SPEECH_END) {
                audio_stop_capture();
                g_recording = false;

                // 保存 WAV
                char path[256];
                snprintf(path, sizeof(path), "/data/agent/memory/audio/note_%ld.wav", time(NULL));
                wav_save(path, g_buffer, g_buffer_len);

                // 触发 ASR → LLM → 记忆索引
                auto_recorder_process(path);
            }
        }
    }
    return NULL;
}
```

### message_bus 集成
```c
// auto_recorder.c
void auto_recorder_on_new_recording(const char *wav_path, const char *transcript)
{
    char json[512];
    snprintf(json, sizeof(json),
        "{\"wav\":\"%s\",\"txt\":\"%s\",\"ts\":%ld}",
        wav_path, transcript, time(NULL));

    message_bus_publish("daynote.new_recording", json);
}

// lvgl_ui_channel.c
static void on_daynote_message(const char *topic, const char *data)
{
    if (strcmp(topic, "daynote.new_recording") == 0) {
        // 更新 UI 时间线
        ui_daynote_add_item(data);
    }
}
```

## 与手机端同步的 API 设计

```
GET  /api/notes              → [{"id":1,"ts":"2026-08-01T09:30:00","dur":45,"highlight":true,"txt":"...","sum":"..."}]
GET  /api/notes/:id          → {"id":1,"ts":"...","dur":45,"txt":"...","sum":"...","keywords":"..."}
GET  /api/notes/:id/audio    → binary WAV file
GET  /api/daily/2026-08-01   → {"date":"2026-08-01","summary":"...","highlights":[...],"keywords":["项目","决策"]}
GET  /api/status             → {"wifi":true,"storage":128,"notes":42}
```

## 注意事项

1. **memory_store.c 的路径**：使用 `/data/agent/memory/` 而不是 `/data/daynote/`
2. **audio_capture.c 的配置**：需要配置为 16kHz/16bit/mono
3. **lvgl_ui_channel.c 的适配**：466x466 → 320x240，需要调整布局
4. **llm_router.c 的后端**：确保 MiMo 可用（需要 API key）
5. **message_bus.c 的队列深度**：可能需要增加（AGENT_BUS_QUEUE_LEN=16 → 32）
