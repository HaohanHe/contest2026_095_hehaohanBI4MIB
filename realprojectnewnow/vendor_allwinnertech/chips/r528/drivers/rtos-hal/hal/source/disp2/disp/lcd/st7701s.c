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

#include "st7701s.h"

static void lcd_power_on(u32 sel);
static void lcd_power_off(u32 sel);
static void lcd_bl_open(u32 sel);
static void lcd_bl_close(u32 sel);

static void lcd_panel_init(u32 sel);
static void lcd_panel_exit(u32 sel);

#define panel_reset(sel, val) sunxi_lcd_gpio_set_value(sel, 0, val)

static void lcd_cfg_panel_info(struct panel_extend_para *info)
{
	u32 i = 0, j = 0;
	u32 items;
	u8 lcd_gamma_tbl[][2] = {
	    {0, 0},     {15, 15},   {30, 30},   {45, 45},   {60, 60},
	    {75, 75},   {90, 90},   {105, 105}, {120, 120}, {135, 135},
	    {150, 150}, {165, 165}, {180, 180}, {195, 195}, {210, 210},
	    {225, 225}, {240, 240}, {255, 255},
	};

	u32 lcd_cmap_tbl[2][3][4] = {
	    {
		{LCD_CMAP_G0, LCD_CMAP_B1, LCD_CMAP_G2, LCD_CMAP_B3},
		{LCD_CMAP_B0, LCD_CMAP_R1, LCD_CMAP_B2, LCD_CMAP_R3},
		{LCD_CMAP_R0, LCD_CMAP_G1, LCD_CMAP_R2, LCD_CMAP_G3},
	    },
	    {
		{LCD_CMAP_B3, LCD_CMAP_G2, LCD_CMAP_B1, LCD_CMAP_G0},
		{LCD_CMAP_R3, LCD_CMAP_B2, LCD_CMAP_R1, LCD_CMAP_B0},
		{LCD_CMAP_G3, LCD_CMAP_R2, LCD_CMAP_G1, LCD_CMAP_R0},
	    },
	};

	items = sizeof(lcd_gamma_tbl) / 2;
	for (i = 0; i < items - 1; i++) {
		u32 num = lcd_gamma_tbl[i + 1][0] - lcd_gamma_tbl[i][0];

		for (j = 0; j < num; j++) {
			u32 value = 0;

			value =
			    lcd_gamma_tbl[i][1] +
			    ((lcd_gamma_tbl[i + 1][1] - lcd_gamma_tbl[i][1]) *
			     j) /
				num;
			info->lcd_gamma_tbl[lcd_gamma_tbl[i][0] + j] =
			    (value << 16) + (value << 8) + value;
		}
	}
	info->lcd_gamma_tbl[255] = (lcd_gamma_tbl[items - 1][1] << 16) +
				   (lcd_gamma_tbl[items - 1][1] << 8) +
				   lcd_gamma_tbl[items - 1][1];

	memcpy(info->lcd_cmap_tbl, lcd_cmap_tbl, sizeof(lcd_cmap_tbl));
}

static s32 lcd_open_flow(u32 sel)
{
	LCD_OPEN_FUNC(sel, lcd_power_on, 0);
	LCD_OPEN_FUNC(sel, lcd_panel_init, 10);
	LCD_OPEN_FUNC(sel, sunxi_lcd_tcon_enable, 10);
	LCD_OPEN_FUNC(sel, lcd_bl_open, 0);
	return 0;
}

static s32 lcd_close_flow(u32 sel)
{
	LCD_CLOSE_FUNC(sel, lcd_bl_close, 0);
	LCD_CLOSE_FUNC(sel, lcd_panel_exit, 10);
	LCD_CLOSE_FUNC(sel, sunxi_lcd_tcon_disable, 0);
	LCD_CLOSE_FUNC(sel, lcd_power_off, 0);

	return 0;
}

static void lcd_power_on(u32 sel)
{
	sunxi_lcd_pin_cfg(sel, 1);
	sunxi_lcd_power_enable(sel, 0);
	// sunxi_lcd_power_enable(sel, 1);
	sunxi_lcd_delay_ms(10);

	/* reset lcd by gpio */
	panel_reset(sel, 1);
	sunxi_lcd_delay_ms(10);
	panel_reset(sel, 0);
	sunxi_lcd_delay_ms(10);
	panel_reset(sel, 1);
	sunxi_lcd_delay_ms(120);
}

static void lcd_power_off(u32 sel)
{
	sunxi_lcd_pin_cfg(sel, 0);
	sunxi_lcd_delay_ms(1);
	panel_reset(sel, 0);
	sunxi_lcd_delay_ms(1);
	sunxi_lcd_power_disable(sel, 0);
}

static void lcd_bl_open(u32 sel)
{
	sunxi_lcd_pwm_enable(sel);
	sunxi_lcd_backlight_enable(sel);
}

static void lcd_bl_close(u32 sel)
{
	sunxi_lcd_backlight_disable(sel);
	sunxi_lcd_pwm_disable(sel);
}

#define REGFLAG_DELAY         0XFE
#define REGFLAG_END_OF_TABLE  0xFC   /* END OF REGISTERS MARKER */

struct LCM_setting_table {
	u8 cmd;
	u32 count;
	u8 para_list[18];
};


static struct LCM_setting_table lcm_initialization_setting[] = {
	//{0x11, 1, {0x00} },
	//{REGFLAG_DELAY, 60, {} },
	{0xff, 5, {0x77, 0x01, 0x00, 0x00, 0x13} },
	{0xef, 1, {0x08} },

