\[ [English](../../../en/api/framework/index.md) | 简体中文 \]

# 应用框架 (Application Framework)

openvela 应用框架为上层应用提供了统一的系统能力接口，涵盖进程间通信、蓝牙与连接管理、多媒体、电话服务、图形界面、安全可信执行等核心子系统。开发者可通过这些 API 快速构建 IoT 及智能设备应用，而无需关注底层硬件差异。

框架按功能领域划分为以下模块：

- **Binder** — 进程间通信（IPC）框架开发指南（API 与 Android NDK Binder 一致，参见 [Android Binder NDK 文档](https://developer.android.com/ndk/reference/group/ndk-binder)）
- **[蓝牙 (Bluetooth)](bluetooth/index.md)** — 蓝牙协议栈接口，支持 BLE、经典蓝牙及多种 Profile（A2DP、HFP、HID 等）
- **[电话服务 (Telephony)](telephony/index.md)** — 蜂窝网络通信接口，涵盖通话、短信、数据连接、SIM 卡管理等
- **[多媒体 (Media)](media/index.md)** — 音视频播放与录制框架
- **[系统服务 (Services)](services/index.md)** — 应用管理（AMS）与权限管理（PMS）等核心系统服务
- **[Feature](feature/index.md)** — 系统能力（SystemCapability）查询接口
- **[快应用 (QuickApp)](quickapp/index.md)** — 轻量级应用运行时框架
- **[工具库 (Utils)](utils/index.md)** — 日志（Log）与性能追踪（Trace）等通用工具
- **[KVDB](kvdb.md)** — 轻量级键值对持久化存储
- **[安全 (Security)](security.md)** — 基于 OP-TEE 的可信执行环境（TEE）接口
- **[uORB](uorb.md)** — 发布/订阅消息总线，用于模块间的异步数据通信
