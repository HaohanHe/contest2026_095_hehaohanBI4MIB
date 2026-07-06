# WiFi适配驱动指南

> Wiki Token: BEuKwE7soiYqpgkfanLc80upnqb
> URL: https://rivotek.feishu.cn/wiki/BEuKwE7soiYqpgkfanLc80upnqb
> 爬取时间: 2026-07-06 (WebFetch补充)
> 最新修改: 04月30日

---

## WiFi适配驱动指南

- **版本**: 1.0
- **日期**: 2026年1月18日
- **平台**: NuttX RTOS (Allwinner R528)
- **驱动路径**: `vendor/allwinnertech/boards/r528/drivers/realtek_ieee80211`

### 1. 概述

#### 1.1 什么是 WiFi？

WiFi（Wireless Fidelity）是一种基于 IEEE 802.11 标准的无线局域网（WLAN）技术。它允许电子设备（如手机、电脑、嵌入式板卡）通过无线电波连接到网络（通常是互联网），无需物理网线。主要有以下特点:

- **工作频段**：2.4 GHz 和 5 GHz（部分新标准支持 6 GHz）
- **主要标准**：802.11a/b/g/n/ac/ax（WiFi 4/5/6）
- **常见拓扑**：基础设施模式（通过 AP 连接）和自组网模式（设备直连）

#### 1.2 WiFi 的核心角色

在典型的 WiFi 网络中有两个关键角色：

- **AP（Access Point，接入点）**：提供无线覆盖，让其他设备接入的设备（如家用路由器）。
- **STA（Station，站点）**：连接到 AP 的终端设备（如手机、笔记本、IoT 设备）。

### 2. 常用命令

#### 设置 WiFi 为 sta 模式

```
wapi mode wlan0 2
```

#### 扫描周围热点

```
wapi scan wlan0
```

#### 设置 WiFi 密码

```
wapi psk wlan0 "123456" 3
```

设置连接 "我是热点" 的 WiFi 密码。

#### 设置需要连接的 WiFi 名称

```
wapi essid wlan0 "我是热点" 1
```

连接正常的话，应该有相应打印输出。

#### 获取 IP 地址

```
renew wlan0
```

正常不会有打印，错误时一般会有错误打印，表示获取 ip 失败。

#### 查看 IP 信息

```
ipconfig
```

### 备注

- 标题序列需要再组织一下，可以参照蓝牙章节。
- 需要补一些 WIFI 的基础知识介绍，可以参考蓝牙概述部分。
