# DayNote Handoff — 2026-08-01

## 项目概况

**项目名称**：DayNote — 全天语音记忆设备  
**队伍**：095 (hehaohanBI4MIB)  
**目标板**：Gemini-S1 (Allwinner R528, 双核 Cortex-A7, 128MB DDR3)  
**竞赛**：2026 首届 openvela AI 硬件开发者大赛  
**产品定位**："得到大脑"式语音笔记设备 — 全天录音 → AI转写 → 每日摘要 → 高光提取

## 当前状态

### ✅ 已完成
1. **固件编译成功**：`gemini-s1_daynote_v1.img` (26MB)
2. **核心模块实现**：
   - VAD静音检测（能量阈值法）
   - 自动分段录音（后台线程，VAD驱动）
   - SiliconFlow ASR API集成
   - SiliconFlow LLM API集成
   - WAV编码
   - 笔记存储（JSON索引 + WAV文件）
   - HippocampusIndex关键词索引
   - DailyDigest每日摘要
   - HTTP同步服务框架
   - DayNote UI（320x240）
3. **编译环境**：
   - 编译树搬到西数500G HDD
   - 符号链接：`/home/bi4mib/openvela-build/` → 西数HDD
   - 拔硬盘插别的电脑即可继续开发
4. **硬件验证**：
   - 屏幕驱动正常（ILI9341, 320x240）
   - I2S音频采集正常（16kHz, 16bit, mono）
   - WiFi自动连接正常
   - LVGL UI正常

### ⏳ 进行中
1. **ai_agent框架集成**：DayNote应作为ai_agent的background service + Skill
   - 通过message_bus通信
   - 复用llm_proxy进行ASR/LLM调用
   - 复用memory_store进行笔记存储
   - 新增DayNote slash命令（/daynote, /summary, /search）
2. **HTTP同步服务**：http_sync.c是框架，需要实现真实网络监听
3. **手机端App**：未开始

### ❌ 不能做的（硬件限制）
- 本地ASR模型（Whisper最小也要~100MB RAM）
- 本地LLM（7B模型需要4GB+）
- 本地TTS
- 流式ASR（同时跑AudioRecord + HTTPS上传 + mbedtls内存不够）
- 录音超过5分钟（内存buffer限制）

## 文件结构

### 竞赛仓库（西数HDD）
```
/run/media/bi4mib/新加卷/ontest2026_095_hehaohanBI4MIB/
├── app/ai_radio_console/           # DayNote源码（source of truth）
│   ├── src/
│   │   ├── main.c                  # App入口
│   │   ├── ui_daynote.c            # DayNote UI
│   │   ├── vad_detector.c          # VAD静音检测
│   │   ├── auto_recorder.c         # 自动分段录音
│   │   ├── note_store.c            # 笔记存储
│   │   ├── memory_index.c          # HippocampusIndex
│   │   ├── daily_digest.c          # DailyDigest
│   │   ├── http_sync.c             # HTTP同步服务
│   │   ├── siliconflow_client.c    # SiliconFlow API
│   │   ├── wav_encoder.c           # WAV编码
│   │   ├── audio_i2s.c             # I2S采集
│   │   ├── config_store.c          # 配置存储
│   │   ├── spacelog_settings.c     # WiFi/API设置
│   │   └── wifi_auto_connect.c     # WiFi连接
│   ├── include/                    # 头文件
│   ├── Makefile                    # 构建规则
│   └── Kconfig                     # Kconfig配置
├── agent.md                        # Agent交接文档（本文件）
├── DAYNOTE_PLAN_v4.md              # 产品方案v4
└── gemini-s1_daynote_v1.img        # 最新固件
```

### 编译树（西数HDD，通过符号链接访问）
```
/home/bi4mib/openvela-build/ -> /run/media/bi4mib/新加卷/ontest2026_095_hehaohanBI4MIB/openvela-build/
├── nuttx/                          # NuttX内核
├── packages/ai_agent/              # 小米AI Agent框架
│   ├── src/
│   │   ├── agent_main.c            # Agent主入口
│   │   ├── core/
│   │   │   ├── agent_loop.c        # ReAct循环
│   │   │   ├── message_bus.c       # 消息总线
│   │   │   ├── memory_store.c      # 长期记忆
│   │   │   └── session_mgr.c       # 会话管理
│   │   ├── llm/
│   │   │   ├── llm_router.c        # 多LLM路由
│   │   │   └── llm_proxy.c         # LLM代理
│   │   ├── tools/tool_registry.c   # 工具注册
│   │   ├── voice/
│   │   │   ├── voice_channel.c     # 语音通道
│   │   │   ├── voice_asr.c         # ASR抽象层
│   │   │   └── audio_capture.c     # 音频采集
│   │   └── ui/lvgl_ui_channel.c    # LVGL UI通道
│   └── agent_skills/               # 30+ Skills
├── packages/demos/contest2026_095_ai_radio_console/  # App部署副本
└── vendor/allwinnertech/           # Allwinner R528 BSP
```

