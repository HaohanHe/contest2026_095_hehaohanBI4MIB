# AI 电台控制台（openvela/R528 Gemini-S1）结构化建议

> 生成时间：2026-07-06
> 调研范围：
> - `/workspace/feishu_docs_full` 飞书 Wiki 文档 14 篇
> - `/workspace/openvela_build` 当前工程（只读，未触发新编译）
> - `/workspace/openvela_frameworks` 顶层仓库与子仓库列表

---

## 1. 项目目标一句话

在 Allwinner R528 Gemini-S1 开发板上，基于 openvela（NuttX）构建一个带 SPI 屏+触摸的 AI 电台控制台：通过 WiFi 连接云端/本地 AI 服务，实现语音交互（麦克风采集 → ASR → LLM → TTS → 扬声器播放）、频道/模式/PTT 触控 UI，并支持 JS/QuickApp 类小程序扩展。

---

## 2. 必须保留/启用的功能清单及理由

| 功能域 | 当前状态 | 是否必须保留/启用 | 理由 |
|--------|----------|-------------------|------|
| **WiFi STA** | `CONFIG_IEEE80211_REALTEK_WIFI=y`、`CONFIG_IEEE80211_REALTEK_WIFI_RTL8733BS=y`、`CONFIG_WIRELESS_WAPI=y` | ✅ 保留 | AI 电台需联网调用 ASR/LLM/TTS API，Realtek RTL8733BS 是板载 WiFi。 |
| **蓝牙 BLE** | `CONFIG_BLUETOOTH=y`、`CONFIG_BLUETOOTH_BLE_SUPPORT=y`、`CONFIG_BT=y`、`CONFIG_BT_SAMPLE=y` | ✅ 保留 | 可用于遥控器、耳机、Beacon 或未来扩展；ZBlue 协议栈已启用。 |
| **LRADC 按键** | `CONFIG_DRIVERS_LRADC=y`、`CONFIG_INPUT_BUTTONS=y` | ✅ 保留 | 板载物理按键输入通道，PTT/频道切换的硬件入口。 |
| **Audio 录放** | `CONFIG_AUDIO=y`、`CONFIG_R528_AUDIO=y`、`CONFIG_AW_DRIVERS_AUDIO=y`、`CONFIG_SYSTEM_NXRECORDER=y`、`CONFIG_SYSTEM_NXPLAYER=y` | ✅ 保留 | AI 语音交互核心：麦克风采集 + 扬声器/耳机播放。 |
| **SHTC3 温湿度** | `CONFIG_SENSORS_SHTC3=y` | ✅ 保留 | 板载 I2C 0x70，桌面可展示环境信息。 |
| **LTR553 光感/接近** | `CONFIG_SENSORS_LTR553=y` | ✅ 保留 | 板载 I2C 0x23，支持自动亮度、接近感应。 |
| **WS2812 RGB LED** | `CONFIG_WS2812=y`、`CONFIG_LED_RGB_WS2812=y`、`CONFIG_LED_RGB_TEST=y` | ✅ 保留 | 状态指示（PTT/AI 状态/告警）。 |
| **LCD + 触摸** | `CONFIG_LCD=y`、`CONFIG_LCD_ILI9341=y`、`CONFIG_GT911_IIC_TOUCH=y`、`CONFIG_INPUT_TOUCHSCREEN=y`、`CONFIG_GRAPHICS_LVGL=y` | ✅ 保留 | AI 电台控制台的显示与触控交互基础。 |
| **LVGL 桌面 luncher_mini** | `CONFIG_LUNCHER_MINI_APP=y` | ⚠️ 按需保留 | 当前桌面自动启动，展示时间/传感器/LED 控制；若 AI 电台为单应用形态，可禁用以节省资源，或保留作为系统桌面入口。 |
| **AI Radio 应用 airadio** | `CONFIG_AIRADIO_APP=y` | ✅ 必须启用 | 项目核心 UI（频率/模式/PTT/ASR/LLM 状态）。 |
| **网络协议栈** | `CONFIG_NET=y`、TCP/UDP/ICMP/DNS/DHCP 已启用 | ✅ 保留 | HTTP/HTTPS API 调用基础。 |
| **HTTP/HTTPS 客户端** | `CONFIG_UTILS_CURL=y`、`CONFIG_MBEDTLS_THREADING_*=y`、`CONFIG_LIB_CARES=y` | ✅ 保留 | 调用云端 LLM/TTS/ASR API。 |
| **JSON 解析** | `CONFIG_NETUTILS_CJSON=y` | ✅ 保留 | 解析 LLM API 请求/响应。 |
| **异步 I/O** | `CONFIG_LIBUV=y`、`CONFIG_LIBUV_EXTENSION=y` | ✅ 保留 | 方便整合网络、音频、UI 事件循环。 |
| **uORB** | `CONFIG_UORB=y` | ✅ 保留 | 传感器数据发布/订阅标准机制。 |
| **ADB + 串口** | `CONFIG_SYSTEM_ADBD=y`、`CONFIG_UART2_SERIAL_CONSOLE=y`、波特率 1500000 | ✅ 保留 | 调试与日志必备。 |
| **音频测试 audio_test** | `CONFIG_AUDIO_TEST is not set` | ⚠️ 建议启用 | 用于独立验证麦克风和扬声器通路，开发阶段强烈建议打开。 |
| **JS/QuickApp 运行时** | `CONFIG_INTERPRETERS_QUICKJS is not set`、`CONFIG_INTERPRETERS_DUKTAPE is not set`、`CONFIG_INTERPRETERS_WAMR/WASM3/TOYWASM is not set` | ⚠️ 按需启用 | 若产品需要小程序扩展，需至少启用 QuickJS/WAMR；当前未启用。 |
| **ASR/TTS/LLM 后端** | 当前仅有 UI 标签，无实际后端 | ❌ 当前缺失，必须补充 | 这是“AI 电台”的灵魂，需新增或集成。 |

