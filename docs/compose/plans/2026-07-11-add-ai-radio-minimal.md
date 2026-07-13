# Add AI Radio Console to Official Firmware — Minimal Change Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use compose:subagent (recommended) or compose:execute to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add AI Radio Console app to the working official firmware (gemini_s1_mini.img) with zero risk of breaking boot.

**Architecture:** Only modify the defconfig to enable the AI Radio Console app. No changes to any driver, audio, display, or low-level code. If the app doesn't work, the firmware still boots exactly like the official version.

**Tech Stack:** openvela (NuttX RTOS), Allwinner R528 BSP, LVGL v9, SiliconFlow ASR/LLM API

---

## Global Constraints

- **NEVER modify these files** (they work in official firmware):
  - `vendor/allwinnertech/chips/r528/drivers/rtos-hal/hal/source/dma/hal_dma.c`
  - `vendor/allwinnertech/chips/r528/components/audio/sunxi_alsa.c`
  - `vendor/allwinnertech/chips/r528/drv/lradc/drv_lradc.c`
  - `vendor/allwinnertech/boards/r528/r528s3-gemini-s1/src/etc/media/graph.conf`
  - `vendor/allwinnertech/boards/r528/r528s3-gemini-s1/src/etc/media/criteria.txt`
- **ONLY modify:** `defconfig` to add `CONFIG_LVX_USE_DEMO_CONTEST2026_095_AI_RADIO_CONSOLE=y`
- **Build command must use `--cmake`** (as per official openvela-build skill)
- **Build requires 16GB+ RAM** — use swap if needed
- **Test incrementally:** Each change must be verified before proceeding

---

## Task 1: Create Clean Build from Official Firmware

**Covers:** Baseline setup — ensures we start from a known-good state

**Files:**
- Read: `vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/defconfig`
- Read: `packages/ai_agent/defconfigs/gemini-s1/gemini-s1_defconfig`

**Interfaces:**
- Consumes: Official gemini_s1_mini.img (known working)
- Produces: Clean defconfig as starting point

- [ ] **Step 1: Save current defconfig as backup**

```bash
cd /home/bi4mib/openvela-build/vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/
cp defconfig defconfig.v10_broken
```

- [ ] **Step 2: Restore official defconfig**

```bash
cp /home/bi4mib/openvela-build/packages/ai_agent/defconfigs/gemini-s1/gemini-s1_defconfig defconfig
```

- [ ] **Step 3: Verify defconfig matches official**

```bash
diff /home/bi4mib/openvela-build/packages/ai_agent/defconfigs/gemini-s1/gemini-s1_defconfig defconfig
# Expected: no differences
```

- [ ] **Step 4: Build official firmware as baseline**

```bash
cd /home/bi4mib/openvela-build
rm -rf cmake_out/
export PATH=/home/bi4mib/openvela-build/prebuilts/build-tools/linux-x86_64/bin:/home/bi4mib/openvela-build/prebuilts/gcc/linux-x86_64/arm-none-eabi/bin:$PATH
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay --cmake -j$(nproc)
```

Expected: `build completed successfully`

- [ ] **Step 5: Pack and verify image size**

```bash
cd /home/bi4mib/openvela-build/vendor/allwinnertech/lichee
source envsetup.sh
lunch_nuttx
pack
ls -la out/r528s3/gemini-s1_nand/rtos_nuttx_r528s3-gemini-s1_uart0_128Mnand.img
# Expected: ~27MB (same as gemini_s1_mini.img)
```

---

## Task 2: Add AI Radio Console App

**Covers:** Add the contest app to the official firmware

**Files:**
- Modify: `vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/defconfig` (1 line addition)

**Interfaces:**
- Consumes: Clean defconfig from Task 1
- Produces: Defconfig with AI Radio Console enabled

- [ ] **Step 1: Add AI Radio Console config**

```bash
echo "CONFIG_LVX_USE_DEMO_CONTEST2026_095_AI_RADIO_CONSOLE=y" >> /home/bi4mib/openvela-build/vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/defconfig
```

- [ ] **Step 2: Verify the addition**

```bash
tail -5 /home/bi4mib/openvela-build/vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/defconfig
# Expected: CONFIG_LVX_USE_DEMO_CONTEST2026_095_AI_RADIO_CONSOLE=y is the last line
```

- [ ] **Step 3: Build with AI Radio Console**

```bash
cd /home/bi4mib/openvela-build
rm -rf cmake_out/
export PATH=/home/bi4mib/openvela-build/prebuilts/build-tools/linux-x86_64/bin:/home/bi4mib/openvela-build/prebuilts/gcc/linux-x86_64/arm-none-eabi/bin:$PATH
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay --cmake -j$(nproc)
```

