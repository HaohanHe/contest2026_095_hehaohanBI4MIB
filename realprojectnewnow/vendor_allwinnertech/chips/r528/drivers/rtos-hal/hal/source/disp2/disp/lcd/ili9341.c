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

#include "ili9341.h"
#include "sunxi_hal_common.h"
#define CPU_TRI_MODE

/* ILI9341 timing constants (from ILI9341 datasheet) */
#define ILI9341_RESET_PRE_MS         1
#define ILI9341_RESET_LOW_MS         10
#define ILI9341_RESET_POST_MS        120
#define ILI9341_SLEEP_OUT_DELAY_MS   120
#define ILI9341_COMMAND_US_DELAY     10 /* us delay used in bit-banged SPI */

#define ili9341c_spi_scl_1 sunxi_lcd_gpio_set_value(0, 3, 1)
#define ili9341c_spi_scl_0 sunxi_lcd_gpio_set_value(0, 3, 0)
#define ili9341c_spi_sdi_1 sunxi_lcd_gpio_set_value(0, 2, 1)
#define ili9341c_spi_sdi_0 sunxi_lcd_gpio_set_value(0, 2, 0)
#define ili9341c_spi_cs_1 sunxi_lcd_gpio_set_value(0, 1, 1)
#define ili9341c_spi_cs_0 sunxi_lcd_gpio_set_value(0, 1, 0)

#define ili9341c_spi_reset_1 sunxi_lcd_gpio_set_value(0, 0, 1)
#define ili9341c_spi_reset_0 sunxi_lcd_gpio_set_value(0, 0, 0)

#define ili9341c_spi_dc_1 sunxi_lcd_gpio_set_value(0, 4, 1)
#define ili9341c_spi_dc_0 sunxi_lcd_gpio_set_value(0, 4, 0)

static void lcd_panel_ili9341_init(struct disp_panel_para *info);

static void LCD_power_on(u32 sel);
static void LCD_power_off(u32 sel);
static void LCD_bl_open(u32 sel);
static void LCD_bl_close(u32 sel);

static void LCD_panel_init(u32 sel);
static void LCD_panel_exit(u32 sel);

extern s32 tcon0_cpu_set_tri_mode(u32 sel);

void write_tcon_register(int offset, int value)
{
	volatile int *tcon_reg = (int *)(0xf1c0c000 + offset);
	int reg = 0;

	reg = *((volatile int *)tcon_reg);
	reg |= value;
	*((volatile int *)tcon_reg) = reg;
}

static void lcd_cfg_panel_info(struct panel_extend_para *info)
{
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
}

static s32 lcd_open_flow(u32 sel)
{
#if !CONFIG_LCD_ILI9341_SPI
	syslog(LOG_INFO, "ili9341: lcd_open_flow\n");
	LCD_OPEN_FUNC(sel, LCD_power_on, 40);
#ifdef CPU_TRI_MODE
	LCD_OPEN_FUNC(sel, LCD_panel_init, 50);
	LCD_OPEN_FUNC(sel, sunxi_lcd_tcon_enable, 50);
#else
	LCD_OPEN_FUNC(sel, sunxi_lcd_tcon_enable, 50);
	LCD_OPEN_FUNC(sel, LCD_panel_init, 50);
#endif
	LCD_OPEN_FUNC(sel, LCD_bl_open, 0);
#endif
	return 0;
}

static s32 lcd_close_flow(u32 sel)
{
#if !CONFIG_LCD_ILI9341_SPI
	syslog(LOG_INFO, "ili9341: lcd_close_flow\n");
	LCD_CLOSE_FUNC(sel, LCD_bl_close, 50);
#ifdef CPU_TRI_MODE
	LCD_CLOSE_FUNC(sel, sunxi_lcd_tcon_disable, 10);
	LCD_CLOSE_FUNC(sel, LCD_panel_exit, 10);
#else
	LCD_CLOSE_FUNC(sel, LCD_panel_exit, 10);
	LCD_CLOSE_FUNC(sel, sunxi_lcd_tcon_disable, 10);
#endif
	LCD_CLOSE_FUNC(sel, LCD_power_off, 10);
#endif
	return 0;
}