---

## 3. 当前代码/配置的明显问题与修复建议

### 3.1 编译与打包环境问题
- **现象**：`ps` 显示当前仍有 `make -j32` 进程在运行，`build.log` 正在下载 mbedtls/speexdsp 等依赖；`pack.log` 出现多条错误：
  - `busybox: command not found`（pack_img.sh 依赖）
  - `boot0 checksum fail`
  - `cannot stat .../ap.fex`
  - `/workspace/openvela_build/vendor/allwinnertech/lichee/tools/tool/dragon: cannot execute binary file: Exec format error`
- **影响**：打包出的 `rtos_nsh_r528s3-gemini-s1_uart0_128Mnand.img` 可能不完整或无法启动。
- **建议**：
  1. 等待当前编译完成后再打包，**不要并行触发新编译**。
  2. 在 Ubuntu 22.04 环境中安装 `busybox`：`sudo apt install busybox`。
  3. 检查/替换 `vendor/allwinnertech/lichee/tools/tool/dragon` 为与宿主机架构匹配的二进制（当前为错误架构）。
  4. 确认 `ap.fex` 来源：若无需 AP 分区，应在打包配置中关闭；若需要，需先生成并放置到 `board/r528s3/gemini-s1_nand/configs/ap.fex`。
  5. 检查 boot0/uboot 是否已经预先编译：`mboot0` / `muboot`。

### 3.2 AI Radio 应用未自动启动
- **现象**：`vendor/allwinnertech/boards/r528/r528s3-gemini-s1/src/etc/init.d/rcS.nsh` 中通过 `#ifdef CONFIG_LUNCHER_MINI_APP` 自动启动 `luncher_mini &`，但**没有启动 `airadio`**。
- **影响**：烧录后只会进入 luncher_mini 桌面，AI 电台 UI 需要手动在 NSH 输入 `airadio` 才能运行。
- **建议**：
  - 若 AI 电台是主应用：在 `rcS.nsh` 中增加 `#ifdef CONFIG_AIRADIO_APP` → `airadio &`；并考虑是否还需要自动启动 `luncher_mini`（二者都初始化 LVGL + 占 `/dev/input0`、`/dev/lcd0`，同时运行会冲突）。
  - 若保留桌面：`luncher_mini` 应增加一个“AI 电台”图标入口，点击后启动 `airadio`。

### 3.3 重复的 airadio 代码
- **现象**：存在两份几乎相同的源码：
  - `vendor/allwinnertech/apps/airadio/airadio.c`（当前启用，使用 `CONFIG_AIRADIO_APP_INPUT_DEVPATH`）
  - `apps/examples/airadio/airadio.c`（未启用，使用 `CONFIG_EXAMPLES_AIRADIO_INPUT_DEVPATH`）
