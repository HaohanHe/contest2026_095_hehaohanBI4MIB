# AI Radio Console 最小 NuttX defconfig 开关说明

本文件列出构建并运行 `ai_radio_console` 所需的 NuttX/OpenVela 关键配置开关。

> 注意：以下开关名称以 NuttX/OpenVela 常见命名为准，不同板级 defconfig 可能略有差异。若不确定，请根据实际板级 defconfig 调整。

---

## 一、NuttX 基础功能开关

| 功能 | 建议配置 | 说明 |
|------|----------|------|
| pthreads | `CONFIG_PTHREAD=y` | 音频采集、传感器采样、网络请求等依赖多线程 |
| mqueue | `CONFIG_MQ_MAXMSGSIZE=256` 等 | 线程间消息队列（如使用） |
| POSIX timers | `CONFIG_TIMER=y` | LVGL 刷新、UI 定时器等 |
| Networking | `CONFIG_NET=y` | 网络功能总开关 |
| Sockets | `CONFIG_NET_SOCKET=y` | SiliconFlow 等 HTTPS 请求依赖 |
| DNS / gethostbyname | `CONFIG_NETDB_DNSCLIENT=y` | 解析 api.siliconflow.cn 等域名 |
| libc math | `CONFIG_LIBM=y` | FFT、Goertzel、滤波器等数学运算 |
| TLS / mbedtls | `CONFIG_NETUTILS_MBEDTLS=y` | HTTPS/TLS 连接到 SiliconFlow 等云端服务 |

### 示例片段

```
CONFIG_PTHREAD=y
CONFIG_PTHREAD_MUTEX_TYPES=y
CONFIG_PTHREAD_CLEANUP_STACKSIZE=1
CONFIG_MQ_MAXMSGSIZE=256
CONFIG_MQ_PREALLOC_MSGS=4
CONFIG_TIMER=y
CONFIG_NET=y
CONFIG_NET_SOCKET=y
CONFIG_NET_TCP=y
CONFIG_NET_IPv4=y
CONFIG_NETDEV_LATEINIT=y
CONFIG_NETDB_DNSCLIENT=y
CONFIG_NETDB_DNSCLIENT_NAMESIZE=64
CONFIG_LIBM=y
CONFIG_NETUTILS_MBEDTLS=y
```

---

## 二、硬件驱动开关

| 硬件 | 建议配置 | 说明 |
|------|----------|------|
| I2S audio | `CONFIG_AUDIO_I2S=y` | 从 `/dev/audio/pcm0c` 采集音频 |
| Audio 子系统 | `CONFIG_AUDIO=y` | NuttX audio 框架总开关 |
| LRADC input | `CONFIG_INPUT_LRADCC=y`（请根据实际板级 defconfig 调整） | 按键/旋钮输入 |
| UART（GPS） | `CONFIG_UARTx_SERIAL_CONSOLE` / `CONFIG_UARTx=y`（x 为实际串口号） | 连接 GPS 模块读取 NMEA 数据 |
| I2C/SPI（传感器） | `CONFIG_I2C=y` 或 `CONFIG_SPI=y` | SHTC3、LTR553 的连接总线 |
| SHTC3 驱动 | `CONFIG_SENSORS_SHTC3=y`（请根据实际板级 defconfig 调整） | 温湿度传感器 |
| LTR553 驱动 | `CONFIG_SENSORS_LTR553=y`（请根据实际板级 defconfig 调整） | 光照/接近传感器 |

### 示例片段

```
CONFIG_AUDIO=y
CONFIG_AUDIO_I2S=y
CONFIG_AUDIO_FORMAT_PCM=y

# LRADC 输入（请根据实际板级 defconfig 调整）
CONFIG_INPUT=y
CONFIG_INPUT_LRADCC=y

# GPS UART（请根据实际板级 defconfig 调整串口号）
CONFIG_UART1=y
CONFIG_UART1_SERIAL_CONSOLE=n

# 传感器总线（请根据实际连接选择 I2C 或 SPI）
CONFIG_I2C=y
# CONFIG_SPI=y

# 传感器驱动（请根据实际板级 defconfig 调整）
CONFIG_SENSORS_SHTC3=y
CONFIG_SENSORS_LTR553=y
```

