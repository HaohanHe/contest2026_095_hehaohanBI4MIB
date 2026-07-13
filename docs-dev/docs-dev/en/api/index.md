\[ English | [简体中文](../../zh-cn/api/index.md) \]

# API Reference

This document provides the API reference for the openvela operating system, covering the complete interface specification from kernel system calls to application frameworks.

openvela is built on Apache NuttX RTOS, follows the POSIX standard, and supports multiple architectures including ARM, ARM64, RISC-V, and x86_64. Developers can refer to the following sections for APIs at each layer:

- **[Kernel Interfaces](kernel/index.md)** — POSIX-compatible system interfaces for process/thread management, task scheduling, memory management, signal mechanisms, message queues, etc.
- **[Network Interfaces](network/index.md)** — Standard network programming interfaces including BSD sockets, DNS resolution, etc.
- **[Application Framework](framework/index.md)** — Upper-layer capability interfaces including Binder IPC, Bluetooth, multimedia, security (TEE + Keystore), uORB message bus, etc.
