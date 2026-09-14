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
		.v.str = "st7701s",
	},
	{
		.name = "lcd_backlight",
		.type = PROPERTY_INTGER,
		.v.value = 150,
	},
	{
		.name = "lcd_if",
		.type = PROPERTY_INTGER,
		.v.value = 4,
	},
	{
		.name = "lcd_x",
		.type = PROPERTY_INTGER,
		.v.value = 480,
	},
	{
		.name = "lcd_y",
		.type = PROPERTY_INTGER,
		.v.value = 640,
	},
	{
		.name = "lcd_width",
		.type = PROPERTY_INTGER,
		.v.value = 36,
	},
	{
		.name = "lcd_height",
		.type = PROPERTY_INTGER,
		.v.value = 65,
	},
	{
		.name = "lcd_dclk_freq",
		.type = PROPERTY_INTGER,
		.v.value = 25,
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
		.v.value = 9,
	},
	{
		.name = "lcd_pwm_freq",
		.type = PROPERTY_INTGER,
		.v.value = 5000,
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
		.v.value = 70,
	},
	{
		.name = "lcd_ht",
		.type = PROPERTY_INTGER,
		.v.value = 615,
	},
	{
		.name = "lcd_hspw",
		.type = PROPERTY_INTGER,
		.v.value = 8,
	},
	{
		.name = "lcd_vbp",
		.type = PROPERTY_INTGER,
		.v.value = 30,
	},
	{
		.name = "lcd_vt",
		.type = PROPERTY_INTGER,
		.v.value = 690,
	},
	{
		.name = "lcd_vspw",
		.type = PROPERTY_INTGER,
		.v.value = 10,
	},
	{
		.name = "lcd_dsi_if",
		.type = PROPERTY_INTGER,
		.v.value = 0,
	},
	{
		.name = "lcd_dsi_lane",
		.type = PROPERTY_INTGER,
		.v.value = 2,
	},
	{
		.name = "lcd_dsi_format",
		.type = PROPERTY_INTGER,
		.v.value = 0,
	},
	{
		.name = "lcd_dsi_te",
		.type = PROPERTY_INTGER,
		.v.value = 0,
	},
	{
		.name = "lcd_frm",
		.type = PROPERTY_INTGER,
		.v.value = 0,
	},
	{
		.name = "lcd_power",
		.type = PROPERTY_POWER,
		.v.power = {
			.power_name = "dldo1",
			.power_type = AXP2101_REGULATOR,
			.power_id = AXP2101_ID_DLDO1,
			.power_vol = 3300000,
		},
	},
	{
		.name = "lcd_pin_power",
		.type = PROPERTY_POWER,
		.v.power = {
			.power_name = "bldo1",
			.power_type = AXP2101_REGULATOR,
			.power_id = AXP2101_ID_BLDO1,
			.power_vol = 1800000,
			.always_on = true,
		},
	},
	{
		.name = "lcd_gpio_0",
		.type = PROPERTY_GPIO,
		.v.gpio_list = {
			.gpio = GPIOD(9),
			.mul_sel = GPIO_DIRECTION_OUTPUT,
			.pull = 0,
			.drv_level = 3,
			.data = 1,
		},
	},

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
