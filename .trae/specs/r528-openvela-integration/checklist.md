# Checklist

- [x] 启动后自动读取 `/data/etc/WiFi/wapi.conf` 并连接 WiFi，失败时重试 3 次
- [x] 连接成功后 `ifconfig wlan0` 能获取到 IP
- [x] LVGL AI 电台窗口能显示频率、模式、ASR 文本、LLM 摘要
- [x] LLM 报警级别 > NONE 时界面有视觉提示
- [x] LRADC 按键事件能被识别，PTT 长按控制 ASR 启停
- [x] I2S2 麦克风数据持续送入 ASR 引擎
- [x] 配置为 16kHz 采样时 ASR 引擎不再做上采样
- [x] 本地构建成功后自动输出代码行数与 ELF 大小
