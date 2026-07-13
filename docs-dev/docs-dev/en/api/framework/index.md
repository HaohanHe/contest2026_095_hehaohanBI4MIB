\[ English | [简体中文](../../../zh-cn/api/framework/index.md) \]

# Application Framework

The openvela application framework provides unified system capability interfaces for upper-layer applications, covering core subsystems such as inter-process communication, Bluetooth and connectivity management, multimedia, telephony services, graphical user interfaces, and trusted execution environments. Developers can use these APIs to quickly build IoT and smart device applications without worrying about underlying hardware differences.

The framework is organized into the following modules by functional domain:

- **Binder** — Inter-Process Communication (IPC) framework development guide (API is consistent with Android NDK Binder, see [Android Binder NDK Documentation](https://developer.android.com/ndk/reference/group/ndk-binder))
- **[Bluetooth](bluetooth/index.md)** — Bluetooth protocol stack interfaces, supporting BLE, Classic Bluetooth, and various profiles (A2DP, HFP, HID, etc.)
- **[Telephony](telephony/index.md)** — Cellular network communication interfaces, covering voice calls, SMS, data connections, SIM card management, etc.
- **[Media](media/index.md)** — Audio/video playback and recording framework
- **[Services](services/index.md)** — Core system services including Activity Manager Service (AMS) and Package Manager Service (PMS)
- **[Feature](feature/index.md)** — SystemCapability query interfaces
- **[QuickApp](quickapp/index.md)** — Lightweight application runtime framework
- **[Utils](utils/index.md)** — Common utilities including Log and Trace
- **[KVDB](kvdb.md)** — Lightweight key-value persistent storage
- **[Security](security.md)** — Trusted Execution Environment (TEE) interfaces based on OP-TEE
- **[uORB](uorb.md)** — Publish/subscribe message bus for asynchronous inter-module data communication
