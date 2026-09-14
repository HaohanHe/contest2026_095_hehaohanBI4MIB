#include <stdint.h>
#include <hal_clk.h>
#include <hal_gpio.h>
#include "../disp/disp_sys_intf.h"

typedef uint32_t u32;
typedef int32_t  s32;

#include "disp_board_config.h"
/*
#ifndef GPIOB
#define GPIOB(n) ((1 * 32) + (n))
#endif
#ifndef GPIOD
#define GPIOD(n) ((3 * 32) + (n))
#endif
*/
/* T070S140B LCD0 配置参数
 * 严格按照厂家提供的规格配置:
 * VDD= 1.8~2.0V, RESET=VDD, STBYB=VDD, AVDD= 9.6V
 * VCOM= 3.2V, VGH= 18V, VGL= -7V
 * MIPI CLK Speed: 312Mbps (调整以满足R528 DSI DPHY VCO≥1300MHz要求)
 * 像素时钟: 52MHz
 * 时序: H_BP=160, H_FP=160, H_PW=10, V_BP=23, V_FP=12, V_PW=1
 * MIPI命令: 0xB2=0x20, 0x80=0xAC, 0x81=0xB8, 0x82=0x09, 0x83=0x78, 0x84=0x7F, 0x85=0xBB, 0x86=0x70
 */
struct property_t g_lcd0_config[] = {
    // 基本LCD参数配置
    {
        .name = "lcd_used",
        .type = PROPERTY_INTGER,
        .v.value = 1,  // 使能LCD0
    },
    {
        .name = "lcd_driver_name",
        .type = PROPERTY_STRING,
        .v.str = "t070s140b",
    },
    {
        .name = "lcd_if",
        .type = PROPERTY_INTGER,
        .v.value = 4,  // 4=DSI接口
    },
    // 分辨率配置 (基于T070S140B规格)
    {
        .name = "lcd_x",
        .type = PROPERTY_INTGER,
        .v.value = 1024,
    },
    {
        .name = "lcd_y",
        .type = PROPERTY_INTGER,
        .v.value = 600,
    },
    {
        .name = "lcd_width",
        .type = PROPERTY_INTGER,
        .v.value = 154,   // 物理宽度154.21mm
    },
    {
        .name = "lcd_height",
        .type = PROPERTY_INTGER,
        .v.value = 86,    // 物理高度85.92mm
    },
    {
        .name = "lcd_dclk_freq",
        .type = PROPERTY_INTGER,
        .v.value = 52,    // 设置为52MHz，目标FPS=60
    },
    // MIPI DSI配置 (基于EK79007AD2规格)
    {
        .name = "lcd_dsi_if",
        .type = PROPERTY_INTGER,
        .v.value = LCD_DSI_IF_VIDEO_MODE,     // 0=视频模式
    },
    {
        .name = "lcd_dsi_lane",
        .type = PROPERTY_INTGER,
        .v.value = 4,
    },
    {
        .name = "lcd_dsi_format",
        .type = PROPERTY_INTGER,
        .v.value = 0,     // 0=RGB888格式
    },
    {
        .name = "lcd_dsi_te",
        .type = PROPERTY_INTGER,
        .v.value = 0,     // 不使用TE信号
    },

    {
        .name = "lcd_dsi_port_num",
        .type = PROPERTY_INTGER,
        .v.value = 3,     // DSI端口0
    },
    {
        .name = "lcd_dsi_clk_rate",
        .type = PROPERTY_INTGER,
        .v.value = 312,
    },
{
        .name = "lcd_ht",
        .type = PROPERTY_INTGER,
        .v.value = 1354,  // 水平总计 (1024+160+160+10=1354)
    },
    {
        .name = "lcd_hbp",
        .type = PROPERTY_INTGER,
        .v.value = 160,   // 水平后肩(hbp:160 hspw:10)
    },

