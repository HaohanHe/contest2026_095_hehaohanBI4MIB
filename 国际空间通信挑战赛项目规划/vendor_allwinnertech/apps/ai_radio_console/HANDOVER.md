# AI Radio Console 交接文档

> 写给下一任（下辈子）Linux 本地机维护者  
> 最后更新：2026-07-10  
> 项目：AI Radio Console（Gemini-S1 + AI Radio HAT）  
> 目标赛事：2026 国际空天挑战赛空间通信赛道 + 小米 OpenVela AI 硬件挑战赛

---

## 1. 一句话概括

基于 Gemini-S1 开发板 + AI Radio HAT 的“AI 无线电通联控制台”：

- 监听 LINEIN 音频，PTT 触发录音；
- 通过 ai_agent `voice_channel`（推荐）或板端 `nxrecorder+ASR`（回退）完成语音识别；
- 调用 ai_agent 的 `radio-log` Skill 生成结构化 QSO 日志；
- LVGL 本地 UI + REST/WebSocket（端口 28790）供手机 App 查看/配置。

---

## 2. 工作区目录结构

当前工作区位于 `/workspace`，只包含子仓库，**缺少 OpenVela 顶层 `nuttx/`、`prebuilts/`、`build.sh`**，因此无法直接跑完整编译。

```text
/workspace/
├── ai_radio_hat/                  # AI Radio HAT 硬件资料（KiCad、BOM、README）
├── com.agent.coapp/               # Android 伴侣 App 源码（Kotlin + Jetpack Compose）
├── feishu_docs_full/              # 开发板飞书文档导出
├── frameworks/                    # OpenVela frameworks 子仓库
├── openvela-docs-gitee/           # 官方文档（Gitee 镜像）
├── openvela-docs-github/          # 官方文档（GitHub 镜像）
├── packages_ai_agent/             # ai_agent 包（含 voice_channel、BLE GATT、REST API）
├── packages_demos/                # 官方示例（mini_memo、ai_chat 等）
├── tmp_stubs/                     # 主机语法检查用的 stub 头文件
└── vendor_allwinnertech/
    └── apps/
        └── ai_radio_console/      # <-- 本项目主代码
            ├── main.c
            ├── radio_ui.c/h
            ├── audio_recorder.c/h
            ├── ptt_control.c/h
            ├── asr_client.c/h
            ├── radio_agent.c/h
            ├── radio_server.c/h
            ├── radio-log.md
            ├── Kconfig
            ├── Makefile
            ├── README.md
            ├── DEVNOTES.md
            ├── BLE_ANDROID_INTEGRATION.md
            └── HANDOVER.md          # 本文件
```

---

## 3. 已完成的工作

### 3.1 核心功能

- [x] LVGL UI：状态栏、转写区、日志列表、PTT 按钮；线程安全更新走 `lv_async_call`。
- [x] 中文字体：优先 `/data/res/fonts/MiSans-Normal.ttf`，失败回退 `lv_font_simsun_16_cjk`。
- [x] PTT GPIO 控制：通过 `/dev/gpio0` + `GPIOC_SETPINTYPE/GPIOC_WRITE`。
- [x] 双音频后端：
  - `voice_channel`（默认，推荐，复用 ai_agent 录音+ASR+HTTPS）。
  - `nxrecorder + asr_client`（回退，板端 HTTP ASR，仅支持 HTTP）。
- [x] ai_agent 集成：通过 `velaclaw_client_open/velaclaw_ask` 调用 `radio-log` Skill。
- [x] REST/WebSocket 服务器：端口 28790，兼容 `com.agent.coapp`。
- [x] PTT 自检模式：`--ptt-selftest [ms]` / Kconfig `AI_RADIO_CONSOLE_PTT_SELFTEST_MS`。

### 3.2 稳定性修复（近期）

