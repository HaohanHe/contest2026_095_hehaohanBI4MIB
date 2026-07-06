# AI 电台控制台项目 —— 结构化建议文档

> 生成时间：2026-07-06
> 研究范围：
> - 飞书文档：`/workspace/feishu_docs_full`（14 篇 Markdown）
> - 项目现状：`/workspace/openvela_build`（openvela / NuttX 源码树，当前作为 `nuttx` 目录使用）
> - 额外参考：`/workspace/openvela_frameworks`（frameworks 空壳仓库）
> - 当前后台任务：`make` 编译任务正在运行，本文档仅做只读研究与记录。

---

## 1. 执行摘要

AI 电台控制台项目基于 **Allwinner R528（Gemini-S1）** + **openvela（NuttX RTOS）**，目标是在 128 MB DDR3 的嵌入式平台上运行一个包含 LVGL UI、WiFi/蓝牙联网、传感器、音频和 AI 大模型交互能力的电台控制台。

**当前关键状态：**

- 已存在 `vendor/allwinnertech/apps/airadio/` 自定义 AI 电台 LVGL 应用，代码结构清晰，包含频率、模式、PTT 按钮和 ASR/LLM 状态占位。
- 当前 `.config` 基于 `nsh_minidisplay` 配置，已启用 `CONFIG_AIRADIO_APP=y`、`CONFIG_LUNCHER_MINI_APP=y`。
- 最近一次完整 `make` 链接**已成功**（`build2.log`、`build3.log` 末尾显示 `LD: nuttx` 成功，生成 `vela.bin`）。
- 当前 `nuttx` 目录（即 `/workspace/openvela_build`）下存在 `vela.bin`（约 1 MB），但**不存在** `nuttx.bin` / `nuttx.elf`。
- 打包脚本 `pack.sh` 原生仅支持 `r528s3-evb4` 和 `r528s3-x4b`，不支持 `r528s3-gemini-s1`；`pack.log` 显示实际是直接调用了 `pack_img.sh` 对 `gemini-s1_nand` 进行打包。
- 最近一次打包（`pack.log`）出现：
  - `busybox: command not found`（3 处）
  - `boot0 checksum fail`
  - 缺少 `ap.fex`（`cp: cannot stat '.../ap.fex': No such file or directory`）
  - `dragon: cannot execute binary file: Exec format error`
  - 最终未生成可靠 IMG。
- `openvela_build/frameworks/` 中 `multimedia/connectivity/graphics` 仅含空壳 Makefile/Kconfig，缺少 `frameworks_multimedia_media`、`frameworks_system_utils` 等实际子仓库代码。

**当前最紧迫任务：**

1. 修复打包环境（安装 `busybox`、准备正确架构的 `dragon` 工具、重新生成/校验 `boot0`）。
2. 确保 `nuttx.bin` / `ap.fex` 被正确复制到打包目录。
3. 决定是否引入 `open-vela/frameworks` 中的多媒体/系统能力。

---

## 2. 必须保留 / 恢复的功能清单及理由

