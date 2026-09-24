# ai_radio_console（DayNote）

DayNote 是运行在 Gemini-S1（Allwinner R528，openvela/NuttX）上的 AI 语音记录与
电台终端。完整作品说明、编译烧录与配置步骤见仓库根目录 `README.md`，本文件只讲模块本身。

## 模块说明

- `daynote_main.c`：入口、主循环、BLE 命令（llm_config / mmdvm_config）处理。
- `daynote_core.c`：笔记存储、本地关键词分类、LLM 调用、VAD 后台线程。
- `daynote_ui.c`：LVGL 五页界面（Home / Voice / Review / Settings / DMR）。
- `daynote_asr.c`：SiliconFlow ASR 后端，TLS + multipart，注册为 voice_asr_ops。
- `daynote_mmdvm.c`：MMDVM 网络客户端、DMR 帧解析、通话记录持久化。
- `agent_skills/`：daynote-record / search / summary 三个技能规格。
- `subset_font.py`：中文字库裁剪辅助工具。

## 默认服务端点

- LLM：`https://api.siliconflow.cn/v1/chat/completions`，模型 `Qwen/Qwen2.5-7B-Instruct`。
- ASR：`https://api.siliconflow.cn/v1/audio/transcriptions`，模型 `FunAudioLLM/SenseVoiceSmall`。
- 两者均为 OpenAI 兼容接口，可通过 BLE 配置替换 host / path / model / key。

## 构建接入

- Kconfig 开关：`LVX_USE_DEMO_CONTEST2026_095_AI_RADIO_CONSOLE`，
  依赖 `EXAMPLES_AI_AGENT_VELA` 或 `VELACLAW_DAEMON`，选择 `NETUTILS_CJSON`。
- 数据目录：`DAYNOTE_DATA_DIR`（默认 `/data/daynote`）。
- 周期回顾间隔：`DAYNOTE_REVIEW_INTERVAL`（默认 14400 秒）。
- 板级依赖（BLE GATT、音频、FreeType、mbedTLS、libuv、网络）已在
  `board/contest_board/configs/nsh/defconfig` 打开。

## 运行时依赖的框架组件

本模块调用 ai_agent / velaclaw 框架的接口：语音通道（voice/）、LLM 路由（llm/）、
消息总线（core/message_bus）、配置存储（infra/config_store）、TLS（infra/vela_tls）、
音频采集（voice/audio_capture）。这些组件不在本目录，随 openvela 工程提供；
干净环境全链路编译状态见根 README 的"验证状态"一节。
