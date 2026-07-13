# TODO List

> 最后更新: 2026-07-10
> 格式: [x] 已完成 / [ ] 待做 / [~] 进行中

---

## Phase 1: 让它能跑 (P0 致命修复)

- [~] **1.1 启用 LRADC 配置**
  - [x] 已启用 `CONFIG_R528_LRADC=y`
  - [ ] I2S 暂不需要（CONFIG_AW_AUDIO_CODEC 已提供音频）
  - [ ] 验证: 编译通过，`/dev/input/event1` 存在

- [x] **1.2 修复命令注入漏洞**
  - `spacelog_settings.c` — 添加 `shell_escape()` 转义 WiFi 密码/SSID
  - 已同步到构建树

- [~] **1.2.5 修复触摸屏驱动冲突**
  - [x] 禁用 `CONFIG_GT911_IIC_TOUCH`（电容屏，不适用于 CL28CK31-18A 电阻屏）
  - [x] 保留 `CONFIG_DRIVERS_TPADC=y`（电阻屏驱动）
  - 验证: 刷写后触摸屏可工作

- [ ] **1.3 打包 .img 并刷写测试**
  - 需要重新编译: `./build.sh vendor/.../nsh_minidisplay -j$(nproc)`
  - 执行 `source envsetup.sh && lunch_nuttx && pack`
  - 用 PhoenixSuit/LiveSuit 刷写到 Gemini-S1
  - 验证: 屏幕亮起并显示内容

- [ ] **1.4 验证 LVGL UI 显示**
  - 运行 `ai_radio` 命令
  - 验证: 320x240 LCD 上显示 UI 界面

## Phase 2: 核心功能 (P1 重要修复)

- [ ] **2.1 验证音频采集**
  - 检查 `/dev/audio/pcm0c` 是否可读
  - 运行 `ai_radio` 观察音频状态
  - 验证: 音频波形数据可获取

- [ ] **2.2 验证按键输入**
  - 按 LRADC 按键（Vol±/Menu/Enter/Home）
  - 验证: 按键事件被正确识别

- [ ] **2.3 验证 WiFi 连接**
  - `wapi mode wlan0 2 && wapi psk wlan0 "密码" 3 && wapi essid wlan0 "SSID" 1 && renew wlan0`
  - 验证: `ipconfig` 显示 IP 地址

- [ ] **2.4 验证 ASR 功能**
  - 配置 SiliconFlow API key
  - 验证: 语音输入后返回转录文本

- [ ] **2.5 修复 CW 解码器 DSP 错误**
  - `cw_decoder.c:63` — 补充缺失的 b1 系数
  - 验证: CW 解码结果正确

- [ ] **2.6 修复线程安全问题**
  - 环形缓冲区使用原子操作
  - `pthread_create()` 返回值检查
  - 验证: 无崩溃、无数据损坏

## Phase 3: 硬件适配 (P1 重要)

- [ ] **3.1 修复 PTT 引脚**
  - 将 GPADC0 改为正确的 GPIO 输出引脚
  - 实现 PTT GPIO 驱动代码
  - 验证: 按 PTT 按钮可控制发射

- [ ] **3.2 补全 KiCad 原理图**
  - 添加缺失组件: D1, U2, T1, C3-C10, R8-R11, J5
  - 验证: BOM 与原理图一致

- [ ] **3.3 实现音频播放路径**
  - 添加 HPOUT 播放代码 (TTS/数字语音)
  - 验证: 可通过扬声器播放音频

- [ ] **3.4 实现 CAT 串口接口**
  - 添加串口协议代码，控制电台频率/模式
  - 验证: 可读写电台 CAT 命令

## Phase 4: AI 功能 (P2 增强)

- [ ] **4.1 完善 ASR 流水线**
  - VAD 调优、流式识别、部分结果
  - 验证: 实时转录准确

- [ ] **4.2 完善 LLM 分析**
  - QSO 日志自动整理、摘要生成
  - 验证: 通联记录自动生成

- [ ] **4.3 完善翻译功能**
  - `translator_translate_text()` 目前返回原文
  - 验证: EN/JA/RU → ZH 翻译正确

- [ ] **4.4 实现频率推荐**
  - `freq_recommender_scan_band()` 目前返回硬编码值
  - 验证: 基于时间/位置推荐合理频率

## Phase 5: 文档 (P2 必要)

- [ ] **5.1 编写 ILI9341 SPI LCD 文档**
  - 配置说明、初始化流程、常见问题
  - 验证: 新手可照着文档点亮屏幕

- [ ] **5.2 编写 ASR 集成文档**
  - SiliconFlow API 配置、VAD 调参、流式识别
  - 验证: 新手可配置并使用 ASR

- [ ] **5.3 更新 defconfig_notes.md**
  - 修正配置符号不一致
  - 添加实际可用的配置说明
  - 验证: 文档与实际 defconfig 一致

- [ ] **5.4 更新 README.md**
  - 将使用说明书替换为作品说明
  - 添加运行方式、目录结构说明
  - 验证: 评委可照着复现

## Phase 6: 比赛准备 (P2 收尾)

- [ ] **6.1 准备哈工大比赛演示**
  - 实物展示: AI Radio HAT + Gemini-S1
  - 演示: 录音 → ASR → AI 整理通联日志
  - 验证: 可流畅演示完整流程

- [ ] **6.2 准备小米 openvela 比赛提交**
  - 完善代码质量
  - 编写 AI Coding 日志
  - 提交 PR

- [ ] **6.3 提交 AI Coding 日志**
  - 导出对话记录到 `logs/` 目录
  - 验证: 格式符合要求

---

## 进度统计

| Phase | 总数 | 已完成 | 进行中 |
|-------|------|--------|--------|
| Phase 1: 能跑 | 4 | 0 | 0 |
| Phase 2: 核心功能 | 6 | 0 | 0 |
| Phase 3: 硬件适配 | 4 | 0 | 0 |
| Phase 4: AI 功能 | 4 | 0 | 0 |
| Phase 5: 文档 | 4 | 0 | 0 |
| Phase 6: 比赛准备 | 3 | 0 | 0 |
| **合计** | **25** | **0** | **0** |