| 功能 | 当前状态 | 必须保留/恢复理由 | 关键配置项 |
|------|----------|-------------------|------------|
| **WiFi（Realtek RTL8733BS）** | `CONFIG_IEEE80211_REALTEK_WIFI=y`、`CONFIG_IEEE80211_REALTEK_WIFI_RTL8733BS=y`、`CONFIG_WIFI_TEST=y`、`CONFIG_WIRELESS_WAPI=y` 已启用 | AI 电台需联网获取 AI 服务、OTA、电台流媒体等；Realtek SDIO WiFi 是 Gemini-S1 官方网络方案 | `CONFIG_DRIVERS_IEEE80211`、`CONFIG_IEEE80211_REALTEK_WIFI`、`CONFIG_IEEE80211_REALTEK_WIFI_RTL8733BS`、`CONFIG_WIRELESS_WAPI`、`CONFIG_WIFI_TEST` |
| **蓝牙 BLE/Classic（Zblue）** | `CONFIG_BLUETOOTH=y`、`CONFIG_BT=y`、`CONFIG_BT_CLASSIC=y` 已启用；`CONFIG_BT_START` 未启用 | 文档明确蓝牙用于遥控器、耳机、低功耗外设；Zblue 协议栈已集成 | `CONFIG_BLUETOOTH`、`CONFIG_BT`、`CONFIG_BT_CLASSIC`、`CONFIG_BT_H4`、`CONFIG_BT_START`（如需要启动脚本） |
| **luncher_mini（LVGL 示例桌面）** | `CONFIG_LUNCHER_MINI_APP=y` 已启用 | 可作为系统启动器或参考实现；与 airadio 共用 LVGL/Touch/LED 基础设施 | `CONFIG_LUNCHER_MINI_APP`、`CONFIG_GRAPHICS_LVGL` |
| **airadio（AI 电台主应用）** | `CONFIG_AIRADIO_APP=y` 已启用 | 项目核心目标应用，提供 LVGL 电台 UI、PTT、模式切换 | `CONFIG_AIRADIO_APP`、`CONFIG_AIRADIO_APP_STACKSIZE=40960` |
| **传感器（SHTC3 + LTR553）** | `CONFIG_SENSORS=y`、`CONFIG_SENSORS_SHTC3=y`、`CONFIG_SENSORS_LTR553=y` 已启用 | 温湿度/光感/接近数据是 luncher_mini 已有功能，也是 AI 电台环境感知输入 | `CONFIG_SENSORS_SHTC3`、`CONFIG_SENSORS_LTR553`、`CONFIG_R528_TWI0`/`TWI2`、`CONFIG_UORB` |
| **LRADC 按键** | `CONFIG_DRIVERS_LRADC=y` 启用，但 `CONFIG_R528_LRADC` 未启用 | 文档指出 LRADC 是 Gemini-S1 唯一按键接口；PTT/频率调节等物理按键依赖它 | `CONFIG_DRIVERS_LRADC`、`CONFIG_R528_LRADC`、`CONFIG_INPUT_BUTTONS` |
| **音频（Audio Codec + aw-alsa-lib）** | `CONFIG_AUDIO=y`、`CONFIG_R528_AUDIO=y`、`CONFIG_AW_AUDIO_CODEC=y` 已启用 | 电台必须播放/录制音频；R528 内置 Codec 已通过 ALSA 适配层接入 | `CONFIG_AUDIO`、`CONFIG_R528_AUDIO`、`CONFIG_AW_AUDIO_CODEC`、`CONFIG_SYSTEM_NXPLAYER`/`NXRECORDER` |
| **显示（ILI9341 SPI LCD + GT911 触摸）** | `CONFIG_LCD_ILI9341=y`、`CONFIG_GT911_IIC_TOUCH=y`、`CONFIG_LV_USE_NUTTX_LCD=y` 已启用 | airadio / luncher_mini 的 UI 入口 | `CONFIG_LCD`、`CONFIG_LCD_ILI9341`、`CONFIG_GT911_IIC_TOUCH`、`CONFIG_INPUT_TOUCHSCREEN` |
| **WS2812 RGB LED** | `CONFIG_WS2812=y`、`CONFIG_LED_RGB_WS2812=y`、`CONFIG_LED_RGB_TEST=y` 已启用 | luncher_mini 灯光控制示例，可作为电台状态指示灯 | `CONFIG_WS2812`、`CONFIG_LED_RGB_WS2812` |
| **ADB / 串口调试** | `CONFIG_SYSTEM_ADBD=y`、`CONFIG_UART2_SERIAL_CONSOLE=y`、`CONFIG_UART2_BAUD=1500000` 已启用 | 官方推荐的调试手段（UART2 1500000 + adb logcat/adb shell） | `CONFIG_SYSTEM_ADBD`、`CONFIG_UART2_SERIAL_CONSOLE`、`CONFIG_UART2_BAUD=1500000` |
| **Backtrace / Dumpstack** | `CONFIG_SCHED_BACKTRACE=y`、`CONFIG_SYSTEM_DUMPSTACK=y` 已启用 | 嵌入式崩溃调试必备；文档专门一章介绍 | `CONFIG_SCHED_BACKTRACE`、`CONFIG_SYSTEM_DUMPSTACK`、`CONFIG_DEBUG_SYMBOLS` |
| **AI / LLM（DeepSeek 能力）** | `deepseek_demo` 目录**不存在**于 `vendor/allwinnertech/apps/` | 飞书文档明确存在 `deepseek_demo`；AI 电台的语音助手/LLM 交互需要恢复或重建 | 需新增/恢复 `CONFIG_DEEPSEEK_DEMO`（当前无） |