static void LCD_power_on(u32 sel)
{
	syslog(LOG_INFO, "ili9341: LCD_power_on\n");
	/*config lcd_power pin to open lcd power0*/
	sunxi_lcd_power_enable(sel, 0);

	int __ret_pwm = sunxi_lcd_pwm_enable(sel);
	syslog(LOG_INFO, "ili9341: sunxi_lcd_pwm_enable returned %d\n", __ret_pwm);
	// config lcd_bl_en pin to open lcd backlight
	/*pwr_en, active low*/
	// sunxi_lcd_gpio_set_value(sel, 3, 0);
	// sunxi_lcd_backlight_enable(sel);

	hal_gpio_sel_vol_mode(GPIOD(20), POWER_MODE_330);
	hal_gpio_set_driving_level(GPIOD(20), GPIO_DRIVING_LEVEL3);
	hal_gpio_set_direction(GPIOD(20), GPIO_DIRECTION_OUTPUT);
	hal_gpio_set_pull(GPIOD(20), GPIO_PULL_DOWN_DISABLED);
	hal_gpio_set_data(GPIOD(20), 1);

	syslog(LOG_INFO, "ili9341: LCD_power_on end\n");
}

static void LCD_power_off(u32 sel)
{
	syslog(LOG_INFO, "ili9341: LCD_power_off\n");
	sunxi_lcd_pin_cfg(sel, 0);
	/*pwr_en, active low*/
	sunxi_lcd_gpio_set_value(sel, 3, 1);
	/*config lcd_power pin to close lcd power0*/
	sunxi_lcd_power_disable(sel, 0);
}

static void LCD_bl_open(u32 sel)
{
	syslog(LOG_INFO, "ili9341: LCD_bl_open sel=%d\n", sel);
	// int __ret_pwm = sunxi_lcd_pwm_enable(sel);
  // syslog(LOG_INFO, "ili9341: sunxi_lcd_pwm_enable returned %d\n", __ret_pwm);
	// //config lcd_bl_en pin to open lcd backlight
	sunxi_lcd_backlight_enable(sel);
	syslog(LOG_INFO, "ili9341: sunxi_lcd_backlight_enable called\n");
}

static void LCD_bl_close(u32 sel)
{
	syslog(LOG_INFO, "ili9341: LCD_bl_close sel=%d\n", sel);
	/*config lcd_bl_en pin to close lcd backlight*/
	sunxi_lcd_backlight_disable(sel);
	syslog(LOG_INFO, "ili9341: sunxi_lcd_backlight_disable called\n");
	sunxi_lcd_pwm_disable(sel);
}

static void LCD_WRITE_DATA(u32 value)
{
	u32 i;
	ili9341c_spi_cs_0;
	ili9341c_spi_dc_1;

	for (i = 0; i < 8; i++) {
		if (value & 0x80)
			ili9341c_spi_sdi_1;
		else
			ili9341c_spi_sdi_0;
		value <<= 1;
		ili9341c_spi_scl_0;
		ili9341c_spi_scl_1;
	}
	ili9341c_spi_cs_1;
}

static void LCD_WRITE_COMMAND(u32 value)
{
	u32 i;
	ili9341c_spi_cs_0;
	ili9341c_spi_dc_0;

	for (i = 0; i < 8; i++) {
		if (value & 0x80)
			ili9341c_spi_sdi_1;
		else
			ili9341c_spi_sdi_0;
		value <<= 1;
		ili9341c_spi_scl_0;
		ili9341c_spi_scl_1;
	}
	ili9341c_spi_cs_1;
}

static void LCD_panel_init(u32 sel)
{
	syslog(LOG_INFO, "ili9341: LCD_panel_init\n");
	struct disp_panel_para *info =
	    (struct disp_panel_para *)disp_sys_malloc(sizeof(struct disp_panel_para));

	bsp_disp_get_panel_info(sel, info);
	lcd_panel_ili9341_init(info);
	disp_sys_free(info);
	return;
}

static void LCD_panel_exit(u32 sel)
{
	struct disp_panel_para *info =
	    (struct disp_panel_para *)disp_sys_malloc(sizeof(struct disp_panel_para));

	LCD_WRITE_COMMAND(0x28);
	LCD_WRITE_COMMAND(0x10);
	sunxi_lcd_delay_ms(300);
	bsp_disp_get_panel_info(sel, info);
	disp_sys_free(info);
	return;
}

