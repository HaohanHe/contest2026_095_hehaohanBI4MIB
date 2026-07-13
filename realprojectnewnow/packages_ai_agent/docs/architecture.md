# 架构

## 架构图

[查看交互式架构图 (HTML)](agent-arch.html) · [Excalidraw 源文件](agent-layers.excalidraw.json)

## 模块概览

```
agent/
├── include/                  # 公共头文件
├── src/
│   ├── agent_main.c       # 入口
│   ├── core/                 # Agent 核心
│   │   ├── agent_loop.c      # ReAct 循环
│   │   ├── agent_trace.c     # 结构化追踪
│   │   ├── context_builder.c # 上下文构建
│   │   ├── message_bus.c     # 消息总线
│   │   ├── message_bus_tap.c # 消息监听
│   │   ├── memory_store.c    # 长期记忆
│   │   └── session_mgr.c     # 会话管理
│   ├── llm/                  # LLM 代理层
│   │   ├── llm_proxy.c       # OpenAI API 代理
│   │   ├── llm_parse.c       # 响应解析
│   │   ├── llm_vision.c      # 视觉模型
│   │   ├── llm_router.c      # 多后端路由
│   │   └── llm_cache.c       # 响应缓存
│   ├── tools/                # 工具系统
│   │   ├── tool_registry.c   # 工具注册表
│   │   ├── tool_*.c          # 20+ 内置工具
│   │   ├── mcp_server.c      # MCP 服务器
│   │   ├── mcp_client.c      # MCP Client（远程工具）
│   │   ├── skill_loader.c    # 技能加载
│   │   └── tool_guard.c      # 安全加固
│   ├── channels/             # 接入通道
│   │   ├── nsh_commands.c    # NSH CLI
│   │   ├── cmd_*.c           # CLI 子命令
│   │   ├── feishu_*.c        # 飞书 Bot
│   │   ├── weixin_channel.c  # 微信 Bot
│   │   ├── mqtt_channel.c    # MQTT 通道
│   │   └── ws_server.c       # WebSocket 服务器
│   ├── voice/                # 语音管线
│   │   ├── voice_channel.c   # 语音通道
│   │   ├── voice_tts.c       # TTS 抽象层
│   │   ├── voice_asr.c       # ASR 抽象层
│   │   ├── volc_tts*.c       # 火山 TTS 后端
│   │   ├── volc_asr.c        # 火山 ASR 后端
│   │   ├── audio_capture.c   # 音频采集
│   │   └── audio_playback.c  # 音频播放
│   ├── infra/                # 基础设施
│   │   ├── config_store.c    # 配置存储
│   │   ├── cron_service.c    # 定时任务
│   │   ├── heartbeat.c       # 心跳检查
│   │   ├── network_manager.c # 网络管理
│   │   ├── http_proxy.c      # HTTP 代理
│   │   └── vela_tls.c        # mbedTLS HTTPS
│   ├── node/                 # 分布式节点
│   │   ├── node_client.c     # Node 客户端
│   │   └── node_manager.c    # Hub 管理
│   └── stubs.c               # 平台兼容 stub
├── CMakeLists.txt
└── Kconfig
```

## 数据流

```
用户输入 (CLI/飞书/WS/MQTT/Voice)
    ↓
message_bus (inbound)
    ↓
agent_loop (ReAct 循环)
    ├── context_builder → 系统提示 + Skills 摘要
    ├── llm_proxy → LLM API 调用
    ├── tool_registry → 工具执行
    │   ├── 内置工具（直接调用）
    │   ├── mcp_bridge（本地 MCP Server）
    │   ├── node_manager（远程 Node）
    │   └── mcp_client（远程 MCP Server via Streamable HTTP）
    └── session_mgr → 会话持久化
    ↓
message_bus (outbound)
    ↓
用户输出 (CLI/飞书/WS/MQTT/TTS)
```

## 内存管理

| 机制 | 说明 |
|------|------|
| 堆检测 | `mallinfo()` 实时查询，内存不足时拒绝请求 |
| 内存池 | 预分配工具输出缓冲区，避免频繁 malloc |
| 流式输出 | 1KB 起步按需扩展到 64KB |
| 安全分配 | 动态计算安全大小，保留 32KB 给其他子系统 |

## 安全机制

| 机制 | 说明 |
|------|------|
| 工具白名单 | `tool_enabled_<name>` 持久化开关 |
| 敏感工具限流 | 60s 窗口内最多 10 次调用 |
| 输入大小检查 | 单次工具调用上限 32KB |
| Shell 三级策略 | Allowlist / Full / Deny |
| 日志脱敏 | API Key 等敏感信息不打印 |