- **影响**：后续修改 UI 或修复 bug 时容易漏改一份，造成维护负担；`defconfig` 中同时出现 `CONFIG_EXAMPLES_AIRADIO is not set` 与 `CONFIG_AIRADIO_APP=y`，命名易混淆。
- **建议**：删除 `apps/examples/airadio/`，统一维护 `vendor/allwinnertech/apps/airadio/`；如必须保留示例，应将其重构为调用 vendor 版本的薄包装。

### 3.4 无实际 ASR/TTS/LLM 后端
- **现象**：`airadio.c` 中仅有静态文本标签 `ASR: ready`、`LLM: standby`，PTT 按钮只有 UI 状态变化，未调用任何音频采集或网络 API。
- **影响**：产品只能展示 UI，无法完成“按住说话 → 识别 → 推理 → 播报”闭环。
- **建议**：
  1. **音频采集**：使用 `nxrecorder` 或 `/dev/audio/pcmX` 录制 16kHz/16bit/单声道 PCM。
  2. **ASR**：R528 算力有限（双核 A7 1.2GHz + 128MB DDR），建议走云端 ASR（百度/讯飞/阿里）；若必须离线，可评估 tinyML 模型（如 TensorFlow Lite Micro / onnxruntime）但需显著裁剪。
  3. **LLM**：复用飞书文档提到的 `deepseek_demo` 思路——通过 `curl + cJSON + mbedtls` 调用 DeepSeek / OpenAI 兼容 API；当前 `curl/mbedtls/cjson` 已启用，具备基础条件。
  4. **TTS**：云端 TTS 返回 PCM/MP3，通过 `nxplayer` 或 `aw-alsa-lib` 播放；离线 TTS 同样需要轻量模型。
  5. **推荐参考路径**：`vendor/allwinnertech/apps/audio_test/` 可作为麦克风和播放器的封装参考。

### 3.5 JS/QuickApp 小程序未启用
- **现象**：`.config` 中 `CONFIG_INTERPRETERS_QUICKJS`、`CONFIG_INTERPRETERS_WAMR`、`CONFIG_INTERPRETERS_WASM3`、`CONFIG_INTERPRETERS_TOYWASM`、`CONFIG_INTERPRETERS_DUKTAPE` 全部未设置；工程中没有现成的 QuickApp 框架。
- **影响**：无法运行业界常见的 QuickApp/小程序形态应用。
- **建议**：
  - 若只需要简单脚本扩展：启用 `CONFIG_INTERPRETERS_QUICKJS=y`（QuickJS 体积小、ES2020 支持好）。
  - 若需要完整小程序框架：从 openvela frameworks 的 `frameworks_runtimes_services_*`（am/pm/wm/system_server）入手构建多应用框架，并引入 JS/Wasm 运行时；但这是一个较大工程，需评估人力。

### 3.6 WiFi 自动连接依赖预置配置文件
- **现象**：`rcS.nsh` 中 `if [ -f /data/etc/wifi/wapi.conf ]` 才启动 `start_wifi.sh`；`defconfig` 中 `CONFIG_NETINIT_WAPI_SSID=""` 为空。
- **影响**：首次开机不会自动联网，AI 服务不可用。
- **建议**：
  - 生产时在 `usrdata` 或 ROMFS 中预置 `wapi.conf`。
  - 或增加配网流程（BLE/WiFi AP 配网）并在 UI 中提供 SSID/密码输入界面。

### 3.7 蓝牙启动延迟
- **现象**：`rcS.nsh` 中 `sleep 8` 后才启动 `bluetoothd`。
- **影响**：拖慢开机到可用状态的时间。
- **建议**：将蓝牙初始化改为事件驱动或缩短/移除硬编码 sleep，确保 WiFi/AI 应用优先启动。

### 3.8 栈大小与稳定性
- **现象**：`AIRADIO_APP_STACKSIZE=40960`、`LUNCHER_MINI_APP_STACKSIZE=102400`、系统 `dumpstack` 已启用。
- **影响**：若后续在 airadio 中增加网络 + 音频 + JSON 解析，40KB 栈可能吃紧；飞书文档也提到“默认 stacksize 较小会导致运行崩溃”。
- **建议**：
  - AI 电台任务栈建议预留 **64KB ~ 128KB**。
  - 保持 `CONFIG_SCHED_BACKTRACE=y`、`CONFIG_SYSTEM_DUMPSTACK=y`、`CONFIG_ALLSYMS=y` 以便于崩溃定位。

