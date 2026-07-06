# OpenVela 第一个应用实践

> Wiki Token: O7P3wW0IhivuEKkSvNXclR8enZc
> URL: https://rivotek.feishu.cn/wiki/O7P3wW0IhivuEKkSvNXclR8enZc
> 爬取时间: 2026-07-06 (WebFetch补充)
> 最新修改: 01月18日

---

## OpenVela 第一个应用实践

本文档将以 `led_rgb` 应用为例，指导开发者如何在 OpenVela 系统（基于 NuttX）中从零开始创建一个新的应用程序。我们将深入解析必要的构建文件（Kconfig, Make.defs, Makefile）以及源代码实现。

### 1. 目标

创建一个名为 `rgb_led` 的命令行应用程序：

- 可以通过命令行参数 `rgb_led <color_value>` 设置 LED 颜色。
- 如果不带参数，则循环显示预定义的颜色序列。
- 支持直接调用 HAL 接口或通过文件系统 `/dev/leds0` 两种方式操作。

### 2. 目录结构准备

首先，在 `vendor/allwinnertech/apps/` 目录下创建一个新的文件夹 `led_rgb`。

```bash
mkdir -p vendor/allwinnertech/apps/led_rgb
```

### 3. 构建文件说明

#### 3.1 Kconfig

定义应用的配置选项，用于 menuconfig 中启用/禁用该应用。

#### 3.2 Make.defs

定义编译规则和源文件列表。

#### 3.3 Makefile

具体的编译规则，包含源文件、头文件路径、链接库等。

### 4. 源码实现

- 通过 HAL 接口或 `/dev/leds0` 文件操作控制 LED
- 解析命令行参数获取颜色值
- 主循环实现颜色切换