---

## 三、LVGL 相关开关

| 功能 | 建议配置 | 说明 |
|------|----------|------|
| LVGL 库 | `CONFIG_LVX_USE_LVGL=y`（请根据实际板级 defconfig 调整） | LVGL 图形库 |
| 显示屏驱动 | `CONFIG_LCD=y` / `CONFIG_LCD_ILI9341=y`（请根据实际板级 defconfig 调整） | 2.8" ILI9341 SPI LCD |
| 输入设备 | `CONFIG_LVX_USE_INPUT=y`（请根据实际板级 defconfig 调整） | 触摸屏/按键输入 |

### 示例片段

```
# LVGL（请根据实际板级 defconfig 调整）
CONFIG_LVX_USE_LVGL=y
CONFIG_LV_USE_DEMO_WIDGETS=n

# 显示屏（请根据实际板级 defconfig 调整）
CONFIG_LCD=y
CONFIG_LCD_ILI9341=y

# 输入设备（请根据实际板级 defconfig 调整）
CONFIG_LVX_USE_INPUT=y
```

---

## 四、Wi-Fi 相关开关

| 功能 | 建议配置 | 说明 |
|------|----------|------|
| WLAN | `CONFIG_WLAN=y`（请根据实际板级 defconfig 调整） | 无线网络支持 |
| WAPI | `CONFIG_NETUTILS_WAPI=y`（请根据实际板级 defconfig 调整） | Wi-Fi 配置与连接工具 |

### 示例片段

```
# Wi-Fi（请根据实际板级 defconfig 调整）
CONFIG_WLAN=y
CONFIG_NETUTILS_WAPI=y
```

---

## 五、快速检查清单

复制以下内容到 `.config` 或 `defconfig` 后，再根据实际板级配置微调：

```
# 基础功能
CONFIG_PTHREAD=y
CONFIG_PTHREAD_MUTEX_TYPES=y
CONFIG_PTHREAD_CLEANUP_STACKSIZE=1
CONFIG_MQ_MAXMSGSIZE=256
CONFIG_MQ_PREALLOC_MSGS=4
CONFIG_TIMER=y
CONFIG_NET=y
CONFIG_NET_SOCKET=y
CONFIG_NET_TCP=y
CONFIG_NET_IPv4=y
CONFIG_NETDEV_LATEINIT=y
CONFIG_NETDB_DNSCLIENT=y
CONFIG_NETDB_DNSCLIENT_NAMESIZE=64
CONFIG_LIBM=y
CONFIG_NETUTILS_MBEDTLS=y

# 硬件驱动
CONFIG_AUDIO=y
CONFIG_AUDIO_I2S=y
CONFIG_AUDIO_FORMAT_PCM=y
CONFIG_INPUT=y
CONFIG_INPUT_LRADCC=y
CONFIG_UART1=y
CONFIG_I2C=y
CONFIG_SENSORS_SHTC3=y
CONFIG_SENSORS_LTR553=y

# LVGL
CONFIG_LVX_USE_LVGL=y
CONFIG_LCD=y
CONFIG_LCD_ILI9341=y
CONFIG_LVX_USE_INPUT=y

# Wi-Fi
CONFIG_WLAN=y
CONFIG_NETUTILS_WAPI=y
```

---

## 六、manifest 集成说明

`contest2026_095_hehaohanBI4MIB.xml` 中已包含正确的 linkfile：

```xml
<linkfile src="app/ai_radio_console" dest="packages/demos/contest2026_095_ai_radio_console"/>
```

该 linkfile 将本应用链接到 OpenVela 工作区的 `packages/demos/contest2026_095_ai_radio_console`，与 Kconfig 中 `Application Configuration → Demos → Contest 2026 team 095 AI Radio Console` 的路径一致。
