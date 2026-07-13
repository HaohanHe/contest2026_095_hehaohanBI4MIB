# openvela Frameworks 可复用能力研究报告

> 研究对象：`/workspace/openvela_frameworks`（https://gitee.com/open-vela/frameworks）及其 README 中列出的全部子仓库  
> 目标项目：全志 R528 Gemini-S1 + openvela（NuttX）+ LVGL 的 AI 电台控制台  
> 核心需求：WiFi/蓝牙联网、音频/ASR、板载传感器（SHTC3/LTR553/LRADC/WS2812）、launcher_mini 桌面、第三方 JS/QuickApp/TS 小程序运行能力、最终打包 IMG  
> 说明：本文只做研究，未修改 `/workspace/openvela_build` 源码，未触发编译。

---

## 1. 结论速览

| 优先级 | 子仓库 |
|---|---|
| **必读/高价值** | `frameworks_graphics_uikit`、`frameworks_multimedia_media`、`frameworks_multimedia_media_pfw`、`frameworks_runtimes_services_am/pm/wm/system_server`、`frameworks_runtimes_wasm`、`frameworks_bluetooth`、`frameworks_system_topics`、`frameworks_system_utils`、`frameworks_runtimes_services_xmsdemo` |
| **按需集成** | `frameworks_runtimes_services_brightness`、`frameworks_system_ota`、`frameworks_system_healthd`、`frameworks_system_charger`、`frameworks_system_binder`、`frameworks_system_utils_uv`、`frameworks_runtimes_typescript_ts2native/ts2wasm` |
| **当前项目价值较低** | `frameworks_connectivity_telephony`、`frameworks_security`、`frameworks_security_optee_vela`、`frameworks_system_vibrator` |

**推荐技术栈组合**：

- **UI 层**：`uikit`（LVGL 扩展 + 字体/视频/异步刷新）+ `wm`（窗口管理）+ `xmsdemo` 中的 `launcher` 参考实现。
- **音频/ASR 层**：`media`（播放/录制/焦点/策略/会话）+ `media_pfw`（音频策略状态机）。
- **应用框架层**：`system_server` 拉起 `am` + `pm` + `wm`，实现多应用生命周期与包管理。
- **小程序/第三方应用层**：`wasm`（WAMR 运行时）+ `ts2wasm`/`ts2native` 作为 TS 小程序编译链路；QuickApp 能力由 openvela 快应用框架（QuickJS）提供，蓝牙 README 中已明确提到 QuickApp Feature，但不在本 frameworks 清单内。
- **传感器层**：`system_topics`（uORB topic 定义）+ NuttX 传感器驱动。
- **系统基础**：`system_utils`（kvdb、log、trace、gdbus）。
- **联网**：`bluetooth` 覆盖 BT/BLE/BT-Audio；WiFi 不在本 frameworks 清单，建议直接使用 NuttX 无线协议栈 + 上层 socket/wpa_supplicant。

---

## 2. 子仓库清单与一句话描述

