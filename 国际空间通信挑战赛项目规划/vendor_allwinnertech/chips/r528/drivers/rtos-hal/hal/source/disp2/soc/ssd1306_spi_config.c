#include <stdint.h>
#include <hal_clk.h>
#include <hal_gpio.h>
#include "../disp/disp_sys_intf.h"

typedef uint32_t u32;
typedef int32_t  s32;

#include "disp_board_config.h"

struct property_t g_lcd0_config[] = {

    {
        .name = "lcd_used",
        .type = PROPERTY_INTGER,
        .v.value = 1,
    },
    {
        .name = "lcd_driver_name",
        .type = PROPERTY_STRING,
        .v.str = "ssd1306",
    },
    {
        .name = "lcd_if",
        .type = PROPERTY_INTGER,
        .v.value = LCD_IF_CPU,
    },
    {
        .name = "lcd_x",
        .type = PROPERTY_INTGER,
        .v.value = 128,
    },
    {
        .name = "lcd_y",
        .type = PROPERTY_INTGER,
        .v.value = 64,
    },
    {
        .name = "lcd_width",
        .type = PROPERTY_INTGER,
        .v.value = 22,
    },
    {
        .name = "lcd_height",
        .type = PROPERTY_INTGER,
        .v.value = 11,
    },
    {
        .name = "lcd_dclk_freq",
        .type = PROPERTY_INTGER,
        .v.value = 1000000,
    },
    {
        .name = "lcd_frm",
        .type = PROPERTY_INTGER,
        .v.value = 0,
    },
    {
        .name = "lcd_io_phase",
        .type = PROPERTY_INTGER,
        .v.value = 0x0000,
    },
    {
        .name = "lcd_cpu_mode",
        .type = PROPERTY_INTGER,
        .v.value = LCD_CPU_AUTO_MODE,
    },
    {
        .name = "lcd_cpu_if",
        .type = PROPERTY_INTGER,
        .v.value = 12,    /* LCD_CPU_IF_RGB666_6PIN - SPI interface for SSD1306 */
    },
    {
        .name = "lcd_pwm_used",
        .type = PROPERTY_INTGER,
        .v.value = 0,
    },

