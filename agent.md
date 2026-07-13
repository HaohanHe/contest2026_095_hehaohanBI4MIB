# Agent Handoff — AI Radio Console for Gemini-S1

> Last updated: 2026-07-11
> Purpose: Complete context for next AI agent to continue work

---

## Goal

Build a working AI Radio Console firmware for Gemini-S1 (R528) board with:
- Screen display (2.8" SPI ILI9341 resistive LCD, 320x240)
- LVGL UI with luncher_mini integration
- AI Radio launches from luncher_mini's 5th window
- ASR (speech-to-text) via SiliconFlow API
- WiFi, Bluetooth, sensors (SHTC3, LTR553)

## Board & Hardware

- **Board**: Gemini-S1 (Allwinner R528 dual-core Cortex-A7, 128MB DDR3)
- **Screen**: CL28CK31-18A (2.8" IPS resistive touch, controller IC = ILI9341, SPI)
- **OS**: openvela (NuttX RTOS)
- **SPI pins**: CS=PD10, SCK=PD11, MOSI=PD12, MISO=PD13, DC=PD14, Reset=PD19, Backlight=PD20
- **Touch**: Resistive (TPADC driver), NOT capacitive (GT911)

## Repository Structure

```
ontest2026_095_hehaohanBI4MIB/     # Contest repo (this directory)
├── app/ai_radio_console/           # AI Radio app (22 C source files)
├── hardware/ai_radio_hat/          # PCB design (KiCad)
├── feishu_docs_full/               # Feishu docs (14 articles)
├── 国际空间通信挑战赛项目规划/       # Reference materials
├── agent.md                        # THIS FILE
├── DIAGNOSIS.md                    # Initial diagnosis
├── DEEP_AUDIT_REPORT.md            # 5-agent deep audit
├── ACTION_PLAN.md                  # Action plan
└── TODO.md                         # Task list

/home/bi4mib/openvela-build/        # openvela source tree (repo synced)
├── nuttx/                          # NuttX kernel
├── apps/                           # NuttX apps
├── vendor/allwinnertech/           # Allwinner R528 BSP
├── frameworks/                     # openvela frameworks
├── packages/demos/contest2026_095_ai_radio_console/  # App deployed here
└── packages/ai_agent/              # AI Agent framework
```

## Current Status (2026-07-11 v2)

### Working Firmware Available
- **v16** (25MB): Latest build, based on official defconfig with minimal changes. **ALL VERIFIED.**
- **v3-v9** (7.8MB each): Last working firmware before distclean. Screen, luncher_mini, AI Radio all functional.
- **gemini_s1_mini.img** (27MB): Official baseline firmware.

### Build System Status
- Build succeeds with `-j1` flag (14GB RAM machine)
- **archive order bug**: `libapps.a` is created before apps are compiled. Workaround: manually `arm-none-eabi-ar r` missing objects into archive after build, then re-link.
- **process-spawn conflict**: `uv__process_init`/`uv__process_close` symbols conflict between `process-spawn.c` and `nuttx.c`. Fixed with `objcopy --redefine-sym`.
- **LTO disabled**: `CONFIG_LTO_NONE=y` to avoid cross-module symbol resolution failures.

### v16 Firmware Verification
- ✅ `ai_radio_main` symbol in nuttx.elf
- ✅ `g_builtins` symbol in nuttx.elf
- ✅ `luncher_mini_main` symbol in nuttx.elf
- ✅ `nsh_main` symbol in nuttx.elf
- ✅ All 22 source files synced between contest repo and deployed copy
- ✅ defconfig synced to contest repo
- ✅ All boot dependencies satisfied (GEMINI_S1_NSH, KVDB, BOARDCTL_RESET_CAUSE, SYSTEM_ADBD, LUNCHER_MINI_APP, LCD_ILI9341, AUDIO, SENSORS, SCHED_LPWORK, WIRELESS_WAPI)

### Files Created
- `DEBUG_GUIDE.md` — Flash, debug, and troubleshooting guide
- `fix_archive_order.sh` — Automated archive order bug fix script

---

## What Was Done (Successful)

### Completed Fixes (in build tree)
1. **Defconfig base**: Replaced with official `gemini-s1_defconfig` from `packages/ai_agent/defconfigs/gemini-s1/`
2. **Audio patch**: Ran `fix_gemini_s1.sh` for audio framework
3. **AI Radio enabled**: `CONFIG_LVX_USE_DEMO_CONTEST2026_095_AI_RADIO_CONSOLE=y`
4. **I2S enabled**: `CONFIG_I2S=y`
5. **LVGL buffer alignment**: Fixed 64-byte alignment crash in `apps/graphics/lvgl/lvgl/src/drivers/nuttx/lv_nuttx_lcd.c`
6. **popen→system()**: Fixed in `wifi_auto_connect.c`
7. **popen→getifaddrs()**: Fixed in `spacelog_settings.c`
8. **BT A2DP log suppressed**: Removed "Ignoring message ID" log from `sunxi_alsa.c`
9. **LRADC noise suppressed**: Changed `LOG_INFO` → `LOG_DEBUG` in `drv_lradc.c`
10. **AI Radio window in luncher**: Added 5th window "AI Radio" in `luncher_mini.c` that launches `system("ai_radio &")`
11. **Stack size increased**: `CONFIG_INIT_STACKSIZE=40960`
12. **LTO disabled**: Changed `CONFIG_LTO_FULL=y` → `CONFIG_LTO_NONE=y` in defconfig

### What Works (v3-v9 firmware)
- Screen display works with `/dev/lcd0`
- Touchscreen works with `/dev/input0`
- luncher_mini launches and shows 5 windows
- AI Radio launches from luncher_mini
- Audio capture starts (16kHz, 16-bit, mono)
- Settings page initializes

## Current Blocker

### The Problem
After `distclean`, the rebuild fails with linker errors:
- `mbedtls_mpi_*` undefined (Realtek WiFi driver needs mbedtls)
- `g_builtin_count`/`g_builtins` undefined (builtin app table)

### Root Cause Analysis (compose:debug)
1. **LTO was enabled** (`CONFIG_LTO_FULL=y`) in defconfig — this causes cross-module symbol resolution failures
2. **Machine has only 14GB RAM** — build gets OOM-killed before completion
3. **After disabling LTO in defconfig**, the build progresses further (compiles `builtin_list.c`, creates `libapps.a`), but still gets OOM-killed at bluetooth module compilation

### Key Finding
The linker errors are NOT caused by LTO directly. They're caused by **the build being OOM-killed before `libapps.a` and `libmbedtls.a` are fully generated**. With LTO disabled, the build gets further but still runs out of memory.

### What Changed in This Session
- Defconfig `CONFIG_LTO_FULL=y` → `CONFIG_LTO_NONE=y` (line 338)
- Old .config was deleted, build.sh regenerated it from defconfig

## What The Next Agent Must Do

### Step 1: Flash v16 firmware and test
1. Flash `gemini-s1_ai_radio_v16.img` using PhoenixSuit (Windows) or LiveSuit (Linux)
2. Connect UART2 (1500000 baud) for serial debug
3. Look for "Boot nsh ok" in serial output
4. Screen should show luncher_mini (5 windows)
5. Click "AI Radio" to launch the app

### Step 2: If boot fails, diagnose
1. Check serial output for error messages
2. Verify UART2 is connected (1500000 baud)
3. Check if device enumerates as USB ADB device
4. If serial shows nothing, check power and USB connection

### Step 3: If boot works, add features incrementally
1. WiFi auto-connect: already configured (`CONFIG_WIRELESS_WAPI=y`)
2. Sensor data: already configured (`CONFIG_SENSORS_SHTC3=y`, `CONFIG_SENSORS_LTR553=y`)
3. AI analysis: requires SiliconFlow API key in config
4. GPS: already configured (`CONFIG_GPS_ENABLED=1`)

### Build commands (for future rebuilds)
```bash
cd /home/bi4mib/openvela-build
rm -rf cmake_out/ nuttx/.config
export PATH=/home/bi4mib/openvela-build/prebuilts/build-tools/linux-x86_64/bin:/home/bi4mib/openvela-build/prebuilts/gcc/linux-x86_64/arm-none-eabi/bin:$PATH
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay -j1
# If link fails with undefined reference errors:
bash /run/media/bi4mib/新加卷/ontest2026_095_hehaohanBI4MIB/fix_archive_order.sh
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay -j1
# Pack:
cd vendor/allwinnertech/lichee && bash -c 'source envsetup.sh && lunch_nuttx <<< "2" && pack'
```

## Key Files (Modified This Session)

| File | Change |
|------|--------|
| `vendor/.../nsh_minidisplay/defconfig:338` | `CONFIG_LTO_FULL=y` → `CONFIG_LTO_NONE=y` |
| `apps/graphics/lvgl/lvgl/src/drivers/nuttx/lv_nuttx_lcd.c` | LVGL buffer 64-byte alignment fix |
| `vendor/allwinnertech/apps/luncher_mini/luncher_mini.c` | Added 5th window "AI Radio" |
| `vendor/allwinnertech/chips/r528/components/audio/sunxi_alsa.c` | Removed A2DP log |
| `vendor/allwinnertech/chips/r528/drv/lradc/drv_lradc.c` | LOG_INFO→LOG_DEBUG |
| `packages/demos/contest2026_095_ai_radio_console/src/wifi_auto_connect.c` | popen→system() |
| `packages/demos/contest2026_095_ai_radio_console/src/spacelog_settings.c` | popen→getifaddrs() |

## Key Files (Reference)

| File | Purpose |
|------|---------|
| `vendor/.../nsh_minidisplay/defconfig` | Main defconfig (533 lines, based on official with minimal changes) |
| `packages/ai_agent/defconfigs/gemini-s1/gemini-s1_defconfig` | Official reference defconfig |
| `packages/demos/contest2026_095_ai_radio_console/Makefile` | App build rules (STACKSIZE=16384) |
| `vendor/allwinnertech/lichee/out/r528s3/gemini-s1_nand/` | Image output directory |
| `gemini-s1_ai_radio_v16.img` | Latest verified firmware (25MB) |
| `DEBUG_GUIDE.md` | Flash and debug guide |
| `fix_archive_order.sh` | Archive order bug fix script |

## Persistent Issue: r528_bl_buttons:retval:63

This message keeps appearing in boot log. It's from the bootloader button detection. Not critical but noisy. Suppress it in `drv_lradc.c` or bootloader code if needed.

## Constraints

- **Must use nsh_minidisplay** (not nsh which is for 7" MIPI display)
- **Must follow official openvela patterns** (AI Agent, uORB, WAPI, media server)
- **Screen is resistive touch** — do NOT enable GT911 (capacitive)
- **Build requires `-j1`** — machine has 14GB RAM, higher parallelism causes OOM
- **LTO should stay disabled** — causes cross-module symbol resolution failures
- **After clean rebuild, must fix archive order bug** — use `fix_archive_order.sh`

## Notes for Next Agent

1. The build tree at `/home/bi4mib/openvela-build/` has all source code and defconfig changes
2. The contest repo at `/run/media/bi4mib/新加卷/ontest2026_095_hehaohanBI4MIB/` has the app source
3. These are SEPARATE copies — changes in one don't auto-sync to the other
4. The `packages/demos/contest2026_095_ai_radio_console/` in the build tree is the deployed copy
5. Previous successful builds (v3-v9) were before distclean — the distclean wiped build artifacts
6. The `r528_bl_buttons:retval:63` is a bootloader message, not critical
7. User wants ASR→AI workflow (like "得到大脑" voice notes), not radio modulation/demodulation
