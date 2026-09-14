# AI Radio Console - QuickApp

2026国际空间通信挑战赛 · 匠造空间通信赛道

## 技术方案

**AI Agent框架 + QuickApp小程序**

- UI: QuickApp (JS/HTML/CSS)
- AI: `@app-module/system.velaclaw` 调用端侧AI Agent
- LLM: DeepSeek/MiMo 通过AI Agent框架
- 平台: Gemini-S1 (Allwinner R528)

## 核心代码

```javascript
import velaclaw from '@app-module/system.velaclaw'

// 调用AI分析
velaclaw.ask({
  query: '分析当前频率14.250MHz的通联状态',
  success: (res) => {
    console.log('AI reply:', res.reply)
  }
})
```

## 项目结构

```
quickapp/ai_radio/
├── src/
│   ├── manifest.json          # 应用配置
│   ├── app.ux                 # 应用入口
│   ├── pages/index/index.ux   # 主页面
│   ├── common/                # 公共资源
│   └── i18n/                  # 多语言
└── README.md
```

## 部署步骤

### 1. 刷官方固件
```bash
# 使用PhoenixSuit刷入 gemini_s1_mini.img
```

### 2. 启动AI Agent
```bash
ai_agent &
set_llm deepseek <your_api_key>
```

### 3. 推送QuickApp
```bash
# 打包为rpk（需要AIoT IDE或npm工具）
# 解压并推送
adb push com.openvela.contest2026.ai_radio /data/app/com.openvela.contest2026.ai_radio
```

### 4. 启动应用
```bash
vapp hap://app/com.openvela.contest2026.ai_radio
```

## 功能

- FM收音机频率显示和调谐
- 频谱分析显示（0-4kHz）
- CW莫尔斯码解码
- S-meter信号强度显示
- MAYDAY求救信号检测
- AI语音分析（通过velaclaw调用AI Agent）
