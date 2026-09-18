# DayNote MMDVM 数字语音热点 - 技术方案

## 目标
把 Gemini-S1 (R528) 变成 DMR 数字语音热点，连接 BrandMeister 网络，支持与其他 DMR 电台通信。

## 硬件现状
- **Wi-Fi**: RTL8733BS (SDIO)，已启用，可联网
- **USB**: 仅设备模式（USBDEV/Gadget），主机模式未实现（-ENOSYS）
- **UART**: 可用（UART2 当前为控制台，可复用其他 UART）
- **显示**: 2.8" ILI9341 320x240 电阻屏
- **音频**: I2S 输入/输出（媒体服务器已配置）

## 技术障碍
1. **USB 主机模式未实现**: R528 NuttX BSP 的 USB OTG 主机模式返回 -ENOSYS（otg.c:279），EP 数据通路未实现。无法直接 USB 连接 OpenGD77 DM-1701。
2. **C++ 支持未启用**: defconfig 无 C++ 配置，NuttX 有 libxx (uClibc++) 可启用。
3. **MMDVMHost 是 C++17**: 完整移植工作量大，依赖 pthread/socket/termios/mosquitto。

## 分阶段实施方案

### 阶段1：DMR 网络客户端（纯软件，无需射频硬件）✅ 可立即实现
**原理**: MMDVM Homebrew 协议是纯 UDP 协议，通过 Wi-Fi 直接连接 BrandMeister master，不需要射频调制解调器。

**功能**:
- 连接 BrandMeister master（UDP 62031）
- MMDVM Homebrew 协议握手（登录/配置/心跳）
- 接收 DMR 语音帧，解析源ID/目标ID/时隙/通话类型
- 显示当前通话：呼号/姓名/DMR ID/时隙/TalkGroup/时长
- 通话历史记录
- 发送 PTT 激活（需要射频前端才能实际发射语音，阶段1可发短消息）
- 静态 TalkGroup 订阅

**技术栈**: 纯 C，POSIX socket，集成到 DayNote 应用

**UI**: DayNote 新增第5页 "DMR Hotspot"
- 连接状态（已连接/断开/ master 地址）
- 当前通话卡片（源呼号/ID/目标TG/时隙/时长/信号强度）
- 最近通话列表
- TalkGroup 快速切换
- PTT 按钮（阶段1为占位，阶段2启用）

### 阶段2：UART MMDVM 调制解调器（需硬件）
**方案A**: MMDVM_HS 板子（STM32F103+ADF7021，约¥50-100）
- 通过 UART 连接 Gemini-S1（TX/RX/GND）
- 实现 MMDVM 串口协议（与 MMDVMHost 的 Modem.cpp 兼容）
- 射频收发 DMR 信号

**方案B**: 改装 OpenGD77 DM-1701
- 拆开 DM-1701，焊接 UART 引脚（TX/RX/GND）
- 修改 OpenGD77 固件，支持 UART MMDVM 模式（当前仅 USB）
- 或保持 USB 模式，等阶段3

**功能**: 完整 DMR 射频收发，与其他 DMR 电台直接通信

### 阶段3：USB 主机驱动（需底层开发）
- 实现 R528 USB OTG 主机控制器驱动（参考 contest2026_137 的工作）
- 启用 NuttX usbhost_cdcacm 驱动
- 直接 USB 连接 OpenGD77 DM-1701
- 完整 MMDVMHost 功能

## MMDVM Homebrew 协议（阶段1核心）
基于 UDP，端口 62031（默认）

### 数据包格式
```
字节0: 签名 (0xE0) 或命令类型
字节1-4: 流ID (大端)
字节5+: 数据
```

### 主要命令
- `RPTLOGIN` / `RPTLOGINACK`: 登录握手
- `RPTCONFIG`: 配置请求
- `RPTPING`: 心跳
- `DMRD`: DMR 数据（语音帧/数据帧）
- `DMRDACK`: DMR 数据确认

### DMR 数据帧
```
标签(1B) + 序列(1B) + 源ID(3B) + 目标ID(3B) + 时隙(1B) + 帧类型(1B) + 数据流(33B)
```

## BrandMeister 网络
- Master 服务器: 全球多个节点（如 3101 中国、3102 中国等）
- 默认端口: 62031 UDP
- 需要 DMR ID（可在 radioid.net 免费注册）
- Hotspot Security: 在 BrandMeister SelfCare 设置密码

## 文件结构
```
app/ai_radio_console/
├── src/daynote_mmdvm.c      (新增 - MMDVM 网络客户端)
├── src/daynote_mmdvm_ui.c   (新增 - DMR 热点 UI 页面)
├── include/daynote_mmdvm.h  (新增 - 协议定义和 API)
├── src/daynote_main.c       (修改 - 注册第5页)
└── src/daynote_ui.c         (修改 - 页面切换)
```

## 参考资源
- MMDVMHost: g4klx/MMDVMHost (C++, DMRNetwork.cpp)
- ESP32 MMDVM Hotspot: javastraat/esp32_mmdvm_hotspot (FreeRTOS 嵌入式实现)
- OpenGD77: LibreDMR/OpenGD77 (对讲机固件，支持 USB MMDVM 热点模式)
- MMDVM_HS: juribeparada/MMDVM_HS (STM32 热点板固件)
- BrandMeister: help.brandmeister.network
