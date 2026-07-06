# OpenVela (R528) 按键驱动实现概要

> Wiki Token: Rjp8we1oviCrsgk9OkccFvOhnJh
> URL: https://rivotek.feishu.cn/wiki/Rjp8we1oviCrsgk9OkccFvOhnJh
> 爬取时间: 2026-07-06 (WebFetch补充)
> 最新修改: 01月19日

---

## OpenVela (R528) 按键驱动实现概要

### 1. 总体架构设计

OpenVela 的按键子系统采用了 NuttX 标准的 Lower Half + Upper Half 驱动模型：

- **硬件层**: R528 LRADC (Low Resolution ADC) 模块，支持按键电压采样。
- **HAL 层**: `sunxi_hal_lradc.c`，提供硬件寄存器操作、中断回调注册机制。
- **Lower Half (驱动核心)**: `drv_lradc.c`，实现了 `struct btn_lowerhalf_s` 接口，负责将 LRADC 的模拟电压值映射为标准的 Button Bitmask。
- **Upper Half (字符设备)**: `nuttx/drivers/input/button_upper.c` (在 `drv_lradc.c` 中直接 include 编译)，注册字符设备节点 `/dev/input/eventX`，向用户空间提供 `read/poll` 接口。
- **应用层**: 用户程序通过 POSIX 文件 API 读取按键事件。

### 2. 数据流图

```mermaid
graph TD
    HW[R528 LRADC Hardware] -- IRQ --> HAL[sunxi_hal_lradc]
    HAL -- Callback --> Lower[drv_lradc.c (Lower Half)]
    Lower -- g_btnhandler --> Upper[button_upper.c (Upper Half)]
    Upper -- /dev/input/event1 --> App[factory_test / User App]
```

### 3. 工作流程

1. **硬件采样**：LRADC 模块周期性采样按键电压
2. **中断触发**：当电压值变化超过阈值时触发中断
3. **HAL 处理**：`sunxi_hal_lradc.c` 读取寄存器，清除中断标志，调用注册的回调
4. **Lower Half 映射**：`drv_lradc.c` 将电压值映射为对应的按键 bitmask
5. **Upper Half 通知**：通过 `g_btnhandler` 通知上层有按键事件
6. **应用读取**：用户程序通过 `read()` 从 `/dev/input/eventX` 读取按键事件
