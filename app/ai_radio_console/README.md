# AI Radio Console for openvela

> Contest 2026 - Team 095

AI-powered amateur radio controller running on Gemini-S1 (Allwinner R528) with openvela OS.

## Features

- **Mayday/SOS Detection** - Real-time distress signal detection with keyword matching and confidence scoring
- **CW Morse Decoder** - Real-time Morse code decoding with adjustable WPM (5-40 WPM)
- **Interference Analysis** - FFT-based spectrum analysis with malicious signal detection
- **AI Frequency Recommendation** - AI-powered band/propagation-based frequency suggestions
- **QSO Logging** - ADIF-compatible contact logging with net manager
- **Real-time Translation** - Multi-language QSO translation (EN/JA/RU to ZH)
- **Satellite Pass Alerts** - Satellite pass predictions for ISS, SO-50, AO-91, etc.
- **SiliconFlow ASR/LLM** - 通过 HTTPS/TLS 与 SiliconFlow 云端通信（依赖 `CONFIG_NETUTILS_MBEDTLS`），支持流式 ASR 与对话摘要
- **Real I2S Audio Capture** - 从板载麦克风通过 I2S PCM 设备采集音频，含健壮的错误处理与线程安全
- **Environmental Sensor Display** - 读取 SHTC3（温湿度）与 LTR553（光感/接近）并在 LVGL 界面实时展示

## Hardware

- **Board**: Runxinwei Gemini-S1 (Allwinner R528, dual Cortex-A7)
- **Display**: 2.8" ILI9341 SPI LCD (320x240)
- **Audio**: Onboard microphone + speaker
- **Connectivity**: Wi-Fi + BLE
- **Sensors**: Temperature/Humidity (SHTC3), Light/Proximity (LTR553)

## Project Structure

```
app/ai_radio_console/
├── Kconfig              # Menuconfig option
├── Makefile             # NuttX build rules
├── Make.defs            # Application registration
├── CMakeLists.txt       # CMake build config
├── include/             # Header files
│   ├── radio_config.h
│   ├── agent_bridge.h
│   ├── audio_capture.h
│   ├── cw_decoder.h
│   ├── freq_recommender.h
│   ├── mayday_detector.h
│   ├── radio_log.h
│   ├── signal_analyzer.h
│   ├── translator.h
│   └── ui_manager.h
└── src/                 # Source files
    ├── main.c           # LVGL UI entry point
    ├── agent_bridge.c   # ai_agent bridge (stub for now)
    ├── audio_capture.c  # Audio capture from /dev/audio/pcm0c
    ├── cw_decoder.c     # CW Morse decoder with Goertzel/BPF
    ├── freq_recommender.c
    ├── mayday_detector.c
    ├── radio_log.c      # File-based QSO/event logging
    ├── signal_analyzer.c# FFT spectrum + interference detection
    └── translator.c

web_preview/
└── index.html           # Browser-based LVGL UI simulation
```

## NuttX defconfig 要求

本应用依赖大量 NuttX/OpenVela 子系统与硬件驱动。在编译前，请确保 defconfig 中已启用 pthreads、mqueue、POSIX timers、网络、sockets、DNS、libc math、TLS/mbedtls（`CONFIG_NETUTILS_MBEDTLS`）、I2S audio、LRADC、UART GPS、I2C/SPI 传感器、SHTC3、LTR553、LVGL、显示屏、输入设备、WLAN/WAPI 等开关。

详细开关列表与可直接复制到 `.config` 的检查清单见：

- [`defconfig_notes.md`](defconfig_notes.md)

## 构建要求

- 若使用默认的 SiliconFlow HTTPS 端点，必须启用 `CONFIG_NETUTILS_MBEDTLS`；未启用时 `siliconflow_client.c` 会在编译期或运行期返回明确错误，拒绝回退到不安全的 HTTP。
- I2S 真实音频采集依赖 `CONFIG_AUDIO=y`、`CONFIG_AUDIO_I2S=y` 及对应板级 PCM 驱动。
- 环境传感器展示依赖 `CONFIG_SENSORS_SHTC3=y`、`CONFIG_SENSORS_LTR553=y` 以及对应的 I2C/SPI 总线驱动。
- 完整开关清单与检查清单见 [`defconfig_notes.md`](defconfig_notes.md)。

## 如何将本应用链接到 OpenVela 工作区

`contest2026_095_hehaohanBI4MIB.xml` 已定义 linkfile，将 `app/ai_radio_console` 链接到 OpenVela 工作区的 `packages/demos/contest2026_095_ai_radio_console`。完整步骤如下：

1. 将 manifest 放入 OpenVela 仓库的 manifests 目录：
   ```
   cp contest2026_095_hehaohanBI4MIB.xml .repo/manifests/
   ```

2. 使用本 manifest 初始化并同步工作区：
   ```
   repo init -u <manifest-url> -b dev-ai-contest-2026 -m contest2026_095_hehaohanBI4MIB.xml
   repo sync
   ```

3. 进入应用目录确认 linkfile 已生成：
   ```
   ls packages/demos/contest2026_095_ai_radio_console
   ```

4. 打开 menuconfig 并启用本应用：
   ```
   make menuconfig
   ```
   然后勾选：
   ```
   Application Configuration → Demos → Contest 2026 team 095 AI Radio Console
   ```
   对应配置宏为 `CONFIG_LVX_USE_DEMO_CONTEST2026_095_AI_RADIO_CONSOLE`。

## Building

1. Set up openvela build environment per official docs
2. 按上文“如何将本应用链接到 OpenVela 工作区”完成 repo 同步与 menuconfig 配置
3. 确认 `CONFIG_LVX_USE_DEMO_CONTEST2026_095_AI_RADIO_CONSOLE=y` 已启用
4. Build for Gemini-S1:
   ```
   ./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay -e -Wno-error -j$(nproc)
   ```
5. Flash and run:
   ```
   nsh> ai_radio
   ```

## Web Preview

Open `web_preview/index.html` in a browser to see an interactive simulation of the UI.

## License

Apache 2.0
