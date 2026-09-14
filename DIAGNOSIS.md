# 项目全面诊断报告

> 生成时间: 2026-07-10
> 项目: contest2026_095_hehaohanBI4MIB (AI 电台控制台)
> 目标板: Gemini-S1 (Allwinner R528 双核 Cortex-A7)

---

## 一、问题总结

**用户反馈**: "编译出来它不出字儿，屏幕不亮屏"

**根本原因**: 项目无法编译，因为缺少完整的 openvela 源码树。当前仓库只是一个"参赛仓库"，不是完整的编译环境。

---

## 二、项目结构分析

### 2.1 仓库定位

根据 `contest2026_095_hehaohanBI4MIB.xml` manifest，本仓库通过 `<linkfile>` 机制将代码映射到 openvela 编译树中：

| 本地路径 | openvela 编译树路径 |
|----------|---------------------|
| `app/ai_radio_console/` | `packages/demos/contest2026_095_ai_radio_console` |
| `app/hello_app/` | `packages/demos/contest2026_095_hello_app` |
| `quickapp/hello_quickapp/` | `packages/apps/contest2026_095_hello_quickapp` |
| `board/contest_board/` | `vendor/openvela/boards/contest2026_095_board` |

**关键**: 本仓库只是 openvela 生态中的一个"插件"，必须配合完整的 openvela 源码树才能编译。

### 2.2 两个子项目

| 项目 | 路径 | 状态 | 问题 |
|------|------|------|------|
| AI 电台控制台 | `app/ai_radio_console/` | 代码完整 (22个C源文件) | 无法独立编译，依赖 openvela 源码树 |
| Hello App | `app/hello_app/` | 模板代码 | 同上 |

### 2.3 board/contest_board/ 是占位符

**严重问题**: `board/contest_board/` 只是一个模板占位符：

- `configs/nsh/defconfig` 只有 1 行: `CONFIG_ARCH_BOARD_CONTEST2026_000_BOARD=y`
- `src/board_boot.c` 是空函数: `void openvela_board_initialize(void) {}`
- **没有任何显示驱动配置** (无 SPI LCD, 无 LVGL, 无 ILI9341)

**这个板级配置无法驱动任何显示屏。**

### 2.4 真正的板级配置在哪里

真正的 Gemini-S1 板级配置在 vendor SDK 中（需要 `repo sync` 获取）：

```
vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/defconfig
```

这个 defconfig (603行) 包含完整的显示支持：
- `CONFIG_LCD=y` + `CONFIG_LCD_ILI9341=y` (ILI9341 SPI LCD 驱动)
- `CONFIG_LCD_DEV=y` + `CONFIG_LCD_FRAMEBUFFER=y` (LCD 设备节点和帧缓冲)
- `CONFIG_LV_USE_NUTTX=y` + `CONFIG_LV_USE_NUTTX_LCD=y` (LVGL NuttX 后端)
- `CONFIG_GRAPHICS_LVGL=y` + `CONFIG_LV_USE_ILI9341=y` (LVGL ILI9341 驱动)
- `CONFIG_LUNCHER_MINI_APP=y` (桌面启动器)
- `CONFIG_SENSORS_SHTC3=y` + `CONFIG_SENSORS_LTR553=y` (传感器)
- `CONFIG_IEEE80211_REALTEK_WIFI=y` (WiFi)
- `CONFIG_BLUETOOTH=y` + `CONFIG_BT=y` (蓝牙)

---

## 三、为什么屏幕不亮

### 3.1 编译链路断裂

```
本仓库 (contest2026_095_hehaohanBI4MIB)
    ↓ linkfile 映射
openvela 编译树 (需要 repo sync 获取)
    ↓ build.sh 编译
vendor/allwinnertech/ (Allwinner R528 SDK)
    ↓ 硬件驱动
Gemini-S1 板 (ILI9341 SPI LCD)
```

**当前状态**: openvela 编译树不存在 (`openvela_build/` 目录为空)，所以：
- 无法编译任何代码
- 无法生成固件
- 屏幕自然不亮

### 3.2 即使编译成功，board/contest_board/ 也无法显示

如果使用 `board/contest_board/` 作为板级配置：
- defconfig 只有 1 行配置
- 没有 SPI LCD 驱动
- 没有 LVGL 配置
- 没有帧缓冲
- **结果**: 固件可以启动 NSH shell，但屏幕无任何输出

### 3.3 应用代码本身是正确的

`app/ai_radio_console/src/main.c` 的显示初始化代码是正确的：

```c
lv_nuttx_dsc_t dsc;
lv_nuttx_dsc_init(&dsc);
dsc.fb_path = "/dev/lcd0";      // 正确: 使用 LCD 设备节点
dsc.input_path = "/dev/input0"; // 正确: 使用触摸输入设备
lv_nuttx_init(&dsc, &nuttx_res);
```

如果板级配置正确启用了 ILI9341 LCD，`/dev/lcd0` 会存在，UI 就能正常显示。

---

## 四、修复方案

### 方案 A: 使用已有板级配置 (推荐)

**前提**: 需要在 Ubuntu 22.04 机器上 (80GB+ 磁盘, 16GB+ 内存)