| 序号 | 子仓库 | 一句话描述 |
|---|---|---|
| 1 | `frameworks_bluetooth` | 完整的蓝牙应用 API、Services、SAL 协议栈适配与 HAL 硬件适配层，已通过 Bluetooth 5.4 认证。 |
| 2 | `frameworks_connectivity_telephony` | 蜂窝通信 TAPI（Telephony API），封装 Ofono 提供网络/通话/SMS/数据/SIM 能力。 |
| 3 | `frameworks_graphics_uikit` | 基于 LVGL 的 UI 工具集，提供字体管理器、视频控件、二维码扫描、异步刷新等组件与 demo。 |
| 4 | `frameworks_multimedia_media` | C/S 架构多媒体框架，提供播放、录制、音频焦点、音频策略、媒体会话与跨核 RPC。 |
| 5 | `frameworks_multimedia_media_pfw` | Parameter-Framework 通用状态机框架，用于构造音频策略/路由等配置驱动状态。 |
| 6 | `frameworks_runtimes_services_brightness` | 亮度管理服务，支持自动/手动调光及亮度曲线实时修改。 |
| 7 | `frameworks_runtimes_services_am` | XMS 应用管理服务（AMS），管理 Activity/任务栈/进程/Intent/权限/后台限制。 |
| 8 | `frameworks_runtimes_services_pm` | XMS 包管理服务（PMS），提供应用安装、卸载、查询能力。 |
| 9 | `frameworks_runtimes_services_system_server` | 系统服务总控，负责启动并管理 AM/PM/WM/亮度等核心服务。 |
| 10 | `frameworks_runtimes_services_wm` | XMS 窗口管理服务，负责输入、输出、显示、窗口生命周期与动效。 |
| 11 | `frameworks_runtimes_services_xmsdemo` | XMS 多应用框架示例，包含 launcher 等完整快应用参考实现。 |
| 12 | `frameworks_runtimes_typescript_ts2native` | 将 TypeScript 源码静态编译为 Native（C/C++）代码的实验性工具链。 |
| 13 | `frameworks_runtimes_typescript_ts2wasm` | `Wasmnizer-ts`，将 TypeScript 编译为 WasmGC 字节码的实验性工具链。 |
| 14 | `frameworks_runtimes_wasm` | openvela Wasm 运行扩展与 SDK，目前支持 WAMR，提供 libc/memory/vela-sysroot 等模块。 |
| 15 | `frameworks_security` | openvela TEE 默认安全应用（TA/CA）实现。 |
| 16 | `frameworks_security_optee_vela` | 兼容 openvela 内核的 OPTEE 可信执行环境框架。 |
| 17 | `frameworks_system_binder` | Binder IPC 示例、性能测试及 C 语言实现。 |
| 18 | `frameworks_system_charger` | 电池充电服务（chargerd），支持温控、限流、充放电曲线等。 |
| 19 | `frameworks_system_healthd` | 电池健康监测服务，通过 uORB 发布 `battery_state`。 |
| 20 | `frameworks_system_ota` | OTA 升级（AB 分区 bootctl、打包脚本、升级动画 UI、AVB 验签）。 |
| 21 | `frameworks_system_topics` | openvela 常用 uORB topic 定义，包括系统、连接、虚拟传感器等。 |
| 22 | `frameworks_system_utils` | 系统基础工具集：gdbus、kvdb、Android 风格 log、atrace。 |
| 23 | `frameworks_system_utils_uv` | libuv 风格的数据库与网络异步接口封装。 |
| 24 | `frameworks_system_vibrator` | 振动功能框架，支持自定义振动模式与跨核调用。 |

---

## 3. 可复用价值评估

### 3.1 高价值（项目核心能力直接依赖）

| 子仓库 | 价值评估 | 与项目需求的对应关系 |
|---|---|---|
| `frameworks_graphics_uikit` | **高** | LVGL UI 核心扩展；提供字体管理、视频控件、异步刷新，直接对应 launcher/电台 UI。 |
| `frameworks_multimedia_media` | **高** | 音频播放、录制、焦点管理、策略路由，是 AI 电台音频与 ASR 录音的基础框架。 |
| `frameworks_multimedia_media_pfw` | **高** | 与 media 配套，用于配置音频路由/音量/设备切换策略，适合电台多音频源场景。 |
| `frameworks_runtimes_services_am` | **高** | 多应用框架核心，管理 Activity 生命周期，launcher_mini 与第三方应用都需要。 |
| `frameworks_runtimes_services_pm` | **高** | 安装/卸载/查询小程序包，第三方 JS/QuickApp/TS 小程序运行能力的前提。 |
| `frameworks_runtimes_services_wm` | **高** | 窗口管理与输入输出，LVGL UI 多窗口/多应用显示必需。 |
| `frameworks_runtimes_services_system_server` | **高** | 作为系统大脑统一拉起 am/pm/wm/brightness，建议整体启用。 |
| `frameworks_runtimes_wasm` | **高** | 提供 WAMR 运行时、libc、内存访问、vendor SDK，是运行 Wasm 小程序的关键。 |
| `frameworks_bluetooth` | **高** | 提供 BT/BLE/BT-Audio（A2DP/AVRCP/HFP/LEA 等），满足蓝牙联网与蓝牙音频需求。 |
| `frameworks_system_topics` | **高** | 提供 uORB topic 抽象，SHTC3/LTR553/LRADC/WS2812 等传感器数据可通过 topic 发布订阅。 |
| `frameworks_system_utils` | **高** | kvdb 持久化配置、Android log、trace，系统级基础依赖。 |
| `frameworks_runtimes_services_xmsdemo` | **高/中** | 含 launcher 参考实现，初学者快速上手，可作为 launcher_mini 原型的参考。 |

