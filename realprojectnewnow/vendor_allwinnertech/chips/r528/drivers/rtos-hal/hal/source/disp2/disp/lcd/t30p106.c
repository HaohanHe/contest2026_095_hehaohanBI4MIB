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

#include "t30p106.h"

static void lcd_power_on(u32 sel);
static void lcd_power_off(u32 sel);
static void lcd_bl_open(u32 sel);
static void lcd_bl_close(u32 sel);

static void lcd_panel_init(u32 sel);
static void lcd_panel_exit(u32 sel);

#define Delayms sunxi_lcd_delay_ms
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
	LCD_OPEN_FUNC(sel, lcd_power_on, 10);
	LCD_OPEN_FUNC(sel, lcd_panel_init, 10);
	LCD_OPEN_FUNC(sel, sunxi_lcd_tcon_enable, 50);
	LCD_OPEN_FUNC(sel, lcd_bl_open, 0);
	return 0;
}

static s32 lcd_close_flow(u32 sel)
{
	LCD_CLOSE_FUNC(sel, lcd_bl_close, 0);
	LCD_CLOSE_FUNC(sel, lcd_panel_exit, 200);
	LCD_CLOSE_FUNC(sel, sunxi_lcd_tcon_disable, 0);
	LCD_CLOSE_FUNC(sel, lcd_power_off, 500);

	return 0;
}

static void lcd_power_on(u32 sel)
{
	sunxi_lcd_pin_cfg(sel, 1);

	sunxi_lcd_power_enable(sel, 0);
	sunxi_lcd_power_enable(sel, 1);

	panel_reset(sel, 1);
	sunxi_lcd_delay_ms(50);
	panel_reset(sel, 0);
	sunxi_lcd_delay_ms(50);
	panel_reset(sel, 1);
	sunxi_lcd_delay_ms(120);
}