    {
        .name = "lcd_hfp",
        .type = PROPERTY_INTGER,
        .v.value = 160,   // 水平前肩
    },
    {
        .name = "lcd_hspw",
        .type = PROPERTY_INTGER,
        .v.value = 10,    // 水平同步脉宽
    },
    {
        .name = "lcd_vt",
        .type = PROPERTY_INTGER,
        .v.value = 636,   // 垂直总计 (600+23+12+1=636)
    },
    {
        .name = "lcd_vbp",
        .type = PROPERTY_INTGER,
        .v.value = 23,    // 垂直后肩(vbp:23 vspw:1)
    },

    {
        .name = "lcd_vfp",
        .type = PROPERTY_INTGER,
        .v.value = 12,    // 垂直前肩
    },

    {
        .name = "lcd_vspw",
        .type = PROPERTY_INTGER,
        .v.value = 1,    // 垂直同步脉宽
    },
{
        .name = "lcd_frm",
        .type = PROPERTY_INTGER,
        .v.value = 0,     // 0=RGB, 1=RBG
    },
    // 背光PWM配置
{
        .name = "lcd_pwm_used",
        .type = PROPERTY_INTGER,
        .v.value = 1,     // 使用PWM背光控制
    },
    {
        .name = "lcd_pwm_ch",
        .type = PROPERTY_INTGER,
        .v.value = 4,     // PWM通道4 (PD20对应PWM4)
    },
    {
        .name = "lcd_pwm_freq",
        .type = PROPERTY_INTGER,
        .v.value = 1000,  // PWM频率1KHz (根据厂家设备树: pwm-period=1000000ns = 1KHz)
    },
    {
        .name = "lcd_pwm_pol",
        .type = PROPERTY_INTGER,
        .v.value = 0,     // PWM极性正常
    },
    {
        .name = "lcd_pwm_max_limit",
        .type = PROPERTY_INTGER,
        .v.value = 255,   // 最大亮度值
    },
    // 背光控制高级参数
    {
        .name = "lcd_backlight",
        .type = PROPERTY_INTGER,
        .v.value = 100,   // 默认亮度百分比
    },
    {
        .name = "lcd_backlight_curve",
        .type = PROPERTY_INTGER,
        .v.value = 0,     // 线性亮度曲线
    },
    {
        .name = "lcd_bl_en_power",
        .type = PROPERTY_INTGER,
        .v.value = 1,
    },
    {
        .name = "lcd_gpio_0", // DSI复位信号 (DSI_RESET)
        .type = PROPERTY_GPIO,
        .v.gpio_list = {
            .gpio_name = "PD19",
            .port = 3,            // GPIOD是端口3
            .port_num = 19,       // PD19
            .mul_sel = GPIO_MUXSEL_OUT,
            .pull = GPIO_PULL_DOWN_DISABLED,
            .drv_level = GPIO_DRIVING_LEVEL3,
            .data = GPIO_DATA_LOW,
            .gpio = GPIOD(19),
        },
    },
    #if 0 // 如果使用GT911触摸屏，则启用以下GPIO配置(在触摸屏驱动中已重复定义，所以此处注释掉)
    {
        .name = "lcd_gpio_1", // 触摸屏复位信号 (CTP_RST)
        .type = PROPERTY_GPIO,
        .v.gpio_list = {
            .gpio_name = "PB4",
            .port = 1,            // GPIOB是端口1
            .port_num = 4,        // PB4
            .mul_sel = GPIO_MUXSEL_OUT,
            .pull = GPIO_PULL_DOWN_DISABLED,
            .drv_level = GPIO_DRIVING_LEVEL3,
            .data = GPIO_DATA_LOW,
            .gpio = GPIOB(4),
        },
    },
    {
        .name = "lcd_gpio_2", // 触摸屏中断信号 (CTP_INT)
        .type = PROPERTY_PIN,
        .v.gpio_list = {
            .gpio_name = "PB5",
            .port = 1,            // GPIOB是端口1
            .port_num = 5,        // PB5
            .mul_sel = GPIO_MUXSEL_EINT,
            .pull = GPIO_PULL_DOWN_DISABLED,
            .drv_level = GPIO_DRIVING_LEVEL3,
            .data = GPIO_DATA_LOW,
            .gpio = GPIOB(5),
        },
    },
    {
        .name = "lcd_gpio_sda", // 触摸屏I2C数据线 (CTP_SDA)
        .type = PROPERTY_PIN,
        .v.gpio_list = {
            .gpio_name = "PB2",
            .port = 1,            // GPIOB是端口1
            .port_num = 2,        // PB2
            .mul_sel = GPIO_MUXSEL_FUNCTION4,         // I2C功能
            .pull = GPIO_PULL_UP,
            .drv_level = GPIO_DRIVING_LEVEL3,
            .data = GPIO_DATA_LOW,
            .gpio = GPIOB(2),
        },
    },
    {
        .name = "lcd_gpio_scl", // 触摸屏I2C时钟线 (CTP_SCK)
        .type = PROPERTY_PIN,
        .v.gpio_list = {
            .gpio_name = "PB3",
            .port = 1,
            .port_num = 3,        // PB3
            .mul_sel = GPIO_MUXSEL_FUNCTION4,         // I2C功能
            .pull = GPIO_PULL_UP,
            .drv_level = GPIO_DRIVING_LEVEL3,
            .data = GPIO_DATA_LOW,
            .gpio = GPIOB(3),
        },
    },
    #endif
    // PWM背光控制GPIO配置
    {
        .name = "lcd_bl_en", // PWM背光控制信号 (BL_PWM)
        .type = PROPERTY_PIN,
        .v.gpio_list = {
            .gpio_name = "PD20",
            .port = 3,
            .port_num = 20,       // PD20
            .mul_sel = GPIO_MUXSEL_FUNCTION5,
            .pull = GPIO_PULL_DOWN_DISABLED,
            .drv_level = GPIO_DRIVING_LEVEL3,
            .data = GPIO_DATA_HIGH,
            .gpio = GPIOD(20),
        },
    },
    // DSI数据线配置 (MIPI DSI接口引脚)
    {
        .name = "dsi_dp0",
        .type = PROPERTY_PIN,
        .v.gpio_list = {
            .gpio_name = "PD0",
            .port = 3,            // GPIOD是端口3
            .port_num = 0,        // PD0
            .mul_sel = GPIO_MUXSEL_FUNCTION4,
            .pull = GPIO_PULL_DOWN_DISABLED,
            .drv_level = GPIO_DRIVING_LEVEL3,
            .data = GPIO_DATA_LOW,
            .gpio = GPIOD(0),
        },
    },
    {
        .name = "dsi_dn0",
        .type = PROPERTY_PIN,
        .v.gpio_list = {
            .gpio_name = "PD1",
            .port = 3,            // GPIOD是端口3
            .port_num = 1,        // PD1
            .mul_sel = GPIO_MUXSEL_FUNCTION4,
            .pull = GPIO_PULL_DOWN_DISABLED,
            .drv_level = GPIO_DRIVING_LEVEL3,
            .data = GPIO_DATA_LOW,
            .gpio = GPIOD(1),
        },
    },
    {
        .name = "dsi_dp1",
        .type = PROPERTY_PIN,
        .v.gpio_list = {
            .gpio_name = "PD2",
            .port = 3,            // GPIOD是端口3
            .port_num = 2,        // PD2
            .mul_sel = GPIO_MUXSEL_FUNCTION4,
            .pull = GPIO_PULL_DOWN_DISABLED,
            .drv_level = GPIO_DRIVING_LEVEL3,
            .data = GPIO_DATA_LOW,
            .gpio = GPIOD(2),
        },
    },
    {
        .name = "dsi_dn1",
        .type = PROPERTY_PIN,
        .v.gpio_list = {
            .gpio_name = "PD3",
            .port = 3,            // GPIOD是端口3
            .port_num = 3,        // PD3
            .mul_sel = GPIO_MUXSEL_FUNCTION4,
            .pull = GPIO_PULL_DOWN_DISABLED,
            .drv_level = GPIO_DRIVING_LEVEL3,
            .data = GPIO_DATA_LOW,
            .gpio = GPIOD(3),
        },
    },
    {
        .name = "dsi_ckp",
        .type = PROPERTY_PIN,
        .v.gpio_list = {
            .gpio_name = "PD4",
            .port = 3,            // GPIOD是端口3
            .port_num = 4,        // PD4
            .mul_sel = GPIO_MUXSEL_FUNCTION4,         // DSI时钟功能
            .pull = GPIO_PULL_DOWN_DISABLED,
            .drv_level = GPIO_DRIVING_LEVEL3,
            .data = GPIO_DATA_LOW,
            .gpio = GPIOD(4),
        },
    },
    {
        .name = "dsi_ckn",
        .type = PROPERTY_PIN,
        .v.gpio_list = {
            .gpio_name = "PD5",
            .port = 3,            // GPIOD是端口3
            .port_num = 5,        // PD5
            .mul_sel = GPIO_MUXSEL_FUNCTION4,         // DSI时钟功能
            .pull = GPIO_PULL_DOWN_DISABLED,
            .drv_level = GPIO_DRIVING_LEVEL3,
            .data = GPIO_DATA_LOW,
            .gpio = GPIOD(5),
        },
    },
    {
        .name = "dsi_dp2",
        .type = PROPERTY_PIN,
        .v.gpio_list = {
            .gpio_name = "PD6",
            .port = 3,            // GPIOD是端口3
            .port_num = 6,        // PD6
            .mul_sel = GPIO_MUXSEL_FUNCTION4,         // DSI功能
            .pull = GPIO_PULL_DOWN_DISABLED,
            .drv_level = GPIO_DRIVING_LEVEL3,
            .data = GPIO_DATA_LOW,
            .gpio = GPIOD(6),
        },
    },
    {
        .name = "dsi_dn2",
        .type = PROPERTY_PIN,
        .v.gpio_list = {
            .gpio_name = "PD7",
            .port = 3,            // GPIOD是端口3
            .port_num = 7,        // PD7
            .mul_sel = GPIO_MUXSEL_FUNCTION4,         // DSI功能
            .pull = GPIO_PULL_DOWN_DISABLED,
            .drv_level = GPIO_DRIVING_LEVEL3,
            .data = GPIO_DATA_LOW,
            .gpio = GPIOD(7),
        },
    },
    {
        .name = "dsi_dp3",
        .type = PROPERTY_PIN,
        .v.gpio_list = {
            .gpio_name = "PD8",
            .port = 3,            // GPIOD是端口3
            .port_num = 8,        // PD8
            .mul_sel = GPIO_MUXSEL_FUNCTION4,         // DSI功能
            .pull = GPIO_PULL_DOWN_DISABLED,
            .drv_level = GPIO_DRIVING_LEVEL3,
            .data = GPIO_DATA_LOW,
            .gpio = GPIOD(8),
        },
    },
    {
        .name = "dsi_dn3",
        .type = PROPERTY_PIN,
        .v.gpio_list = {
            .gpio_name = "PD9",
            .port = 3,            // GPIOD是端口3
            .port_num = 9,        // PD9
            .mul_sel = GPIO_MUXSEL_FUNCTION4,         // DSI功能
            .pull = GPIO_PULL_DOWN_DISABLED,
            .drv_level = GPIO_DRIVING_LEVEL3,
            .data = GPIO_DATA_LOW,
            .gpio = GPIOD(9),
        },
    },
    // 电源配置 (T070S140B三路电源 - 正确映射到AXP2101)
    {
        .name = "lcd_power0",  // VDD 1.8V 数字电源
        .type = PROPERTY_POWER,
        .v.power = {
            .power_name = "dcdc1",                  // 描述性名称
            .power_type = AXP2101_REGULATOR,        // AXP2101 PMIC类型
            .power_id = AXP2101_ID_DCDC1,           // DCDC1输出 (1.5V~3.4V, 2A)
            .power_vol = 1800000,                   // 1.8V (微伏)
            .always_on = true,                     // 非常开电源
        },
    },
    {
        .name = "lcd_power1",  // VDD_IF 1.8V 接口电源
        .type = PROPERTY_POWER,
        .v.power = {
            .power_name = "dcdc3",                  // 描述性名称
            .power_type = AXP2101_REGULATOR,        // AXP2101 PMIC类型
            .power_id = AXP2101_ID_DCDC3,           // DCDC3输出 (0.5V~1.84V, 2A)
            .power_vol = 1800000,                   // 1.8V (微伏)
            .always_on = true,                     // 非常开电源
        },
    },
    {
        .name = "lcd_power2",  // AVDD 模拟电源 - 提供3.3V给屏幕内部升压电路
        .type = PROPERTY_POWER,
        .v.power = {
            .power_name = "dcdc2",                  // DCDC2输出 (0.5V~3.4V, 2A)
            .power_type = AXP2101_REGULATOR,        // AXP2101 PMIC类型
            .power_id = AXP2101_ID_DCDC2,           // DCDC2输出
            .power_vol = 9600000,                   // 3.3V (微伏) - 给屏幕内部升压电路
            .always_on = true,                     // 常开电源
        },
    },
{
        .name = "lcd_bright_curve_en",
        .type = PROPERTY_INTGER,
        .v.value = 0,     // 使能亮度曲线
    },
{
        .name = "lcd_size",
        .type = PROPERTY_STRING,
        .v.str = "1024x600",  // 屏幕尺寸字符串
    },
    {
        .name = "lcd_model_name",
        .type = PROPERTY_STRING,
        .v.str = "T070S140B",  // 屏幕型号名称
    },
};

