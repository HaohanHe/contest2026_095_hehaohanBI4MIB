# Gemini-S1 (R528) Board Support for openvela

\[ English | [简体中文](README_zh-cn.md) \]

## Overview

This directory provides the openvela board support package (BSP) for the **Rivotek Gemini-S1** development board (Allwinner R528 platform). It is one of the recommended boards for the openvela AI Hardware Contest. Gemini-S1 ships with openvela natively and targets IoT developers, makers, and education scenarios.

Key hardware specifications (per Rivotek's official documentation):

- **SoC**: Allwinner R528, dual-core Arm Cortex-A7
- **Display**: onboard 2.8-inch SPI screen, also supports a 7-inch MIPI display
- **Audio**: onboard microphone for voice interaction
- **Wireless**: Wi-Fi + Bluetooth dual mode
- **Interfaces**: GPIO, I2C, SPI, UART, ADC, PCM, etc.
- **Sensors**: onboard temperature/humidity, ambient light, and proximity sensors, with external expansion support

> For the full hardware description, pin definitions, peripheral list, and driver development manual, refer to Rivotek's official documentation: [Gemini-S1 Development Board](https://rivotek.feishu.cn/wiki/Onndw4lmniFBnEk0Rb7cDbwOnTc).

> Contest participants should develop on the contest branch `dev-ai-contest-2026`.

## Directory Structure

```
r528s3-gemini-s1/
├── Kconfig            # Board-level Kconfig options
├── include/           # Board headers (board.h, memory map, etc.)
├── src/               # Board bring-up sources (boot, init, LEDs, etc.)
├── scripts/           # Linker script and build rules (Make.defs, sdram.ld.S)
├── build/             # Firmware packaging and OTA scripts
└── configs/           # Build configurations
    ├── nsh            # Basic NSH shell configuration
    ├── nsh_minidisplay# NSH configuration with display
    └── bootloader     # Bootloader configuration
```

## Build

The `build.sh` script at the openvela project root is the unified build entry point. Using the basic `nsh` configuration as an example:

```bash
# Optional: only needed when switching configs or after menuconfig changes
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh -j8 distclean

# Build
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh -j8
```

Replace `nsh` with `nsh_minidisplay` or `bootloader` to build the other configurations.

## Firmware Packaging and Deployment

For the full firmware packaging (`pack`), font setup, application deployment, and flashing flow, refer to the development board documentation:

- [Development Board adaptation cases](https://github.com/open-vela/docs/blob/dev-ai-contest-2026/en/dev_board/Development_Board.md)
- [Chip Porting guide](https://github.com/open-vela/docs/blob/dev-ai-contest-2026/en/chip_porting/porting_guide.md)

## License

Files in this directory follow the license declared in each file's header.
