# `luncher_mini` — 入门指南（面向新手）

本目录包含一个可运行的“迷你启动器”应用：`luncher_mini`。该程序基于 LVGL 提供简单的桌面式 UI，集成了LED控制与传感器（温度/湿度/接近）展示，适合作为学习 LVGL 与设备外设交互的示例。

本指南侧重于 `luncher_mini` 本身的实现逻辑、关键源码位置和构建/运行方法，帮助新手快速上手修改与调试。

## 功能概览

- 启动 LVGL 并创建主界面（时间/日期、4 个功能窗口：T&H / Light / Prox / About）。
- 灯光控制（通过 `lv_demo_panel_rgb_control.c` 实现 LED 控制封装，包括开/关、颜色、亮度）。
- 传感器订阅：通过 uORB 订阅温度、湿度、接近数据并在界面显示。
- 使用 LVGL 的主题/观察者机制更新 UI（时间、传感器数据、交互事件）。

## 关键源码文件

- `luncher_mini.c` — 应用入口及大部分实现：UI 创建、时间/传感器定时器、事件回调、LED 适配层调用、主循环。
- `lv_demo_panel_rgb_control.c` / `.h` — LED 控制实现（对 WS2812 或 HAL LED 的封装）。
- `Makefile`, `Kconfig` — 本 app 的构建与配置项（`CONFIG_LUNCHER_MINI_APP`、`CONFIG_LUNCHER_MINI_APP_PROGNAME` 等）。

当你修改功能时，优先查看 `luncher_mini.c` 中下列函数：

- `luncher_mini_main()` — 程序入口，执行 LVGL 初始化、字体、传感器与 LED 适配器初始化、创建定时器并进入主循环。
- `create_main_screen()` — 构造主 UI（背景、时间/日期、4 个窗口）。
- `create_light_control_window()` / `close_light_window_cb()` — 灯光控制窗口的创建与关闭逻辑。
- `lv_demo_init_sensor_subscriptions()` / `lv_demo_update_sensor_data()` / `lv_demo_cleanup_sensor_subscriptions()` — 传感器订阅与数据更新逻辑。
- `led_adapter_*()` 家族（init/on/off/set_brightness/diagnose）— 调用 `lv_demo_panel_rgb_control` 中的 LED 控制接口。

## 配置（启用/参数）

- 在 `Kconfig` 中：
  - `CONFIG_LUNCHER_MINI_APP` — 启用本 app。
  - `CONFIG_LUNCHER_MINI_APP_PROGNAME` — 可设置二进制在 NSH 中的名字（默认 `luncher_mini`）。
  - `CONFIG_LUNCHER_MINI_APP_PRIORITY` / `CONFIG_LUNCHER_MINI_APP_STACKSIZE` — 任务优先级与栈大小。

此外，LVGL 与 NuttX 后端相关选项（如 `CONFIG_LV_USE_NUTTX_LCD`、`CONFIG_INPUT_TOUCHSCREEN` 等）影响显示与输入设备初始化，请确保对应设备驱动已启用。

## 构建与运行（步骤）

1. 在工程根启用本应用（通过 menuconfig 或手动修改配置文件）：

```bash
# 推荐方式：在工程根运行配置界面，打开应用项并启用
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay menuconfig
# 找到 Applications -> vendor/allwinnertech -> luncher_mini，勾选启用
```

2. 编译整机镜像（或使用项目提供的构建脚本）：

```bash
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay -j8
# 或直接 make（按你的工程构建流程）
make
```

1. 烧写或启动镜像到目标板：

```bash
luncher_mini &
# 如果是内置启动（built-in），则系统会在启动时自动运行
```

## 运行时行为（新手须知）

- 程序会先调用 `lv_init()` 并通过 `lv_nuttx_init()` 初始化 NuttX 后端（`fb` / `input` 路径会根据配置填充）。
- 之后加载字体、创建 UI，并注册若干定时器：时间更新（每秒）和传感器更新（示例中每秒）。
- LED 适配器会在初始化后执行快速开/关自检（若硬件存在）。
- 主循环使用 LVGL 的定时处理；若启用了 libuv 后端则会进入 libuv 循环。

## 快速修改指南

- 修改界面布局：编辑 `create_main_screen()`，使用 LVGL 的对象 API（`lv_obj_create`、`lv_label_create`、`lv_btn_create` 等）。
- 修改 LED 行为：编辑 `lv_demo_panel_rgb_control.c` 或 `led_adapter_*()`，可以调整对 WS2812 的路径（`CONFIG_WS2812_DEV_PATH`）或 HAL 调用。
- 调整传感器源：`lv_demo_init_sensor_subscriptions()` 中使用 uORB 订阅，替换为你的传感器接口或调整 topic。
- 调试日志：文件中使用 `LV_LOG_*` 与 `syslog` 打印，查看串口或 `dmesg` 输出以获取运行时信息。

## 常见问题与排查步骤

- 无界面显示：确认 `CONFIG_LV_USE_NUTTX_LCD` 与 framebuffer 设备（如 `/dev/lcd0`）是否存在；检查 `lv_nuttx_init()` 返回的 `result.disp` 是否为 NULL。
- 触摸无响应：确保 `CONFIG_INPUT_TOUCHSCREEN` 已启用且 `info.input_path` 指向正确设备；检查 `lv_nuttx` 后端的 input 是否注册。
- LED 无反应：确认设备节点（如 `/dev/leds0` 或 HAL）是否存在，并检查 `CONFIG_LED_RGB_WS2812` 是否匹配硬件。
- 传感器数据不更新：检查 uORB topic 是否有数据，查看 `lv_demo_init_sensor_subscriptions()` 的订阅 fd 是否有效。

## 代码阅读建议（学习路径）

1. 从 `luncher_mini_main()` 阅读程序启动顺序（初始化 → UI → 定时器 → 主循环）。
2. 读懂 UI 创建（`create_main_screen()`）如何使用 LVGL 对象组织界面。
3. 理解事件回调（如 `window_click_cb()`、`light_switch_event_cb()`）如何驱动应用逻辑与外设调用。
4. 阅读 `lv_demo_panel_rgb_control.c` 理解 LED 控制的封装层（对硬件访问的抽象）。