### 3.2 中价值（按硬件/产品化需求选装）

| 子仓库 | 价值评估 | 说明 |
|---|---|---|
| `frameworks_runtimes_services_brightness` | **中** | 带屏电台需要自动/手动背光调节；无屏或固定亮度场景可跳过。 |
| `frameworks_system_ota` | **中** | 产品化升级必备，含 AB 分区、升级动画、AVB 验签，但最终 IMG 打包阶段再集成。 |
| `frameworks_system_healthd` | **中** | 电池供电设备需要电量/充电状态；纯外部供电可跳过。 |
| `frameworks_system_charger` | **中** | 同上，仅当设备带电池充电管理时启用。 |
| `frameworks_system_binder` | **中** | XMS 服务间 IPC 可能已使用 Binder 风格机制；通常作为依赖被隐式引入，按需显式启用。 |
| `frameworks_system_utils_uv` | **中** | 若应用/服务基于 libuv 异步模型，可使用其 DB/网络接口。 |
| `frameworks_runtimes_typescript_ts2native` | **中** | 可作为 TS 小程序编译链路之一，但处于实验阶段，不建议单独作为主力。 |
| `frameworks_runtimes_typescript_ts2wasm` | **中** | 与 `wasm` 配套，将 TS 编译为 WasmGC，实验性，需评估 WAMR GC 支持。 |

### 3.3 低价值（当前项目暂不推荐）

| 子仓库 | 价值评估 | 说明 |
|---|---|---|
| `frameworks_connectivity_telephony` | **低** | 面向蜂窝调制解调器（Ofono），R528 电台控制台未规划蜂窝模块，可直接跳过。 |
| `frameworks_security` | **低** | TEE/TA/CA，除非有 DRM/安全启动/密钥隔离需求，否则初期不启用。 |
| `frameworks_security_optee_vela` | **低** | 同上，可信执行环境对电台控制台非必需。 |
| `frameworks_system_vibrator` | **低** | 振动反馈对电台控制台不是核心交互方式，可跳过。 |

---

## 4. 高价值子仓库详解与集成建议

### 4.1 `frameworks_graphics_uikit` — LVGL UI 扩展

**核心能力**

- 字体管理器：支持加载/创建/删除字体、字体缓存、emoji/图片字体（png/bin）。
- 视频控件：在 media 框架基础上扩展 LVGL image 控件，支持 video buffer 控制、获取与渲染。
- 二维码扫描：基于 quirc 库，支持 YUYV/NV12/RGBA8888/RGB565/RGB332 转灰度并返回字符串结果。
- 异步刷新：在 LVGL 下次刷新前/后注册回调，执行耗时/同步任务，避免阻塞渲染。

**集成建议**

1. 在 Kconfig 中启用 `CONFIG_UIKIT` 相关选项（参考子仓库 `Kconfig`）。
2. 字体管理器可集中管理电台 UI 的多语言/图标字体，避免重复 IO；emoji/图片字体适合 WS2812 灯效图标或频道图标。
3. 视频控件用于需要在 LVGL 中嵌入视频预览或可视化频谱的场景；若仅播放音频，可不启用视频控件。
4. 二维码扫描可用于配网（WiFi/蓝牙配网二维码识别）。
5. 异步刷新适合将 ASR 状态更新、网络状态刷新等非渲染任务放到刷新前后执行。