---

## 3. 当前代码 / 配置的明显问题与修复建议

### 3.1 构建链接风险（历史失败记录，需持续关注）

`/workspace/build.log` 早期曾出现链接失败，主要未定义符号：

```text
fs/yaffs/yaffs_vfs.c: undefined reference to `yaffsfs_GetLastError'
apps/system/libuv/.../loop.c: undefined reference to `uv__process_init'
apps/graphics/lvgl/lvgl/...: undefined reference to `ASSERT'
vendor/allwinnertech/apps/factory_test/factory_test.c: undefined reference to `check_bt_valid'
vendor/allwinnertech/apps/wifi_test/test_sdio_wifi.c: undefined reference to `wifi_on', `rltk_wlan_running', `wext_set_*'
fatfs/fatfs_vfs.c: undefined reference to `SS'
```

最近 `build2.log` / `build3.log` 已成功链接并生成 `vela.bin`，说明部分问题可能已通过配置调整或源码修复解决，但以下风险点仍需在后续构建中监控：

1. **LVGL `ASSERT` 未定义**
   - 原因：LVGL 的断言宏默认映射到 `ASSERT()`，但当前 `.config` 中 `CONFIG_DEBUG_ASSERTIONS` **未启用**。
   - 修复：在 `make menuconfig` 中启用 `CONFIG_DEBUG_ASSERTIONS=y`。

2. **YAFFS `yaffsfs_GetLastError` 未定义**
   - 原因：`fs/yaffs/yaffs_vfs.c` 被编译进镜像，但对应实现可能未完整参与编译或 LTO 剥离。
   - 修复：检查 `File Systems -> YAFFS` 依赖是否完整；确认 `CONFIG_FS_YAFFS` 启用时，底层 `yaffsfs.c` 已正确加入构建。

3. **libuv `uv__process_init` 未定义**
   - 原因：NuttX 版 libuv 补丁中定义了该函数，但 patch 可能未应用或相关文件未编译。
   - 修复：确认 `apps/system/libuv/0001-libuv-port-for-nuttx.patch` 已应用；检查 `CONFIG_LIBUV_EXTENSION=y` 是否启用。

4. **WiFi 符号未定义**
   - 原因：Realtek 预编译库 `librtl8733bs.a` 与当前 `CONFIG_LTO_FULL=y` / GCC 13 工具链可能存在兼容性问题，或驱动源码对象未正确链接。
   - 修复：
     - 临时关闭 `CONFIG_LTO_FULL`（改 `CONFIG_LTO_NONE=y`）验证是否为 LTO 导致。
     - 检查 `librtl8733bs.a` 是否为 ARM thumb-2 / 当前 ABI 编译。

5. **factory_test `check_bt_valid` 未定义**
   - 原因：`factory_test.c` 引用 `check_bt_valid()`，但 `CONFIG_BT_START` 未设置，相关蓝牙启动工具未编译。
   - 修复：要么启用 `CONFIG_BT_START` 并确保 `bt_instance` 应用参与编译，要么在 `factory_test.c` 中补充完整防护。

6. **fatfs `SS` 未定义**
   - 原因：FatFs 配置头 `ffconf.h` 与 `fatfs_vfs.c` 版本不匹配。
   - 修复：确认 `CONFIG_FS_FATFS_SECTOR_RATIO=256` 与 `SS` 宏定义一致。

### 3.2 配置一致性 / 板级支持问题

| 问题 | 说明 | 建议 |
|------|------|------|
| `CONFIG_R528_LRADC` 未启用 | `CONFIG_DRIVERS_LRADC=y` 但 R528 具体 LRADC 驱动未打开 | 启用 `CONFIG_R528_LRADC=y`，否则物理按键无法工作 |
| `CONFIG_DEBUG_ASSERTIONS` 未启用 | 当前 `# CONFIG_DEBUG_ASSERTIONS is not set` | 建议启用，避免 LVGL 等库出现 `ASSERT` 未定义 |
| `CONFIG_LTO_FULL=y` | 链接时优化全开 | 可能导致预编译库符号被剥离；建议改为 `CONFIG_LTO_NONE=y` 或至少验证 WiFi/蓝牙库兼容性 |
| `CONFIG_R528_TWI0`/`TWI2` 已启用 | 传感器和触摸依赖 I2C | 保持启用；确认设备树/板级 bringup 中正确注册 I2C 总线 |
| `deepseek_demo` 缺失 | 飞书文档列出的示例应用不存在 | 从官方仓库恢复或新建；若不需要，应在文档中标注废弃 |
| `luncher_mini` 与 `airadio` 同时启用 | 两个 LVGL 应用都会初始化显示/触摸 | 确认启动策略：NSH 启动脚本中只启动一个，或一个作为系统应用、一个作为命令行演示 |
| `CONFIG_LCD_ILI9341` 与 MIPI 大面板配置混用 | `nsh` 默认配置使用 `T070S140B` MIPI；`nsh_minidisplay` 使用 ILI9341 | AI 电台硬件接的是 SPI LCD，应继续使用 `nsh_minidisplay` 基础，不要切回 `nsh` 大面板配置 |

