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
        .v.str = "ili9341",
    },
    {
        .name = "lcd_if",
        .type = PROPERTY_INTGER,
        .v.value = LCD_IF_CPU,  // lcd_if = 0 from config
    },
    {
        .name = "lcd_x", 
        .type = PROPERTY_INTGER,
        .v.value = 240,
    },
    {
        .name = "lcd_y",
        .type = PROPERTY_INTGER,
        .v.value = 320,
    },
    {
        .name = "lcd_width",
        .type = PROPERTY_INTGER,
        .v.value = 240,
    },
    {
        .name = "lcd_height",
        .type = PROPERTY_INTGER,
        .v.value = 320,
    },
    {
        .name = "lcd_dclk_freq",
        .type = PROPERTY_INTGER,
        .v.value = 60,
    },
    {
        .name = "lcd_hbp",
        .type = PROPERTY_INTGER,
        .v.value = 20,
    },
    {
        .name = "lcd_ht",
        .type = PROPERTY_INTGER,
        .v.value = 1000,
    },
    {
        .name = "lcd_hspw",
        .type = PROPERTY_INTGER,
        .v.value = 10,
    },
    {
        .name = "lcd_vbp",
        .type = PROPERTY_INTGER,
        .v.value = 5,
    },
    {
        .name = "lcd_vt",
        .type = PROPERTY_INTGER,
        .v.value = 340,
    },
    {
        .name = "lcd_vspw",
        .type = PROPERTY_INTGER,
        .v.value = 2,
    },
    {
        .name = "lcd_frm",
        .type = PROPERTY_INTGER,
        .v.value = 0,
    },
    {
        .name = "lcd_hv_if",
        .type = PROPERTY_INTGER,
        .v.value = 8,  // lcd_hv_if = 8 from config
    },
    {
        .name = "lcd_hv_clk_phase",
        .type = PROPERTY_INTGER,
        .v.value = 0,
    },
    {
        .name = "lcd_hv_sync_polarity",
        .type = PROPERTY_INTGER,
        .v.value = 0,
    },
    {
        .name = "lcd_hv_srgb_seq",
        .type = PROPERTY_INTGER,
        .v.value = 0,
    },
    {
        .name = "lcd_io_phase",
        .type = PROPERTY_INTGER,
        .v.value = 0x0000,
    },
    {
        .name = "lcd_gamma_en",
        .type = PROPERTY_INTGER,
        .v.value = 0,
    },
    {
        .name = "lcd_bright_curve_en",
        .type = PROPERTY_INTGER,
        .v.value = 1,
    },
    {
        .name = "lcd_cmap_en",
        .type = PROPERTY_INTGER,
        .v.value = 0,
    },
    {
        .name = "lcd_rb_swap",
        .type = PROPERTY_INTGER,
        .v.value = 0,
    },
    {
        .name = "deu_mode",
        .type = PROPERTY_INTGER,
        .v.value = 0,
    },
    {
        .name = "lcdgamma4iep",
        .type = PROPERTY_INTGER,
        .v.value = 22,
    },
    {
        .name = "smart_color",
        .type = PROPERTY_INTGER,
        .v.value = 90,
    },
    {
        .name = "lcd_pwm_used",
        .type = PROPERTY_INTGER,
        .v.value = 1,
    },
    {
        .name = "lcd_pwm_ch",
        .type = PROPERTY_INTGER,
        .v.value = 0,
    },
    {
        .name = "lcd_pwm_freq",
        .type = PROPERTY_INTGER,
        .v.value = 50000,
    },
    {
        .name = "lcd_pwm_pol",
        .type = PROPERTY_INTGER,
        .v.value = 1,
    },
    {
        .name = "lcd_pwm_max_limit",
        .type = PROPERTY_INTGER,
        .v.value = 255,
    },
    {
        .name = "lcd_size",
        .type = PROPERTY_STRING,
        .v.str = "240x320",
    },
    {
        .name = "lcd_model_name",
        .type = PROPERTY_STRING,
        .v.str = "ili9341",
    },
    {
        .name = "lcd_bl_en_power",
        .type = PROPERTY_INTGER,
        .v.value = 1,
    },

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
     /* Power 0: VDD (3.3V) - 逻辑电源，最先上电 */
    {
        .name = "lcd_power",
        .type = PROPERTY_POWER,
        .v.power = {
            .power_name = "dcdc1",                  // VDD 逻辑电源
            .power_type = AXP2101_REGULATOR,        // AXP2101 PMIC类型
            .power_id = AXP2101_ID_DCDC1,           // DCDC1输出 (1.5V~3.4V, 2A)
            .power_vol = 3300000,                   // 3.3V (微伏)
            .always_on = false,                     // 可控电源
        },
    },
    {
        .name = "lcd_gpio_0",  // reset
        .type = PROPERTY_GPIO,
        .v.gpio_list = {
            .gpio_name = "PD19",
            .port = 3,
            .port_num = 19,
            .mul_sel = GPIO_MUXSEL_OUT,
            .pull = GPIO_PULL_DOWN_DISABLED,
            .drv_level = GPIO_DRIVING_LEVEL3,
            .data = GPIO_DATA_HIGH,  // <default><1> means default high
            .gpio = GPIOD(19),
        },
    },
    {
        .name = "lcd_gpio_1",  // cs
        .type = PROPERTY_GPIO,
        .v.gpio_list = {
            .gpio_name = "PD10",
            .port = 3,
            .port_num = 10,
            .mul_sel = GPIO_MUXSEL_OUT,
            .pull = GPIO_PULL_DOWN_DISABLED,
            .drv_level = GPIO_DRIVING_LEVEL3,
            .data = GPIO_DATA_HIGH,  // <default><1> means default high (CS inactive)
            .gpio = GPIOD(10),
        },
    },
    {
        .name = "lcd_gpio_2",  // sdi
        .type = PROPERTY_GPIO,
        .v.gpio_list = {
            .gpio_name = "PD12",
            .port = 3,
            .port_num = 12,
            .mul_sel = GPIO_MUXSEL_OUT,
            .pull = GPIO_PULL_DOWN_DISABLED,
            .drv_level = GPIO_DRIVING_LEVEL3,
            .data = GPIO_DATA_LOW,  // <default><0> means default low
            .gpio = GPIOD(12),
        },
    },
    {
        .name = "lcd_gpio_3",  // sck
        .type = PROPERTY_GPIO,
        .v.gpio_list = {
            .gpio_name = "PD11",
            .port = 3,
            .port_num = 11,
            .mul_sel = GPIO_MUXSEL_OUT,
            .pull = GPIO_PULL_DOWN_DISABLED,
            .drv_level = GPIO_DRIVING_LEVEL3,
            .data = GPIO_DATA_LOW,  // <default><0> means default low
            .gpio = GPIOD(11),
        },
    },
    {
        .name = "lcd_gpio_4",  // DC
        .type = PROPERTY_GPIO,
        .v.gpio_list = {
            .gpio_name = "PD14",
            .port = 3,
            .port_num = 14,
            .mul_sel = GPIO_MUXSEL_OUT,
            .pull = GPIO_PULL_DOWN_DISABLED,
            .drv_level = GPIO_DRIVING_LEVEL3,
            .data = GPIO_DATA_LOW,  // <default><0> means default low
            .gpio = GPIOD(14),
        },
    },
    #if 0
    {
        .name = "lcdd3",
        .type = PROPERTY_PIN,
        .v.gpio_list = {
            .gpio_name = "PD01",
            .port = 3,
            .port_num = 1,
            .mul_sel = 0,  // not used for SPI; keep as GPIO
            .pull = GPIO_PULL_DOWN_DISABLED,
            .drv_level = GPIO_DRIVING_LEVEL3,
            .data = GPIO_DATA_LOW,
            .gpio = GPIOD(1),
        },
    },

    {
        .name = "lcdd4",
        .type = PROPERTY_PIN,
        .v.gpio_list = {
            .gpio_name = "PD02",
            .port = 3,
            .port_num = 2,
            .mul_sel = 0,  // not used for SPI; keep as GPIO
            .pull = GPIO_PULL_DOWN_DISABLED,
            .drv_level = GPIO_DRIVING_LEVEL3,
            .data = GPIO_DATA_LOW,
            .gpio = GPIOD(2),
        },
    },
    {
        .name = "lcdd5",
        .type = PROPERTY_PIN,
        .v.gpio_list = {
            .gpio_name = "PD03",
            .port = 3,
            .port_num = 3,
            .mul_sel = 0,  // not used for SPI; keep as GPIO
            .pull = GPIO_PULL_DOWN_DISABLED,
            .drv_level = GPIO_DRIVING_LEVEL3,
            .data = GPIO_DATA_LOW,
            .gpio = GPIOD(3),
        },
    },
    {
        .name = "lcdd6",
        .type = PROPERTY_PIN,
        .v.gpio_list = {
            .gpio_name = "PD04",
            .port = 3,
            .port_num = 4,
            .mul_sel = 0,  // not used for SPI; keep as GPIO
            .pull = GPIO_PULL_DOWN_DISABLED,
            .drv_level = GPIO_DRIVING_LEVEL3,
            .data = GPIO_DATA_LOW,
            .gpio = GPIOD(4),
        },
    },
    {
        .name = "lcdd7",
        .type = PROPERTY_PIN,
        .v.gpio_list = {
            .gpio_name = "PD05",
            .port = 3,
            .port_num = 5,
            .mul_sel = 0,  // not used for SPI; keep as GPIO
            .pull = GPIO_PULL_DOWN_DISABLED,
            .drv_level = GPIO_DRIVING_LEVEL3,
            .data = GPIO_DATA_LOW,
            .gpio = GPIOD(5),
        },
    },
    {
        .name = "lcdd10",
        .type = PROPERTY_PIN,
        .v.gpio_list = {
            .gpio_name = "PD06",
            .port = 3,
            .port_num = 6,
            .mul_sel = 0,  // not used for SPI; keep as GPIO
            .pull = GPIO_PULL_DOWN_DISABLED,
            .drv_level = GPIO_DRIVING_LEVEL3,
            .data = GPIO_DATA_LOW,
            .gpio = GPIOD(6),
        },
    },
    {
        .name = "lcdd11",
        .type = PROPERTY_PIN,
        .v.gpio_list = {
            .gpio_name = "PD07",
            .port = 3,
            .port_num = 7,
            .mul_sel = 2,  // function 2 for data line
            .pull = GPIO_PULL_DOWN_DISABLED,
            .drv_level = GPIO_DRIVING_LEVEL3,
            .data = GPIO_DATA_LOW,
            .gpio = GPIOD(7),
        },
    },
    {
        .name = "lcdd12",
        .type = PROPERTY_PIN,
        .v.gpio_list = {
            .gpio_name = "PD08",
            .port = 3,
            .port_num = 8,
            .mul_sel = 2,  // function 2 for data line
            .pull = GPIO_PULL_DOWN_DISABLED,
            .drv_level = GPIO_DRIVING_LEVEL3,
            .data = GPIO_DATA_LOW,
            .gpio = GPIOD(8),
        },
    },
    {
        .name = "lcdclk",
        .type = PROPERTY_PIN,
        .v.gpio_list = {
            .gpio_name = "PD18",
            .port = 3,
            .port_num = 18,
            .mul_sel = 3,  // function 3 for clock/sync signals
            .pull = GPIO_PULL_DOWN_DISABLED,
            .drv_level = GPIO_DRIVING_LEVEL3,
            .data = GPIO_DATA_LOW,
            .gpio = GPIOD(18),
        },
    },
    {
        .name = "lcdde",
        .type = PROPERTY_PIN,
        .v.gpio_list = {
            .gpio_name = "PD19",
            .port = 3,
            .port_num = 19,
            .mul_sel = 3,  // function 3 for clock/sync signals
            .pull = GPIO_PULL_DOWN_DISABLED,
            .drv_level = GPIO_DRIVING_LEVEL3,
            .data = GPIO_DATA_LOW,
            .gpio = GPIOD(19),
        },
    },
    {
        .name = "lcdhsync",
        .type = PROPERTY_PIN,
        .v.gpio_list = {
            .gpio_name = "PD20",
            .port = 3,
            .port_num = 20,
            .mul_sel = 3,  // function 3 for clock/sync signals
            .pull = GPIO_PULL_DOWN_DISABLED,
            .drv_level = GPIO_DRIVING_LEVEL3,
            .data = GPIO_DATA_LOW,
            .gpio = GPIOD(20),
        },
    },
    {
        .name = "lcdvsync",
        .type = PROPERTY_PIN,
        .v.gpio_list = {
            .gpio_name = "PD21",
            .port = 3,
            .port_num = 21,
            .mul_sel = 3,  // function 3 for clock/sync signals
            .pull = GPIO_PULL_DOWN_DISABLED,
            .drv_level = GPIO_DRIVING_LEVEL3,
            .data = GPIO_DATA_LOW,
            .gpio = GPIOD(21),
        },
    },
    #endif

};