#define GREEN 0x07E0
#define LCD_WIDTH 320
#define LCD_HEIGHT 240
#define TOTAL_PIXELS (LCD_WIDTH * LCD_HEIGHT)

static void test_fill_green(void) {
    uint32_t i;

    /* 设置X坐标范围 */
    LCD_WRITE_COMMAND(0x2A);
    LCD_WRITE_DATA(0x00);
    LCD_WRITE_DATA(0x00);
    LCD_WRITE_DATA(0x00);
    LCD_WRITE_DATA(LCD_WIDTH - 1);

    /* 设置Y坐标范围 */
    LCD_WRITE_COMMAND(0x2B);
    LCD_WRITE_DATA(0x00);
    LCD_WRITE_DATA(0x00);
    LCD_WRITE_DATA(0x00);
    LCD_WRITE_DATA(LCD_HEIGHT - 1);

    /* 开始写显存 */
    LCD_WRITE_COMMAND(0x2C);

    /* 填充绿色 */
    for (i = 0; i < TOTAL_PIXELS; i++) {
        LCD_WRITE_DATA(GREEN);
    }
}


static void lcd_panel_ili9341_init(struct disp_panel_para *info)
{
	syslog(LOG_INFO, "ili9341: lcd_panel_ili9341_init\n");
	//************* Start Initial Sequence **********//
	//************* Reset LCD Driver ****************//
	ili9341c_spi_reset_1;
	sunxi_lcd_delay_ms(ILI9341_RESET_PRE_MS);
	ili9341c_spi_reset_0;
	/*Delay ILI9341_RESET_LOW_MS ms  This delay time is necessary*/
	sunxi_lcd_delay_ms(ILI9341_RESET_LOW_MS);
	ili9341c_spi_reset_1;
	/*Delay ILI9341_RESET_POST_MS ms*/
	sunxi_lcd_delay_ms(ILI9341_RESET_POST_MS);
	/************** Start Initial Sequence ***********/
	/*Pixel Format Set*/
	LCD_WRITE_COMMAND(0xCF);
	LCD_WRITE_DATA(0x00);
	LCD_WRITE_DATA(0xC1);
	LCD_WRITE_DATA(0x30);

	LCD_WRITE_COMMAND(0xED);
	LCD_WRITE_DATA(0x64);
	LCD_WRITE_DATA(0x03);
	LCD_WRITE_DATA(0x12);
	LCD_WRITE_DATA(0x81);

	LCD_WRITE_COMMAND(0xE8);
	LCD_WRITE_DATA(0x85);
	LCD_WRITE_DATA(0x01);
	LCD_WRITE_DATA(0x7A);

	LCD_WRITE_COMMAND(0xCB);
	LCD_WRITE_DATA(0x39);
	LCD_WRITE_DATA(0x2C);
	LCD_WRITE_DATA(0x00);
	LCD_WRITE_DATA(0x34);
	LCD_WRITE_DATA(0x02);

	LCD_WRITE_COMMAND(0xF7);
	LCD_WRITE_DATA(0x20);

	LCD_WRITE_COMMAND(0xEA);
	LCD_WRITE_DATA(0x00);
	LCD_WRITE_DATA(0x00);

	LCD_WRITE_COMMAND(0xC0); //Power control
	LCD_WRITE_DATA(0x21); //VRH[5:0]

	LCD_WRITE_COMMAND(0xC1); //Power control
	LCD_WRITE_DATA(0x11); //SAP[2:0];BT[3:0]

	LCD_WRITE_COMMAND(0xC5); //VCM control
	LCD_WRITE_DATA(0x31);
	LCD_WRITE_DATA(0x3C);

	LCD_WRITE_COMMAND(0xC7); //VCM control2
	LCD_WRITE_DATA(0x9f);

	LCD_WRITE_COMMAND(0x36); // Memory Access Control
	LCD_WRITE_DATA(0x08);

	LCD_WRITE_COMMAND(0x3A); // Pixel Format
	LCD_WRITE_DATA(0x55);

	LCD_WRITE_COMMAND(0xB1);
	LCD_WRITE_DATA(0x00);
	LCD_WRITE_DATA(0x1B);

	LCD_WRITE_COMMAND(0xB6); // Display Function Control
	LCD_WRITE_DATA(0x0A);
	LCD_WRITE_DATA(0xA2);

	LCD_WRITE_COMMAND(0xF2); // 3Gamma Function Disable
	LCD_WRITE_DATA(0x00);

	LCD_WRITE_COMMAND(0x26); //Gamma curve selected
	LCD_WRITE_DATA(0x01);

	LCD_WRITE_COMMAND(0xE0); //Set Gamma
	LCD_WRITE_DATA(0x0F);
	LCD_WRITE_DATA(0x20);
	LCD_WRITE_DATA(0x1D);
	LCD_WRITE_DATA(0x0B);
	LCD_WRITE_DATA(0x10);
	LCD_WRITE_DATA(0x0A);
	LCD_WRITE_DATA(0x49);
	LCD_WRITE_DATA(0xA9);
	LCD_WRITE_DATA(0x3B);
	LCD_WRITE_DATA(0x0A);
	LCD_WRITE_DATA(0x15);
	LCD_WRITE_DATA(0x06);
	LCD_WRITE_DATA(0x0C);
	LCD_WRITE_DATA(0x06);
	LCD_WRITE_DATA(0x00);

	LCD_WRITE_COMMAND(0xE1); //Set Gamma
	LCD_WRITE_DATA(0x00);
	LCD_WRITE_DATA(0x1F);
	LCD_WRITE_DATA(0x22);
	LCD_WRITE_DATA(0x04);
	LCD_WRITE_DATA(0x0F);
	LCD_WRITE_DATA(0x05);
	LCD_WRITE_DATA(0x36);
	LCD_WRITE_DATA(0x46);
	LCD_WRITE_DATA(0x46);
	LCD_WRITE_DATA(0x05);
	LCD_WRITE_DATA(0x0B);
	LCD_WRITE_DATA(0x09);
	LCD_WRITE_DATA(0x33);
	LCD_WRITE_DATA(0x39);
	LCD_WRITE_DATA(0x0F);

	/* Sleep out and display on */
	LCD_WRITE_COMMAND(0x11);
	sunxi_lcd_delay_ms(ILI9341_SLEEP_OUT_DELAY_MS);
	LCD_WRITE_COMMAND(0x29);
	/* Memory Write */
	LCD_WRITE_COMMAND(0x2C);
	sunxi_lcd_delay_ms(ILI9341_SLEEP_OUT_DELAY_MS);

	test_fill_green();

}

