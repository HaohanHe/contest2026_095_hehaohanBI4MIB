# Checklist

- [x] GPS UART 设备可打开，NMEA 数据可读
- [x] `$GNGGA` / `$GNRMC` 解析正确，校验和验证通过
- [x] 无效 fix 时位置缓存标记为无效，系统不崩溃
- [x] `location_service_get_fix()` 线程安全返回最新坐标
- [x] 电台日志条目包含 UTC 时间、经纬度、海拔
- [x] LLM 分析 prompt 包含当前位置上下文
- [x] `radio_config.h` 中 GPS 配置宏完整
- [x] 本地 `make -f Makefile.local` 构建成功
- [x] 代码注释中说明了推荐 GPS 模块型号与接线
