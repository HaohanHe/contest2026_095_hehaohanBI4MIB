# DeepSeek LLM Demo 实现设计与新手指南

> Wiki Token: DatswzFGAiPdbSkc5wGc5TVcnhf
> URL: https://rivotek.feishu.cn/wiki/DatswzFGAiPdbSkc5wGc5TVcnhf
> 爬取时间: 2026-07-06 (WebFetch补充)
> 最新修改: 01月19日

---

## DeepSeek LLM Demo 实现设计与新手指南

本文档基于 `vendor/allwinnertech/apps/deepseek_demo` 源码，详细解析 OpenVela 系统上 DeepSeek 大模型对话应用的实现流程、架构设计及配置说明，帮助开发者从零开始构建此类 AI 应用。

### 1. 项目简介

**deepseek_demo** 是一个运行在 OpenVela (NuttX) 系统上的 C++ 应用程序，它通过 HTTP 协议与 DeepSeek 在线 API 进行交互，实现了智能对话功能。该应用展示了如何在嵌入式系统上集成大语言模型 (LLM) 能力，并支持以下高级特性：

- **CLI 交互模式**：直接在终端进行对话。
- **服务模式 (Agent Mode)**：作为后台服务运行，响应来自其他应用的请求。
- **工具调用 (MCP Integration)**：支持 Model Context Protocol (MCP)，允许模型调用系统工具（如读取传感器、控制设备）。

### 2. 目录结构与模块说明

deepseek_demo 应用的源码位于 `vendor/allwinnertech/apps/deepseek_demo`，主要包含以下模块：

- **HTTP 客户端模块**：负责与 DeepSeek API 的网络通信
- **JSON 解析模块**：处理 API 请求/响应数据
- **对话管理模块**：管理对话上下文和历史记录
- **MCP 工具调用模块**：实现 Model Context Protocol，支持工具调用
- **CLI / Agent 模式切换**：支持多种运行模式