---

## 4. 完整构建 → 打包 → 刷机命令流

> ⚠️ **注意**：截至调研时，`/workspace/openvela_build` 中仍有 `make -j32` 编译进程在运行。请等待其完成或确认失败后，再执行新的构建/打包命令，避免资源冲突。

### 4.1 前置环境（Ubuntu 22.04）
```bash
sudo apt install \
  bison flex gettext texinfo libncurses5-dev libncursesw5-dev xxd \
  git gperf automake libtool build-essential gperf genromfs \
  libgmp-dev libmpc-dev libmpfr-dev libisl-dev binutils-dev libelf-dev \
  libexpat1-dev gcc-multilib g++-multilib picocom u-boot-tools util-linux \
  dfu-util libx11-dev libxext-dev net-tools pkgconf unionfs-fuse zlib1g-dev \
  libusb-1.0-0-dev libv4l-dev libuv1-dev npm nodejs nasm yasm libdivsufsort-dev \
  libc++-dev libc++abi-dev libprotobuf-dev protobuf-compiler protobuf-c-compiler mtools \
  busybox
```

### 4.2 构建 NuttX 主系统
```bash
cd /workspace
./openvela_build/tools/build.sh \
  vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay \
  -j$(nproc)
```

- 若需调整配置：
```bash
./openvela_build/tools/build.sh \
  vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay \
  menuconfig
```

### 4.3 打包生成固件镜像
```bash
cd /workspace/openvela_build/vendor/allwinnertech/lichee
source envsetup.sh
lunch_nuttx   # 按提示选择 r528s3-gemini-s1（或直接用 lunch_nuttx r528s3-gemini-s1）
pack
```

- 正常输出镜像示例：`rtos_nsh_r528s3-gemini-s1_uart0_128Mnand.img`
- 输出目录：`/workspace/openvela_build/vendor/allwinnertech/lichee/out/r528s3/gemini-s1_nand/image/`
- 若需要安全签名：
```bash
pack -s
```

### 4.4 刷机
当前飞书文档未公开详细软件烧录指南。按全志 R528 常规流程：
1. 使用 USB 线连接开发板与 PC。
2. 让开发板进入烧录模式（通常按住特定 BOOT/RECOVERY 键上电，具体以硬件手册为准）。
3. 使用全志烧录工具（如 **PhoenixSuit / Livesuit**）加载上一步生成的 `.img` 文件烧录。
4. 烧录完成后重启，使用串口工具连接 UART2，波特率 **1500000**；或使用 `adb logcat` / `adb shell` 调试。

---

## 5. 可从 openvela_frameworks 引入的能力建议

`/workspace/openvela_frameworks` 是一个顶层超级仓库（super repository），通过子仓库方式组织；**当前子仓库未克隆到本地**，仅包含 README 与 CMake 入口。若需引入，应通过 `git submodule` 或手动克隆对应 `frameworks_*` 仓库。

| 子仓库 | 对 AI 电台的价值 | 建议引入优先级 |
|--------|------------------|----------------|
| **frameworks_bluetooth** | 提供更高层 BLE/GATT API，便于做遥控器、耳机、Beacon 配网。 | 中 |
| **frameworks_graphics_uikit** | 基于 LVGL 的 video、扩展字体管理、demo，可提升 UI 组件化能力。 | 中 |
| **frameworks_multimedia_media** | 多媒体播放/录制、音频焦点管理、音频策略；适合整合 ASR/TTS 语音流。 | **高** |
| **frameworks_multimedia_media_pfw** | 通用状态机框架，可用于音频状态机（录音中/播放中/空闲）。 | 中 |
| **frameworks_runtimes_services_am** | 多应用框架下的 Activity 生命周期管理。 | 低（需配合 pm/wm） |
| **frameworks_runtimes_services_pm** | 应用包管理（安装/卸载/查询），是小程序生态基础。 | 低 |
| **frameworks_runtimes_services_wm** | 窗口管理，支持多窗口/多应用显示。 | 低 |
| **frameworks_runtimes_services_system_server** | 启动/管理多媒体、亮度、包管理等服务。 | 中 |
| **frameworks_runtimes_typescript_ts2native** | 若小程序使用 TypeScript，可转 Native。 | 低 |
| **frameworks_runtimes_typescript_ts2wasm** | TypeScript 转 Wasm，提升脚本性能。 | 低 |
| **frameworks_runtimes_wasm** | Wasm 运行时扩展 API，便于把现有 C/C++ 算法封装给脚本调用。 | 中 |
| **frameworks_system_utils** / **frameworks_system_utils_uv** | kvdb、trace、libuv 风格的数据库/网络接口；可替换或增强当前直接调用 POSIX API 的方式。 | 中 |
| **frameworks_system_topics** | 标准化 uORB topic 定义，便于传感器/系统事件统一发布。 | 中 |
| **frameworks_system_ota** | OTA 升级与 AVB 验签，产品化必备。 | 中 |
| **frameworks_security** / **frameworks_security_optee_vela** | 安全存储 WiFi 密码、API Key 等敏感信息。 | 中 |
| **frameworks_system_vibrator** | 震动反馈，增强交互体验。 | 低 |

