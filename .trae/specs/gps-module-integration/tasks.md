# Tasks

- [x] Task 1: 新增 GPS 接收器模块与 NMEA 解析
  - [x] SubTask 1.1: 新增 `gps_receiver.h`，定义 GPS fix 结构体与接口
  - [x] SubTask 1.2: 新增 `gps_receiver.c`，实现 UART 打开、NMEA 读取、`$GNGGA`/`$GNRMC` 解析
  - [x] SubTask 1.3: 实现校验和检查与无效 fix 处理
  - [x] SubTask 1.4: 本地构建验证

- [x] Task 2: 新增位置服务缓存
  - [x] SubTask 2.1: 新增 `location_service.h`，声明 `location_service_init/start/stop/get_fix`
  - [x] SubTask 2.2: 新增 `location_service.c`，缓存最新 fix，线程安全
  - [x] SubTask 2.3: 在 `main.c` 中初始化并启动位置服务
  - [x] SubTask 2.4: 本地构建验证

- [x] Task 3: 在 `radio_config.h` 中增加 GPS 配置宏
  - [x] SubTask 3.1: 增加 `GPS_ENABLED`、`GPS_UART_DEV`、`GPS_BAUD_RATE`、`GPS_UPDATE_HZ`
  - [x] SubTask 3.2: 本地构建验证

- [x] Task 4: 电台日志自动附带位置与时间戳
  - [x] SubTask 4.1: 修改 `radio_log.c` 的 `radio_log_qso_text`，从 location_service 获取 fix
  - [x] SubTask 4.2: 日志格式增加 `timestamp`, `latitude`, `longitude`, `altitude`
  - [x] SubTask 4.3: 本地构建验证

- [x] Task 5: LLM 分析 prompt 附加位置上下文
  - [x] SubTask 5.1: 修改 `llm_analyzer.c` 的分析 prompt，附加当前坐标
  - [x] SubTask 5.2: 修改 `analysis_result_t` 增加 `location` 字段
  - [x] SubTask 5.3: 本地构建验证

- [x] Task 6: 文档化硬件接口
  - [x] SubTask 6.1: 在代码注释中说明推荐 GPS 模块型号、引脚连接、供电要求
  - [x] SubTask 6.2: 本地构建验证

# Task Dependencies

- Task 2 依赖 Task 1
- Task 4 依赖 Task 2
- Task 5 依赖 Task 2
- Task 3 无依赖
- Task 6 无依赖