### 3.3 打包 / 刷机问题

| 问题 | 来源 | 修复建议 |
|------|------|----------|
| `busybox: command not found` | `pack.log` 第 7/8/912 行 | 在构建环境中安装 `busybox`（`sudo apt install busybox`）或准备同名兼容脚本 |
| `boot0 checksum fail` | `pack.log` 第 20 行 | 重新生成 boot0（执行 `mboot0`）并确认使用的 board 参数为 `r528s3-gemini-s1` |
| 缺少 `ap.fex` | `pack.log` 第 40 行 | 确保编译后 `nuttx.bin` 被复制到 `vendor/allwinnertech/lichee/board/r528s3/gemini-s1_nand/configs/ap.fex`，或修正打包脚本中的复制逻辑 |
| `dragon: cannot execute binary file: Exec format error` | `pack.log` 第 1001 行 | `lichee/tools/tool/dragon` 为错误架构的可执行文件；需替换为 x86_64 Linux 版本或重新编译 |
| `pack.sh` 不支持 gemini-s1 | `vendor/allwinnertech/lichee/pack.sh` 仅支持 `r528s3-evb4` / `r528s3-x4b` | 直接调用 `tools/scripts/pack_img.sh` 并传入正确的 `-b r528s3-gemini-s1 -f r528s3/gemini-s1_nand -g r528s3/gemini-s1_nand` |

---

## 4. 从源码到刷机 IMG 的完整命令流程

### 4.1 环境准备

```bash
# 1. 安装依赖（Ubuntu 22.04）
sudo apt install \
  bison flex gettext texinfo libncurses5-dev libncursesw5-dev xxd \
  git gperf automake libtool build-essential gperf genromfs \
  libgmp-dev libmpc-dev libmpfr-dev libisl-dev binutils-dev libelf-dev \
  libexpat1-dev gcc-multilib g++-multilib picocom u-boot-tools util-linux \
  dfu-util libx11-dev libxext-dev net-tools pkgconf unionfs-fuse zlib1g-dev \
  libusb-1.0-0-dev libv4l-dev libuv1-dev npm nodejs nasm yasm libdivsufsort-dev \
  libc++-dev libc++abi-dev libprotobuf-dev protobuf-compiler protobuf-c-compiler mtools \
  busybox

# 2. 进入 SDK 根目录并设置环境
# 注意：/workspace/openvela_build 实际为 nuttx 源码树，SDK 根目录应为 /workspace
cd /workspace/openvela_build/vendor/allwinnertech/lichee
source envsetup.sh
envsetup
lunch_nuttx   # 选择 r528s3-gemini-s1 / nsh_minidisplay
```