```bash
# 1. 安装依赖
sudo apt install -y bison flex gettext texinfo libncurses5-dev libncursesw5-dev xxd \
  git gperf automake libtool build-essential genromfs \
  libgmp-dev libmpc-dev libmpfr-dev libisl-dev binutils-dev libelf-dev \
  libexpat1-dev gcc-multilib g++-multilib picocom u-boot-tools util-linux \
  dfu-util libx11-dev libxext-dev net-tools pkgconf unionfs-fuse zlib1g-dev \
  libusb-1.0-0-dev libv4l-dev libuv1-dev npm nodejs nasm yasm libdivsufsort-dev \
  libc++-dev libc++abi-dev libprotobuf-dev protobuf-compiler protobuf-c-compiler mtools

# 2. 安装 repo
mkdir -p ~/.bin
curl https://storage.googleapis.com/git-repo-downloads/repo > ~/.bin/repo
chmod a+x ~/.bin/repo
export PATH="${HOME}/.bin:${PATH}"

# 3. 初始化 openvela 源码树
mkdir -p ~/openvela_build && cd ~/openvela_build
repo init -u https://gitee.com/open-vela/manifests.git -b dev-ai-contest-2026
repo sync -c -j8

# 4. 将本仓库代码链接进去
# 方法1: 使用 local manifest
mkdir -p .repo/local_manifests
cp /path/to/contest2026_095_hehaohanBI4MIB/contest2026_095_hehaohanBI4MIB.xml .repo/local_manifests/
repo sync

# 方法2: 直接软链接
ln -sf /path/to/contest2026_095_hehaohanBI4MIB/app/ai_radio_console \
  packages/demos/contest2026_095_ai_radio_console
ln -sf /path/to/contest2026_095_hehaohanBI4MIB/board/contest_board \
  vendor/openvela/boards/contest2026_095_board

# 5. 使用正确的板级配置编译
source vendor/allwinnertech/lichee/envsetup.sh
lunch_nuttx

# 6. 启用 AI Radio Console 应用
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay menuconfig
# 在 menuconfig 中启用:
#   Demos -> Contest 2026 team 095 AI Radio Console = y

# 7. 编译
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay -j$(nproc)

# 8. 打包
cd vendor/allwinnertech/lichee
source envsetup.sh
lunch_nuttx
pack
```

### 方案 B: 修复 board/contest_board/ (不推荐)

如果坚持使用自定义板级配置，需要将 `国际空间通信挑战赛项目规划/vendor_allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/defconfig` 的关键配置复制到 `board/contest_board/configs/nsh/defconfig`，并实现 `board_boot.c` 中的硬件初始化。

**但这需要深入理解 Allwinner R528 的 BSP，工作量巨大，不建议在比赛期间做。**

---

## 五、app/ai_radio_console/ 代码质量分析

### 5.1 优点
- 代码结构清晰，模块化良好
- 24个头文件 + 22个源文件，功能完整
- LVGL UI 设计合理 (320x240 横屏)
- 音频 DSP (Goertzel 频谱分析) 实现正确
- SiliconFlow ASR/LLM 集成完整
- 传感器 (SHTC3/LTR553) 驱动已接入 UI

### 5.2 需要注意的问题
1. `siliconflow_client.c` 禁用了 TLS 证书验证 (`MBEDTLS_SSL_VERIFY_NONE`)
2. `radio_log.c` 使用 `system()` 和 `popen()` (安全隐患)
3. `ui_ai_radio.c` 的 `g_transcript_history` 缓冲区无线程同步
4. `cw_decoder.c` 模块编译了但从未被调用 (死代码)
5. `ui_manager.h` 声明了函数但没有对应的 `.c` 文件

### 5.3 缺失的依赖
- `frameworks/system/topics/include` (openvela framework headers)
- `frameworks/system/utils/include` (openvela framework utils)
- `graphics/lvgl` (LVGL 库)

这些都需要 openvela 源码树才能提供。

---

## 六、预编译镜像

仓库中有一个 `gemini-s1_ai_radio.img` (22MB)，这可能是一个旧版本的固件。

**尝试刷写这个镜像看看屏幕是否能亮**:
1. 使用 PhoenixSuit (Windows) 或 LiveSuit (Linux)
2. 按住 FEL 按键，USB 连接 OTG 口
3. 刷写完成后重新上电

如果这个镜像能让屏幕亮起来，说明硬件没问题，问题出在编译配置上。

---

## 七、总结

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| 无法编译 | 缺少 openvela 源码树 | `repo sync` 获取完整源码 |
| 屏幕不亮 | board/contest_board/ 是占位符 | 使用 vendor 的 nsh_minidisplay 配置 |
| 应用未启用 | defconfig 中未开启 | menuconfig 中启用 AI Radio Console |
| 两个项目都不好使 | 都依赖 openvela 源码树 | 先完成 repo sync |

**下一步行动**:
1. 在满足配置的 Ubuntu 机器上执行 repo sync
2. 使用 `nsh_minidisplay` 板级配置
3. 在 menuconfig 中启用 AI Radio Console
4. 编译、打包、刷写
