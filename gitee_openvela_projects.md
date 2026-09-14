# open-vela 组织仓库浏览报告（AI 电台控制台方向）

**浏览时间**：2026-07-07  
**浏览范围**：[open-vela 组织](https://gitee.com/organizations/open-vela/projects) 全部仓库，共 **275** 个  
**任务目标**：筛选与 AI 电台控制台项目（R528、LVGL、WiFi/蓝牙、音频/ASR、传感器、JS/QuickApp 运行时、桌面启动器）强相关的仓库，记录名称、链接与一句话描述，并给出最值得优先学习的 Top 10。

---

## 一、相关仓库分类列表

### 1. R528 / 全志硬件平台

| 仓库名称 | 链接 | 一句话描述 |
|---------|------|-----------|
| vendor_allwinnertech | https://gitee.com/open-vela/vendor_allwinnertech | 全志（Allwinner）R528 芯片与开发板支持仓库，trunk/dev-ai-contest-2026 分支包含 boards/r528（r528s3-evb4、r528s3-gemini-s1、r528s3-velaevb1）与 chips/r528 驱动，并集成 WiFi/BT/音频/传感器/启动器示例应用。 |
| libs_openvela_vela | https://gitee.com/open-vela/libs_openvela_vela | openvela vela 开发板预编译库。 |

### 2. LVGL / 图形 / 桌面启动器

| 仓库名称 | 链接 | 一句话描述 |
|---------|------|-----------|
| apps_graphics_lvgl | https://gitee.com/open-vela/apps_graphics_lvgl | LVGL（Light and Versatile Graphics Library）轻量级多功能图形库，为 AIoT 设备提供 UI 组件、主题与渲染能力。 |
| frameworks_graphics_uikit | https://gitee.com/open-vela/frameworks_graphics_uikit | UIKit 图形界面工具包，提供字体管理器、二维码扫描器、视频控件等通用 UI 组件。 |
| frameworks_graphics | https://gitee.com/open-vela/frameworks_graphics | openvela 图形框架根目录。 |
| external_thorvg | https://gitee.com/open-vela/external_thorvg | ThorVG 矢量图形渲染库。 |
| external_rlottie | https://gitee.com/open-vela/external_rlottie | Lottie 动画渲染库。 |
| external_freetype | https://gitee.com/open-vela/external_freetype | 字体渲染库 FreeType。 |
| external_harfbuzz | https://gitee.com/open-vela/external_harfbuzz | 文本塑形库 HarfBuzz。 |
| apps_graphics_libyuv | https://gitee.com/open-vela/apps_graphics_libyuv | YUV 图像格式处理库。 |
| packages_apps | https://gitee.com/open-vela/packages_apps | openvela JS 应用官方 IDE（AIoT-IDE，基于 VS Code）及可穿戴/智能音箱应用模板；wearable/launcher 为快应用版桌面启动器。 |
| vendor_allwinnertech/apps/luncher_mini | （位于 vendor_allwinnertech 内） | 基于 LVGL 的“迷你启动器”示例，集成时间日期、LED 控制、温湿度/接近传感器展示，适合作为桌面启动器入门。 |

### 3. WiFi / 蓝牙

| 仓库名称 | 链接 | 一句话描述 |
|---------|------|-----------|
| frameworks_bluetooth | https://gitee.com/open-vela/frameworks_bluetooth | openvela 蓝牙应用框架，已通过 Bluetooth 5.4 认证，支持 BR/EDR/BLE、GAP、L2CAP、GATT、A2DP、AVRCP、HFP、SPP、HID、HOGP、LEA、Mesh 等 Profile。 |
| external_fluoride | https://gitee.com/open-vela/external_fluoride | Android Fluoride 蓝牙协议栈。 |
| external_zblue | https://gitee.com/open-vela/external_zblue | Zephyr 蓝牙协议栈。 |
| external_wpa_supplicant | https://gitee.com/open-vela/external_wpa_supplicant | Wi-Fi 连接管理工具 wpa_supplicant。 |
| external_iw | https://gitee.com/open-vela/external_iw | 基于 nl80211 的无线配置工具。 |
| vendor_allwinnertech | https://gitee.com/open-vela/vendor_allwinnertech | 包含 R528 无线驱动 r528_wlan.c 及 wifi_test、bt_instance 等连接性示例应用。 |
| frameworks_connectivity | https://gitee.com/open-vela/frameworks_connectivity | 连接性框架顶层目录（umbrella）。 |
| frameworks_connectivity_telephony | https://gitee.com/open-vela/frameworks_connectivity_telephony | 电话/蜂窝连接框架。 |

### 4. 音频 / ASR / AI 语音

| 仓库名称 | 链接 | 一句话描述 |
|---------|------|-----------|
| packages_ai_agent | https://gitee.com/open-vela/packages_ai_agent | 运行在 openvela 上的 AI Agent 框架，支持多 LLM 后端、ReAct 工具调用、语音交互（ASR+TTS）、MCP、多节点协作与低内存优化。 |
| external_bailian_sdk | https://gitee.com/open-vela/external_bailian_sdk | 阿里云百炼（Bailian）SDK for OpenVela，可用于接入云端大模型与语音能力。 |
| frameworks_multimedia | https://gitee.com/open-vela/frameworks_multimedia | openvela 多媒体框架根目录。 |
| frameworks_multimedia_media | https://gitee.com/open-vela/frameworks_multimedia_media | Media Framework 音视频处理库，采用 C/S 架构，支持播放/录制、Media Player/Recorder/Focus/Policy/Session。 |
| vendor_bes | https://gitee.com/open-vela/vendor_bes | 恒玄科技（BES）芯片 vendor 支持，专注无线连接与音频方案（如 BES2600 系列）。 |
| external_ffmpeg | https://gitee.com/open-vela/external_ffmpeg | FFmpeg 多媒体处理库，支持音视频编解码、转码与流媒体。 |
| external_opus | https://gitee.com/open-vela/external_opus | Opus 音频编解码器。 |
| external_lc3 | https://gitee.com/open-vela/external_lc3 | LC3（Low Complexity Communication Codec）蓝牙低功耗音频编解码器。 |
| external_liblc3 | https://gitee.com/open-vela/external_liblc3 | LC3 编解码器另一实现。 |
| external_aac | https://gitee.com/open-vela/external_aac | AAC 音频编解码器。 |
| external_libhelix-aac | https://gitee.com/open-vela/external_libhelix-aac | Helix AAC 解码器。 |
| external_libhelix-mp3 | https://gitee.com/open-vela/external_libhelix-mp3 | Helix MP3 解码器。 |
| external_libfluoride-sbc | https://gitee.com/open-vela/external_libfluoride-sbc | SBC 蓝牙音频编解码器。 |
| external_silk-v3-decoder | https://gitee.com/open-vela/external_silk-v3-decoder | SILK 语音编解码器。 |
| external_libopencore-amr | https://gitee.com/open-vela/external_libopencore-amr | AMR 音频编解码器。 |
| nuttx_libs_libc_audio_libsrc_libsamplerate | https://gitee.com/open-vela/nuttx_libs_libc_audio_libsrc_libsamplerate | 音频采样率转换库 libsamplerate。 |
| apps_mlearning_tflite-micro | https://gitee.com/open-vela/apps_mlearning_tflite-micro | TensorFlow Lite Micro 机器学习运行时，可用于端侧 ASR/唤醒/关键词识别等模型推理。 |

### 5. 传感器

| 仓库名称 | 链接 | 一句话描述 |
|---------|------|-----------|
| external_android_system_chre | https://gitee.com/open-vela/external_android_system_chre | Android Context Hub Runtime Environment（CHRE），在独立低功耗处理器上运行传感器 nanoapp，适合 always-on 传感器后台处理。 |
| vendor_allwinnertech/apps | （位于 vendor_allwinnertech 内） | 集成 ltr553（光/接近传感器）、shtc3（温湿度传感器）等传感器示例应用。 |
| frameworks_system_vibrator | https://gitee.com/open-vela/frameworks_system_vibrator | 振动马达服务。 |
| frameworks_system_healthd | https://gitee.com/open-vela/frameworks_system_healthd | 健康与电池管理服务。 |
| frameworks_system_charger | https://gitee.com/open-vela/frameworks_system_charger | 充电管理服务。 |

### 6. JS / QuickApp 运行时

| 仓库名称 | 链接 | 一句话描述 |
|---------|------|-----------|
| frameworks_runtimes_quickapp | https://gitee.com/open-vela/frameworks_runtimes_quickapp | openvela 快应用运行时容器，基于 QuickJS + LVGL + Yoga，支持 RPK 包加载、页面路由、生命周期、JavaScript Feature 扩展与 CDP 远程调试。 |
| apps_interpreters_quickjs | https://gitee.com/open-vela/apps_interpreters_quickjs | OpenVela 版 QuickJS JavaScript 引擎。 |
| apps_interpreters_wamr | https://gitee.com/open-vela/apps_interpreters_wamr | WebAssembly Micro Runtime（WAMR）。 |
| apps_interpreters_lua | https://gitee.com/open-vela/apps_interpreters_lua | Lua 解释器。 |
| frameworks_runtimes_typescript | https://gitee.com/open-vela/frameworks_runtimes_typescript | TypeScript 运行时支持。 |
| frameworks_runtimes_wasm | https://gitee.com/open-vela/frameworks_runtimes_wasm | WASM 运行时支持。 |
| frameworks_runtimes_services | https://gitee.com/open-vela/frameworks_runtimes_services | 运行时系统服务集合，包含 AM（Activity）、WM（Window）、PM（Power）、Brightness 等 Manager。 |
| frameworks_runtimes | https://gitee.com/open-vela/frameworks_runtimes | 运行时框架顶层目录。 |
| packages_apps | https://gitee.com/open-vela/packages_apps | openvela JS/快应用官方 IDE 与模板工程。 |

### 7. 系统 / 基础 / 示例

| 仓库名称 | 链接 | 一句话描述 |
|---------|------|-----------|
| nuttx | https://gitee.com/open-vela/nuttx | openvela 内核，基于开源实时操作系统 NuttX，提供任务调度、跨进程通信、文件系统、TCP/IP 协议栈、设备驱动、电源管理及 POSIX 接口。 |
| nuttx-apps | https://gitee.com/open-vela/nuttx-apps | NuttX 配套应用集合，含工具、Shell、网络工具、解释器、图形库、加密库等。 |
| docs | https://gitee.com/open-vela/docs | openvela 官方文档仓库。 |
| manifests | https://gitee.com/open-vela/manifests | openvela repo 清单，用于多仓库源码同步。 |
| build | https://gitee.com/open-vela/build | openvela 构建系统。 |
| packages_demos | https://gitee.com/open-vela/packages_demos | openvela 原生应用示例仓库，含音乐播放器、智能手环、自行车码表、小游戏等，可直接作为参考实现。 |

---

## 二、Top 10 最值得克隆/学习的仓库及理由

| 排名 | 仓库 | 链接 | 推荐理由 |
|-----|------|------|---------|
| 1 | vendor_allwinnertech | https://gitee.com/open-vela/vendor_allwinnertech | **R528 硬件全栈入口**：包含 R528 芯片驱动、开发板配置、WiFi（r528_wlan.c）、蓝牙、音频、传感器及 luncher_mini 启动器示例，是 AI 电台控制台硬件适配的第一站。 |
| 2 | nuttx | https://gitee.com/open-vela/nuttx | **操作系统内核**：理解任务调度、IPC、驱动模型、电源管理的基础，所有上层框架都运行在其之上。 |
| 3 | apps_graphics_lvgl | https://gitee.com/open-vela/apps_graphics_lvgl | **图形 UI 核心**：电台控制台的屏幕界面、控件、动画均依赖 LVGL，社区成熟、文档丰富。 |
| 4 | frameworks_runtimes_quickapp | https://gitee.com/open-vela/frameworks_runtimes_quickapp | **快应用/JS 运行时**：基于 QuickJS+LVGL+Yoga，适合用 JavaScript 快速开发电台控制面板、Launcher 与业务逻辑。 |
| 5 | frameworks_bluetooth | https://gitee.com/open-vela/frameworks_bluetooth | **蓝牙连接能力**：已通过 Bluetooth 5.4 认证，支持 A2DP（音频传输）、AVRCP（遥控）、HFP（通话）、BLE 等，是蓝牙音箱/耳机/遥控器的必备框架。 |
| 6 | packages_ai_agent | https://gitee.com/open-vela/packages_ai_agent | **AI 语音交互**：内置 ASR+TTS、多 LLM 后端、ReAct 工具调用，可直接用于构建“语音电台助手”。 |
| 7 | frameworks_multimedia_media | https://gitee.com/open-vela/frameworks_multimedia_media | **音视频媒体框架**：C/S 架构的播放/录制引擎，是电台音频播放、网络电台流、本地音乐播放的核心基础设施。 |
| 8 | external_android_system_chre | https://gitee.com/open-vela/external_android_system_chre | **传感器低功耗处理**：Context Hub 架构适合 always-on 的传感器场景（如环境光、接近感应自动熄屏/唤醒）。 |
| 9 | packages_demos | https://gitee.com/open-vela/packages_demos | **参考实现宝库**：音乐播放器、智能手环等 Demo 可直接借鉴，缩短 POC 与产品化周期。 |
| 10 | nuttx-apps | https://gitee.com/open-vela/nuttx-apps | **应用生态与工具集**：包含解释器、网络工具、加密库、图形库等，是构建完整系统时不可缺的“工具箱”。 |

---

## 三、快速上手建议

1. **同步源码**：使用 manifests 仓库的 repo 工具同步 openvela 完整源码。  
   ```bash
   repo init -u https://gitee.com/open-vela/manifests.git -b dev
   repo sync -c -j8
   ```
2. **R528 编译**：关注 vendor_allwinnertech 的 trunk/dev-ai-contest-2026 分支，参考 README 中的 lunch 与 build 命令。  
   示例：`./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/ -j8`
3. **UI 开发**：以 apps_graphics_lvgl + frameworks_graphics_uikit 为基础，结合 vendor_allwinnertech/apps/luncher_mini 启动器示例修改桌面。
4. **语音/AI 功能**：优先集成 packages_ai_agent，按需引入 external_bailian_sdk 或 apps_mlearning_tflite-micro。
5. **音频播放**：基于 frameworks_multimedia_media + 所需音频编解码器（opus/lc3/aac/mp3 等）搭建播放器。
6. **无线连接**：R528 的 WiFi/蓝牙驱动在 vendor_allwinnertech 中，协议栈使用 frameworks_bluetooth + external_wpa_supplicant。

---

## 四、说明

- 本报告仅基于仓库名称、描述、README 及目录结构进行浏览与记录，**未下载或修改任何源码**。
- 部分仓库未填写 Gitee 描述，描述根据目录结构、README 或官方文档推断而得。
- 仓库默认分支多为 `dev`，vendor_allwinnertech 的 R528 代码主要在 `trunk` 与 `dev-ai-contest-2026` 分支。
- 组织仓库总数为 275 个，本报告筛选出与 AI 电台控制台强相关的约 40 个仓库进行重点介绍。
