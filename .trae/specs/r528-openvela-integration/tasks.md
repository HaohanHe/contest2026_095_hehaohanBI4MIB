# Tasks

- [x] Task 1: 实现启动时自动 WiFi 联网
  - [x] SubTask 1.1: 新增 `wifi_auto_connect.c/.h`，封装 `wapi.conf` 解析与连接流程
  - [x] SubTask 1.2: 在 `main.c` 启动早期调用 `wifi_auto_connect_start()`
  - [x] SubTask 1.3: 实现失败重试（最多 3 次，间隔 5s）与日志输出
  - [x] SubTask 1.4: 本地 Makefile.local 构建验证

- [x] Task 2: 新增 LVGL AI 电台状态显示页面
  - [x] SubTask 2.1: 新增 `ui_ai_radio.c/.h`，基于 LVGL 创建状态窗口
  - [x] SubTask 2.2: 注册回调，把 ASR partial/final 文本显示到界面
  - [x] SubTask 2.3: 注册回调，把 LLM 分析摘要与报警级别显示到界面
  - [x] SubTask 2.4: 报警时切换背景色/闪烁提示
  - [x] SubTask 2.5: 本地构建验证

- [x] Task 3: 接入 LRADC 按键输入
  - [x] SubTask 3.1: 新增 `input_lradc.c/.h`，打开 `/dev/input/event1` 并解析按键 bitmask
  - [x] SubTask 3.2: 实现按键长按/短按检测线程
  - [x] SubTask 3.3: PTT 长按控制 ASR 启动/停止，菜单短按触发 UI 切换
  - [x] SubTask 3.4: 本地构建验证

- [x] Task 4: 接入 I2S2 真实音频采集
  - [x] SubTask 4.1: 新增 `audio_i2s.c/.h`，打开 `/dev/pcmC0D0c` 读取 16kHz PCM
  - [x] SubTask 4.2: 将采集数据送入 `asr_engine_feed_audio`
  - [x] SubTask 4.3: 在 `radio_config.h` 增加 `AUDIO_CAPTURE_FROM_I2S` 宏切换模拟/I2S 源
  - [x] SubTask 4.4: 当配置为 16kHz 时，在 `asr_engine.c` 中跳过 upsample
  - [x] SubTask 4.5: 本地构建验证

- [x] Task 5: 构建时代码规模统计
  - [x] SubTask 5.1: 在 `Makefile.local` 中增加 `codesize` 目标
  - [x] SubTask 5.2: 使用 `wc` 或 `cloc` 统计 `src/` 下 .c/.h 行数，输出 ELF 大小
  - [x] SubTask 5.3: 让默认 `make` 目标依赖 `codesize`，构建完成后自动输出
  - [x] SubTask 5.4: 本地验证统计输出

# Task Dependencies

- Task 2 依赖 Task 1（UI 显示需要联网后才能获取真实 ASR/LLM 结果，但功能上可独立开发）
- Task 4 依赖 Task 3（PTT 按键是音频采集的触发器之一）
- Task 5 无依赖，可并行完成
