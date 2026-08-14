# DayNote Handoff — 2026-08-14

## 先读我

本文件是给**下一个 AI 助手**的完整交接文档。不是给人看的。

---

## 项目是什么

**DayNote** — 全天语音记忆设备。运行在 Gemini-S1 (Allwinner R528, 双核 Cortex-A7, 128MB DDR3) 开发板上。产品概念："得到大脑"式语音笔记——全天候录音 → AI 转写 → 每日摘要 → 高光提取。

**不是"AI 无线电"**。项目从 AI Radio 转型而来，`app/ai_radio_console/` 这个目录名是历史遗留。代码里很多文件、注释、TODO 说的"无线电"、"CW 解码"、"CAT 串口"、"PTT"等都是过时的，不要被误导。

**"095"** 是比赛报名编号，不是"队伍 095"。用户对这类说法敏感。

---

## 仓库结构

### 竞赛仓库（source of truth）

```
/run/media/bi4mib/新加卷/ontest2026_095_hehaohanBI4MIB/
├── app/ai_radio_console/        # DayNote 源码（唯一真实来源！）
│   ├── src/                     # 19 个 .c 源文件
│   │   ├── main.c               # 入口，完整实现
│   │   ├── ui_daynote.c         # 320x240 LVGL UI（完整，但 REC 按钮缺点击回调）
│   │   ├── vad_detector.c       # VAD 静音检测（完整）
│   │   ├── auto_recorder.c      # 自动分段录音（完整，自动触发 ASR+LLM）
│   │   ├── note_store.c         # 笔记存储 JSON 持久化（完整但 JSON 解析脆弱）
│   │   ├── memory_index.c       # 海马体关键词索引（完整，线程安全）
│   │   ├── daily_digest.c       # 每日摘要（完整，同步调用 LLM）
│   │   ├── daily_summary.c      # 每日摘要（与 daily_digest 功能重复）
│   │   ├── siliconflow_client.c # SiliconFlow API HTTPS 客户端（最完整的模块 ~645行）
│   │   ├── llm_analyzer.c       # LLM 异步分析 + 翻译（完整）
│   │   ├── agent_bridge.c       # 统一 API 封装（完整，但 install_skills 是桩）
│   │   ├── daynote_agent.c      # AI Agent 集成层（半桩：init/start/stop 空壳）
│   │   ├── audio_i2s.c          # I2S 音频采集（完整，与 audio_capture 重叠）
│   │   ├── audio_capture.c      # 音频采集封装（完整，与 audio_i2s 重叠）
│   │   ├── wav_encoder.c        # WAV 编码器（完整但似乎未被任何模块引用）
│   │   ├── config_store.c       # INI 配置读写（完整，与 spacelog_settings 重叠）
│   │   ├── spacelog_settings.c  # WiFi/API 设置 UI（最大的文件 ~906行，完整）
│   │   ├── wifi_auto_connect.c  # WiFi 自动连接（完整，shell 转义已修复）
│   │   ├── input_lradc.c        # LRADC 按键处理（完整，ENTER 长按触发 ASR）
│   │   └── translator.c         # 翻译模块（新建，功能待实现）
│   ├── include/                 # 19 个对应的 .h 文件
│   ├── Makefile                 # 构建规则
│   └── CMakeLists.txt           # CMake 构建规则
├── HANDOFF.md                   # 本文件
├── AGENTS.md                    # AI 助手快速指南
├── TODO.md                      # 任务跟踪（过时，需要更新）
├── DAYNOTE_PLAN_v4.md           # 产品方案 v4
├── .agentsskills/               # openvela 官方 AI 技能
└── feishu_docs_full/            # 飞书文档（Gemini-S1 开发板资料）
```

### 编译树（西数 HDD，通过符号链接访问）

```
/home/bi4mib/openvela-build/
  → /run/media/bi4mib/新加卷/ontest2026_095_hehaohanBI4MIB/openvela-build/
├── nuttx/                       # NuttX 内核源代码
├── apps/                        # NuttX 应用层
├── vendor/allwinnertech/        # Allwinner R528 BSP + 驱动
├── frameworks/                  # openvela 框架
└── packages/demos/contest2026_095_ai_radio_console/  # App 部署副本
```

---

## 关键原则（必须遵守）

### 1. 两个副本需要手动同步

代码改动在 `app/ai_radio_console/` 后，必须手动复制到 `openvela-build/packages/demos/contest2026_095_ai_radio_console/`。**不会自动同步。**

```bash
for f in src/*.c include/*.h Makefile CMakeLists.txt; do
  cp app/ai_radio_console/$f openvela-build/packages/demos/contest2026_095_ai_radio_console/$f
done
```

### 2. 不要滥用禁用来修复编译错误

