# AgentApp 开发笔记

> 供 AI Coding Agent 参考的需求与进度文档

## 项目概述

OpenVela AI Agent 的 Android 配套 App，用于：
1. BLE 扫描发现并配网 Agent 设备（手表/开发板）
2. WiFi 连接后管理 Agent 配置（LLM API Key、模型等）
3. 技能推送、查看、删除
4. 实时对话（WebSocket）
5. 查看设备运行日志

## 技术栈

- Kotlin + Jetpack Compose + Material 3
- BLE（蓝牙低功耗）扫描 + GATT 通信
- OkHttp（REST API + WebSocket）
- MVVM 架构（ViewModel + StateFlow）
- DataStore 配置持久化
- 包名：`com.agent.coapp`

## 项目结构

```
app/src/main/java/com/agent/coapp/
├── AgentApplication.kt
├── MainActivity.kt              # 单Activity，ChatViewModel 提升到此层级
├── ble/BleManager.kt            # BLE扫描+连接+配网
├── data/
│   ├── ChatMessage.kt           # 聊天消息、WebSocket响应、BLE设备
│   ├── ConfigData.kt            # AgentConfig（toDeviceMap/fromDeviceMap）
│   └── SkillData.kt             # Skill（含content）、SkillPushRequest
├── network/
│   ├── DeviceApiService.kt      # HTTP API（GET/PUT config, skills CRUD, logs）
│   └── WebSocketManager.kt      # WebSocket对话
├── repository/
│   ├── ConfigRepository.kt      # DataStore + loadDefaultConfig()
│   └── DeviceRepository.kt      # 设备通信统一入口
├── ui/
│   ├── chat/ChatScreen.kt       # 对话界面（长按复制、SelectionContainer）
│   ├── config/ConfigScreen.kt   # 配置界面（自动同步、保存并推送）
│   ├── logs/LogsScreen.kt       # 日志界面（统一横滚、关键词过滤）
│   ├── navigation/Navigation.kt
│   ├── provisioning/BleProvisioningScreen.kt
│   ├── skills/SkillsScreen.kt   # 技能管理（展开内容、长按复制）
│   └── theme/
└── viewmodel/
    ├── ChatViewModel.kt         # Activity scope，保持WebSocket连接
    ├── ConfigViewModel.kt       # syncFromDevice / syncToDevice(edited)
    ├── LogsViewModel.kt
    ├── ProvisioningViewModel.kt
    └── SkillsViewModel.kt       # pushSkill(name, content)
```

## 设备端 REST API

设备（OpenVela AI Agent）通过 WiFi 提供 HTTP API，端口 28789：

| 端点 | 方法 | 请求体 | 说明 |
|------|------|--------|------|
| `/api/config` | GET | - | 获取全部配置 |
| `/api/config` | PUT | `{"key":"value",...}` | 更新配置（支持 llm_backend_0 嵌套） |
| `/api/skills` | GET | - | 获取技能列表（含 content） |
| `/api/skills` | POST | `{"name":"x","content":"..."}` | 推送技能 |
| `/api/skills/{name}` | DELETE | - | 删除技能（支持中文 URL decode） |
| `/api/logs` | GET | - | 获取 agent 运行日志（环形 buffer） |
| `/a2a/health` | GET | - | 健康检查 |
| ws://IP:28789 | WS | - | WebSocket 对话 |

## 当前进度

### ✅ 已完成
- 5 个 Tab 全部功能实现并联调通过
- Config：从手表拉取 → 合并默认值 → 编辑 → 保存并推送 → llm_router 热加载
- Skills：列表展示（含 content 展开）、推送（name+content）、删除（支持中文）
- Chat：WebSocket 对话、长按复制、Tab 切换保持连接
- Logs：设备日志展示、统一横滚、关键词过滤
- BLE 配网界面（待设备端 UUID 对齐）
- 敏感信息清洗，可 public

### 🐛 已知问题 / 历史教训

| 问题 | 根因 | 修复 |
|------|------|------|
| 推送成功但设备未生效 | peek 循环只读 headers，body 还在 socket 里 | `read_full_body()` 根据 Content-Length 读完整 body |
| config_show 不显示 | 设备用 `llm_backend_0` 嵌套 JSON | 写 backend 的同时也写 flat key |
| LLM 对话不用新模型 | llm_router 启动时加载缓存 | 配置写入后调 `llm_router_init()` |
| 多次操作后连接失败 | TCP TIME_WAIT 耗尽 backlog | close 前 `SO_LINGER(0)` 强制 RST |
| 日志接口 crash 重启 | 栈上 25KB（100×256）超过 12KB 线程栈 | 改用 `agent_logbuf` 环形 buffer |
| 日志接口路径不匹配 | App 发 `/api/logs?lines=100`，strcmp 不匹配 | 改用 `strncmp` 前缀匹配 |
| 删除中文技能无效 | OkHttp URL encode 中文，设备端未 decode | 添加 `url_decode()` |
| Tab 切换断开连接 | ChatViewModel 随 composable 销毁 | 提升到 Activity scope |
| 配置推送字段为空 | `saveConfig` 异步未完成就读 DataStore | `syncToDevice(edited)` 直接用编辑值 |

### 🔄 待开发/优化

#### P1
- [ ] BLE 配网与设备端 UUID 对齐
- [ ] 日志自动刷新（定时轮询）

#### P2
- [ ] WebSocket 断线自动重连
- [ ] 深色/浅色主题切换
- [ ] 技能编辑（修改已有技能内容）
- [ ] 配置项表单验证

## 配置同步策略

**核心原则：手表是 source of truth**

```
进入配置页 → GET /api/config（从手表拉）
  ├─ 成功 → 用手表值填充，空字段用 default_config.json 补充
  └─ 失败 → 用本地已保存配置 + 默认值

修改后点"保存并推送" → saveConfig(edited) + PUT /api/config
  → 设备端同时更新 llm_backend_0 + flat keys + llm_router_init()
```

## 设备端配置存储

- 路径：设备端 `<data_dir>/config/config.json`
- LLM 字段（api_key/model/llm_host/llm_path）嵌套在 `llm_backend_0` JSON 中
- 其他字段（tavily_key/volc_*/proxy_*）是 flat key-value
- `llm_router_init()` 从 `llm_backend_0` 加载到内存，运行时不再读文件