void lcd_reflush(void)
{
	/************** Start Initial Sequence ***********/
	/************** Reset LCD Driver *****************/
	ili9341c_spi_reset_1;
	sunxi_lcd_delay_ms(ILI9341_RESET_PRE_MS);
	ili9341c_spi_reset_0;
	/*Delay ILI9341_RESET_LOW_MS ms  This delay time is necessary*/
	sunxi_lcd_delay_ms(ILI9341_RESET_LOW_MS);
	ili9341c_spi_reset_1;
	/*Delay ILI9341_RESET_POST_MS ms*/
	sunxi_lcd_delay_ms(ILI9341_RESET_POST_MS);
	/************** Start Initial Sequence ***********/
	/*Pixel Format Set*/
	LCD_WRITE_COMMAND(0xCF);
	LCD_WRITE_DATA(0x00);
	LCD_WRITE_DATA(0xC1);
	LCD_WRITE_DATA(0x30);

	LCD_WRITE_COMMAND(0xED);
	LCD_WRITE_DATA(0x64);
	LCD_WRITE_DATA(0x03);
	LCD_WRITE_DATA(0x12);
	LCD_WRITE_DATA(0x81);

	LCD_WRITE_COMMAND(0xE8);
	LCD_WRITE_DATA(0x85);
	LCD_WRITE_DATA(0x01);
	LCD_WRITE_DATA(0x7A);

	LCD_WRITE_COMMAND(0xCB);
	LCD_WRITE_DATA(0x39);
	LCD_WRITE_DATA(0x2C);
	LCD_WRITE_DATA(0x00);
	LCD_WRITE_DATA(0x34);
	LCD_WRITE_DATA(0x02);

	LCD_WRITE_COMMAND(0xF7);
	LCD_WRITE_DATA(0x20);

	LCD_WRITE_COMMAND(0xEA);
	LCD_WRITE_DATA(0x00);
	LCD_WRITE_DATA(0x00);

	LCD_WRITE_COMMAND(0xC0); //Power control
	LCD_WRITE_DATA(0x21); //VRH[5:0]

	LCD_WRITE_COMMAND(0xC1); //Power control
	LCD_WRITE_DATA(0x11); //SAP[2:0];BT[3:0]

	LCD_WRITE_COMMAND(0xC5); //VCM control
	LCD_WRITE_DATA(0x31);
	LCD_WRITE_DATA(0x3C);

	LCD_WRITE_COMMAND(0xC7); //VCM control2
	LCD_WRITE_DATA(0x9f);

	LCD_WRITE_COMMAND(0x36); // Memory Access Control
	LCD_WRITE_DATA(0x08);

	LCD_WRITE_COMMAND(0x3A); // Pixel Format
	LCD_WRITE_DATA(0x55);

	LCD_WRITE_COMMAND(0xB1);
	LCD_WRITE_DATA(0x00);
	LCD_WRITE_DATA(0x1B);

	LCD_WRITE_COMMAND(0xB6); // Display Function Control
	LCD_WRITE_DATA(0x0A);
	LCD_WRITE_DATA(0xA2);

	LCD_WRITE_COMMAND(0xF2); // 3Gamma Function Disable
	LCD_WRITE_DATA(0x00);

	LCD_WRITE_COMMAND(0x26); //Gamma curve selected
	LCD_WRITE_DATA(0x01);

	LCD_WRITE_COMMAND(0xE0); //Set Gamma
	LCD_WRITE_DATA(0x0F);
	LCD_WRITE_DATA(0x20);
	LCD_WRITE_DATA(0x1D);
	LCD_WRITE_DATA(0x0B);
	LCD_WRITE_DATA(0x10);
	LCD_WRITE_DATA(0x0A);
	LCD_WRITE_DATA(0x49);
	LCD_WRITE_DATA(0xA9);
	LCD_WRITE_DATA(0x3B);
	LCD_WRITE_DATA(0x0A);
	LCD_WRITE_DATA(0x15);
	LCD_WRITE_DATA(0x06);
	LCD_WRITE_DATA(0x0C);
	LCD_WRITE_DATA(0x06);
	LCD_WRITE_DATA(0x00);

	LCD_WRITE_COMMAND(0xE1); //Set Gamma
	LCD_WRITE_DATA(0x00);
	LCD_WRITE_DATA(0x1F);
	LCD_WRITE_DATA(0x22);
	LCD_WRITE_DATA(0x04);
	LCD_WRITE_DATA(0x0F);
	LCD_WRITE_DATA(0x05);
	LCD_WRITE_DATA(0x36);
	LCD_WRITE_DATA(0x46);
	LCD_WRITE_DATA(0x46);
	LCD_WRITE_DATA(0x05);
	LCD_WRITE_DATA(0x0B);
	LCD_WRITE_DATA(0x09);
	LCD_WRITE_DATA(0x33);
	LCD_WRITE_DATA(0x39);
	LCD_WRITE_DATA(0x0F);

	LCD_WRITE_COMMAND(0x11);
	sunxi_lcd_delay_ms(ILI9341_SLEEP_OUT_DELAY_MS);
	LCD_WRITE_COMMAND(0x29);
	/*Display on*/
	LCD_WRITE_COMMAND(0x2C);
}

static s32 lcd_user_defined_func(u32 sel, u32 para1, u32 para2, u32 para3)
{
	return 0;
}

struct __lcd_panel ili9341_panel = {
    /* panel driver name, must mach the name of lcd_drv_name in sys_config.fex
       */
	.name = "ili9341",
	.func = {
		.cfg_panel_info = lcd_cfg_panel_info,
		.cfg_open_flow = lcd_open_flow,
		.cfg_close_flow = lcd_close_flow,
		.lcd_user_defined_func = lcd_user_defined_func,
		},
};