---

### 4.2 `frameworks_multimedia_media` + `frameworks_multimedia_media_pfw` — 音频核心

**核心能力**

- C/S 架构：Client 通过 RPC 将命令发送到 Server，Server loop 执行实际工作；支持跨核/多 CPU。
- Media Player：创建播放实例，支持多格式播放、播放/暂停/停止/快进等控制。
- Media Recorder：创建录制实例，支持音频录制，是 ASR 语音输入的前置能力。
- Media Focus：焦点抢占机制，确保同一时间仅一个应用持有媒体焦点。
- Media Policy：通过 PFW 构造路由策略、音频策略，支持插件扩展、FFmpeg 命令、设备参数设置。
- Media Session：控制者/被控制者架构，统一多客户端的媒体控制与状态通知。
- Media RPC：双 socket（Trans + Notify），支持同步/异步模式。

**`media_pfw` 补充**

- 通用状态机框架，通过 `criteria.txt`（变量定义）和 `settings.pfw`（状态规则）配置。
- 典型变量类型：数值型、枚举型（Exclusive）、掩码型（Inclusive）。
- 动作为插件回调，可用于切换音频输入输出链路、调整采样率/音量。

**集成建议**

1. 在 `menuconfig` 中启用 `CONFIG_MULTIMEDIA_MEDIA` 及 Server/Client 配置，根据 R528 是否多核决定 RPC 是否跨核。
2. 电台播放走 Media Player，ASR 录音走 Media Recorder；通过 Media Focus 避免播放与录音冲突。
3. 使用 Media Policy + PFW 配置：
   - 正常播放 → 扬声器/耳机/蓝牙 A2DP 输出；
   - ASR 唤醒 → 切换到麦克风输入并降低播放音量；
   - 蓝牙通话 → 切换 SCO 链路。
4. 建议先跑通 `mediatool` 测试程序验证播放/录制链路，再集成到 UI。
5. PFW 配置文件路径建议放在 `/etc/media/` 或 board 配置目录，通过 ROMFS 打包进 IMG。

---

### 4.3 `frameworks_runtimes_services_am/pm/wm/system_server` — 多应用框架

#### 4.3.1 `am`（Activity Manager Service）

- 管理 Activity 生命周期（创建、启动、暂停、恢复、销毁）。
- 任务栈/进程管理、Intent 通信、权限检查、应用状态跟踪、多窗口支持。

**示例代码**：

```cpp
Intent intent;
makeIntent(intent);
intent.setFlag(intent.mFlag | Intent::FLAG_ACTIVITY_NEW_TASK);
android::sp<android::IBinder> token = new android::BBinder();
ActivityManager am;
am.startActivity(token, intent, -1);
```

#### 4.3.2 `pm`（Package Manager Service）

- 提供包安装、卸载、查询能力。
- 命令行：`pm install [packagename]`、`pm list`。
- C++ API：`PackageManager::installPackage()`、`getAllPackageInfo()`、`uninstallPackage()`。

#### 4.3.3 `wm`（Window Manager Service）

- 输入管理、输出管理、显示管理。
- 窗口属性/风格、生命周期、事件监听、过渡动效。
- 语言要求：C++11 或以上，依赖 OpenVela Core 服务。

**示例代码**：

```cpp
WindowManager windowManager = (WindowManager) getService(WindowManager::name());
WindowManager.LayoutParams layoutParams = new WindowManager.LayoutParams();
layoutParams.type = WindowManager.LayoutParams.TYPE_APPLICATION;
layoutParams.format = PixelFormat.FORMAT_RGB_888;
layoutParams.width = WindowManager.LayoutParams.MATCH_PARENT;
layoutParams.height = WindowManager.LayoutParams.MATCH_PARENT;
BaseWindow window = new BaseWindow(context, this);
windowManager.addWindow(window, layoutParams, visibility);
```