之前犯过错误：禁用 `CONFIG_MEDIA_SERVER`、`CONFIG_DRIVERS_TPADC` 来逃避编译错误。这是**砍头砍脚**。正确做法：
- 找到根因（比如 `touch_lowerhalf_s` 缺少定义 → 加 `CONFIG_INPUT_TOUCHSCREEN=y`）
- 链接错误（`localtime_r` 重复定义 → 加 `--allow-multiple-definition`）
- 配置缺失（`arm_cpuhead.S` 找不到 → 加 `CONFIG_ARCH_ARMV7A=y`）

**永远不要为了编译通过而禁用功能。**

### 3. DayNote 需要所有功能在线

DayNote 依赖音频采集、触摸屏、WiFi、LLM API。禁用任何一项都会让产品变砖。

### 4. 不要修改驱动代码

`hal_dma.c`、`sunxi_alsa.c`、`drv_lradc.c` 等驱动文件不要碰。驱动问题在 defconfig 中解决。

### 5. 用户风格

- 说中文，语气直接
- 要结果不要解释——修好它，不要说为什么修不好
- 期望 AI 能编译、构建、烧录，不只是给建议
- 用户对"砍头砍脚"式的修复非常反感

---

## 当前编译状态

### 已应用到 openvela-build 的修复

**defconfig 修改**（`vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/defconfig`）：

| 配置 | 值 | 原因 |
|------|-----|------|
| `CONFIG_LVX_USE_DEMO_CONTEST2026_095_AI_RADIO_CONSOLE` | =y | 启用 DayNote app |
| `CONFIG_LTO_NONE` | =y | 禁用 LTO 避免链接错误 |
| `CONFIG_I2S` | =y | I2S 音频 |
| `CONFIG_DRIVERS_TPADC` | =y | 电阻触摸屏 |
| `CONFIG_INPUT_TOUCHSCREEN` | =y | **关键修复**：定义 `touch_lowerhalf_s` 结构体 |
| `CONFIG_ARM_TOOLCHAIN_GNU_EABI` | =y | 选择 arm-none-eabi 交叉编译器 |
| `CONFIG_STACK_USAGE_WARNING` | =0 | 修复 `-Wstack-usage=` 空参数错误 |
| `CONFIG_SCHED_WORKQUEUE` | =y | 修复 worker thread 支持 |
| `CONFIG_ARCH_ARMV7A` | =y | **关键修复**：修复 `arm_cpuhead.S` 找不到 |
| `# CONFIG_GT911_IIC_TOUCH` | is not set | 禁用电容触摸（我们是电阻屏） |
| `# CONFIG_UTILS_CURL` | is not set | 禁用 curl |

**board Make.defs 修改**（`vendor/allwinnertech/boards/r528/r528s3-gemini-s1/scripts/Make.defs`）：
- 在第 31 行添加 `LDFLAGS += --allow-multiple-definition` — 修复 `localtime_r` 等函数重复定义链接错误

**lv_conf.h 修改**（`openvela-build/apps/graphics/lvgl/lv_conf.h`）：
- 第 16 行：`#if 1` — 启用 LVGL 配置（原为 `#if 0`）

### 构建命令

```bash
export PATH=/home/bi4mib/openvela-build/prebuilts/build-tools/linux-x86_64/bin:/home/bi4mib/openvela-build/prebuilts/gcc/linux-x86_64/arm-none-eabi/bin:/usr/bin:/bin:/usr/sbin:/sbin:$PATH
cd /home/bi4mib/openvela-build
rm -rf cmake_out/ nuttx/.config
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay -j1
```

**固件产物**：`/home/bi4mib/openvela-build/nuttx/nuttx.bin` (7.0MB)

### 打包命令

```bash
cd /home/bi4mib/openvela-build/vendor/allwinnertech/lichee
bash -c 'source envsetup.sh && lunch_nuttx 2 && pack'
# 镜像输出：lichee/out/r528s3/gemini-s1_nand/rtos_nuttx_r528s3-gemini-s1_uart0_128Mnand.img
```

---

## 代码审计：哪些是真实代码，哪些是空的

### 完整真实实现（可以直接用）

| 文件 | 质量 | 说明 |
|------|------|------|
| `siliconflow_client.c` | ⭐⭐⭐⭐⭐ | HTTPS 客户端，ASR + LLM，645 行，最完整的模块 |
| `auto_recorder.c` | ⭐⭐⭐⭐⭐ | 后台线程录音 + VAD 分段 + ASR/LLM 自动处理 |
| `main.c` | ⭐⭐⭐⭐ | 完整启动流程，音频 ioctl 配置 |
| `spacelog_settings.c` | ⭐⭐⭐⭐ | 906 行 WiFi/API 设置 UI，LVGL 交互 |
| `vad_detector.c` | ⭐⭐⭐⭐ | 自适应能量阈值 VAD，完整状态机 |
| `note_store.c` | ⭐⭐⭐⭐ | JSON 持久化笔记存储 |
| `memory_index.c` | ⭐⭐⭐⭐ | 海马体索引，线程安全，JSON 持久化 |
| `wifi_auto_connect.c` | ⭐⭐⭐⭐ | WiFi 连接，shell 转义，重试机制 |
| `input_lradc.c` | ⭐⭐⭐⭐ | LRADC 按键，ENTER 长按触发 ASR |
| `llm_analyzer.c` | ⭐⭐⭐⭐ | 异步 LLM 分析 + 翻译 |
| `agent_bridge.c` | ⭐⭐⭐⭐ | 统一 API 封装 |

