# AI 电台控制台项目上下文总览

> 生成时间：2026-07-07  
> 用途：防止会话上下文丢失，汇总项目目标、现状、关键文件、已做工作、待办事项与下一步命令。

---

## 1. 项目目标

为全志 R528 Gemini-S1 开发板构建一个可刷写到实体机并正常运行的 **AI 电台控制台** 固件。

核心需求（来自用户与飞书文档）：
- 保留并启用 **WiFi / 蓝牙**（联网、AI/ASR 必需）。
- 保留并启用 **luncher_mini 桌面**（安装/运行第三方 JS/QuickApp 小程序入口）。
- 利用板载 **SHTC3（温湿度）、LTR553（光感/接近）、LRADC 按键、WS2812 RGB LED** 等传感器与外设。
- 提供 **LVGL UI**：频率、模式、PTT、ASR 文本、LLM 摘要、报警状态。
- 实现 **I2S 真实音频采集** 供 ASR 使用。
- 接入云端 **SiliconFlow ASR/LLM**。
- 最终输出可刷机的 **`.img`** 文件，并给出实体机刷写/运行命令。

---

## 2. 当前工作区结构

```
/workspace/
├── app/ai_radio_console/          # AI 电台应用源码（ contest2026_095 队伍作品）
│   ├── src/                       # 所有 C 模块
│   ├── include/                   # 头文件
│   ├── Makefile                   # NuttX Makefile（已修复，现包含全部 CSRCS）
│   ├── Kconfig                    # 配置项 LVX_USE_DEMO_CONTEST2026_095_AI_RADIO_CONSOLE
│   ├── Make.defs                  # 注册到 packages/demos/...
│   ├── Makefile.local             # Linux x86 本地模拟构建
│   └── README.md
├── board/contest_board/           # 自定义板级配置（会被 manifest 链接到 vendor/openvela/boards/...）
├── quickapp/hello_quickapp/       # 快应用示例
├── simulator/                     # Web 端模拟器（React + Vite）
├── feishu_docs_full/              # Gemini-S1 / openvela 飞书文档离线副本
├── openvela_frameworks/           # git clone https://gitee.com/open-vela/frameworks
├── openvela.xml                   # openvela repo manifest（基础仓库清单）
├── contest2026_095_hehaohanBI4MIB.xml  # 队伍 manifest（包含 openvela.xml + 自身 linkfile）
├── gemini-s1_ai_radio.img         # 22MB 预编译镜像（可能已包含旧版本固件）
├── ai_radio_recommendations.md    # 项目建议文档（由 agent 生成）
├── frameworks_research.md         # openvela frameworks 研究报告
├── gitee_openvela_projects.md     # open-vela Gitee 组织项目筛选报告
└── openvela_build/                # 当前为空；原计划作为 openvela 源码树根目录
```

---

## 3. 关键发现与已修复问题

