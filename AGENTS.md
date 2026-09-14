# AGENTS.md

## What This Is

openvela AI contest repo (team 095) for Gemini-S1 (Allwinner R528) board. Goal: AI Radio Console firmware with screen, LVGL UI, ASR via SiliconFlow API.

## Repository Layout

```
contest2026_095_hehaohanBI4MIB/   # This repo (contest code + manifest)
├── app/ai_radio_console/          # Main app (22 C source files)
├── board/contest_board/           # Board adapter (placeholder, don't use)
├── hardware/ai_radio_hat/         # PCB design (KiCad)
├── quickapp/                      # Quick app skeleton
└── contest2026_095_hehaohanBI4MIB.xml  # Manifest (linkfiles)

/home/bi4mib/openvela-build/       # Full openvela source tree (repo synced)
├── nuttx/                         # NuttX kernel
├── apps/                          # NuttX apps
├── vendor/allwinnertech/          # Allwinner R528 BSP
├── frameworks/                    # openvela frameworks
└── packages/demos/contest2026_095_ai_radio_console/  # Deployed app copy
```

## Critical: Two Separate Copies

Changes in `contest2026_095_hehaohanBI4MIB/app/ai_radio_console/` do NOT auto-sync to `/home/bi4mib/openvela-build/packages/demos/contest2026_095_ai_radio_console/`. You must manually copy between them.

## Build Commands

```bash
# From /home/bi4mib/openvela-build
export PATH=/home/bi4mib/openvela-build/prebuilts/build-tools/linux-x86_64/bin:/home/bi4mib/openvela-build/prebuilts/gcc/linux-x86_64/arm-none-eabi/bin:$PATH

# Build (requires unionfs-fuse or falls back to in-tree)
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay -j1

# Pack image
cd vendor/allwinnertech/lichee && source envsetup.sh && lunch_nuttx && pack
```

## Board & Config

- **Board config**: `vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/`
- **Official reference**: `packages/ai_agent/defconfigs/gemini-s1/gemini-s1_defconfig`
- **Display**: 2.8" SPI ILI9341 resistive LCD (320x240)
- **Touch**: Resistive (TPADC), NOT capacitive (GT911)
- **Do NOT enable** `CONFIG_GT911_IIC_TOUCH` — it's for capacitive touch panels

## Known Build Issues

1. **unionfs-fuse required**: Build uses unionfs-fuse for write overlay. If not installed (`sudo apt install unionfs-fuse`), build falls back to in-tree mode.
2. **LTO**: Official defconfig has `CONFIG_LTO_FULL=y`. Disabled (`CONFIG_LTO_NONE=y`) to avoid cross-module symbol resolution failures during distclean rebuilds.
3. **Linker errors after distclean**: `mbedtls_mpi_*`, `g_builtin_count`, `sqrtf` undefined. Root cause: build OOM-killed before `libapps.a` fully generated. On 14GB RAM machine, use `-j1`.
4. **archive order bug**: `libapps.a` created before apps compiled — objects exist but aren't archived. Workaround: manually `arm-none-eabi-ar r` missing objects into archive.

## Defconfig Modification Rules

1. Start from official `gemini-s1_defconfig` (only diff should be `CONFIG_LVX_USE_DEMO_CONTEST2026_095_AI_RADIO_CONSOLE=y`)
2. Never modify driver code (`hal_dma.c`, `sunxi_alsa.c`, `drv_lradc.c`) unless absolutely necessary
3. Never modify `graph.conf` or media configs
4. After editing defconfig, always `rm -rf cmake_out/ nuttx/.config` before rebuild

## Flash & Test

```bash
# Flash using PhoenixSuit (Windows) or LiveSuit (Linux)
# Boot log shows: LCD /dev/lcd0, Touchscreen /dev/input0
# luncher_mini shows 5 windows, click "AI Radio" to launch
```

## Other Important Files

- `agent.md` — Detailed handoff context (read this first for full history)
- `DIAGNOSIS.md` — Initial problem diagnosis
- `DEEP_AUDIT_REPORT.md` — 5-agent code audit
- `TODO.md` — Task tracking
- `gemini-s1_ai_radio_v9.img` — Last known working firmware (pre-distclean)
- `gemini_s1_mini.img` — Official baseline firmware (27MB)

## User Preferences

- User communicates in Chinese, informal/urgent tone
- Wants results over discussion — fix it, don't explain the problem
- Expects AI to compile and build, not just advise
- Product concept: "得到大脑" style voice note device (record → ASR → AI summary)