### 有功能重复的

| 文件 | 问题 |
|------|------|
| `daily_digest.c` 和 `daily_summary.c` | 功能完全重复，都是"今日笔记→LLM→摘要"，二选一 |
| `audio_i2s.c` 和 `audio_capture.c` | 功能重叠，都是音频采集 |
| `config_store.c` 和 `spacelog_settings.c` | INI 读写功能重复 |
| `wav_encoder.c` | 实现完整但未被其他模块引用（auto_recorder 内联写入 WAV） |

### 半桩/需要补全

| 文件 | 问题 |
|------|------|
| `daynote_agent.c` | `init/start/stop` 是空壳，`/daynote search` 返回 "not yet implemented" |
| `translator.c` | 新建文件，`translator_translate_text()` 返回原文 |
| `http_sync.c` | 框架代码，真实网络监听未实现 |
| `ui_daynote.c` | REC 按钮**缺少点击事件回调**，点它什么都没发生 |

---

## 已知问题（必须修）

### P0（编译/运行）

1. **构建系统**：`openvela-build` 的 git 不跟踪 defconfig。`nsh_minidisplay/defconfig` 是手动从 `nsh/defconfig` 复制 + 修改的。如果 `git checkout` 或 `git clean`，defconfig 会丢失。需要定期备份到竞赛仓库。
2. **archive order bug**：`libapps.a` 在 app 编译前就创建了，目标文件存在但未归档。如果编译到链接阶段报 `undefined reference` 且符号属于 `libapps.a`，手动补入：`arm-none-eabi-ar r staging/libapps.a missing_object.o`

### P1（功能）

1. **REC 按钮无回调** — UI 上的录音按钮没有注册点击事件，用户点了没反应。修复方法：在 `ui_daynote_init()` 中加 `lv_obj_add_event_cb(g_record_btn, record_btn_cb, LV_EVENT_CLICKED, NULL)`，回调中调用 `auto_recorder_start/stop` 和 `ui_daynote_update_recording_state()`。
2. **daynote_agent.c 是空壳** — 需要实现 message_bus 集成，让 DayNote 通过 ai_agent 框架发布消息。
3. **JSON 解析脆弱** — 全部用 `strstr` 逐行搜索，不支持嵌套对象，不支持转移字符反转义。
4. **daily_digest 同步阻塞** — 调用 LLM 时阻塞调用者。需要改成异步 + 回调。

### P2（健壮性）

1. **note_store deinit 没释放内存** — `transcript` 和 `summary` 是 malloc 的，但 deinit 只 `memset` 清零了结构体数组。
2. **input_lradc 线程退出不优雅** — `input_lradc_stop()` 靠 sleep 等线程退出，没有用条件变量。
3. **TLS 证书验证被禁用** — `MBEDTLS_SSL_VERIFY_NONE`，有中间人攻击风险。

---

## 下一步做什么

### 短期（下一轮）

1. **REC 按钮点击回调** — 让用户能通过 UI 控制录音
2. **清理重复代码** — daily_digest/daily_summary 合并，audio_i2s/audio_capture 合并
3. **打包 .img 并烧录测试** — 验证屏幕、触摸、录音是否能真的工作

### 中期

1. **daynote_agent.c** — 实现 message_bus 集成
2. **http_sync.c** — 实现真实 HTTP server
3. **VAD 自适应阈值** — 当前是固定阈值，需要根据环境噪声动态调整

### 长期

1. **手机端 App** — Flutter/Compose
2. **WiFi 发现（mDNS）**
3. **自动同步**

---

## 对 AI 助手的忠告

1. 用户中文交流，语气直接。**不要解释为什么有问题，直接修。**
2. **不要砍头砍脚**。禁用功能来通过编译是最蠢的修复方式，用户会发火。
3. 编译问题要找根因：配置缺失加配置，结构体未定义加包含头文件或定义宏。
4. 这个项目是比赛作品，所有代码都需要**真正工作**，不只是编译通过。
5. `feishu_docs_full/` 目录下有 Gemini-S1 开发板的完整文档，包含 defconfig 配置方法、烧录教程。
6. `.agentsskills/` 目录下有 openvela 官方 AI 技能，包含编译、构建、Kconfig 修改等工具。
7. **先审查，再动手。** 不要假设文件是干什么的，Read 它。