## 快速开始（新电脑）

### 1. 环境准备
```bash
# 安装交叉编译工具链
sudo apt install arm-none-eabi-gcc arm-none-eabi-gdb

# 或使用openvela预编译工具链（推荐）
# 工具链已在openvela-build/prebuilts/目录下
```

### 2. 挂载西数HDD
```bash
# 西数HDD通常自动挂载到 /run/media/你的用户名/新加卷/
# 如果没有自动挂载：
sudo mount /dev/sdb1 /run/media/你的用户名/新加卷/
```

### 3. 重建符号链接
```bash
ln -s /run/media/你的用户名/新加卷/ontest2026_095_hehaohanBI4MIB/openvela-build ~/openvela-build
```

### 4. 编译
```bash
export PATH=~/openvela-build/prebuilts/gcc/linux-x86_64/arm-none-eabi/bin:~/openvela-build/prebuilts/build-tools/linux-x86_64/bin:$PATH
cd ~/openvela-build
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay -j1
```

### 5. 打包
```bash
cd vendor/allwinnertech/lichee
bash -c 'source envsetup.sh && lunch_nuttx 2 && pack'
```

### 6. 刷写
```bash
# 使用PhoenixSuit (Windows) 或 LiveSuit (Linux)
# 镜像路径：~/openvela-build/vendor/allwinnertech/lichee/out/r528s3/gemini-s1_nand/rtos_nuttx_r528s3-gemini-s1_uart0_128Mnand.img
```

## 关键配置

### defconfig路径
```
vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/defconfig
```

### 重要配置项
- `CONFIG_LVX_USE_DEMO_CONTEST2026_095_AI_RADIO_CONSOLE=y` - DayNote app
- `CONFIG_LTO_NONE=y` - LTO禁用
- `# CONFIG_UTILS_CURL is not set` - 禁用curl工具
- `CONFIG_I2S=y` - I2S音频
- `CONFIG_DRIVERS_TPADC=y` - 电阻触摸屏
- `# CONFIG_GT911_IIC_TOUCH is not set` - 禁用电容触摸

## 已知问题

1. **VAD阈值需要调整**：当前是固定阈值，实际使用中需要根据环境噪声自适应调整
2. **HTTP服务未实现**：http_sync.c是框架，需要实现真实网络监听
3. **ai_agent集成未完成**：message_bus + agent_loop + slash命令需要实现
4. **手机端App未开始**：需要开发Flutter/Compose App
5. **内存优化**：128MB RAM下需要优化内存使用，避免OOM

## 下一步计划

### 短期（1-2周）
1. 集成ai_agent框架（message_bus + agent_loop）
2. 实现HTTP同步服务真实网络监听
3. 优化VAD算法（自适应阈值）
4. 添加关键词提取功能

### 中期（2-4周）
1. 实现手机端App（Flutter/Compose）
2. WiFi发现（mDNS）
3. 自动同步（后台服务）
4. 通知推送（新录音完成）

### 长期（1-2月）
1. 与小米AI Agent框架深度集成
2. 本地TTS（如果内存允许）
3. 多模态输入（图片+语音）
4. 知识图谱构建

## 参考文档

- `DAYNOTE_PLAN_v4.md` - 完整产品方案v4
- `agent.md` - Agent交接文档
- `packages/ai_agent/docs/architecture.md` - ai_agent架构文档
- `packages/ai_agent/agent_skills/` - 30+ Skills参考

## 联系方式

- 项目repo：`ontest2026_095_hehaohanBI4MIB/`
- 编译树：`openvela-build/`（符号链接）
- 固件：`gemini-s1_daynote_v1.img`

---

**祝你好运！** 🚀
