# 项目交接文档（Handoff）

> 当前 agent 切换时，请下一位 agent 详细阅读本文档，快速接续工作。

---

## 1. 项目概况

**队伍**：contest2026_095_hehaohanBI4MIB
**产品概念**：AI 无线电对讲机控制台（"得到大脑"风格语音笔记设备：录音 → ASR → AI 摘要）
**主控芯片**：Allwinner R528（Gemini-S1 开发板）
**屏幕**：2.8" SPI ILI9341 电阻触摸屏（320x240）
**RTOS**：NuttX（OpenVela 发行版）

---

## 2. 仓库结构

### GitHub 远程仓库
- **地址**：https://github.com/HaohanHe/contest2026_095_hehaohanBI4MIB
- **当前分支**：`dev-ai-contest-2026`（本地对应 `trae/agent-6ZHkX8`）
- **最新 commit**：`a55d61cd`（logs: add 4 session export files for HaohanHe）

### 工作目录
```
/run/media/bi4mib/新加卷/ontest2026_095_hehaohanBI4MIB/
├── .gitignore              # 排除规则（已完善）
├── app/ai_radio_console/   # 早期主应用源码（较旧）
├── board/contest_board/    # 板级适配
├── hardware/ai_radio_hat/  # PCB 设计文件（KiCad）
├── realprojectnewnow/      # ★ 主项目目录（已改名）
│   ├── Veeridio/           # Android 端 App（Kotlin）
│   ├── com.agent.coapp/    # 合作方 App（已排除在 .gitignore）
│   ├── ai_radio_hat/       # 硬件设计文件
│   ├── feishu_docs/        # 飞书文档（已排除）
│   ├── feishu_docs_full/   # 飞书文档压缩（已排除）
│   ├── frameworks/         # OpenVela 框架源码
│   ├── openvela-docs-gitee/  # 文档（已排除）
│   ├── openvela-docs-github/ # 文档（已排除）
│   ├── packages_ai_agent/  # AI Agent 核心应用（C 语言）
│   ├── packages_demos/     # 官方 Demo
│   └── vendor_allwinnertech/ # Allwinner BSP
└── logs/                   # AI Coding 日志目录
    └── <haohanhe>/         # 当前用户的会话记录
```

### OpenVela 全量源码（独立目录，与上面不互相同步）
```
/home/bi4mib/openvela-build/
├── .repo/                  # repo 工作区标识
├── nuttx/                  # NuttX 内核
├── apps/                   # NuttX 应用
├── vendor/allwinnertech/   # Allwinner BSP
├── frameworks/             # OpenVela 框架
├── packages/demos/contest2026_095_ai_radio_console/  # 部署的 app 副本
└── cmake_out/              # CMake 构建缓存
```

**关键提醒**：`app/ai_radio_console/` 和 `packages/demos/contest2026_095_ai_radio_console/` 是两个独立副本，修改后必须手动同步！

---

## 3. 已完成的里程碑

### 固件开发
- [x] 中文字体方块问题修复（启用 LV_USE_FONT_COMPRESSED）
- [x] 创建 ai_radio_font.c（lv_font_conv 提取汉字）
- [x] main.c、ui_ai_radio.c、spacelog_settings.c 字体继承修复
- [x] 固件成功编译并打包（26MB img）

### 商店与代码管理
- [x] 项目目录由"国际空间通信挑战赛项目规划"重命名为 `realprojectnewnow`
- [x] GitHub 仓库初始化并推送（强制更新分支）
- [x] .gitignore 完善（排除参考文件、文档、编译产物、二进制资源）
- [x] 安装 contest-log-collector（待确认）

### 已知问题
- [ ] AI 无线电控制台打开后屏幕卡顿 1/3（未解决）
- [ ] 音频驱动注册待检查
- [ ] 手机配网、AI Agent、ASR 等上层功能待梳理

---

## 4. 关键文件路径速查

### 编译相关
```bash
# 编译 firmware（-j1，防止 OOM）
export PATH=/home/bi4mib/openvela-build/prebuilts/build-tools/linux-x86_64/bin:/home/bi4mib/openvela-build/prebuilts/gcc/linux-x86_64/arm-none-eabi/bin:$PATH
cd /home/bi4mib/openvela-build
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/ -j1

# 打包 img
cd vendor/allwinnertech/lichee && source envsetup.sh && lunch_nuttx r528s3-gemini-s1 && pack
```

### 输出镜像
```
/home/bi4mib/openvela-build/vendor/allwinnertech/lichee/out/r528s3/gemini-s1_nand/rtos_nuttx_r528s3-gemini-s1_uart0_128Mnand.img
```

### 主要源码
- **主程序**：`/home/bi4mib/openvela-build/packages/demos/contest2026_095_ai_radio_console/src/main.c`
- **AI 界面**：`/home/bi4mib/openvela-build/packages/demos/contest2026_095_ai_radio_console/src/ui_ai_radio.c`
- **设置页**：`/home/bi4mib/openvela-build/packages/demos/contest2026_095_ai_radio_console/src/spacelog_settings.c`
- **中文字体**：`/home/bi4mib/openvela-build/packages/demos/contest2026_095_ai_radio_console/src/ai_radio_font.c`
- **构建配置**：`/home/bi4mib/openvela-build/vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/defconfig`

---

## 5. AI Coding 日志归集

### 官方要求
- 只支持 4 种工具：**Claude Code**（含 AIoT-IDE 内嵌）、**OpenCode**、**Codex CLI**
- 不支持 Trae IDE / Cursor / ChatGPT 等（原来用的 Trae 采集不到）
- 日志自动写入 `logs/<github_login>/` 目录

### 采集器安装命令
```bash
cd /run/media/bi4mib/新加卷/ontest2026_095_hehaohanBI4MIB
bash /home/bi4mib/openvela-build/.claude/skills/contest-log-collector/onboarding/install.sh \
  --team-id contest2026_095_hehaohanBI4MIB \
  --github-login HaohanHe
```

### 健康检查
```bash
bash /home/bi4mib/openvela-build/.claude/skills/contest-log-collector/onboarding/verify-setup.sh
```

### 提交日志
```bash
git add logs/
git commit -s -m "logs: sync AI sessions"
git push
```

---

## 6. 下一位 agent 优先做的事

1. **读这个文档**，搞清楚现状
2. **装 contest-log-collector**（上面的命令）
3. **用 Claude Code / OpenCode** 开发（别用 Trae/Cursor，采集不到）
4. **解决屏幕卡顿 1/3 的性能问题**（打开 AI 视频后）
5. **提交代码+日志到 GitHub**

---

## 7. 用户偏好（干活风格）

- 用户说中文，非正式/着急语气，不喜欢废话
- 要结果不要讨论——直接修，别说有什么问题
- 期望 AI 自己编译验证，不只是给建议
- 关键文件路径必须给完整，别说"在那个目录里"
- 不要主动创建 .md 文件，除非明确要求

---

*文档由 agent 创建于 2026-07-13，切换 agent 时请一并交接。*