struct property_t g_disp_config[] = {
    {
        .name = "disp_init_enable",
        .type = PROPERTY_INTGER,
        .v.value = 1,
    },
    {
        .name = "disp_mode",
        .type = PROPERTY_INTGER,
        .v.value = DISP_INIT_MODE_SCREEN0,
    },
    {
        .name = "screen0_output_type",
        .type = PROPERTY_INTGER,
        .v.value = DISP_OUTPUT_TYPE_LCD,
    },
    {
        .name = "screen1_output_mode",
        .type = PROPERTY_INTGER,
        .v.value = DISP_OUTPUT_TYPE_NONE,
    },
    {
        .name = "screen1_output_type",
        .type = PROPERTY_INTGER,
        .v.value = DISP_OUTPUT_TYPE_NONE,
    },
};

/*LCD1配置 (如果需要双屏)*/
struct property_t g_lcd1_config[] = {
    {
        .name = "lcd_used",
        .type = PROPERTY_INTGER,
        .v.value = 0,
    },
};

/*多配置支持 (扩展功能)*/
struct property_t *g_lcd0_config_list[] = {
    g_lcd0_config,
    NULL,
    NULL,
};

u32 g_lcd0_config_len_list[] = {
    sizeof(g_lcd0_config) / sizeof(struct property_t),
    sizeof(g_lcd1_config) / sizeof(struct property_t),
};

u32 g_lcd0_config_list_len = sizeof(g_lcd0_config_len_list) / sizeof(u32);
u32 g_lcd1_config_len = sizeof(g_lcd1_config) / sizeof(struct property_t);
u32 g_disp_config_len = sizeof(g_disp_config) / sizeof(struct property_t);
u32 g_lcd0_config_len = sizeof(g_lcd0_config) / sizeof(struct property_t);