- Worker 线程栈扩至 40KB（对齐 mini_memo）。
- `voice_channel` 返回空文本时不再误走 nxrecorder 回退。
- `radio_server.c` 大 JSON 响应改为堆分配，避免栈溢出；增加 bind 重试、客户端优雅关闭、EINTR 重试。
- `handle_config_put` 增加 `strdup` 失败保护。
- PTT 逻辑改为**先启动音频，成功后再 key 外部电台**，避免音频未就绪时误发射。
- 释放 PTT 先于停止录音，避免 ASR 处理期间仍占用信道。
- `radio_agent.c`：VelaClaw open/ask 增加重试；新增 `radio_agent_save_raw_log()` 本地降级，agent 不可用时仍保留转写。
- `asr_client.c`：HTTP ASR 请求增加重试机制。
- Android 端：BLE 重连改为指数退避；WebSocket 自动重连；REST PTT 作为 WebSocket 失败回退。
- Android 端所有默认端口统一为 `28790`，与 `radio_server` 一致。
- 修复 `radio_ui.c` 注释块中 `*/` 提前结束注释的语法陷阱。
- 修复 `radio_agent.c` 缺少 `<stdbool.h>` / `<sys/stat.h>`。
- 统一日志前缀：`[radio_*]` / `[main]` / `[asr]`。

---

## 4. 当前状态

- **本地语法检查**：板端 C 代码双后端均通过 `gcc -fsyntax-only -Wall -Wextra`；Android 端因环境无 Android SDK/Gradle 分发版，未执行 Gradle 编译，需在你本地 Android Studio 中编译。
- **完整编译**：未执行（环境缺失 OpenVela 顶层）。
- **真机验证**：未执行。
- **手机 App 联调**：未执行。

---

## 5. 继续开发的第一步

### 5.1 若拿到完整 OpenVela 源码树

把当前目录按 OpenVela 路径放好：

```bash
# 假设完整源码树根为 /path/to/openvela
rm -rf /path/to/openvela/vendor/allwinnertech/apps/ai_radio_console
cp -r /workspace/vendor_allwinnertech/apps/ai_radio_console \
      /path/to/openvela/vendor/allwinnertech/apps/

# 同样处理 packages_ai_agent、packages_demos 等（若版本不一致）
```

然后按 [DEVNOTES.md](DEVNOTES.md) 第 6.2 节编译：

```bash
cd /path/to/openvela

cp packages_ai_agent/defconfigs/gemini-s1/gemini-s1_defconfig \
   vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/defconfig

bash packages_ai_agent/fix_gemini_s1.sh

./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay menuconfig
# 勾选：
#   Application Configuration → AI Radio Console (Gemini-S1 HAT) : y
#   Application Configuration → Packages → Vela AI Agent : y
#   Vela AI Agent → Enable BLE GATT data channel : y
#   Vela AI Agent → Enable REST API endpoints : y

./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/ \
    -e -Wno-error -j"$(nproc)"

source envsetup.sh && lunch_nuttx && pack
```

### 5.2 若只能在本工作区继续

至少每次改完代码跑一遍静态检查：

```bash
cd /workspace

INC="-Itmp_stubs -Ipackages_ai_agent/include -Ipackages_ai_agent/src -Ivendor_allwinnertech/apps/ai_radio_console"
SRC="vendor_allwinnertech/apps/ai_radio_console/main.c \
     vendor_allwinnertech/apps/ai_radio_console/audio_recorder.c \
     vendor_allwinnertech/apps/ai_radio_console/radio_ui.c \
     vendor_allwinnertech/apps/ai_radio_console/ptt_control.c \
     vendor_allwinnertech/apps/ai_radio_console/asr_client.c \
     vendor_allwinnertech/apps/ai_radio_console/radio_agent.c \
     vendor_allwinnertech/apps/ai_radio_console/radio_server.c"

# voice_channel 后端
gcc -fsyntax-only $INC -Wall -Wextra -Werror $SRC && echo "voice_channel backend: PASS"

# nxrecorder 回退后端
gcc -fsyntax-only $INC -DCONFIG_AI_RADIO_CONSOLE_USE_VOICE_CHANNEL=0 -Wall -Wextra $SRC && echo "nxrecorder fallback backend: PASS"
```

