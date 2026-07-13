\[ [English](../../../en/api/kernel/index.md) | 简体中文 \]

# 内核接口

openvela 内核基于 Apache NuttX RTOS，提供符合 POSIX 标准的系统接口，涵盖进程/线程管理、任务调度、内存管理、信号机制、消息队列等核心功能。本章节详细介绍各内核子系统的 API 接口及使用说明。

- **[线程管理](thread.md)** — POSIX Thread (pthread) 接口
- **[任务调度](sched.md)** — 调度策略、优先级、任务属性
- **[内存管理](mem.md)** — 堆内存分配、内存池、内存信息查询
- **[信号机制](signal.md)** — POSIX 信号、信号处理、信号集
- **[消息队列](msgqueue.md)** — POSIX 消息队列
