/* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.

 * Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
 * the the People's Republic of China and other countries.
 * All Allwinner Technology Co.,Ltd. trademarks are used with permission.

 * DISCLAIMER
 * THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
 * IF YOU NEED TO INTEGRATE THIRD PARTY’S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
 * IN ALLWINNERS’SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
 * ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
 * ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
 * COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
 * YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY’S TECHNOLOGY.


 * THIS SOFTWARE IS PROVIDED BY ALLWINNER"AS IS" AND TO THE MAXIMUM EXTENT
 * PERMITTED BY LAW, ALLWINNER EXPRESSLY DISCLAIMS ALL WARRANTIES OF ANY KIND,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING WITHOUT LIMITATION REGARDING
 * THE TITLE, NON-INFRINGEMENT, ACCURACY, CONDITION, COMPLETENESS, PERFORMANCE
 * OR MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 * IN NO EVENT SHALL ALLWINNER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS, OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <hal_clk.h>
#include <hal_gpio.h>
#include "../disp/disp_sys_intf.h"
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
		.v.str = "default_lcd",
	},
	{
		.name = "lcd_backlight",
		.type = PROPERTY_INTGER,
		.v.value = 150,
	},
	{
		.name = "lcd_if",
		.type = PROPERTY_INTGER,
		.v.value = 3,
	},
	{
		.name = "lcd_x",
		.type = PROPERTY_INTGER,
		.v.value = 1280,
	},
	{
		.name = "lcd_y",
		.type = PROPERTY_INTGER,
		.v.value = 800,
	},
	{
		.name = "lcd_width",
		.type = PROPERTY_INTGER,
		.v.value = 150,
	},
	{
		.name = "lcd_height",
		.type = PROPERTY_INTGER,
		.v.value = 94,
	},
	{
		.name = "lcd_dclk_freq",
		.type = PROPERTY_INTGER,
		.v.value = 71,
	},
	{
		.name = "lcd_rb_swap",
		.type = PROPERTY_INTGER,
		.v.value = 0,
	},	
	{
		.name = "lcd_pwm_used",
		.type = PROPERTY_INTGER,
		.v.value = 1,
	},
	{
		.name = "lcd_pwm_ch",
		.type = PROPERTY_INTGER,
		.v.value = 7,
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
		.name = "lcd_hbp",
		.type = PROPERTY_INTGER,
		.v.value = 20,
	},
	{
		.name = "lcd_ht",
		.type = PROPERTY_INTGER,
		.v.value = 1418,
	},
	{
		.name = "lcd_hspw",
		.type = PROPERTY_INTGER,
		.v.value = 10,
	},
	{
		.name = "lcd_vbp",
		.type = PROPERTY_INTGER,
		.v.value = 10,
	},
	{
		.name = "lcd_vt",
		.type = PROPERTY_INTGER,
		.v.value = 814,
	},
	{
		.name = "lcd_vspw",
		.type = PROPERTY_INTGER,
		.v.value = 5,
	},
	{
		.name = "lcd_lvds_if",
		.type = PROPERTY_INTGER,
		.v.value = 0,
	},
	{
		.name = "lcd_lvds_colordepth",
		.type = PROPERTY_INTGER,
		.v.value = 1,
	},
	{
		.name = "lcd_lvds_mode",
		.type = PROPERTY_INTGER,
		.v.value = 0,
	},
	{
		.name = "lcd_frm",
		.type = PROPERTY_INTGER,
		.v.value = 1,
	},
	{
		.name = "lcd_io_phase",
		.type = PROPERTY_INTGER,
		.v.value = 0,
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
		.name = "lcd_gamma_en",
		.type = PROPERTY_INTGER,
		.v.value = 0,
	},
	{
		.name = "lcd_bright_curve_en",
		.type = PROPERTY_INTGER,
		.v.value = 0,
	},
	{
		.name = "lcd_cmap_en",
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


//gpio
	{
		.name = "LCD0_D0",
		.type = PROPERTY_PIN,
		.v.gpio_list = {
			.gpio = GPIOD(0),
			.mul_sel = 3,
			.pull = 0,
			.drv_level = 3,
		},
	},
	{
		.name = "LCD0_D1",
		.type = PROPERTY_PIN,
		.v.gpio_list = {
			.gpio = GPIOD(1),
			.mul_sel = 3,
			.pull = 0,
			.drv_level = 3,
		},
	},
	{
		.name = "LCD0_D2",
		.type = PROPERTY_PIN,
		.v.gpio_list = {
			.gpio = GPIOD(2),
			.mul_sel = 3,
			.pull = 0,
			.drv_level = 3,
		},
	},
	{
		.name = "LCD0_D3",
		.type = PROPERTY_PIN,
		.v.gpio_list = {
			.gpio = GPIOD(3),
			.mul_sel = 3,
			.pull = 0,
			.drv_level = 3,
		},
	},
	{
		.name = "LCD0_D4",
		.type = PROPERTY_PIN,
		.v.gpio_list = {
			.gpio = GPIOD(4),
			.mul_sel = 3,
			.pull = 0,
			.drv_level = 3,
		},
	},
	{
		.name = "LCD0_D5",
		.type = PROPERTY_PIN,
		.v.gpio_list = {
			.gpio = GPIOD(5),
			.mul_sel = 3,
			.pull = 0,
			.drv_level = 3,
		},
	},
	{
		.name = "LCD0_D6",
		.type = PROPERTY_PIN,
		.v.gpio_list = {
			.gpio = GPIOD(6),
			.mul_sel = 3,
			.pull = 0,
			.drv_level = 3,
		},
	},
	{
		.name = "LCD0_D7",
		.type = PROPERTY_PIN,
		.v.gpio_list = {
			.gpio = GPIOD(7),
			.mul_sel = 3,
			.pull = 0,
			.drv_level = 3,
		},
	},
	{
		.name = "LCD0_D8",
		.type = PROPERTY_PIN,
		.v.gpio_list = {
			.gpio = GPIOD(8),
			.mul_sel = 3,
			.pull = 0,
			.drv_level = 3,
		},
	},
	{
		.name = "LCD0_D9",
		.type = PROPERTY_PIN,
		.v.gpio_list = {
			.gpio = GPIOD(9),
			.mul_sel = 3,
			.pull = 0,
			.drv_level = 3,
		},
	},
#if 0
	{
		.name = "LCD0_D14",
		.type = PROPERTY_PIN,
		.v.gpio_list = {
			.gpio = GPIOD(10),
			.mul_sel = 2,
			.pull = 0,
			.drv_level = 3,
		},
	},
	{
		.name = "LCD0_D15",
		.type = PROPERTY_PIN,
		.v.gpio_list = {
			.gpio = GPIOD(11),
			.mul_sel = 2,
			.pull = 0,
			.drv_level = 3,
		},
	},
	{
		.name = "LCD0_D18",
		.type = PROPERTY_PIN,
		.v.gpio_list = {
			.gpio = GPIOD(12),
			.mul_sel = 2,
			.pull = 0,
			.drv_level = 3,
		},
	},
	{
		.name = "LCD0_D19",
		.type = PROPERTY_PIN,
		.v.gpio_list = {
			.gpio = GPIOD(13),
			.mul_sel = 2,
			.pull = 0,
			.drv_level = 3,
		},
	},
	{
		.name = "LCD0_D20",
		.type = PROPERTY_PIN,
		.v.gpio_list = {
			.gpio = GPIOD(14),
			.mul_sel = 2,
			.pull = 0,
			.drv_level = 3,
		},
	},
	{
		.name = "LCD0_D21",
		.type = PROPERTY_PIN,
		.v.gpio_list = {
			.gpio = GPIOD(15),
			.mul_sel = 2,
			.pull = 0,
			.drv_level = 3,
		},
	},
	{
		.name = "LCD0_D22",
		.type = PROPERTY_PIN,
		.v.gpio_list = {
			.gpio = GPIOD(16),
			.mul_sel = 2,
			.pull = 0,
			.drv_level = 3,
		},
	},
	{
		.name = "LCD0_D23",
		.type = PROPERTY_PIN,
		.v.gpio_list = {
			.gpio = GPIOD(17),
			.mul_sel = 2,
			.pull = 0,
			.drv_level = 3,
		},
	},
	{
		.name = "LCD0_CLK",
		.type = PROPERTY_PIN,
		.v.gpio_list = {
			.gpio = GPIOD(18),
			.mul_sel = 2,
			.pull = 0,
			.drv_level = 3,
		},
	},
	{
		.name = "LCD0_DE",
		.type = PROPERTY_PIN,
		.v.gpio_list = {
			.gpio = GPIOD(19),
			.mul_sel = 2,
			.pull = 0,
			.drv_level = 3,
		},
	},
	{
		.name = "LCD0_HSYNC",
		.type = PROPERTY_PIN,
		.v.gpio_list = {
			.gpio = GPIOD(20),
			.mul_sel = 2,
			.pull = 0,
			.drv_level = 3,
		},
	},
	{
		.name = "LCD0_VSYNC",
		.type = PROPERTY_PIN,
		.v.gpio_list = {
			.gpio = GPIOD(21),
			.mul_sel = 2,
			.pull = 0,
			.drv_level = 3,
		},
	},
#endif

};

struct property_t g_lcd1_config[] = {
	{
		.name = "lcd_used",
		.type = PROPERTY_INTGER,
		.v.value = 0,
	},
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
		.v.value = 0,
	},
	{
		.name = "screen0_output_type",
		.type = PROPERTY_INTGER,
		.v.value = 1,
	},
	{
		.name = "screen0_output_mode",
		.type = PROPERTY_INTGER,
		.v.value = 4,
	},
	{
		.name = "screen1_output_type",
		.type = PROPERTY_INTGER,
		.v.value = 1,
	},
	{
		.name = "screen1_output_mode",
		.type = PROPERTY_INTGER,
		.v.value = 4,
	},
};

u32 g_lcd0_config_len = sizeof(g_lcd0_config) / sizeof(struct property_t);
u32 g_lcd1_config_len = sizeof(g_lcd1_config) / sizeof(struct property_t);
u32 g_disp_config_len = sizeof(g_disp_config) / sizeof(struct property_t);