#### 4.3.4 `system_server`

- 启动并管理 AM、PM、WM、Brightness 服务。
- 编译开关：`SYSTEM_SERVER`、`SYSTEM_ACTIVITY_SERVICE`、`SYSTEM_PACKAGE_SERVICE`、`SYSTEM_WINDOW_SERVICE`、`SYSTEM_BRIGHTNESS_SERVICE`。
- 目标端以 root 权限后台运行 `SystemServer`（二进制名可能为 `systemd`）。

**集成建议**

1. 如果项目需要运行多个应用（launcher + 电台主应用 + 第三方小程序），建议整体启用 XMS 框架：
   - 打开 `SYSTEM_SERVER` 及对应子服务开关。
   - 在 `rcS` 启动脚本中以 `systemserver &` 形式启动系统服务。
2. launcher_mini 可基于 `xmsdemo` 中的 `LauncherApplication` / `HomeActivity` 参考实现改造：
   - 定义 `LauncherApplication` 并 `REGISTER_ACTIVITY(HomeActivity)`；
   - `HomeActivity` 中实现频道卡片、设置入口、小程序列表。
3. 第三方小程序通过 `pm install` 安装后，由 `am.startActivity()` 启动。
4. 窗口层与 LVGL 绑定：在 `wm` 的 `BaseWindow` 渲染回调中调用 LVGL 刷新；输入事件（按键/触屏）由 wm 分发给当前 Activity。
5. 若项目为单应用架构（仅一个电台应用 + 内部页面管理），可暂时不启用 am/pm/wm，直接用 `uikit` + LVGL 管理页面；但为了未来扩展第三方小程序，建议初期即启用 XMS。

---

### 4.4 `frameworks_runtimes_wasm` + `ts2wasm` + `ts2native` — 第三方小程序运行能力

#### 4.4.1 `frameworks_runtimes_wasm`

- 基于 WAMR（WebAssembly Micro Runtime）的 Wasm 运行环境。
- 模块：
  - `chre`：Context Hub Runtime Environment 到 Wasm 运行时能力。
  - `libc`：Wasm 用 C 标准库。
  - `memory`：Wasm 与 Native 内存访问管理。
  - `vela-sysroot`：构建 Wasm 应用所需头文件。
  - `vendor`：第三方芯片 SDK，目前支持 Bouffalo Lab BL616。
- 使用步骤：
  1. `menuconfig` 启用 WAMR；
  2. 启用 `WASM_LIBC` 或 `WASM_MEMORY_OPERATIONS`；
  3. 通过 `iwasm` 命令行或 SDK 编译运行 Wasm 应用。

#### 4.4.2 `frameworks_runtimes_typescript_ts2wasm`（Wasmnizer-ts）

- 将 TypeScript 编译为 **WasmGC** 字节码。
- 三组件：`ts2wasm-compiler`、`ts2wasm-stdlib`、`ts2wasm-runtime-library`（libdyntype、libstruct-indirect、libstd）。
- 运行时需要支持 WasmGC、Exception Handling、stringref 等 proposal，目前 WAMR 已实现相关 host API。
- **注意**：项目 README 明确标注 "highly experimental and under active development, DO NOT use in production"。

#### 4.4.3 `frameworks_runtimes_typescript_ts2native`

- 将 TypeScript 静态编译为 Native C/C++ 代码。
- 编译产物为 `.c/.cc`，再编译为 `.so`，通过 `runtime/tsshell` 运行。
- 同样属于实验性能力。

**集成建议**

1. **主推 Wasm 路线**：
   - 第三方小程序/QuickApp 优先编译为 Wasm，由 WAMR 加载运行。
   - 启用 `frameworks_runtimes_wasm` 中的 `libc` 与 `memory` 模块，确保系统能力可导入 Wasm。
   - 为小程序暴露的 openvela API（网络、蓝牙、媒体、传感器）通过 `vela-sysroot`/扩展模块注册到 WAMR。
