# 在openvela上构建lvgl页面程序

> Wiki Token: JtP2wBTl2iVketkcQ8ucFt4Znpd
> URL: https://rivotek.feishu.cn/wiki/JtP2wBTl2iVketkcQ8ucFt4Znpd
> 爬取时间: 2026-07-06 (WebFetch补充)

---

## 在openvela上构建lvgl页面程序

### 概述

SDK 包含一个可运行的"迷你启动器"应用：**luncher_mini**。该程序基于 LVGL 提供简单的桌面式 UI，集成了 LED 控制与传感器（温度/湿度/接近）展示，适合作为学习 LVGL 与设备外设交互的示例。

本指南侧重于 luncher_mini 本身的实现逻辑、关键源码位置和构建/运行方法，帮助新手快速上手修改与调试。

### 功能概览

- 启动 LVGL 并创建主界面（时间/日期、4 个功能窗口：T&H / Light / Prox / About）。
- 灯光控制（通过 `lv_demo_panel_rgb_control.c` 实现 LED 控制封装，包括开/关、颜色、亮度）。
- 传感器订阅：通过 uORB 订阅温度、湿度、接近数据并在界面显示。
- 使用 LVGL 的主题/观察者机制更新 UI（时间、传感器数据、交互事件）。

### 关键源码文件

- **luncher_mini.c** — 应用入口及大部分实现：UI 创建、时间/传感器定时器、事件回调、LED 适配层调用、主循环。
- **lv_demo_panel_rgb_control.c / .h** — LED 控制实现（对 WS2812 或 HAL LED 的封装）。
- **Makefile, Kconfig** — 本 app 的构建与配置项（`CONFIG_LUNCHER_MINI_APP`、`CONFIG_LUNCHER_MINI_APP_PROGNAME` 等）。