**落地建议**：
1. **短期**：优先引入 `frameworks_multimedia_media` 规范语音流管理；用 `frameworks_system_utils` 的 kvdb 保存配网信息。
2. **中期**：若要做小程序生态，再整体引入 `frameworks_runtimes_services_*` + QuickJS/WAMR。
3. **注意**：引入 frameworks 通常需要在 `nuttx/.config` 中开启对应 Kconfig，并在 `CMakeLists.txt` 中将其加入构建；需确认与现有 `vendor/allwinnertech` 驱动无符号冲突。

---

## 6. 关键文件路径、配置项、命令速查表

### 6.1 关键文件路径

| 用途 | 路径 |
|------|------|
| AI 电台应用源码 | `/workspace/openvela_build/vendor/allwinnertech/apps/airadio/airadio.c` |
| AI 电台应用构建配置 | `/workspace/openvela_build/vendor/allwinnertech/apps/airadio/Kconfig` |
| 重复示例应用源码 | `/workspace/openvela_build/apps/examples/airadio/airadio.c` |
| LVGL 桌面源码 | `/workspace/openvela_build/vendor/allwinnertech/apps/luncher_mini/luncher_mini.c` |
| LVGL 桌面 RGB LED 控制 | `/workspace/openvela_build/vendor/allwinnertech/apps/luncher_mini/lv_demo_panel_rgb_control.c` |
| 音频测试应用 | `/workspace/openvela_build/vendor/allwinnertech/apps/audio_test/main.c` |
| 板级 defconfig | `/workspace/openvela_build/vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/defconfig` |
| 当前生效 .config | `/workspace/openvela_build/.config` |
| 开机脚本 | `/workspace/openvela_build/vendor/allwinnertech/boards/r528/r528s3-gemini-s1/src/etc/init.d/rcS.nsh` |
| 传感器驱动（SHTC3） | `/workspace/openvela_build/vendor/allwinnertech/chips/r528/drivers/rtos-hal/hal/source/sensor/temperature/shtc3.c` |
| 传感器驱动（LTR553） | `/workspace/openvela_build/vendor/allwinnertech/chips/r528/drivers/rtos-hal/hal/source/sensor/als/ltr553.c` |
| 音频 HAL | `/workspace/openvela_build/vendor/allwinnertech/chips/r528/components/audio/` |
| WiFi 驱动 | `/workspace/openvela_build/vendor/allwinnertech/boards/r528/drivers/realtek_ieee80211/` |
| 构建脚本 | `/workspace/openvela_build/tools/build.sh` |
| 环境设置脚本 | `/workspace/openvela_build/vendor/allwinnertech/lichee/tools/scripts/envsetup.sh` |
| 一键 source 入口 | `/workspace/openvela_build/vendor/allwinnertech/lichee/envsetup.sh` |
| 打包脚本（函数） | `/workspace/openvela_build/vendor/allwinnertech/lichee/tools/scripts/envsetup.sh` 中的 `pack()` |
| 打包输出目录 | `/workspace/openvela_build/vendor/allwinnertech/lichee/out/r528s3/gemini-s1_nand/image/` |
| 顶层 frameworks 索引 | `/workspace/openvela_frameworks/README.md` / `README_zh-cn.md` |

### 6.2 关键 Kconfig / 配置项