	{0xff, 5, {0x77, 0x01, 0x00, 0x00, 0x10} },
	{0xc0, 2, {0x63, 0x00} },
	{0xc1, 2, {0x07, 0x07} },
	{0xc2, 2, {0x07, 0x08} },
	{0xcc, 1, {0x18} },

	{0xb0, 16, {0x0f, 0x12, 0x16, 0x0b, 0x0e, 0x04, 0x00, 0x07, 0x06, 0x18,
		   0x05, 0x13, 0x11, 0x1f, 0x27, 0x1f} },

	{0xb1, 16, {0x0f, 0x17, 0x1b, 0x0c, 0x10, 0x05, 0x00, 0x08, 0x08, 0x1b,
		    0x05, 0x13, 0x11, 0x22, 0x2a, 0x1f} },

	{0xff, 5, {0x77, 0x01, 0x00, 0x00, 0x11} },

	{0xb0, 1, {0x7d} },
	{0xb1, 1, {0x52} },
	{0xb2, 1, {0x87} },
	{0xb3, 1, {0x80} },
	{0xb5, 1, {0x49} },
	{0xb7, 1, {0x87} },
	{0xb8, 1, {0x21} },
	{0xb9, 1, {0x10} },
	{0xbb, 1, {0x03} },
	{0xc0, 1, {0x07} },
	{0xc1, 1, {0x78} },
	{0xc2, 1, {0x78} },

	{0xd0, 1, {0x88} },
	{0xe0, 3, {0x80, 0x00, 0x02} },
	{0xe1, 11, {0x01, 0xa0, 0x00, 0x00, 0x02, 0xa0, 0x00, 0x00, 0x00, 0x20,
		    0x20} },
	{0xe2, 13, {0x30, 0x30, 0x20, 0x20, 0x27, 0xa0, 0x00, 0x00, 0x28, 0xa0,
		    0x00, 0x00, 0x00} },
	{0xe3, 4, {0x00, 0x00, 0x33, 0x33} },
	{0xe4, 2, {0x44, 0x44} },
	{0xe5, 16, {0x03, 0x33, 0xa0, 0xa0, 0x05, 0x2d, 0xa0, 0xa0, 0x07, 0x2f,
		    0xa0, 0xa0, 0x09, 0x31, 0xa0, 0xa0} },

	{0xe6, 4, {0x00, 0x00, 0x33, 0x33} },
	{0xe7, 2, {0x44, 0x44} },
	{0xe8, 16, {0x04, 0x34, 0xa0, 0xa0, 0x06, 0x2e, 0xa0, 0xa0, 0x08, 0x30,
		    0xa0, 0xa0, 0x0a, 0x32, 0xa0, 0xa0} },
	{0xe9, 2, {0x36, 0x00} },
	{0xeb, 7, {0x00, 0x02, 0x93, 0x93, 0x88, 0x00, 0x40} },

	{0xec, 2, {0xff, 0x01} },
	{0xed, 16, {0x2f, 0x0a, 0xb4, 0x56, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff,
		    0xff, 0xf7, 0x65, 0x4b, 0xa0, 0xf2} },
	{0xef, 6, {0x08, 0x08, 0x08, 0x45, 0x3f, 0x54} },
	{0xff, 5, {0x77, 0x01, 0x00, 0x00, 0x13} },

	{0xe8, 2, {0x00, 0x0e} },
	{0xff, 5, {0x77, 0x01, 0x00, 0x00, 0x00} },
	{0x11, 1, {0x00} },
	{REGFLAG_DELAY, 120, {} },

	{0xff, 5, {0x77, 0x01, 0x00, 0x00, 0x13} },
	{0xe8, 2, {0x00, 0x0c} },
	{REGFLAG_DELAY, 20, {} },
	{0xe8, 2, {0x00, 0x00} },
	{0xe6, 2, {0x16, 0x7c} },
	{0xff, 5, {0x77, 0x01, 0x00, 0x00, 0x00} },
	{0x36, 1, {0x00} },
	{0x29, 1, {0x00} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static void lcd_panel_init(u32 sel)
{
	u32 i = 0;

	sunxi_lcd_dsi_clk_enable(sel);
	sunxi_lcd_delay_ms(10);

	for (i = 0;; i++) {
		if (lcm_initialization_setting[i].cmd == REGFLAG_END_OF_TABLE)
			break;
		else if (lcm_initialization_setting[i].cmd == REGFLAG_DELAY)
			sunxi_lcd_delay_ms(lcm_initialization_setting[i].count);
		else {
			dsi_dcs_wr(0, lcm_initialization_setting[i].cmd,
				   lcm_initialization_setting[i].para_list,
				   lcm_initialization_setting[i].count);
		}
	}
}

static void lcd_panel_exit(u32 sel)
{
	sunxi_lcd_dsi_dcs_write_0para(sel, 0x28);
	sunxi_lcd_delay_ms(10);
	sunxi_lcd_dsi_dcs_write_0para(sel, 0x10);
	sunxi_lcd_delay_ms(10);
}

/*sel: 0:lcd0; 1:lcd1*/
static s32 lcd_user_defined_func(u32 sel, u32 para1, u32 para2, u32 para3)
{
	return 0;
}

struct __lcd_panel st7701s_panel = {
	/* panel driver name, must mach the name of
	 * lcd_drv_name in sys_config.fex
	 */
	.name = "st7701s",
	.func = {
		.cfg_panel_info = lcd_cfg_panel_info,
			.cfg_open_flow = lcd_open_flow,
			.cfg_close_flow = lcd_close_flow,
			.lcd_user_defined_func = lcd_user_defined_func,
	},
};