Expected: `build completed successfully` + `ai_radio` appears in registered apps

- [ ] **Step 4: Pack firmware**

```bash
cd /home/bi4mib/openvela-build/vendor/allwinnertech/lichee
source envsetup.sh
lunch_nuttx
pack
```

- [ ] **Step 5: Copy to project directory for flashing**

```bash
cp /home/bi4mib/openvela-build/vendor/allwinnertech/lichee/out/r528s3/gemini-s1_nand/rtos_nuttx_r528s3-gemini-s1_uart0_128Mnand.img /run/media/bi4mib/新加卷/ontest2026_095_hehaohanBI4MIB/gemini-s1_ai_radio_v11.img
```

- [ ] **Step 6: Flash and test**

Flash `gemini-s1_ai_radio_v11.img` to Gemini-S1 using PhoenixSuit.

Expected:
- Device boots normally
- ADB connects
- luncher_mini shows 5 windows
- Clicking "AI Radio" launches the app
- Screen shows AI Radio UI

---

## Task 3: If Task 2 Boot Fails — Diagnose

**Covers:** Root cause analysis if adding AI Radio breaks boot

**Files:**
- Read: Build log from Task 2
- Read: `.config` from `cmake_out/` directory

**Interfaces:**
- Consumes: Failed build or boot from Task 2
- Produces: Root cause identification

- [ ] **Step 1: Check build log for errors**

```bash
grep -i "error\|warning.*ai_radio\|undefined.*ai_radio" /home/bi4mib/openvela-build/build.log 2>/dev/null | head -20
```

- [ ] **Step 2: Check if app is in .config**

```bash
grep "AI_RADIO_CONSOLE" /home/bi4mib/openvela-build/cmake_out/vela_*/nuttx/.config 2>/dev/null
```

- [ ] **Step 3: Check if app compiled**

```bash
find /home/bi4mib/openvela-build -name "ai_radio*" -type f 2>/dev/null | head -10
```

- [ ] **Step 4: If app doesn't compile, check dependencies**

The AI Radio Console app depends on:
- LVGL (CONFIG_GRAPHICS_LVGL=y)
- WiFi (CONFIG_IEEE80211_REALTEK_WIFI=y)
- Audio (CONFIG_AUDIO=y)
- TLS (CONFIG_CRYPTO_MBEDTLS=y)

Verify all are enabled in .config.

---

## Task 4: If Boot Works — Add More Features

**Covers:** Incrementally add features after confirming base works

**Files:**
- Modify: `defconfig` (one config at a time)

**Interfaces:**
- Consumes: Working firmware from Task 2
- Produces: Enhanced firmware

- [ ] **Step 1: Add WiFi auto-connect (if needed)**

```bash
echo "CONFIG_WIRELESS_WAPI=y" >> defconfig
```

- [ ] **Step 2: Add sensor support (if needed)**

```bash
echo "CONFIG_SENSORS_SHTC3=y" >> defconfig
echo "CONFIG_SENSORS_LTR553=y" >> defconfig
```

- [ ] **Step 3: Add Bluetooth (if needed)**

```bash
echo "CONFIG_BLUETOOTH=y" >> defconfig
echo "CONFIG_BT=y" >> defconfig
```

Each addition: rebuild → pack → flash → test → proceed only if working.

---

## Key Files

| File | Purpose |
|------|---------|
| `vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/defconfig` | Main defconfig (ONLY file to modify) |
| `packages/demos/contest2026_095_ai_radio_console/` | AI Radio Console app source |
| `packages/demos/contest2026_095_ai_radio_console/Kconfig` | App config option |
| `packages/demos/contest2026_095_ai_radio_console/Make.defs` | App build registration |
| `packages/demos/contest2026_095_ai_radio_console/Makefile` | App build rules |

---

## What NOT to Do (Lessons Learned)

1. **Do NOT modify driver code** (hal_dma.c, sunxi_alsa.c, drv_lradc.c) — they work in official firmware
2. **Do NOT modify graph.conf or media configs** — they work in official firmware
3. **Do NOT add AI Agent configs** (CONFIG_AI_AGENT_*, CONFIG_EXAMPLES_AI_AGENT_VELA) — they may have unmet dependencies
4. **Do NOT add LittleFS, Event FD, or other unnecessary configs** — they may break boot
5. **Do NOT use `--cmake` flag incorrectly** — must use it for CMake build
6. **Do NOT skip the clean rebuild** — always `rm -rf cmake_out/` before building
7. **Do NOT modify more than one thing at a time** — test each change individually
