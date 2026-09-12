# DayNote - AI 语音笔记设备

基于 openvela 官方 mini_memo + ai_agent 框架重构的"得到大脑"式全天候语音笔记设备。

## 硬件
- Gemini-S1 (Allwinner R528 双核 Cortex-A7, 128MB DDR3)
- 2.8" SPI ILI9341 电阻屏 (320x240)
- I2S 音频输入/输出

## 功能
- PTT 按键录音 + 自动录音状态显示
- ASR 语音转文字（ai_agent voice_channel，火山引擎）
- LLM 自动摘要 + 关键词提取（支持多后端：DeepSeek/Kimi/通义/GLM/SiliconFlow）
- 时间线展示笔记
- 卡片式笔记详情（摘要 + 关键词 + 时间）
- FreeType 运行时加载 TTF 中文字体
- 本地分类兜底（LLM 不可用时）

## 源码结构
```
app/ai_radio_console/
├── src/daynote_main.c    (7KB, LVGL初始化+字体加载+主循环)
├── src/daynote_core.c    (35KB, 存储+分类+语音通道)
├── src/daynote_ui.c      (35KB, 4页UI)
├── include/daynote_core.h
├── include/daynote_ui.h
├── Kconfig / Make.defs / Makefile / CMakeLists.txt
```

部署副本：`openvela-build/packages/demos/contest2026_095_ai_radio_console/`

## 编译
```bash
cd /home/bi4mib/openvela-build
export PATH=/home/bi4mib/openvela-build/prebuilts/build-tools/linux-x86_64/bin:/home/bi4mib/openvela-build/prebuilts/gcc/linux-x86_64/arm-none-eabi/bin:$PATH
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay -j1
cd vendor/allwinnertech/lichee && source envsetup.sh && lunch_nuttx && pack
```

## 配置 LLM
NSH 命令行中执行：
```
set_llm deepseek sk-your-api-key
# 或 SiliconFlow（OpenAI兼容）:
set_llm openai sk-your-key
# 设置 base URL（如 SiliconFlow）:
# 通过 ai_agent 配置文件设置
```

## 中文字体
将 TTF 字体文件放到 `/data/daynote/font.ttf`，程序启动时自动加载。
推荐：思源黑体 / 阿里巴巴普惠体（需控制文件大小，建议 < 5MB）

## 数据存储
- 路径：`/data/daynote/memos.json`
- 格式：JSON，每条笔记含 id/type/content/transcript/summary/keywords/timestamp/duration_ms
- 最多 100 条，自动淘汰最旧的已读条目

## 已知问题 / 后续优化
1. **VAD 自动录音**：当前为 PTT 手动录音 + 自动录音状态UI。真正的 VAD 自动检测需修改 ai_agent voice_channel（加能量检测+静音超时），或独立打开音频设备做 VAD。建议在 Linux 编译环境验证后实施。
2. **ASR 后端**：当前用 ai_agent 内置火山引擎 ASR。如需切换 SiliconFlow ASR，需在 voice_asr.c 注册新后端。
3. **触控校准**：电阻屏 TPADC 坐标转换需实机调试。官方 mini_memo 已设置 LV_INDEV_MODE_TIMER + 30ms 轮询。
4. **编译验证**：当前在 Windows 环境无法编译，需在 Linux 环境执行 build.sh 验证。
5. **音频修复**：fix_gemini_s1.sh 三步修复（媒体图+DMA IRQ+ffmpeg）已确认全部应用。

## 官方资源复用
- `packages/ai_agent/` - 官方 AI Agent 框架（LLM/ASR/TTS/UI/Skills）
- `packages/demos/mini_memo/` - 官方语音备忘录 demo（DayNote 基于此重构）
- `packages/ai_agent/defconfigs/gemini-s1/` - 官方 Gemini-S1 专用配置
- `packages/ai_agent/fix_gemini_s1.sh` - 官方音频修复脚本