### 3.1 应用 Makefile 致命缺陷（已修复）
- **原问题**：`app/ai_radio_console/Makefile` 只编译 `src/main.c`，其余模块全部缺失，链接必然失败。
- **修复**：将 `src/` 下所有 `.c` 文件加入 `CSRCS`，`MAINSRC = main.c`。
- **文件**：[app/ai_radio_console/Makefile](file:///workspace/app/ai_radio_console/Makefile#L12-L18)

### 3.2 关键问题与修复状态
1. **HTTPS/TLS（已修复）**：`siliconflow_client.c` 已支持 mbedtls，在启用 `CONFIG_NETUTILS_MBEDTLS` 时通过 TLS 连接 SiliconFlow HTTPS 端点；未启用时返回明确错误。
2. **音频设备节点（已通过配置宏处理）**：PCM 设备路径已在 `radio_config.h` 中统一为配置宏，目标板上仍需确认实际节点名称。
3. **NuttX 功能依赖（已在 defconfig_notes.md 中整理）**：pthread、mqueue、POSIX timers、socket、DNS、mbedtls、audio/I2S、传感器、LVGL 等依赖开关已整理到 `app/ai_radio_console/defconfig_notes.md`。
4. **传感器未接入 UI（已修复）**：`sensor_env.c` 读取 SHTC3/LTR553 数据，并在 LVGL 界面通过标签/卡片实时展示，不可用时显示 `--`。
5. **manifest 路径**：本地源码需通过 `repo sync` 后以 `<linkfile>` 方式链接到 `packages/demos/contest2026_095_ai_radio_console`（`contest2026_095_hehaohanBI4MIB.xml` 中 linkfile 已配置）。

---

## 4. 关键文件路径

| 用途 | 路径 |
|------|------|
| 队伍 manifest | `/workspace/contest2026_095_hehaohanBI4MIB.xml` |
| 基础 manifest | `/workspace/openvela.xml` |
| AI 电台应用源码 | `/workspace/app/ai_radio_console/` |
| 应用 Makefile（已修复） | `/workspace/app/ai_radio_console/Makefile` |
| 自定义板级 | `/workspace/board/contest_board/` |
| 飞书文档 | `/workspace/feishu_docs_full/` |
| 预编译镜像 | `/workspace/gemini-s1_ai_radio.img` |
| openvela frameworks 顶层 | `/workspace/openvela_frameworks/` |
| 项目建议 | `/workspace/ai_radio_recommendations.md` |
| frameworks 研究 | `/workspace/frameworks_research.md` |
| Gitee 项目筛选 | `/workspace/gitee_openvela_projects.md` |

目标板级配置（当 openvela 源码树同步后）：
- `vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/defconfig`
- 或队伍自定义板：`vendor/openvela/boards/contest2026_095_board/`

---

## 5. 构建/打包/刷写命令速查

### 5.1 在本地 Ubuntu 22.04 上准备完整源码树

> 当前沙箱只有 40GB 磁盘 / 4GB 内存，不满足 openvela 官方推荐（80GB 磁盘 / 16GB 内存）。以下命令需在用户自己的 Linux 主机上执行。

```bash
# 1. 安装依赖（Ubuntu 22.04）
sudo apt install -y bison flex gettext texinfo libncurses5-dev libncursesw5-dev xxd \
  git gperf automake libtool build-essential genromfs \
  libgmp-dev libmpc-dev libmpfr-dev libisl-dev binutils-dev libelf-dev \
  libexpat1-dev gcc-multilib g++-multilib picocom u-boot-tools util-linux \
  dfu-util libx11-dev libxext-dev net-tools pkgconf unionfs-fuse zlib1g-dev \
  libusb-1.0-0-dev libv4l-dev libuv1-dev npm nodejs nasm yasm libdivsufsort-dev \
  libc++-dev libc++abi-dev libprotobuf-dev protobuf-compiler protobuf-c-compiler mtools

# 2. 安装 repo 工具
mkdir -p ~/.bin
curl https://storage.googleapis.com/git-repo-downloads/repo > ~/.bin/repo
chmod a+x ~/.bin/repo
export PATH="${HOME}/.bin:${PATH}"

# 3. 创建 openvela 源码根目录并初始化
mkdir -p ~/openvela_build && cd ~/openvela_build
repo init -u https://gitee.com/open-vela/manifests.git -b dev-ai-contest-2026

# 4. 同步（首次耗时较长，建议 -j8 或更高）
repo sync -c -j8

# 5. 用队伍 manifest 替换默认 manifest（如需包含本地作品）
# 将 /workspace/contest2026_095_hehaohanBI4MIB.xml 放到 manifest git 仓库中，
# 或作为 local manifest 使用：
# cp contest2026_095_hehaohanBI4MIB.xml .repo/local_manifests/
# repo sync
```

### 5.2 配置与编译

```bash
cd ~/openvela_build
source vendor/allwinnertech/lichee/envsetup.sh
lunch_nuttx
# 选择 r528s3-gemini-s1 / nsh_minidisplay

# 图形化配置（启用 WiFi/蓝牙/LVGL/luncher_mini/AI_RADIO_CONSOLE 等）
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay menuconfig

# 编译
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay -j$(nproc)
```

关键配置项（在 menuconfig 中确认）：
- `CONFIG_LVX_USE_DEMO_CONTEST2026_095_AI_RADIO_CONSOLE=y`
- `CONFIG_LUNCHER_MINI_APP=y`
- `CONFIG_GRAPHICS_LVGL=y`
- `CONFIG_IEEE80211_REALTEK_WIFI=y`
- `CONFIG_BLUETOOTH=y`
- `CONFIG_BT=y`
- `CONFIG_SENSORS_SHTC3=y`
- `CONFIG_SENSORS_LTR553=y`
- `CONFIG_AUDIO=y`
- `CONFIG_NET=y`

### 5.3 打包 IMG

```bash
cd ~/openvela_build/vendor/allwinnertech/lichee
source envsetup.sh
lunch_nuttx
pack
```

打包产物一般在 `out/` 或 `vendor/allwinnertech/lichee/out/` 下，文件名为 `.img`。

### 5.4 刷写到 Gemini-S1 实体机

参考全志 R528 通用刷机流程：

#### Windows（推荐）
1. 下载并安装 **PhoenixSuit**（全志官方刷机工具）。
2. 安装 `Drivers/AW_Driver/InstallUSBDrv.exe`。
3. 打开 PhoenixSuit → “一键刷机” → 选择 `.img` 文件。
4. 按住板子上的 **UBOOT/BOOT/FEL** 按键，用 USB 线连接板子 **OTG** 口到电脑，松手。
5. 工具提示“发现设备”后，确认开始刷机。
6. 等待“烧录成功”，断开 USB，重新上电开机。

#### Linux（LiveSuit）
```bash
# 安装依赖
sudo apt update
sudo apt install -y dkms

# 解压 LiveSuit 并运行
sudo chmod +x LiveSuit.run
./LiveSuit.run
sudo ~/Bin/livesuit/LiveSuit

# 选择 IMG，进入 FEL 模式后刷机
```

#### 刷后验证
```bash
# 串口登录（UART2，1500000 baud）
picocom /dev/ttyUSB0 -b 1500000

# 或 ADB
adb devices
adb shell
adb logcat

# 在板子里运行
airadio          # 启动 AI 电台应用
luncher_mini     # 启动桌面
wapi mode wlan0 2
wapi scan wlan0
wapi psk wlan0 "密码" 3
wapi essid wlan0 "SSID" 1
renew wlan0
ipconfig
```

---

## 6. 已生成的文档

| 文档 | 说明 |
|------|------|
| `/workspace/ai_radio_recommendations.md` | 项目现状、飞书文档摘要、必须保留功能、问题与修复建议、构建流程 |
| `/workspace/frameworks_research.md` | open-vela frameworks 可复用能力评估与集成建议 |
| `/workspace/gitee_openvela_projects.md` | open-vela Gitee 组织 275 个仓库中相关仓库列表与 Top 10 |
| `/workspace/PROJECT_CONTEXT.md` | 本文件：上下文总览 |

---

## 7. 待办事项（按优先级）

- [ ] 在用户本地或云主机上完成 openvela 源码 `repo sync`（需要 80GB+ 磁盘、16GB+ 内存）。
- [x] 确认或修复 `siliconflow_client.c` 的 HTTPS/TLS 支持。
- [x] 统一音频 PCM 设备节点路径，并在 defconfig 中启用音频/I2S。
- [x] 在 defconfig 中启用 WiFi/蓝牙/传感器/LVGL/luncher_mini/AI_RADIO_CONSOLE 等全部必需配置。
- [ ] 成功编译出 `nuttx.bin` / `nuttx.elf`。
- [ ] 运行 `pack` 生成 `.img`。
- [ ] 在 Gemini-S1 实体机上刷写并验证 `airadio`、`luncher_mini`、WiFi、传感器、ASR/LLM 功能。

---

## 8. 环境限制说明

当前 TRAE 远程沙箱：
- 磁盘：40GB（已用 33GB，可用约 7GB）。
- 内存：4GB。
- 无 `repo` 工具。

因此无法在当前沙箱内完成 openvela 完整源码同步与固件编译。本会话已完成：
- 文档研读与整理；
- frameworks 克隆与研究；
- Gitee 项目浏览；
- 应用 Makefile 修复；
- 刷机流程梳理。

建议用户在满足官方推荐配置的 Ubuntu 22.04 机器上执行第 5 节命令完成最终构建与刷机。

---

## 9. 本次修复记录

- **HTTPS/TLS 支持不足 → 已修复**：`siliconflow_client.c` 在启用 `CONFIG_NETUTILS_MBEDTLS` 时通过 mbedtls 支持 SiliconFlow HTTPS/TLS 通信，未启用时明确报错。
- **音频设备节点硬编码 → 已通过配置宏处理**：PCM 设备路径统一收敛到 `radio_config.h` 中的配置宏，便于在不同板级间调整。
- **NuttX 功能依赖未启用 → 已在 defconfig_notes.md 中整理**：`app/ai_radio_console/defconfig_notes.md` 已列出 TLS、网络、pthread、mqueue、audio、I2S、UART、LRADC、SHTC3、LTR553 等必要开关及检查清单。
- **传感器未接入 UI → 已修复**：SHTC3/LTR553 数据通过 `sensor_env.c` 读取并在 LVGL UI 中展示，不可用时显示 `--` 并提示。
