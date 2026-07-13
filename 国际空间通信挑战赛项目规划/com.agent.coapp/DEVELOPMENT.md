# 开发流程

## 环境准备

### 必需工具
- JDK 17（命令行编译用）或 Android Studio Hedgehog+
- Android SDK 34
- 一台 Android 手机（minSdk 26，即 Android 8.0+）
- （可选）一台 OpenVela 设备 + USB 串口线（联调用）

### 首次设置
```bash
# 1. 克隆仓库
git clone git@github.com:tnaught/com.vela.openapp.git
cd com.vela.openapp

# 2. 创建本地配置文件（含真实 API Key，不提交 git）
cp app/src/main/assets/default_config.example.json \
   app/src/main/assets/default_config.json
# 编辑 default_config.json 填入你的 key

# 3. 编译
JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64 ./gradlew assembleDebug
```

---

## 编译与安装

### App 端
```bash
# 编译
JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64 ./gradlew assembleDebug

# 安装到手机
adb install -r app/build/outputs/apk/debug/app-debug.apk

# 启动
adb shell am start -n com.agent.coapp/.MainActivity
```

### 设备端（OpenVela AI Agent）

设备端固件编译和烧录请参考 [OpenVela 官方文档](https://github.com/open-vela)。
确保设备运行的固件包含 AI Agent 应用（提供 REST API + WebSocket 服务）。

---

## 联调

### 网络连通
- App 手机和 OpenVela 设备需在同一 WiFi
- 设备 IP 通过串口 `ifconfig` 获取
- 默认端口 28789

### 调试日志
```bash
# App 日志
adb logcat --pid=$(adb shell pidof com.agent.coapp)

# 设备串口日志
minicom -D /dev/ttyACMx -b 115200
```

### REST API 手动测试
```bash
DEVICE=http://<手表IP>:28789

# 获取配置
curl $DEVICE/api/config

# 推送配置
curl -X PUT $DEVICE/api/config -H "Content-Type: application/json" \
  -d '{"model":"mimo-v2.5-pro"}'

# 获取技能列表
curl $DEVICE/api/skills

# 推送技能
curl -X POST $DEVICE/api/skills -H "Content-Type: application/json" \
  -d '{"name":"test","content":"---\ndescription: test skill\n---\nHello"}'

# 删除技能
curl -X DELETE $DEVICE/api/skills/test

# 获取日志
curl $DEVICE/api/logs
```

---

## 分支策略
```
main          ← 稳定版本
├── feature/* ← 功能开发
└── bugfix/*  ← Bug 修复
```

---

## 验证清单

| # | 功能 | 验证步骤 | 预期结果 |
|---|------|----------|----------|
| 1 | BLE 扫描 | 配网页 → 点击扫描 | 列出附近 Agent 设备 |
| 2 | 配置拉取 | 配置页 → 输入 IP → 从设备拉取 | 显示设备当前配置 |
| 3 | 配置推送 | 修改配置 → 保存并推送 | 设备 config_show 显示新值 |
| 4 | 技能列表 | 技能页 → 加载 | 显示设备已安装技能 |
| 5 | 技能推送 | 点击 + → 填写名称+内容 → 推送 | 设备热加载新技能 |
| 6 | 技能删除 | 长按删除 → 确认 | 设备移除技能文件 |
| 7 | 技能内容 | 点击技能卡片展开 | 显示完整 markdown 内容 |
| 8 | 对话 | 对话页 → 连接 → 发消息 | 收到设备回复 |
| 9 | 对话复制 | 长按消息气泡 | 复制到剪贴板 |
| 10 | 日志查看 | 日志页 → 刷新 | 显示 agent 运行日志 |
| 11 | Tab 切换 | 对话中切到其他 Tab 再回来 | WebSocket 连接保持 |

---

## 项目约定

- **代码风格**：Kotlin 官方风格，Compose 函数大驼峰
- **提交规范**：`type: description`（feat/fix/refactor/docs/chore）
- **包结构**：按功能分层（data/network/repository/viewmodel/ui）
- **状态管理**：StateFlow + collectAsState
- **敏感信息**：API Key 放 `default_config.json`（已 gitignore），不提交