2. **TS 小程序**：
   - 使用 `ts2wasm` 将 TypeScript 源码编译为 WasmGC，再交给 WAMR 运行。
   - 需确认 R528 上的 WAMR 版本支持 WasmGC；如不支持， fallback 到 `ts2native` 生成 Native 库。
3. **JS/QuickApp**：
   - openvela 快应用（QuickApp）基于 QuickJS 引擎，由 C++ 实现系统 API（蓝牙 README 中已提到 QuickApp Feature）。
   - 该 QuickJS/QuickApp 运行时不属于本 frameworks 清单，可能在其他仓库（如 `frameworks_runtimes_lite` 或 vendor 层），需额外定位。
   - 若必须运行 JS 小程序，建议调研 openvela 官方 QuickApp 框架与 WAMR 的对应关系，决定使用 QuickJS 还是 Wasm 方案。
4. **风险**：`ts2wasm`/`ts2native` 均为实验性，不建议作为唯一小程序方案；生产环境建议以 Native/QuickJS 为主，Wasm 为扩展。

---

### 4.5 `frameworks_bluetooth` — 蓝牙联网与音频

**核心能力**

- 通过 Bluetooth 5.4 认证。
- 支持 BR/EDR/BLE、GAP、L2CAP、GATT Client/Server。
- 音频相关：A2DP SRC/SNK、AVRCP CT/TG、HFP AG/HF、LEA（TMAP/CAP/BAP/MCP/CCP/VCP）。
- 数据/外设：PAN、SPP、HID、HOGP、Mesh。
- 支持 Zephyr、Bluez、Bluedroid、Barrot 等多种协议栈。
- 提供 QuickApp Feature 和 NDK 接口两种应用开发方式。
- 驱动架构：芯片厂商实现 `struct bt_driver_s`（open/send/ioctl/close）并注册。

**集成建议**

1. 根据 R528 蓝牙模组选择协议栈与 HCI 驱动（常见为 UART/BTH4）：
   - 实现 `bt_driver_s` 并调用 `bt_driver_register()`；
   - 收到 HCI 数据时调用 `bt_netdev_receive()`。
2. 电台场景建议启用：
   - **BLE**：用于配网、OTA、手机 App 控制；
   - **A2DP SNK**：作为蓝牙音箱接收手机音频；
   - **AVRCP CT/TG**：控制播放/暂停/切歌；
   - **HFP HF**：蓝牙通话音频路由到电台。
3. 蓝牙控制 UI 可通过 QuickApp/NDK 调用 `frameworks_bluetooth` API；驱动层对接 R528 的 UART/USB HCI。
4. 蓝牙与 WiFi 共存问题需在 board 配置中协调天线/总线资源。

---

### 4.6 `frameworks_system_topics` — 传感器数据总线

**核心能力**

- 定义系统级 uORB topic，包括 connectivity、location、media、miai、sensor、system 等类型。
- 应用不直接访问驱动节点，而是通过 uORB API 订阅 topic 获取 sensor 事件。
- 每个 topic 支持多实例（instance）、采样率（interval）、batch latency、持久化（persist）通知类 topic。
- 通过字符设备节点 + 环形缓冲区实现发布/订阅，支持 `poll`。

**集成建议**

1. 在 NuttX 中启用 `CONFIG_UORB` 与 `CONFIG_UORB_LISTENER`。
2. 板载传感器（SHTC3 温湿度、LTR553 光线/接近、LRADC 按键/电池、WS2812 灯效）的驱动：
   - 将原始数据发布到对应 topic（如 `sensor_humi`、`sensor_temp`、`sensor_light`、`sensor_prox`）；
   - 自定义 topic 可声明 `ORB_DECLARE(xxx)` 并定义 `ORB_DEFINE(xxx, xxx_s)`。