### 4.2 编译 NuttX（AP 镜像）

```bash
# 方式 A：使用顶层 build.sh（推荐）
cd /workspace
./openvela_build/tools/build.sh \
  vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay \
  -j$(nproc) -e -Wno-error

# 方式 B：使用 vela_env.sh 中的快捷命令
# source /workspace/openvela_build/vendor/allwinnertech/lichee/vela_env.sh
# map -j$(nproc)

# 编译产物（若成功）应位于：
# /workspace/openvela_build/vela.bin
# /workspace/openvela_build/nuttx.bin（打包需要）
# /workspace/openvela_build/nuttx.elf
```

### 4.3 打包成 IMG

```bash
# 由于 pack.sh 不直接支持 gemini-s1，建议直接调用 pack_img.sh
cd /workspace/openvela_build/vendor/allwinnertech/lichee

# 确保 nuttx.bin 已复制为 ap.fex
mkdir -p board/r528s3/gemini-s1_nand/configs
cp /workspace/openvela_build/nuttx.bin board/r528s3/gemini-s1_nand/configs/ap.fex

# 执行打包
./tools/scripts/pack_img.sh \
  -c sun8iw20p1 \
  -p rtos \
  -b r528s3-gemini-s1 \
  -o nuttx \
  -d uart0 \
  -s none \
  -m normal \
  -w none \
  -v none \
  -i none \
  -t $(pwd) \
  -f r528s3/gemini-s1_nand \
  -g r528s3/gemini-s1_nand

# 输出 IMG 路径示例：
# /workspace/openvela_build/vendor/allwinnertech/lichee/out/r528s3/gemini-s1_nand/rtos_nuttx_r528s3-gemini-s1_uart0_128Mnand.img
```

### 4.4 刷机

```bash
# 使用 PhoenixSuit / LiveSuit / 全志官方烧录工具
# 或命令行（视工具 availability）：
sudo phoenixsuit -i /workspace/openvela_build/vendor/allwinnertech/lichee/out/r528s3/gemini-s1_nand/rtos_nuttx_r528s3-gemini-s1_uart0_128Mnand.img
```

---

## 5. 是否需要 / 如何从 `https://gitee.com/open-vela/frameworks` 引入额外能力

### 5.1 当前 `frameworks` 子仓库状态

`/workspace/openvela_build/frameworks/` 当前为稀疏检出（sparse checkout），实际有代码的子仓库只有：

- `frameworks/system/utils/`（包含 kvdb、log、trace、gdbus）
- `frameworks/runtimes/services/`（仅含少量头文件）

其余子目录如 `multimedia/`、`connectivity/`、`graphics/` 仅有空壳 `Makefile` / `Kconfig`，**没有实际源码**。

### 5.2 AI 电台项目建议引入的子仓库

| 子仓库 | 是否建议引入 | 理由 | 引入方式 |
|--------|--------------|------|----------|
| `frameworks_multimedia_media` | **建议** | 提供音频播放/录制、音频焦点、策略管理；可简化电台音频流程 | 通过 `repo` / `git submodule` 克隆到 `openvela_build/frameworks/multimedia/media/` |
| `frameworks_system_utils` | **建议** | 提供 kvdb、trace、uv 等基础组件；当前 `system/utils` 已有部分代码，但完整仓库更稳定 | 克隆到 `openvela_build/frameworks/system/utils/` |
| `frameworks_system_topics` | 可选 | 提供标准 uORB topic 定义；若已有自定义 topic 可暂不引入 | 克隆到 `openvela_build/frameworks/system/topics/` |
| `frameworks_graphics_uikit` | 可选 | 基于 LVGL 的 UI 组件扩展；若 airadio 已自足可暂缓 | 克隆到 `openvela_build/frameworks/graphics/uikit/` |
| `frameworks_bluetooth` | 可选 | 更上层蓝牙 API；当前 Zblue 已满足基本需求 | 视蓝牙功能复杂度决定 |

