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
    ├── audio_capture.c  # Audio capture from /dev/pcmC0D0c
    ├── cw_decoder.c     # CW Morse decoder with Goertzel/BPF
    ├── freq_recommender.c
    ├── mayday_detector.c
    ├── radio_log.c      # File-based QSO/event logging
    ├── signal_analyzer.c# FFT spectrum + interference detection
    └── translator.c

web_preview/
└── index.html           # Browser-based LVGL UI simulation
```

## Building

1. Set up openvela build environment per official docs
2. Copy repo manifest:
   ```
   cp contest2026_095_hehaohanBI4MIB.xml .repo/manifests/
   repo init -u <manifest-url> -b dev-ai-contest-2026 -m contest2026_095_hehaohanBI4MIB.xml
   repo sync
   ```
3. Configure with menuconfig and enable:
   ```
   Application Configuration → Demos → Contest 2026 team 095 AI Radio Console
   ```
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
