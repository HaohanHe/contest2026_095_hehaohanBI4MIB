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


#include "default_eink.h"

static void EINK_power_on(u32 sel);
static void EINK_power_off(u32 sel);

static void EINK_cfg_panel_info(struct panel_extend_para *info)
{
	u32 i = 0, j = 0;
	u32 items;
	u8 lcd_gamma_tbl[][2] = {
		/* {input value, corrected value} */
		{0, 0},
		{15, 15},
		{30, 30},
		{45, 45},
		{60, 60},
		{75, 75},
		{90, 90},
		{105, 105},
		{120, 120},
		{135, 135},
		{150, 150},
		{165, 165},
		{180, 180},
		{195, 195},
		{210, 210},
		{225, 225},
		{240, 240},
		{255, 255},
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
			    ((lcd_gamma_tbl[i + 1][1] -
			      lcd_gamma_tbl[i][1]) * j) / num;
			info->lcd_gamma_tbl[lcd_gamma_tbl[i][0] + j] =
			    (value << 16) + (value << 8) + value;
		}
	}
	info->lcd_gamma_tbl[255] =
	    (lcd_gamma_tbl[items - 1][1] << 16) +
	    (lcd_gamma_tbl[items - 1][1] << 8) + lcd_gamma_tbl[items - 1][1];

	memcpy(info->lcd_cmap_tbl, lcd_cmap_tbl, sizeof(lcd_cmap_tbl));

}

static s32 EINK_open_flow(u32 sel)
{
	LCD_OPEN_FUNC(sel, EINK_power_on, 2);
	LCD_OPEN_FUNC(sel, sunxi_lcd_tcon_enable, 0);
	return 0;
}

static s32 EINK_close_flow(u32 sel)
{
	LCD_CLOSE_FUNC(sel, sunxi_lcd_tcon_disable, 0);
	LCD_CLOSE_FUNC(sel, EINK_power_off, 2);

	return 0;
}

static void EINK_power_on(u32 sel)
{
	/* pwr3 pb2 */
	sunxi_lcd_gpio_set_value(sel, 0, 1);
	sunxi_lcd_delay_ms(1);
	/* pwr0 pb0 */
	sunxi_lcd_gpio_set_value(sel, 1, 1);
	sunxi_lcd_delay_ms(1);
	/* pb1 */
	sunxi_lcd_gpio_set_value(sel, 2, 1);
	sunxi_lcd_delay_ms(1);
	/* pwr2 pd6 */
	sunxi_lcd_gpio_set_value(sel, 3, 1);
	sunxi_lcd_delay_ms(1);
	/* pwr com pd7 */
	sunxi_lcd_gpio_set_value(sel, 4, 1);
	sunxi_lcd_delay_ms(1);

	sunxi_lcd_gpio_set_value(sel, 5, 1);
	sunxi_lcd_delay_ms(2);

	sunxi_lcd_pin_cfg(sel, 1);
}

static void EINK_power_off(u32 sel)
{
	sunxi_lcd_pin_cfg(sel, 0);

	sunxi_lcd_gpio_set_value(sel, 5, 0);
	sunxi_lcd_delay_ms(1);
	sunxi_lcd_gpio_set_value(sel, 4, 0);
	sunxi_lcd_delay_ms(1);
	sunxi_lcd_gpio_set_value(sel, 3, 0);
	sunxi_lcd_delay_ms(1);
	sunxi_lcd_gpio_set_value(sel, 2, 0);
	sunxi_lcd_delay_ms(1);
	sunxi_lcd_gpio_set_value(sel, 1, 0);
	sunxi_lcd_delay_ms(1);
	sunxi_lcd_gpio_set_value(sel, 0, 0);
	sunxi_lcd_delay_ms(2);
}

/* sel: 0:lcd0; 1:lcd1 */
static s32 EINK_user_defined_func(u32 sel, u32 para1, u32 para2, u32 para3)
{
	return 0;
}

struct __lcd_panel default_eink = {
	/* panel driver name, must mach the lcd_drv_name in sys_config.fex */
	.name = "default_eink",
	.func = {
		 .cfg_panel_info = EINK_cfg_panel_info,
		 .cfg_open_flow = EINK_open_flow,
		 .cfg_close_flow = EINK_close_flow,
		 .lcd_user_defined_func = EINK_user_defined_func,
		 }
	,
};