---

## 6. 关键配置项

| Kconfig 项 | 含义 | 建议 |
|---|---|---|
| `AI_RADIO_CONSOLE_USE_VOICE_CHANNEL` | 使用 ai_agent voice_channel 还是 nxrecorder+ASR | 真机首选 `y` |
| `AI_RADIO_CONSOLE_PTT_SELFTEST_MS` | 启动后自动模拟 PTT 时长 | 无头测试时设 3000 |
| `AI_RADIO_CONSOLE_PTT_GPIO` | PTT GPIO 设备 | `/dev/gpio0` |
| `AI_RADIO_CONSOLE_PTT_PIN` | PTT GPIO pin | `0` |
| `AI_RADIO_CONSOLE_AUDIO_DEV` | 录音设备（仅 nxrecorder 路径有效） | `/dev/audio/pcm0c` |
| `AI_RADIO_CONSOLE_SERVER_PORT` | 内置 REST/WebSocket 端口 | `28790` |

---

## 7. 真机运行前准备

```nsh
# 创建数据目录
nsh> mkdir -p /data/agent/config /data/agent/skills /data/agent/memory/radio

# 若使用 nxrecorder + 第三方 ASR，写入 key
nsh> echo "sk-xxxx" > /data/agent/config/asr_key.txt

# 确保中文字体存在
nsh> ls /data/res/fonts/MiSans-Normal.ttf

# 启动 ai_agent
nsh> ai_agent

# 启动电台控制台（PTT 自检 3 秒）
nsh> ai_radio_console --ptt-selftest 3000
```

---

## 8. 常见坑与排查

| 现象 | 可能原因 | 排查 |
|---|---|---|
| 编译找不到 `<voice/voice_channel.h>` | Makefile include 路径未加 `packages_ai_agent/src` | 检查 Makefile 中 `AI_AGENT_BASE_DIR/src` |
| 录音无数据 / PTT 卡住 | 未执行 `fix_gemini_s1.sh` | 重新打补丁并检查媒体图 |
| 中文显示方块 | `/data/res/fonts/MiSans-Normal.ttf` 不存在 | 从 ai_chat 资源拷贝 |
| HTTPS ASR 失败 | 板端 asr_client 仅支持 HTTP | 切 voice_channel 或跑 relay.py |
| App 连不上 28790 | 防火墙/端口被占用 | 先 `telnet IP 28790` 验证 |
| 日志前缀乱 | 代码被改回 `[ptt]` | 全局搜索确保 `[radio_*]` |

---

## 9. 关键文件速查

| 文件 | 职责 |
|---|---|
| [main.c](main.c) | 主循环、PTT 轮询、模块初始化、命令行解析 |
| [radio_ui.c/h](radio_ui.c) | LVGL UI、字体、异步更新 |
| [audio_recorder.c/h](audio_recorder.c) | 双后端录音接口 |
| [ptt_control.c/h](ptt_control.c) | GPIO PTT 控制 |
| [asr_client.c/h](asr_client.c) | 板端 HTTP ASR 客户端（nxrecorder 路径） |
| [radio_agent.c/h](radio_agent.c) | VelaClaw + radio-log Skill |
| [radio_server.c/h](radio_server.c) | 28790 REST/WebSocket 服务 |
| [radio-log.md](radio-log.md) | ai_agent Skill 文件 |
| [Kconfig](Kconfig) | 构建配置项 |
| [Makefile](Makefile) | 编译规则、include 路径兼容 |
| [DEVNOTES.md](DEVNOTES.md) | 开发历程、问题清单、构建说明 |
| [BLE_ANDROID_INTEGRATION.md](BLE_ANDROID_INTEGRATION.md) | BLE 配网与 Android App 集成 |

---

## 10. 外部资源

