# Gemini-S1 (R528) 开发板对 openvela 的支持

\[ [English](README.md) | 简体中文 \]

## 简介

本目录为 **润芯微 Gemini-S1**（全志 R528 平台）开发板提供 openvela 板级支持（BSP），是 openvela AI 硬件大赛推荐的开发板之一。Gemini-S1 原生搭载 openvela 系统，面向物联网开发者、创客与教育场景。

主要硬件规格（以润芯微官方文档为准）：

- **主控**：全志 R528，双核 Arm Cortex-A7
- **显示**：板载 2.8 寸 SPI 屏，同时支持 7 寸 MIPI 大屏
- **音频**：板载麦克风，支持语音交互
- **无线**：Wi-Fi + 蓝牙双模
- **接口**：GPIO、I2C、SPI、UART、ADC、PCM 等
- **传感器**：板载温湿度、光照、接近等环境传感器，支持外挂扩展

> 完整硬件说明、引脚定义、外设列表与驱动开发手册，请参考润芯微官方文档：[Gemini-S1 开发板](https://rivotek.feishu.cn/wiki/Onndw4lmniFBnEk0Rb7cDbwOnTc)。

> 大赛参赛者请基于大赛分支 `dev-ai-contest-2026` 进行开发。

## 目录结构

```
r528s3-gemini-s1/
├── Kconfig            # 板级 Kconfig 选项
├── include/           # 板级头文件（board.h、内存映射等）
├── src/               # 板级 bring-up 源码（启动、初始化、LED 等）
├── scripts/           # 链接脚本与构建规则（Make.defs、sdram.ld.S）
├── build/             # 固件打包与 OTA 相关脚本
└── configs/           # 编译配置
    ├── nsh            # 基础 NSH 命令行配置
    ├── nsh_minidisplay# 带屏显示的 NSH 配置
    └── bootloader     # Bootloader 配置
```

## 编译

openvela 工程根目录的 `build.sh` 是统一编译入口。以基础 `nsh` 配置为例：

```bash
# 可选：仅在切换配置或修改 menuconfig 后需要清理
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh -j8 distclean

# 编译
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh -j8
```

将命令中的 `nsh` 替换为 `nsh_minidisplay` 或 `bootloader` 即可编译其他配置。

## 固件打包与部署

完整的固件打包（`pack`）、字体配置、应用部署与烧录流程，请参考开发板相关文档：

- [开发板适配案例](https://github.com/open-vela/docs/blob/dev-ai-contest-2026/zh-cn/dev_board/Development_Board.md)
- [芯片移植（Chip Porting）章节](https://github.com/open-vela/docs/blob/dev-ai-contest-2026/zh-cn/chip_porting/porting_guide.md)

## 许可协议

本目录下文件遵循各自文件头部声明的许可协议。
