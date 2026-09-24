# DayNote · AI 智能电台主控台

> 参赛队伍：**hehaohanBI4MIB**（何浩瀚 / HaohanHe，呼号 BI4MIB）
> 赛道：AI 硬件产品创新
> 平台：openvela（NuttX）on Gemini-S1（Allwinner R528）

## 一、作品简介

在田间、野外架台、电台桌前，双手常常被占着——拧旋钮、握话筒、干农活，腾不出手打字记录。
**DayNote 是一台"开口就能记、还能替你通联"的 AI 智能电台主控台**：对着设备说话，它自动完成
**转写 → 理解 → 摘要 → 归档**，把口述沉淀成可检索的笔记；同时通过一块 **AI Radio HAT** 扩展板
接入业余电台，用语音完成数字模式（MMDVM/DMR）通联与控制。

它把三样东西合在了一台 320×240 小屏设备里：

1. **全天候语音笔记**：PTT 或 VAD 触发录音，ASR 转写，LLM 自动分类（备忘 / 待办 / 日程）、摘要与关键词，时间线 + 卡片式回看；
2. **AI Agent 技能体系**：记录、检索、每日回顾三个 Skill，端侧可离线兜底，联网时调用大模型；
3. **业余电台主控**：MMDVM/DMR 热点逻辑、PTT、CAT 控制，配合 AI Radio HAT 硬件实现音频与控制接口。

## 二、系统组成

```text
┌─────────────────────────── Gemini-S1 (R528, openvela/NuttX) ───────────────────────────┐
│                                                                                         │
│   app/ai_radio_console（DayNote 主控，C）                                                │
│   ├── daynote_main.c   主入口、LVGL/字体、主循环                                         │
│   ├── daynote_core.c   笔记存储、分类、语音通道（ASR/LLM）                               │
│   ├── daynote_ui.c     时间线 / 录音 / 回顾 / 卡片 四页 UI（320x240）                    │
│   ├── daynote_asr.c    语音识别接入（ai_agent 火山 ASR，可切 SiliconFlow SenseVoice）    │
│   ├── daynote_mmdvm.c  MMDVM/DMR 热点与电台控制逻辑                                      │
│   └── agent_skills/    daynote-record / daynote-search / daynote-summary 三个 Skill      │
│                                                                                         │
│   quickapp/ai_radio（配套快应用：手机/手表端查看与联动）                                  │
│   board/contest_board（板级配置：NSH defconfig 已开启所需能力）                          │
│                                                                                         │
│   ────────────────  AI Radio HAT（hardware/ai_radio_hat，外接）  ────────────────        │
│   电台音频↔R528 LINEIN/HPOUT · 光耦隔离 PTT · CAT 串口 · I2C 传感器扩展                  │
└─────────────────────────────────────────────────────────────────────────────────────────┘
```

- 基于 openvela 官方 **mini_memo + ai_agent** 框架重构，生产仓库（nuttx / packages / vendor）零改动；
- 多 LLM 后端：DeepSeek / Kimi / 通义 / GLM / SiliconFlow（OpenAI 兼容）；
- 断网降级：录音、本地规则分类、端侧检索与聚合均离线可用，恢复网络后补跑。

## 三、仓库目录

| 路径 | 内容 |
|---|---|
| `app/ai_radio_console/` | DayNote 主控应用（源码、构建脚本、Skill、方案文档） |
| `quickapp/ai_radio/` | 配套快应用（manifest、首页 index.ux、i18n） |
| `board/contest_board/` | Gemini-S1 板级配置（`configs/nsh/defconfig` 已修改） |
| `hardware/ai_radio_hat/` | AI Radio HAT：KiCad / 嘉立创 EDA 工程、原理图、BOM |
| `logs/HaohanHe/` | AI Coding 对话日志与会话清单 |
| `contest2026_095_hehaohanBI4MIB.xml` | repo manifest，含各目录到编译树的 `<linkfile>` 映射 |

## 四、Agent Skills

| Skill | 触发 | 能力 |
|---|---|---|
| `daynote-record` | "记录一下" / PTT / VAD 分段 | 录音 → ASR 转写 → LLM 分类摘要关键词 → 写入 `/data/daynote/memos.json`；断网走本地规则兜底 |
| `daynote-search` | "找一下关于 X 的记录" / 回顾页检索 | 端侧按关键词 / 时间 / 类型检索笔记，卡片展示，结果不离开设备 |
| `daynote-summary` | "总结一下今天" / 定时回顾 | 聚合当天笔记，输出今日高光、待办与关键词，LLM 生成连贯日报 |

## 五、编译与烧录

按官方方式拉取整套工程（本仓 manifest 会把各目录软链进编译树）：

```bash
repo init -u https://github.com/open-vela/contest2026_095_hehaohanBI4MIB \
  -b dev-ai-contest-2026 -m contest2026_095_hehaohanBI4MIB.xml
repo sync -c -j8
```

在 openvela 工作区根目录（本仓的上一级）构建 Gemini-S1 固件：

```bash
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay -j8
cd vendor/allwinnertech/lichee && source envsetup.sh && lunch_nuttx && pack
```

> 板级能力开关见 `board/contest_board/configs/nsh/defconfig`；官方音频修复脚本
> `fix_gemini_s1.sh`（媒体图 + DMA IRQ + ffmpeg）已应用。

## 六、运行配置

- **LLM**：NSH 中 `set_llm deepseek sk-xxx`（或 `set_llm openai sk-xxx` 走 SiliconFlow 兼容接口）；
- **中文字体**：将 TTF（建议 < 5MB，如思源黑体 / 阿里巴巴普惠体）放到 `/data/daynote/font.ttf`，启动时 FreeType 运行时加载；
- **数据**：`/data/daynote/memos.json`，每条含 id/type/content/transcript/summary/keywords/timestamp/duration_ms，上限 100 条自动淘汰；
- **硬件接线**：见 `hardware/ai_radio_hat/README.md`（LINEIN / HPOUT / CAT / GPADC0 / TWI2）。

## 七、AI Radio HAT 硬件

为 Gemini-S1 设计的 80mm×50mm 双层业余无线电扩展板（KiCad 7 + 嘉立创 EDA 双工程）：
电台音频进 LINEIN 供 ASR、R528 音频出至电台 MIC 做 TTS 数字语音、光耦隔离 PTT、CAT 串口、I2C 传感器扩展。
工程文件、ASCII 原理图与物料清单见 `hardware/ai_radio_hat/`。

## 八、AI Coding 日志

开发过程与 AI 工具的对话记录见 `logs/HaohanHe/`（含会话清单 `manifest.json`）。
本次主要在 Trae / MiMo-V2.5 中开发，无法导出官方 JSONL，故提交 Markdown 原始全文，详见该目录说明。

## 九、已知限制与后续

1. VAD 自动录音当前以 PTT 为主，真正的能量检测 + 静音分段需扩展 ai_agent voice_channel；
2. 电阻屏触控坐标需实机校准；
3. AI Radio HAT 的 PCB 已完成板框与原理，布局布线与打样为下一步；
4. 获奖后按要求将作品 PR 至 openvela 上游对应仓库，走标准 PR + CI 流程。