| 配置项 | 当前值 | 说明 |
|--------|--------|------|
| `CONFIG_AIRADIO_APP` | `y` | 启用 AI 电台应用 |
| `CONFIG_AIRADIO_APP_PROGNAME` | `"airadio"` | NSH 命令名 |
| `CONFIG_AIRADIO_APP_STACKSIZE` | `40960` | 建议视音频+网络需求加大 |
| `CONFIG_LUNCHER_MINI_APP` | `y` | 启用 LVGL 桌面 |
| `CONFIG_LUNCHER_MINI_APP_STACKSIZE` | `102400` | 桌面任务栈 |
| `CONFIG_GRAPHICS_LVGL` | `y` | LVGL 图形库 |
| `CONFIG_INPUT_TOUCHSCREEN` | `y` | 触摸屏输入 |
| `CONFIG_LCD_ILI9341` | `y` | SPI LCD 驱动 |
| `CONFIG_GT911_IIC_TOUCH` | `y` | 触控 IC 驱动 |
| `CONFIG_IEEE80211_REALTEK_WIFI_RTL8733BS` | `y` | 板载 WiFi |
| `CONFIG_BLUETOOTH` / `CONFIG_BT` | `y` | 蓝牙协议栈 |
| `CONFIG_AUDIO` / `CONFIG_R528_AUDIO` | `y` | 音频子系统 |
| `CONFIG_SENSORS_SHTC3` | `y` | 温湿度传感器 |
| `CONFIG_SENSORS_LTR553` | `y` | 光感/接近传感器 |
| `CONFIG_WS2812` / `CONFIG_LED_RGB_WS2812` | `y` | RGB LED |
| `CONFIG_UTILS_CURL` | `y` | HTTP 客户端 |
| `CONFIG_NETUTILS_CJSON` | `y` | JSON 解析 |
| `CONFIG_LIBUV` | `y` | 异步 IO |
| `CONFIG_INTERPRETERS_QUICKJS` | 未设置 | 需要 JS 时启用 |
| `CONFIG_INTERPRETERS_WAMR` | 未设置 | 需要 Wasm 时启用 |
| `CONFIG_AUDIO_TEST` | 未设置 | 建议开发阶段启用 |
| `CONFIG_GEMINI_S1_NSH` | `y` | 使用 NSH 启动脚本 |

### 6.3 调试与运行命令速查

| 操作 | 命令 |
|------|------|
| 启动 AI 电台 UI | `nsh> airadio &` |
| 启动桌面 | `nsh> luncher_mini &` |
| 连接 WiFi | `wapi mode wlan0 2`、`wapi scan wlan0`、`wapi psk wlan0 "密码" 3`、`wapi essid wlan0 "SSID" 1`、`renew wlan0` |
| 查看 IP | `ipconfig` |
| ADB 日志 | `adb logcat` |
| ADB 进入 shell | `adb shell` |
| 串口参数 | UART2、1500000 baud、8N1 |
| 查看 backtrace | `nsh> ps` → `nsh> backtrace <pid>` |
| 音频录制测试 | `nsh> audio_test record 5`（需先启用 `CONFIG_AUDIO_TEST`） |
| 音频回环测试 | `nsh> audio_test loopback 10 16000 16 1 1` |

---

## 7. 下一步行动建议（按优先级）

1. **完成并稳定当前编译**：确认 `make` 进程结束后再打包；解决 `busybox` / `dragon` / `boot0 checksum` 等打包环境问题。
2. **确定产品形态**：单应用（airadio 自启动）还是桌面入口（luncher_mini + airadio）？相应修改 `rcS.nsh`。
3. **清理重复代码**：移除 `apps/examples/airadio/`，统一 vendor 版本。
4. **补齐 AI 后端**：在 `airadio.c` 中接入 `nxrecorder`/`nxplayer` + `curl` + `cJSON`，走云端 ASR/LLM/TTS；可先复用 `vendor/allwinnertech/apps/audio_test` 的音频封装。
5. **启用开发辅助功能**：打开 `CONFIG_AUDIO_TEST`，方便验证麦克风/扬声器。
6. **评估 JS/QuickApp 需求**：若需要，启用 QuickJS/WAMR 并规划多应用框架；若暂不需要，明确在产品路线图中标注。
7. **引入 frameworks**：短期优先 `frameworks_multimedia_media` 与 `frameworks_system_utils`；长期按需引入 runtimes/ota/security。