    /* SSD1306 Power Configuration - 按照上电时序要求配置 */
    /* Power 0: VDD (3.3V) - 逻辑电源，最先上电 */
    {
        .name = "lcd_power0",
        .type = PROPERTY_POWER,
        .v.power = {
            .power_name = "dcdc1",                  // VDD 逻辑电源
            .power_type = AXP2101_REGULATOR,        // AXP2101 PMIC类型
            .power_id = AXP2101_ID_DCDC1,           // DCDC1输出 (1.5V~3.4V, 2A)
            .power_vol = 3300000,                   // 3.3V (微伏)
            .always_on = false,                     // 可控电源
        },
    },
    /* Power 1: VCC (3.3V) - 显示电源，在复位后上电 */
    {
        .name = "lcd_power1",
        .type = PROPERTY_POWER,
        .v.power = {
            .power_name = "dcdc2",                  // VCC 显示电源
            .power_type = AXP2101_REGULATOR,        // AXP2101 PMIC类型
            .power_id = AXP2101_ID_DCDC2,           // DCDC2输出 (0.5V~3.4V, 2A)
            .power_vol = 3300000,                   // 3.3V (微伏)
            .always_on = false,                     // 可控电源
        },
    },
{
        .name = "lcd_gpio_0",       //DC
        .type = PROPERTY_GPIO,
        .v.gpio_list = {
            .gpio_name = "PD14",
            .port = 3,
            .port_num = 14,
            .mul_sel = GPIO_MUXSEL_OUT,
            .pull = GPIO_PULL_DOWN_DISABLED,
            .drv_level = GPIO_DRIVING_LEVEL3,
            .data = GPIO_DATA_LOW,
            .gpio = GPIOD(14),
        },
    },
{
        .name = "lcd_gpio_1",       //RES引脚
        .type = PROPERTY_GPIO,
        .v.gpio_list = {
            .gpio_name = "PD13",
            .port = 3,
            .port_num = 13,
            .mul_sel = GPIO_MUXSEL_OUT,
            .pull = GPIO_PULL_UP,
            .drv_level = GPIO_DRIVING_LEVEL3,
            .data = GPIO_DATA_HIGH,
            .gpio = GPIOD(13),
        },
    },
#ifdef	CONFIG_LCD_SSD1306_SPI_SW

{
        .name = "lcd_gpio_2",       //D1 (MOSI) - SPI数据线
        .type = PROPERTY_GPIO,
        .v.gpio_list = {
            .gpio_name = "PD12",
            .port = 3,
            .port_num = 12,
            .mul_sel = GPIO_MUXSEL_OUT,
            .pull = GPIO_PULL_DOWN_DISABLED,
            .drv_level = GPIO_DRIVING_LEVEL3,
            .data = GPIO_DATA_LOW,
            .gpio = GPIOD(12),
        },
    },
{
        .name = "lcd_gpio_3",       //D0 SCLK - SPI时钟线
        .type = PROPERTY_GPIO,
        .v.gpio_list = {
            .gpio_name = "PD11",
            .port = 3,
            .port_num = 11,
            .mul_sel = GPIO_MUXSEL_OUT,
            .pull = GPIO_PULL_DOWN_DISABLED,
            .drv_level = GPIO_DRIVING_LEVEL3,
            .data = GPIO_DATA_LOW,
            .gpio = GPIOD(11),
        },
    },
{
        .name = "lcd_gpio_4",       //CS - SPI片选
        .type = PROPERTY_GPIO,
        .v.gpio_list = {
            .gpio_name = "PD10",
            .port = 3,
            .port_num = 10,
            .mul_sel = GPIO_MUXSEL_OUT,
            .pull = GPIO_PULL_DOWN_DISABLED,
            .drv_level = GPIO_DRIVING_LEVEL3,
            .data = GPIO_DATA_HIGH,
            .gpio = GPIOD(10),
        },
    },
#elif defined (CONFIG_LCD_SSD1306_SPI_HW)

{
        .name = "lcd_gpio_2",       //D1  (MOSI) - SPI数据线
        .type = PROPERTY_PIN,
        .v.gpio_list = {
            .gpio_name = "PD12",
            .port = 3,
            .port_num = 12,
            .mul_sel = GPIO_MUXSEL_FUNCTION4,
            .pull = GPIO_PULL_DOWN_DISABLED,
            .drv_level = GPIO_DRIVING_LEVEL3,
            .data = GPIO_DATA_LOW,
            .gpio = GPIOD(12),
        },
    },
{
        .name = "lcd_gpio_3",       //D0 SCLK - SPI时钟线
        .type = PROPERTY_PIN,
        .v.gpio_list = {
            .gpio_name = "PD11",
            .port = 3,
            .port_num = 11,
            .mul_sel = GPIO_MUXSEL_FUNCTION4,
            .pull = GPIO_PULL_DOWN_DISABLED,
            .drv_level = GPIO_DRIVING_LEVEL3,
            .data = GPIO_DATA_LOW,
            .gpio = GPIOD(11),
        },
    },
{
        .name = "lcd_gpio_4",       //CS - SPI片选
        .type = PROPERTY_PIN,
        .v.gpio_list = {
            .gpio_name = "PD10",
            .port = 3,
            .port_num = 10,
            .mul_sel = GPIO_MUXSEL_FUNCTION4,
            .pull = GPIO_PULL_DOWN_DISABLED,
            .drv_level = GPIO_DRIVING_LEVEL3,
            .data = GPIO_DATA_HIGH,
            .gpio = GPIOD(10),
        },
    },
#endif
{
        .name = "lcd_size",
        .type = PROPERTY_STRING,
        .v.str = "128x64",
    },
    {
        .name = "lcd_model_name",
        .type = PROPERTY_STRING,
        .v.str = "ssd1306",
    },
};

struct property_t g_disp_config[] = {
    {
        .name = "disp_init_enable",
        .type = PROPERTY_INTGER,
        .v.value = 1,     // 使能显示初始化
    },
    {
        .name = "disp_mode",
        .type = PROPERTY_INTGER,
        .v.value = DISP_INIT_MODE_SCREEN0,     // 0=屏幕模式
    },
    {
        .name = "screen0_output_type",
        .type = PROPERTY_INTGER,
        .v.value = DISP_OUTPUT_TYPE_LCD,     // 1=LCD输出
    },
    {
        .name = "screen1_output_mode",
        .type = PROPERTY_INTGER,
        .v.value = DISP_OUTPUT_TYPE_NONE,     // 4=DSI输出
    },
    {
        .name = "screen1_output_type",
        .type = PROPERTY_INTGER,
        .v.value = DISP_OUTPUT_TYPE_NONE,     // 不使用screen1
    },
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
    g_lcd0_config,     // 配置0:
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