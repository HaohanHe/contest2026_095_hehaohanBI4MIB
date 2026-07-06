# openvela-Backtrace 使用指南

> Wiki Token: TOccwKc3Ni8BZqkPRZWcVPVjnMf
> URL: https://rivotek.feishu.cn/wiki/TOccwKc3Ni8BZqkPRZWcVPVjnMf
> 爬取时间: 2026-07-06 (WebFetch补充)
> 最新修改: 01月18日

---

## openvela-Backtrace 使用指南

### 一. 概述

在日常开发中，查看指定线程的栈信息是常见需求，尤其在处理死锁、高 CPU 使用率、忙循环、系统崩溃或内存调试等场景时。通常，开发者可借助 JLink 等调试工具，通过 gdb 和断点 (bp) 实现这些功能，但设备封包发布后，外围调试功能关闭，这些方法在真实设备上往往失效。

为解决这一限制，openvela 支持在运行环境中直接查看特定线程的栈信息。当程序崩溃或线程阻塞时，backtrace (回溯/堆栈跟踪) 能清晰展示从当前执行点到初始调用者的完整调用链，是诊断死锁、崩溃等问题的核心工具。

### 二. 配置说明

#### 1. 通用配置说明

openvela 支持 ARM、RISC-V 和 Xtensa 三种架构的通用配置，具体如下：

```makefile
# 开启 backtrace 功能，默认不显示函数名称
CONFIG_SCHED_BACKTRACE=y
CONFIG_SYSTEM_DUMPSTACK=y

# 如果需要符号名支持，启用以下选项；
# 若 Flash 空间不足，可以通过 addr2line 手动解析地址到符号
CONFIG_ALLSYMS=y

# 启用架构支持
CONFIG_ARCH_HAVE_BACKTRACE=y
```

### 三. 使用方法

#### 1. 查看当前线程 backtrace

在 NSH 命令行中：

```
nsh> ps
nsh> backtrace <pid>
```

#### 2. 崩溃时自动 dump 栈

启用 `CONFIG_SYSTEM_DUMPSTACK` 后，系统崩溃时会自动打印当前栈信息。

#### 3. 使用 addr2line 解析符号

如果未启用 `CONFIG_ALLSYMS`，可以通过 addr2line 工具手动解析：

```bash
arm-none-eabi-addr2line -e nuttx.elf 0x40012abc
```

### 四. 常见问题

#### Q1: backtrace 显示 `??` 而不是函数名

- 检查是否启用 `CONFIG_ALLSYMS=y`
- 或使用 `addr2line` 手动解析

#### Q2: 栈信息不完整

- 增大栈大小 (stacksize)
- 检查栈溢出
- 确保编译时未使用 `-fomit-frame-pointer` 选项
