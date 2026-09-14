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

#include "st7789v_cpu.h"

#define CPU_TRI_MODE

#define DBG_INFO(format, args...) (printk("[ST7789V LCD INFO] LINE:%04d-->%s:"format, __LINE__, __func__, ##args))
#define DBG_ERR(format, args...) (printk("[ST7789V LCD ERR] LINE:%04d-->%s:"format, __LINE__, __func__, ##args))
#define panel_reset(val) sunxi_lcd_gpio_set_value(sel, 0, val)
#define lcd_cs(val)  sunxi_lcd_gpio_set_value(sel, 1, val)

static void lcd_panel_st7789v_init(u32 sel, struct disp_panel_para *info);
static void LCD_power_on(u32 sel);
static void LCD_power_off(u32 sel);
static void LCD_bl_open(u32 sel);
static void LCD_bl_close(u32 sel);

static void LCD_panel_init(u32 sel);
static void LCD_panel_exit(u32 sel);

static void LCD_cfg_panel_info(struct panel_extend_para *info)
{
#if 0
	u32 i = 0, j = 0;
	u32 items;
	u8 lcd_gamma_tbl[][2] = {
	    //{input value, corrected value}
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
#endif
}

static s32 LCD_open_flow(u32 sel)
{
	LCD_OPEN_FUNC(sel, LCD_power_on, 120);
#ifdef CPU_TRI_MODE
	LCD_OPEN_FUNC(sel, LCD_panel_init, 100);
	LCD_OPEN_FUNC(sel, sunxi_lcd_tcon_enable, 50);
#else
	LCD_OPEN_FUNC(sel, sunxi_lcd_tcon_enable, 100);
	LCD_OPEN_FUNC(sel, LCD_panel_init, 50);
#endif
	LCD_OPEN_FUNC(sel, LCD_bl_open, 0);

	return 0;
}

static s32 LCD_close_flow(u32 sel)
{
	LCD_CLOSE_FUNC(sel, LCD_bl_close, 20);
#ifdef CPU_TRI_MODE
	LCD_CLOSE_FUNC(sel, sunxi_lcd_tcon_disable, 10);
	LCD_CLOSE_FUNC(sel, LCD_panel_exit, 50);
#else
	LCD_CLOSE_FUNC(sel, LCD_panel_exit, 10);
	LCD_CLOSE_FUNC(sel, sunxi_lcd_tcon_disable, 10);
#endif
	LCD_CLOSE_FUNC(sel, LCD_power_off, 0);

	return 0;
}

static void LCD_power_on(u32 sel)
{
	/*config lcd_power pin to open lcd power0 */
	sunxi_lcd_power_enable(sel, 0);
	sunxi_lcd_pin_cfg(sel, 1);

}

static void LCD_power_off(u32 sel)
{
	/*lcd_cs, active low */
	lcd_cs(1);
	sunxi_lcd_delay_ms(10);
	/*lcd_rst, active hight */
	panel_reset(1);
	sunxi_lcd_delay_ms(10);

	sunxi_lcd_pin_cfg(sel, 0);
	/*config lcd_power pin to close lcd power0 */
	sunxi_lcd_power_disable(sel, 0);
}

static void LCD_bl_open(u32 sel)
{
	sunxi_lcd_pwm_enable(sel);
	/*config lcd_bl_en pin to open lcd backlight */
	sunxi_lcd_backlight_enable(sel);
}

static void LCD_bl_close(u32 sel)
{
	/*config lcd_bl_en pin to close lcd backlight */
	sunxi_lcd_backlight_disable(sel);
	sunxi_lcd_pwm_disable(sel);
}

/*static int bootup_flag = 0;*/
static void LCD_panel_init(u32 sel)
{
	struct disp_panel_para *info =
	    kmalloc(sizeof(struct disp_panel_para), GFP_KERNEL);

	DBG_INFO("\n");
	bsp_disp_get_panel_info(sel, info);
	lcd_panel_st7789v_init(sel, info);

	disp_sys_free(info);
	return;
}

static void LCD_panel_exit(u32 sel)
{
	sunxi_lcd_cpu_write_index(0, 0x28);
	sunxi_lcd_cpu_write_index(0, 0x10);
}


static void lcd_panel_st7789v_init(u32 sel, struct disp_panel_para *info)
{
	DBG_INFO("\n");
	/*lcd_cs, active low */
	lcd_cs(0);
	sunxi_lcd_delay_ms(10);
	panel_reset(1);
	sunxi_lcd_delay_ms(20);
	panel_reset(0);
	sunxi_lcd_delay_ms(20);
	panel_reset(1);
	sunxi_lcd_delay_ms(20);
	sunxi_lcd_cpu_write_index(0, 0x11);
	sunxi_lcd_delay_ms(120);
	sunxi_lcd_cpu_write_index(0, 0x36);
	sunxi_lcd_cpu_write_data(0, 0x00);
	sunxi_lcd_cpu_write_index(0, 0x3a);
	sunxi_lcd_cpu_write_data(0, 0x06);
	sunxi_lcd_cpu_write_index(0, 0x21);
	sunxi_lcd_cpu_write_index(0, 0xb2);
	sunxi_lcd_cpu_write_data(0, 0x05);
	sunxi_lcd_cpu_write_data(0, 0x05);
	sunxi_lcd_cpu_write_data(0, 0x00);
	sunxi_lcd_cpu_write_data(0, 0x33);
	sunxi_lcd_cpu_write_data(0, 0x33);
	sunxi_lcd_cpu_write_index(0, 0xb7);
	sunxi_lcd_cpu_write_data(0, 0x35);
	/*ST7789V Power setting */
	sunxi_lcd_cpu_write_index(0, 0xb8);
	sunxi_lcd_cpu_write_data(0, 0x2f);
	sunxi_lcd_cpu_write_data(0, 0x2b);
	sunxi_lcd_cpu_write_data(0, 0x2f);
	sunxi_lcd_cpu_write_index(0, 0xbb);
	sunxi_lcd_cpu_write_data(0, 0x20);
	sunxi_lcd_cpu_write_index(0, 0xc0);
	sunxi_lcd_cpu_write_data(0, 0x2c);
	sunxi_lcd_cpu_write_index(0, 0xc2);
	sunxi_lcd_cpu_write_data(0, 0x01);
	sunxi_lcd_cpu_write_index(0, 0xc3);
	sunxi_lcd_cpu_write_data(0, 0x0b);
	sunxi_lcd_cpu_write_index(0, 0xc4);
	sunxi_lcd_cpu_write_data(0, 0x20);
	sunxi_lcd_cpu_write_index(0, 0xc6);
	sunxi_lcd_cpu_write_data(0, 0x11);
	sunxi_lcd_cpu_write_index(0, 0xd0);
	sunxi_lcd_cpu_write_data(0, 0xa4);
	sunxi_lcd_cpu_write_data(0, 0xa1);
	sunxi_lcd_cpu_write_index(0, 0xe8);
	sunxi_lcd_cpu_write_data(0, 0x03);
	sunxi_lcd_cpu_write_index(0, 0xe9);
	sunxi_lcd_cpu_write_data(0, 0x0d);
	sunxi_lcd_cpu_write_data(0, 0x12);
	sunxi_lcd_cpu_write_data(0, 0x00);
	/*ST7789V gamma setting */
	sunxi_lcd_cpu_write_index(0, 0xe0);
	sunxi_lcd_cpu_write_data(0, 0xd0);
	sunxi_lcd_cpu_write_data(0, 0x06);
	sunxi_lcd_cpu_write_data(0, 0x0b);
	sunxi_lcd_cpu_write_data(0, 0x0a);
	sunxi_lcd_cpu_write_data(0, 0x09);
	sunxi_lcd_cpu_write_data(0, 0x05);
	sunxi_lcd_cpu_write_data(0, 0x2e);
	sunxi_lcd_cpu_write_data(0, 0x43);
	sunxi_lcd_cpu_write_data(0, 0x44);
	sunxi_lcd_cpu_write_data(0, 0x09);
	sunxi_lcd_cpu_write_data(0, 0x16);
	sunxi_lcd_cpu_write_data(0, 0x15);
	sunxi_lcd_cpu_write_data(0, 0x23);
	sunxi_lcd_cpu_write_data(0, 0x27);
	sunxi_lcd_cpu_write_index(0, 0xe1);
	sunxi_lcd_cpu_write_data(0, 0xd0);
	sunxi_lcd_cpu_write_data(0, 0x06);
	sunxi_lcd_cpu_write_data(0, 0x0b);
	sunxi_lcd_cpu_write_data(0, 0x09);
	sunxi_lcd_cpu_write_data(0, 0x08);
	sunxi_lcd_cpu_write_data(0, 0x06);
	sunxi_lcd_cpu_write_data(0, 0x2e);
	sunxi_lcd_cpu_write_data(0, 0x44);
	sunxi_lcd_cpu_write_data(0, 0x44);
	sunxi_lcd_cpu_write_data(0, 0x3a);
	sunxi_lcd_cpu_write_data(0, 0x15);
	sunxi_lcd_cpu_write_data(0, 0x15);
	sunxi_lcd_cpu_write_data(0, 0x23);
	sunxi_lcd_cpu_write_data(0, 0x26);

#if defined(CPU_TRI_MODE)
	/* enable te, mode 0 */

	sunxi_lcd_cpu_write_index(0, 0x35);
	sunxi_lcd_cpu_write_data(0, 0x00);

	sunxi_lcd_cpu_write_index(0, 0x44);
	sunxi_lcd_cpu_write_data(0, 0x00);
	sunxi_lcd_cpu_write_data(0, 0x80);
#endif
	sunxi_lcd_cpu_write_index(0, 0x29);
	sunxi_lcd_cpu_write_index(0, 0x2c);

}


/* panel driver name, must mach the name of lcd_drv_name in sys_config.fex */
struct __lcd_panel st7789v_cpu_panel = {
	.name = "st7789v_cpu",
	.func = {
		.cfg_panel_info = LCD_cfg_panel_info,
		.cfg_open_flow = LCD_open_flow,
		.cfg_close_flow = LCD_close_flow,
	},
};