- 小米 OpenVela AI 硬件赛道指南：`openvela-docs-github/zh-cn/contest_2026/ai_hardware/`
- ai_agent 快速开始：`openvela-docs-github/zh-cn/contest_2026/ai_hardware/ai_agent_quickstart.md`
- 官方示例：`packages_demos/mini_memo/`、`packages_demos/ai_chat/`
- Android 伴侣 App：`com.agent.coapp/`
- 开发板文档：`feishu_docs_full/`

---

## 11. 明日编译与验证 Checklist

### 11.1 环境准备

- [ ] 将 `/workspace/vendor_allwinnertech/apps/ai_radio_console` 复制到完整 OpenVela 源码树对应位置。
- [ ] 确认 `packages_ai_agent`、`packages_demos` 版本与源码树一致。
- [ ] 安装/检查 Android Studio、JDK 17、Gradle 8.5 可用。

### 11.2 开发板编译

- [ ] 复制 defconfig：`cp packages_ai_agent/defconfigs/gemini-s1/gemini-s1_defconfig vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/defconfig`
- [ ] 运行补丁：`bash packages_ai_agent/fix_gemini_s1.sh`
- [ ] `menuconfig` 中确认：
  - [ ] `Application Configuration → AI Radio Console (Gemini-S1 HAT)` = **y**
  - [ ] `Application Configuration → AI Radio Console → Use ai_agent voice_channel for PTT+ASR` = **y**（首选）
  - [ ] `Application Configuration → AI Radio Console → Enable built-in REST/WebSocket server` = **y**
  - [ ] `Application Configuration → Packages → Vela AI Agent` = **y**
  - [ ] `Vela AI Agent → Enable BLE GATT data channel` = **y**
  - [ ] `Vela AI Agent → Enable REST API endpoints` = **y**
- [ ] 执行完整编译：`./build.sh ... -e -Wno-error -j$(nproc)`
- [ ] `source envsetup.sh && lunch_nuttx && pack`

### 11.3 开发板首次运行

- [ ] 烧录固件，上电进入 nsh。
- [ ] 创建目录：`mkdir -p /data/agent/config /data/agent/skills /data/agent/memory/radio`
- [ ] 确认中文字体：`ls /data/res/fonts/MiSans-Normal.ttf`
- [ ] 启动 ai_agent：`ai_agent`
- [ ] 启动控制台（带 3 秒 PTT 自检）：`ai_radio_console --ptt-selftest 3000`
- [ ] 观察日志：应看到 `[radio_rec] voice_channel`、PTT key/unkey、ASR 结果或“未识别到语音”。

### 11.4 Android App 编译与联调

- [ ] 在 Android Studio 中打开 `/workspace/Veeridio`。
- [ ] 同步 Gradle，编译 Debug APK。
- [ ] 安装到手机，授予蓝牙与位置权限。
- [ ] BLE 扫描并连接 `AI Radio`，发送 WiFi 配置。
- [ ] 收到 `BleStatusResponse` 含 `network=true` 与 IP 后，检查 App 自动保存 IP:28790。
- [ ] 进入“日志”页，点击刷新，应能拉取 `/api/logs`。
- [ ] 进入“PTT”页，按住按钮，开发板应 key PTT；松开应生成日志并推送 WebSocket。

### 11.5 问题兜底

- [ ] 若 `voice_channel` 录音异常，切到 nxrecorder 后端重编验证（需要 HTTP ASR relay）。
- [ ] 若 App 连不上 28790，先用 `telnet 板子IP 28790` 确认端口监听。
- [ ] 若中文显示方块，检查 `/data/res/fonts/MiSans-Normal.ttf` 是否存在并重新打包资源。

### 11.6 赛后/后续

1. 根据真机反馈调整 LINEIN 增益 / 采样率。
2. 准备演示脚本与录屏。
3. 整理双赛（哈工大 + 小米 OpenVela）提交材料清单。

---

## 12. 给下任的话

代码已通过主机语法检查，但还没在真机上跑过。双后端都保留了，真机建议先用 `voice_channel`，出问题再切 `nxrecorder`。环境没完整源码树时，先用 `gcc -fsyntax-only` 兜底，别等到上板才发现低级语法错误。祝顺利。