3. AI 电台应用订阅 sensor topic：
   - 光线传感器 → 自动调节屏幕亮度（配合 brightness 服务）；
   - 温湿度 → 环境信息显示；
   - LRADC → 物理按键事件；
   - WS2812 → 灯效控制（可定义为自定义 topic 或直接使用 PWM/GPIO 驱动）。
4. 使用 `uorb_listener` 工具在开发阶段验证 sensor 数据流。

---

### 4.7 `frameworks_system_utils` — 系统基础工具

**核心能力**

- `kvdb`：本地键值对数据库，支持持久化（`persist.` 前缀）、跨核调用（Unix domain socket/rpmsg）、UnQLite/MTD CONFIG/File 三种后端；提供 `getprop`/`setprop` 命令。
- `log`：Android 风格 Log API 封装，便于 Android 应用移植。
- `trace`：用户空间 atrace 打点，配合 Perfetto 可视化。
- `gdbus`：D-Bus 接口封装，简化 D-Bus 调用。

**集成建议**

1. **kvdb 必启**：
   - 保存用户配置（音量、频道、WiFi 密码、蓝牙配对信息、主题等）；
   - 选择 `CONFIG_KVDB_DIRECT`（单核/无需跨核）或 `CONFIG_KVDB_SERVER`（多核）；
   - 后端根据存储介质选择 `CONFIG_KVDB_FILE`（文件系统）或 `CONFIG_KVDB_NVS`（Flash）。
2. **log**：统一使用 Android log 风格，便于与 media/am/pm/wm 等服务的日志对齐。
3. **trace**：在调试 ASR 延迟、音频播放卡顿、UI 刷新问题时启用 atrace。
4. **gdbus**：如果启用 Telephony 或其他 D-Bus 服务需要；电台项目本身不直接依赖，但 system services 可能间接使用。

---

### 4.8 `frameworks_runtimes_services_xmsdemo` — launcher 原型参考

**核心能力**

- 提供 XMS 多应用框架的完整示例，重点展示包管理和 Activity 管理。
- 含 `launcher` 模块，演示如何定义 `LauncherApplication` 和 `HomeActivity`：

```cpp
class LauncherApplication : public Application {
    void onCreate() override { REGISTER_ACTIVITY(HomeActivity) }
    void onForeground() override {}
    void onBackground() override {}
    void onDestroy() override {}
};
#define APPLICATION LauncherApplication
#include <app/AppMain.h>
```

**集成建议**

1. 将 `xmsdemo/launcher` 作为 launcher_mini 的启动模板，替换为电台频道卡片、设置、小程序入口。
2. 学习其 `HomeActivity` 生命周期实现，确保在 `onResume`/`onPause` 中正确处理音频焦点。
3. 不要直接用于生产，需根据 UI 设计重写布局和交互。

---

## 5. 需求映射与缺失项说明

| 项目需求 | 对应 openvela frameworks 能力 | 缺失/注意 |
|---|---|---|
| **WiFi/蓝牙联网** | `frameworks_bluetooth` 覆盖蓝牙；WiFi 不在本清单，使用 NuttX 无线协议栈。 | 需确认 R528 WiFi 驱动与 wpa_supplicant 集成。 |
| **音频/ASR** | `frameworks_multimedia_media` + `media_pfw` 提供播放、录制、焦点、策略。 | ASR 算法本身（如离线唤醒、云端 ASR）需另选引擎；media 提供录音数据。 |
| **板载传感器 SHTC3/LTR553/LRADC/WS2812** | `frameworks_system_topics` 提供 uORB 数据总线；实际驱动在 NuttX。 | 需在驱动层发布 topic 或自定义 topic。 |
| **launcher_mini 桌面** | `frameworks_runtimes_services_xmsdemo` 含 launcher 参考；`am/pm/wm/system_server` 提供运行环境。 | 需重写 UI 适配电台场景。 |
| **第三方 JS/QuickApp/TS 小程序** | `frameworks_runtimes_wasm` + `ts2wasm/ts2native`；QuickApp 基于 QuickJS（不在本清单）。 | 实验性工具链需谨慎；建议调研 openvela QuickApp 完整仓库。 |
| **最终打包 IMG** | `frameworks_system_ota` 提供打包/验签/bootctl 参考；最终打包由 NuttX/openvela 构建系统完成。 | 需配置 ROMFS、分区表、AVB 签名等。 |