static void lcd_power_off(u32 sel)
{
	sunxi_lcd_pin_cfg(sel, 0);
	sunxi_lcd_delay_ms(20);

	sunxi_lcd_power_disable(sel, 1);
	sunxi_lcd_delay_ms(5);
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

#define REGFLAG_DELAY         0XFC
#define REGFLAG_END_OF_TABLE  0xFE   /* END OF REGISTERS MARKER */

struct LCM_setting_table {
	u8 cmd;
	u32 count;
	u8 para_list[16];
};


static struct LCM_setting_table lcm_initialization_setting[] = {
	{0x11, 1, {0x00} },
	{0x29, 1, {0x00} },
	{0x34, 1, {0x00} },
	{REGFLAG_DELAY, 100, {} },
	{0xFF, 5, {0x77, 0x01, 0x00, 0x00, 0x10} },
	{REGFLAG_DELAY, 5, {} },
	{0xc0, 2, {0x4F, 0x00} },
	{REGFLAG_DELAY, 5, {} },
	{0xc1, 2, {0x07, 0x02} },
	{REGFLAG_DELAY, 5, {} },
	{0xc2, 2, {0x31, 0x05} },
	{REGFLAG_DELAY, 5, {} },
	{0xb0, 16, {0x00, 0x0A, 0x11, 0x0C, 0x10,
			   0x05, 0x00, 0x08, 0x08, 0x1F, 0x07, 0x13,
			   0x10, 0xA9, 0x30, 0x18} },
	{REGFLAG_DELAY, 5, {} },
	{0xb1, 16, {0x00, 0x0B, 0x11, 0x0D, 0x0F,
			   0x05, 0x02, 0x07, 0x06, 0x20, 0x05, 0x15,
			   0x13, 0xA9, 0x30, 0x18} },
	{REGFLAG_DELAY, 5, {} },
	{0xFF, 5, {0x77, 0x01, 0x00, 0x00, 0x11} },
	{REGFLAG_DELAY, 5, {} },
	{0xb0, 1, {0x53} },
	{REGFLAG_DELAY, 5, {} },
	{0xb1, 1, {0x60} },/*te on*/
	{REGFLAG_DELAY, 5, {} },
	{0xb2, 1, {0x07} },/*24bit/pixel*/
	{REGFLAG_DELAY, 5, {} },
	{0xb3, 1, {0x80} },
	{REGFLAG_DELAY, 5, {} },
	{0xb5, 1, {0x49} },
	{REGFLAG_DELAY, 5, {} },
	{0xb7, 1, {0x85} },
	{REGFLAG_DELAY, 5, {} },
	{0xb8, 1, {0x21} },
	{REGFLAG_DELAY, 5, {} },
	{0xc1, 1, {0x78} },
	{REGFLAG_DELAY, 5, {} },
	{0xc2, 1, {0x78} },
	{REGFLAG_DELAY, 100, {} },
	{0xe0, 3, {0x00, 0x00, 0x02} },
	{REGFLAG_DELAY, 5, {} },
	{0xe1, 11, {0x03, 0xA0, 0x00, 0x00, 0x02,
			   0xA0, 0x00, 0x00, 0x00, 0x33, 0x33} },
	{REGFLAG_DELAY, 5, {} },
	{0xe2, 12, {0x22, 0x22, 0x33, 0x33, 0x88,
			   0xA0, 0x00, 0x00, 0x87, 0xA0, 0x00,
			   0x00} },
	{REGFLAG_DELAY, 5, {} },
	{0xe3, 4, {0x00, 0x00, 0x22, 0x22} },
	{REGFLAG_DELAY, 5, {} },
	{0xe4, 2, {0x44, 0x44} },
	{REGFLAG_DELAY, 5, {} },
	{0xe5, 16, {0x04, 0x84, 0xA0, 0xA0, 0x06,
			   0x86, 0xA0, 0xA0, 0x08, 0x88, 0xA0, 0xA0,
			   0x0A, 0x8A, 0xA0, 0xA0} },
	{REGFLAG_DELAY, 5, {} },
	{0xe6, 4, {0xE6, 0x00, 0x00, 0x22, 0x22} },
	{REGFLAG_DELAY, 5, {} },
	{0xe7, 4, {0x44, 0x44} },
	{REGFLAG_DELAY, 5, {} },
	{0xe8, 16, {0x03, 0x83, 0xA0, 0xA0, 0x05,
			   0x85, 0xA0, 0xA0, 0x06, 0x87, 0xa0, 0xa0,
			   0x09, 0x89, 0xA0, 0xA0} },
	{REGFLAG_DELAY, 5, {} },
	{0xeb, 7, {0x00, 0x01, 0xE4, 0xE4, 0x88,
			  0x00, 0x40} },
	{REGFLAG_DELAY, 5, {} },
	{0xec, 2, {0x3c, 0x01} },
	{REGFLAG_DELAY, 5, {} },
	{0xed, 16, {0xAB, 0x89, 0x76, 0x54, 0x02,
			   0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x20,
			   0x45, 0x67, 0x98, 0xBA} },
	{REGFLAG_DELAY, 120, {} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static void lcd_panel_init(u32 sel)
{
	u32 i = 0;

	sunxi_lcd_dsi_clk_enable(sel);
	sunxi_lcd_delay_ms(100);

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
	Delayms(100);

	sunxi_lcd_dsi_dcs_write_0para(sel, 0x10);
	Delayms(100);
}

/*sel: 0:lcd0; 1:lcd1*/
static s32 lcd_user_defined_func(u32 sel, u32 para1, u32 para2, u32 para3)
{
	return 0;
}

struct __lcd_panel t30p106_panel = {
	/* panel driver name, must mach the name of
	 * lcd_drv_name in sys_config.fex
	 */
	.name = "t30p106",
	.func = {
		.cfg_panel_info = lcd_cfg_panel_info,
			.cfg_open_flow = lcd_open_flow,
			.cfg_close_flow = lcd_close_flow,
			.lcd_user_defined_func = lcd_user_defined_func,
	},
};