/*显示器通用配置*/
struct property_t g_disp_config[] = {
    {
        .name = "disp_init_enable",
        .type = PROPERTY_INTGER,
        .v.value = 1,     // 使能显示初始化
    },
    {
        .name = "disp_mode",
        .type = PROPERTY_INTGER,
        .v.value = 0,     // 0=屏幕模式
    },
    {
        .name = "screen0_output_type",
        .type = PROPERTY_INTGER,
        .v.value = 1,     // 1=LCD输出
    },
    {
        .name = "screen0_output_mode",
        .type = PROPERTY_INTGER,
        .v.value = 4,     // 4=DSI输出
    },
    {
        .name = "screen1_output_type",
        .type = PROPERTY_INTGER,
        .v.value = 4,     // 不使用screen1
    },
    {
        .name = "fb0_buffer_num",
        .type = PROPERTY_INTGER,
        .v.value = 2,     //
    },
    {
        .name = "fb0_width",
        .type = PROPERTY_INTGER,
        .v.value = 1024,   // 旋转后宽度
    },
    {
        .name = "fb0_height",
        .type = PROPERTY_INTGER,
        .v.value = 600,  // 旋转后高度
    },
#ifdef CONFIG_SUNXI_DISP2_FB_HW_ROTATION_SUPPORT
    {
        .name = "disp_rotation_used",
        .type = PROPERTY_INTGER,
        .v.value = 1,
    },
    {
        .name = "degree0",
        .type = PROPERTY_INTGER,
        .v.value = 2,
    },
#endif
};

/*LCD1配置 (如果需要双屏)*/
struct property_t g_lcd1_config[] = {
    {
        .name = "lcd_used",
        .type = PROPERTY_INTGER,
        .v.value = 0,     // 不使用LCD1
    },
};

/*多配置支持 (扩展功能)*/

struct property_t *g_lcd0_config_list[] = {
    g_lcd0_config,     // 配置0: T070S140B标准配置
    NULL,              // 配置1: 预留
    NULL,              // 配置2: 预留
};

u32 g_lcd0_config_len_list[] = {
    sizeof(g_lcd0_config) / sizeof(struct property_t),
    sizeof(g_lcd1_config) / sizeof(struct property_t),
};

u32 g_lcd0_config_list_len = sizeof(g_lcd0_config_len_list) / sizeof(u32);
u32 g_lcd1_config_len = sizeof(g_lcd1_config) / sizeof(struct property_t);
u32 g_disp_config_len = sizeof(g_disp_config) / sizeof(struct property_t);
u32 g_lcd0_config_len = sizeof(g_lcd0_config) / sizeof(struct property_t);