---

## 6. 推荐集成顺序

1. **第一阶段：系统基础与 UI**
   - 启用 `system_utils`（kvdb、log）。
   - 启用 `graphics_uikit` + LVGL，跑通基础页面。
2. **第二阶段：音频能力**
   - 启用 `multimedia_media` + `media_pfw`，用 `mediatool` 验证播放/录音。
3. **第三阶段：多应用框架**
   - 启用 `system_server`、`am`、`pm`、`wm`。
   - 基于 `xmsdemo` 实现 launcher_mini。
4. **第四阶段：联网与传感器**
   - 启用 `bluetooth`，对接 R528 HCI 驱动。
   - 配置 NuttX WiFi 驱动。
   - 启用 `system_topics`，将传感器接入 uORB。
5. **第五阶段：小程序与产品化**
   - 启用 `frameworks_runtimes_wasm`，集成 WAMR。
   - 按需启用 `ts2wasm`/`ts2native` 作为 TS 小程序编译链路。
   - 按需启用 `system_ota`、`healthd`、`charger`、`brightness`。
   - 最终使用 openvela 构建系统打包 IMG，参考 `ota/tools` 进行签名与 AB 分区配置。

---

## 7. 参考链接

- openvela frameworks 顶层仓库：https://gitee.com/open-vela/frameworks
- `frameworks_graphics_uikit`：https://gitee.com/open-vela/frameworks_graphics_uikit
- `frameworks_multimedia_media`：
  - Client 模块：https://gitee.com/open-vela/frameworks_multimedia_media/blob/dev/client/README_zh-cn.md
  - Server 模块：https://gitee.com/open-vela/frameworks_multimedia_media/blob/dev/server/README_zh-cn.md
  - Mediatool：https://gitee.com/open-vela/frameworks_multimedia_media/blob/dev/mediatool_zh-cn.md
- `frameworks_multimedia_media_pfw`：https://gitee.com/open-vela/frameworks_multimedia_media_pfw
- `frameworks_runtimes_services_am`：https://gitee.com/open-vela/frameworks_runtimes_services_am
- `frameworks_runtimes_services_pm`：https://gitee.com/open-vela/frameworks_runtimes_services_pm
- `frameworks_runtimes_services_wm`：https://gitee.com/open-vela/frameworks_runtimes_services_wm
- `frameworks_runtimes_services_system_server`：https://gitee.com/open-vela/frameworks_runtimes_services_system_server
- `frameworks_runtimes_services_xmsdemo`：https://gitee.com/open-vela/frameworks_runtimes_services_xmsdemo
- `frameworks_runtimes_wasm`：https://gitee.com/open-vela/frameworks_runtimes_wasm
- `frameworks_runtimes_typescript_ts2wasm`：https://gitee.com/open-vela/frameworks_runtimes_typescript_ts2wasm
- `frameworks_runtimes_typescript_ts2native`：https://gitee.com/open-vela/frameworks_runtimes_typescript_ts2native
- `frameworks_bluetooth`：https://gitee.com/open-vela/frameworks_bluetooth
- `frameworks_system_topics`：https://gitee.com/open-vela/frameworks_system_topics
- `frameworks_system_utils`：https://gitee.com/open-vela/frameworks_system_utils
- `frameworks_system_ota`：https://gitee.com/open-vela/frameworks_system_ota
- `frameworks_system_healthd`：https://gitee.com/open-vela/frameworks_system_healthd
- `frameworks_system_charger`：https://gitee.com/open-vela/frameworks_system_charger

---

*报告生成时间：2026-07-07*