### 5.3 引入步骤示例

```bash
# 以 frameworks_multimedia_media 为例
cd /workspace/openvela_build/frameworks/multimedia
git clone https://gitee.com/open-vela/frameworks_multimedia_media.git media

# 确认 Kconfig 已包含 media 子目录
# 编辑 /workspace/openvela_build/frameworks/multimedia/Kconfig
# 添加：source "/workspace/openvela_build/frameworks/multimedia/media/Kconfig"

# 重新配置并编译
cd /workspace
./openvela_build/tools/build.sh \
  vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay \
  menuconfig

# 在 menuconfig 中启用对应多媒体组件，然后重新编译
```

### 5.4 注意事项

- 引入新子仓库会增加镜像体积，R528 仅有 128 MB DDR3，需谨慎评估内存占用。
- 每个子仓库通常依赖 `frameworks_system_utils`，建议优先引入 `utils`。
- 若使用 `repo` 管理，应在 SDK 根目录的 manifest 中声明，避免后续 `repo sync` 丢失。

---

## 6. 关键文件路径 / 配置项 / 命令速查

### 6.1 关键文件路径

| 用途 | 路径 |
|------|------|
| AI 电台主应用 | `/workspace/openvela_build/vendor/allwinnertech/apps/airadio/airadio.c` |
| AI 电台 Kconfig | `/workspace/openvela_build/vendor/allwinnertech/apps/airadio/Kconfig` |
| luncher_mini 应用 | `/workspace/openvela_build/vendor/allwinnertech/apps/luncher_mini/luncher_mini.c` |
| 推荐 defconfig | `/workspace/openvela_build/vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/defconfig` |
| 当前有效配置 | `/workspace/openvela_build/.config` |
| 编译产物 | `/workspace/openvela_build/vela.bin` |
| 打包所需 AP 镜像 | `/workspace/openvela_build/nuttx.bin`（需复制为 `ap.fex`） |
| 顶层构建脚本 | `/workspace/openvela_build/tools/build.sh` |
| 环境设置脚本 | `/workspace/openvela_build/vendor/allwinnertech/lichee/envsetup.sh` |
| 环境设置实际实现 | `/workspace/openvela_build/vendor/allwinnertech/lichee/tools/scripts/envsetup.sh` |
| 打包入口脚本 | `/workspace/openvela_build/vendor/allwinnertech/lichee/pack.sh` |
| 实际打包脚本 | `/workspace/openvela_build/vendor/allwinnertech/lichee/tools/scripts/pack_img.sh` |
| 快捷命令定义 | `/workspace/openvela_build/vendor/allwinnertech/lichee/vela_env.sh` |
| 板级打包/OTA 脚本 | `/workspace/openvela_build/vendor/allwinnertech/boards/r528/r528s3-gemini-s1/build/` |
| 分区/打包配置 | `/workspace/openvela_build/vendor/allwinnertech/lichee/board/r528s3/gemini-s1_nand/configs/` |
| WiFi 驱动目录 | `/workspace/openvela_build/vendor/allwinnertech/boards/r528/drivers/realtek_ieee80211` |
| 传感器驱动目录 | `/workspace/openvela_build/vendor/allwinnertech/chips/r528/drivers/rtos-hal/hal/source/sensor/` |
| 音频驱动目录 | `/workspace/openvela_build/vendor/allwinnertech/chips/r528/components/audio` |
| LRADC 按键文档 | `/workspace/feishu_docs_full/03_驱动开发/005_OpenVela (R528) 按键驱动实现概要.md` |
| frameworks 空壳 | `/workspace/openvela_build/frameworks/` |
| frameworks 完整仓库 | `/workspace/openvela_frameworks/` |

### 6.2 关键配置项

