# Gemini-S1 开发板

> Wiki Token: Onndw4lmniFBnEk0Rb7cDbwOnTc
> URL: https://rivotek.feishu.cn/wiki/Onndw4lmniFBnEk0Rb7cDbwOnTc
> 爬取时间: 2026-07-06 (WebFetch补充)
> 最新修改: 06月11日

---

## Gemini-S1 开发板

**Gemini-S1开发板：你的智能硬件创意孵化器**

想让智能硬件创意快速落地？想轻松实现语音交互、环境监测、无线联网的项目？Gemini-S1开发板就是你的理想选择！

Gemini-S1 是一款集高性能计算、丰富交互方式、多模联网能力于一体的智能硬件开发平台，专为物联网开发者、创客、教育机构和智能家居爱好者设计。它集高性能内核、直观交互、智能语音、多模联网于一体，并原生搭载开放的 openvela 系统，为创客、学生、开发者提供从硬件到软件的一站式智能硬件开发平台。

### ▶ 强劲内核，稳定可靠

基于全志 R528 双核 Arm® Cortex®-A7 高性能主控，带来充沛算力，轻松应对复杂任务——无论是数据处理还是多设备协同，都能稳定运行，为项目筑牢性能基础。

### ▶ 核心规格

- **主控**: Allwinner R528 双核 Cortex-A7 @ 1.2GHz
- **内存**: 128MB DDR3
- **存储**: SPI NAND Flash / SD 卡
- **OS**: openvela (基于 NuttX RTOS)
- **网络**: WiFi (Realtek) + BLE
- **音频**: 内置 Audio Codec + 麦克风 + 扬声器接口
- **显示**: SPI LCD 屏接口
- **传感器**: SHTC3 (温湿度), LTR553 (光感/接近), 按键 (LRADC)
- **LED**: WS2812 RGB LED
- **调试**: USB 转串口 (UART2, 1500000 baud), ADB

### ▶ 开发资源

- **SDK 路径**: `vendor/allwinnertech/`
- **构建配置**: `vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh`
- **构建命令**: `./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh`
- **menuconfig**: `./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh menuconfig`
- **固件打包**: `source envsetup.sh && lunch_nuttx && pack`

### ▶ 应用示例

- `led_rgb` - LED 灯控制程序
- `luncher_mini` - LVGL 桌面启动器
- `deepseek_demo` - DeepSeek LLM 对话应用
- `factory_test` - 工厂测试程序
