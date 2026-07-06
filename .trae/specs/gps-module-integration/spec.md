# GPS 模块集成 Spec

## Why

无线电通联场景下，位置信息是监管、求救信号定位、QSO 日志回溯的核心数据。当前 AI 无线电控制台缺少位置能力，无法回答"这条求救信号来自哪里"、"这次通联发生在什么坐标"等问题。借助小米免费 SMT 券，可以把 GPS 模块直接焊到 Gemini-S1 主控板上，作为标配硬件能力。

## What Changes

- **硬件接口设计**：在 Gemini-S1 扩展接口中预留 GPS 模块插座/焊盘，默认通过 UART 连接，PPS 引脚可选接入 GPIO。
- **GPS 驱动模块**：新增 `gps_receiver.c/.h`，打开 UART 设备，解析 NMEA-0183（`$GNGGA` / `$GNRMC`），提取经纬度、海拔、UTC 时间、速度、定位状态、卫星数。
- **位置服务集成**：新增 `location_service.c/.h`，缓存最新 GPS  fix，提供 `location_service_get_fix()` 接口。
- **日志增强**：电台日志（`radio_log_qso_text`）自动附带当前坐标与时间戳。
- **LLM 上下文增强**：LLM 分析 prompt 中附加当前位置，使"求救信号识别"、"违规通联判断"具备空间维度。
- **板级配置**：在 `radio_config.h` 中增加 `GPS_ENABLED`、`GPS_UART_DEV`、`GPS_BAUD_RATE` 等宏。

**BREAKING**：无破坏性变更；GPS 模块默认为可选启用，未连接时系统继续运行，位置字段显示为无效。

## Impact

- Affected specs：AI 分析管线、电台日志、配置持久化。
- Affected code：
  - `app/ai_radio_console/src/radio_log.c`
  - `app/ai_radio_console/src/agent_bridge.c`
  - `app/ai_radio_console/src/llm_analyzer.c`
  - `app/ai_radio_console/src/main.c`
  - 新增 `app/ai_radio_console/src/gps_receiver.c`
  - 新增 `app/ai_radio_console/include/gps_receiver.h`
  - 新增 `app/ai_radio_console/src/location_service.c`
  - 新增 `app/ai_radio_console/include/location_service.h`

## ADDED Requirements

### Requirement: GPS 硬件接口

The system SHALL provide a hardware interface on Gemini-S1 for a standard UART GPS module.

#### Scenario: 默认连接

- **GIVEN** 主控板已焊接 GPS 模块
- **THEN** GPS TX 接 R528 UART RX，GPS RX 接 R528 UART TX，GPS PPS 接可选 GPIO
- **AND** 供电 3.3V，模块推荐 ATGM336H 或 u-blox NEO-6M/7M

### Requirement: NMEA 解析

The system SHALL parse NMEA-0183 sentences from the GPS module.

#### Scenario: 有效定位

- **WHEN** GPS 输出 `$GNGGA` 或 `$GNRMC` 且 fix 有效
- **THEN** 系统提取纬度、经度、海拔、UTC 时间、速度、卫星数
- **AND** 更新当前位置缓存

#### Scenario: 无效定位

- **WHEN** GPS 尚未定位或信号丢失
- **THEN** 位置缓存标记为无效，系统继续运行

### Requirement: 位置服务接口

The system SHALL provide a location service for other modules to query the latest fix.

#### Scenario: 查询最新位置

- **WHEN** 其他模块调用 `location_service_get_fix()`
- **THEN** 返回最新有效 fix 或无效标记

### Requirement: 日志附带位置

The system SHALL attach location and UTC timestamp to radio QSO logs.

#### Scenario: 写入通联日志

- **WHEN** ASR final 结果写入日志
- **THEN** 日志条目包含频率、模式、文本、UTC 时间、经纬度、海拔

### Requirement: LLM 分析附带位置上下文

The system SHALL include location context in LLM analysis prompts.

#### Scenario: 求救信号分析

- **WHEN** LLM 分析一段包含求救关键词的转写文本
- **THEN** prompt 中附加当前坐标
- **AND** LLM 输出包含位置信息

## MODIFIED Requirements

无。

## REMOVED Requirements

无。