```text
# 板级 / 架构
CONFIG_ARCH_BOARD_R528S3_GEMINI_S1=y
CONFIG_ARCH_CHIP_R528=y
CONFIG_ARCH_BOARD_CUSTOM_DIR="../vendor/allwinnertech/boards/r528/r528s3-gemini-s1"

# 显示 / 触摸
CONFIG_LCD_ILI9341=y
CONFIG_GT911_IIC_TOUCH=y
CONFIG_LV_USE_NUTTX_LCD=y
CONFIG_LV_USE_NUTTX_TOUCHSCREEN=y
CONFIG_INPUT_TOUCHSCREEN=y

# WiFi / 蓝牙
CONFIG_IEEE80211_REALTEK_WIFI=y
CONFIG_IEEE80211_REALTEK_WIFI_RTL8733BS=y
CONFIG_BLUETOOTH=y
CONFIG_BT=y
CONFIG_BT_CLASSIC=y

# 传感器
CONFIG_SENSORS=y
CONFIG_SENSORS_SHTC3=y
CONFIG_SENSORS_LTR553=y

# 按键（注意 R528_LRADC 未启用，需修复）
CONFIG_DRIVERS_LRADC=y
# CONFIG_R528_LRADC is not set  <-- 应改为 y

# 音频
CONFIG_AUDIO=y
CONFIG_R528_AUDIO=y
CONFIG_AW_AUDIO_CODEC=y

# 应用
CONFIG_AIRADIO_APP=y
CONFIG_LUNCHER_MINI_APP=y

# 调试
CONFIG_SYSTEM_ADBD=y
CONFIG_UART2_SERIAL_CONSOLE=y
CONFIG_UART2_BAUD=1500000
CONFIG_SCHED_BACKTRACE=y
CONFIG_SYSTEM_DUMPSTACK=y

# 需要调整的配置
# CONFIG_DEBUG_ASSERTIONS is not set  <-- 建议启用
CONFIG_LTO_FULL=y                   <-- 建议改为 LTO_NONE
```

### 6.3 常用命令

```bash
# 配置
cd /workspace
./openvela_build/tools/build.sh \
  vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay \
  menuconfig

# 编译
cd /workspace
./openvela_build/tools/build.sh \
  vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay \
  -j$(nproc) -e -Wno-error

# 保存 defconfig
cd /workspace/openvela_build
make savedefconfig

# 设置环境
cd /workspace/openvela_build/vendor/allwinnertech/lichee
source envsetup.sh
envsetup
lunch_nuttx

# 编译 boot0/uboot（如需重新生成 boot0）
mboot0
muboot

# 打包（直接调用 pack_img.sh）
cd /workspace/openvela_build/vendor/allwinnertech/lichee
cp /workspace/openvela_build/nuttx.bin board/r528s3/gemini-s1_nand/configs/ap.fex
./tools/scripts/pack_img.sh \
  -c sun8iw20p1 -p rtos -b r528s3-gemini-s1 -o nuttx -d uart0 \
  -s none -m normal -w none -v none -i none \
  -t $(pwd) -f r528s3/gemini-s1_nand -g r528s3/gemini-s1_nand

# 串口调试
picocom -b 1500000 /dev/ttyUSB0

# ADB 调试
adb logcat
adb shell
```

---

## 7. 后续行动建议

1. **立即修复配置：** 启用 `CONFIG_R528_LRADC`、`CONFIG_DEBUG_ASSERTIONS`，关闭 `CONFIG_LTO_FULL`。
2. **重新完整编译一次：** 在后台 `make` 任务完成后，检查 `nuttx.bin` / `nuttx.elf` 是否生成。
3. **修复打包环境：** 安装 `busybox`、校验/替换 `dragon` 工具、重新生成 `boot0`。
4. **确保 ap.fex 路径正确：** 将 `nuttx.bin` 复制到 `board/r528s3/gemini-s1_nand/configs/ap.fex`。
5. **验证 IMG：** 成功打包后使用全志工具刷机，确认串口输出正常。
6. **恢复/引入 AI 能力：** 决定是恢复 `deepseek_demo` 还是基于现有 `airadio` 直接集成 LLM API。
7. **按需引入 frameworks：** 优先引入 `frameworks_multimedia_media` 和 `frameworks_system_utils`。

---

*本文档为只读研究成果，未修改任何源码或配置。*
