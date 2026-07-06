# 飞书Wiki文档爬取完成索引

> 爬取来源: https://rivotek.feishu.cn/wiki/Onndw4lmniFBnEk0Rb7cDbwOnTc
> 爬取时间: 2026-07-06
> 爬取方法: Playwright (动态渲染) + WebFetch (内容补充)
> 文档总数: 14

## 文档分类总览

### 00_首页
- [001_Gemini-S1 开发板.md](00_首页/001_Gemini-S1%20开发板.md) - 开发板总览、核心规格、开发资源

### 01_入门指南
- [002_开发编译环境准备.md](01_入门指南/002_开发编译环境准备.md) - Ubuntu 22.04 编译环境搭建
- [014_LTR553 环境光与接近传感器实现概述.md](01_入门指南/014_LTR553%20环境光与接近传感器实现概述.md) - Sensor驱动开发示例1

### 03_驱动开发
- [005_OpenVela (R528) 按键驱动实现概要.md](03_驱动开发/005_OpenVela%20%28R528%29%20按键驱动实现概要.md) - LRADC按键驱动
- [006_WiFi适配驱动指南.md](03_驱动开发/006_WiFi适配驱动指南.md) - WiFi STA模式配置命令
- [007_蓝牙适配指南.md](03_驱动开发/007_蓝牙适配指南.md) - BLE开发概述
- [008_OpenVela (R528) Audio 驱动设计与实现流程文档.md](03_驱动开发/008_OpenVela%20%28R528%29%20Audio%20驱动设计与实现流程文档.md) - 音频驱动架构

### 04_场景应用
- [010_在openvela上构建lvgl页面程序.md](04_场景应用/010_在openvela上构建lvgl页面程序.md) - luncher_mini LVGL应用
- [011_DeepSeek LLM Demo 实现设计与新手指南.md](04_场景应用/011_DeepSeek%20LLM%20Demo%20实现设计与新手指南.md) - DeepSeek大模型集成

### 05_故障排除
- [012_调试与故障排除.md](05_故障排除/012_调试与故障排除.md) - 串口/ADB调试、烧录失败处理

### 06_内核与架构
- [003_Openvela nuttx内核开发概述.md](06_内核与架构/003_Openvela%20nuttx内核开发概述.md) - NuttX内核分层架构

### 99_其他
- [004_SHTC3 温湿度传感器实现概述.md](99_其他/004_SHTC3%20温湿度传感器实现概述.md) - Sensor驱动开发示例2
- [009_OpenVela 第一个应用实践.md](99_其他/009_OpenVela%20第一个应用实践.md) - led_rgb应用开发
- [013_openvela-Backtrace 使用指南.md](99_其他/013_openvela-Backtrace%20使用指南.md) - 栈回溯调试

## 关键技术点速查

### 硬件平台
- **主控**: Allwinner R528 双核 Cortex-A7 @ 1.2GHz
- **内存**: 128MB DDR3
- **OS**: openvela (基于 NuttX RTOS)
- **网络**: WiFi (Realtek) + BLE
- **音频**: 内置 Audio Codec + 麦克风
- **传感器**: SHTC3 (温湿度, I2C 0x70), LTR553 (光感/接近, I2C 0x23)
- **按键**: LRADC (Low Resolution ADC)
- **LED**: WS2812 RGB
- **调试**: UART2 @ 1500000 baud, ADB

### 构建命令
```bash
# 编译
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh

# menuconfig
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh menuconfig

# 打包固件
source envsetup.sh && lunch_nuttx && pack
```

### WiFi 配置
```bash
wapi mode wlan0 2              # STA模式
wapi scan wlan0                # 扫描热点
wapi psk wlan0 "密码" 3         # 设置密码
wapi essid wlan0 "SSID" 1      # 连接热点
renew wlan0                    # 获取IP
ipconfig                       # 查看IP
```

### 调试命令
```bash
adb logcat                     # 日志
adb shell                      # 命令行
# 串口调试: UART2, 1500000 baud
```

### 驱动架构
所有驱动遵循 NuttX "Upper Half + Lower Half" 模型：
- 硬件层 → HAL层 → Lower Half驱动 → Upper Half字符设备 → 应用层
- 传感器通过 uORB 发布数据
- 应用通过 POSIX 文件 API 访问设备

### 关键源码路径
- **应用**: `vendor/allwinnertech/apps/`
  - `led_rgb/` - LED控制
  - `luncher_mini/` - LVGL桌面
  - `deepseek_demo/` - DeepSeek LLM
- **驱动**: `vendor/allwinnertech/chips/r528/drivers/`
  - `rtos-hal/hal/source/sensor/` - 传感器驱动
  - `realtek_ieee80211/` - WiFi驱动
- **音频**: `vendor/allwinnertech/chips/r528/components/audio/`
- **板级配置**: `vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh`

### DeepSeek LLM 集成
- 位置: `vendor/allwinnertech/apps/deepseek_demo`
- 特性: CLI模式 + Agent服务模式 + MCP工具调用
- 通信: HTTP协议调用DeepSeek API

## 未实现/未公开文档
首页标注"板载功能持续更新中"，以下功能暂无文档：
- Usb host
- Camera
- Audio DSP
- SPI屏驱动 (用户提到但飞书Wiki未公开)
- 触摸屏驱动 (用户提到但飞书Wiki未公开)
- 软件烧录指南 (用户提到但飞书Wiki未公开)
