#include "t070s140b.h"
#include <string.h>
#include <syslog.h>

extern s32 bsp_disp_lcd_set_bright(u32 disp, u32 bright);

static void lcd_power_on(u32 sel);
static void lcd_power_off(u32 sel);
static void lcd_bl_open(u32 sel);
static void lcd_bl_close(u32 sel);
static void lcd_panel_init(u32 sel);
static void lcd_panel_exit(u32 sel);

// EK79007AD2复位控制 (GRB信号)
#define panel_reset(sel, val) sunxi_lcd_gpio_set_value(sel, 0, val)

// EK79007AD2寄存器配置结构
struct ek79007ad2_config {
    u8 address_mode;     // R36h地址模式
    u8 power_enable;     // RB0h电源使能
    u8 frame_inversion;  // RB3h帧反转
    u8 gamma_values[7];  // R80h~R86h Gamma校正值
    u8 lane_config;      // RB2设置Lane数量
    u8 res_config;       // RB1设置屏幕分辨率
};

// 获取EK79007AD2芯片级配置的函数
static u32 get_ek79007ad2_config(u32 sel, struct ek79007ad2_config *config)
{
    struct disp_panel_para panel_info;
    config->address_mode = 0x01;    // SHLR=1右移模式
    config->power_enable = 0x01;    // PWR_EN=1
    config->frame_inversion = 0x00; // FRAME=0默认帧反转

    config->gamma_values[0] = 0xAC; // R80h
    config->gamma_values[1] = 0xB8; // R81h
    config->gamma_values[2] = 0x09; // R82h
    config->gamma_values[3] = 0x78; // R83h
    config->gamma_values[4] = 0x7F; // R84h
    config->gamma_values[5] = 0xBB; // R85h
    config->gamma_values[6] = 0x70; // R86h

    if (bsp_disp_get_panel_info(sel, &panel_info) != 0) {
        syslog(LOG_INFO, "fisker get panel_info fail\n");
        return 1;
    }
    // 根据配置文件中的分辨率设置RES位
    if (panel_info.lcd_x == 1024 && panel_info.lcd_y == 600) {
        config->res_config = 0x30; // RES[2:1]=00 :1024x600,RES[5] = 1 : H-FRC enable,RES[4] = 1 :Enable internal dithering function
    }
    else{
        syslog(LOG_INFO, "fisker lcd_x and lcd_y set wrong\n");
        return 1;
    }
    if(panel_info.lcd_dsi_lane == 4){
        config->lane_config = 0x30;        // RB2h: En_3lane=1, En_2lane=1 (4-lane), NBW=0 (normally white)
        syslog(LOG_INFO, "fisker T070S140B: Setting 4-lane MIPI (En_3lane=1, En_2lane=1)\n");
    }
    else{
        syslog(LOG_INFO, "fisker lane number set wrong\n");
        return 1;
    }
return 0;
}

static void lcd_cfg_panel_info(struct panel_extend_para *info){

    syslog(LOG_INFO, "fisker T070S140B: Configuring panel info...\n");

    if (!info) {
        syslog(LOG_ERR, "fisker T070S140B: ERROR - panel info pointer is NULL!\n");
        return;
    }

    u32 i = 0, j = 0;
    u32 items;

    u8 lcd_gamma_tbl[][2] = {
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

/* EK79007AD2颜色映射表 (基于R36h地址模式配置)*/
    u32 lcd_cmap_tbl[2][3][4] = {
        {   // SHLR=1 (右移模式)
            {LCD_CMAP_G0, LCD_CMAP_B1, LCD_CMAP_G2, LCD_CMAP_B3},
            {LCD_CMAP_B0, LCD_CMAP_R1, LCD_CMAP_B2, LCD_CMAP_R3},
            {LCD_CMAP_R0, LCD_CMAP_G1, LCD_CMAP_R2, LCD_CMAP_G3},
        },
        {   // SHLR=0 (左移模式)
            {LCD_CMAP_B3, LCD_CMAP_G2, LCD_CMAP_B1, LCD_CMAP_G0},
            {LCD_CMAP_R3, LCD_CMAP_B2, LCD_CMAP_R1, LCD_CMAP_B0},
            {LCD_CMAP_G3, LCD_CMAP_R2, LCD_CMAP_G1, LCD_CMAP_R0},
        },
    };
    memset(info, 0, sizeof(struct panel_extend_para));

    // 生成完整的Gamma校正表
    items = sizeof(lcd_gamma_tbl) / 2;
    for (i = 0; i < items - 1; i++) {
        u32 num = lcd_gamma_tbl[i + 1][0] - lcd_gamma_tbl[i][0];

        for (j = 0; j < num; j++) {
            u32 value = 0;

            value = lcd_gamma_tbl[i][1] +
                ((lcd_gamma_tbl[i + 1][1] - lcd_gamma_tbl[i][1]) * j) / num;
            info->lcd_gamma_tbl[lcd_gamma_tbl[i][0] + j] =
                (value << 16) + (value << 8) + value;
        }
    }

    info->lcd_gamma_tbl[255] = (lcd_gamma_tbl[items - 1][1] << 16) +
                               (lcd_gamma_tbl[items - 1][1] << 8) +
                               lcd_gamma_tbl[items - 1][1];

    // 设置颜色映射表
    memcpy(info->lcd_cmap_tbl, lcd_cmap_tbl, sizeof(lcd_cmap_tbl));

    syslog(LOG_INFO, "fisker T070S140B: Panel configuration completed successfully\n");
}

/*开机时序配置*/
static s32 lcd_open_flow(u32 sel){
    syslog(LOG_INFO, "fisker T070S140B: Starting LCD open flow, sel=%lu\n", sel);

    LCD_OPEN_FUNC(sel, lcd_power_on, 10);
    LCD_OPEN_FUNC(sel, lcd_panel_init, 10);
    LCD_OPEN_FUNC(sel, sunxi_lcd_tcon_enable, 50);
    LCD_OPEN_FUNC(sel, lcd_bl_open, 0);

    return 0;
}

/*关机时序配置*/
static s32 lcd_close_flow(u32 sel)
{
    syslog(LOG_INFO, "fisker T070S140B: Starting LCD close flow, sel=%lu\n", sel);

    LCD_CLOSE_FUNC(sel, lcd_bl_close, 0);
    LCD_CLOSE_FUNC(sel, sunxi_lcd_tcon_disable, 0);
    LCD_CLOSE_FUNC(sel, lcd_panel_exit, 200);
    LCD_CLOSE_FUNC(sel, lcd_power_off, 500);

    return 0;
}
//上电时序配置
static void lcd_power_on(u32 sel)
{
    panel_reset(sel, GPIO_DATA_LOW);
    sunxi_lcd_power_enable(sel, AXP2101_ID_DCDC1);
    sunxi_lcd_power_enable(sel, AXP2101_ID_DCDC3);
    sunxi_lcd_delay_ms(20);
    sunxi_lcd_pin_cfg(sel, 1);
    sunxi_lcd_delay_ms(50);
    panel_reset(sel, GPIO_DATA_HIGH);
    sunxi_lcd_delay_ms(20);
    syslog(LOG_INFO, "fisker T070S140B: Step 3 - Release RESET (HIGH - active state)\n");
    panel_reset(sel, GPIO_DATA_LOW);
    sunxi_lcd_delay_ms(30);

    panel_reset(sel, GPIO_DATA_HIGH);
    sunxi_lcd_delay_ms(30);
    syslog(LOG_INFO, "fisker T070S140B: Step 5 - Enabling AVDD (3.3V - after reset release)\n");
    sunxi_lcd_power_enable(sel, AXP2101_ID_DCDC2); // power_id=2 对应配置中的 lcd_power2
    sunxi_lcd_delay_ms(20);
    syslog(LOG_INFO, "fisker T070S140B: Step 6 - Enabling DSI clock\n");
    sunxi_lcd_dsi_clk_enable(sel);
    syslog(LOG_INFO, "fisker T070S140B: Power-on sequence completed\n");
}

/*下电函数(上电的逆序)*/
static void lcd_power_off(u32 sel)
{
    syslog(LOG_INFO, "fisker T070S140B: Power off sequence started (reverse of power-on), sel=%lu\n", sel);
    sunxi_lcd_dsi_clk_disable(sel);
    sunxi_lcd_delay_ms(20);
    sunxi_lcd_power_disable(sel, AXP2101_ID_DCDC2);
    sunxi_lcd_delay_ms(20);
    panel_reset(sel, GPIO_DATA_LOW);
    sunxi_lcd_delay_ms(20);

    sunxi_lcd_pin_cfg(sel,0);
    sunxi_lcd_delay_ms(20);

    sunxi_lcd_power_disable(sel, AXP2101_ID_DCDC3); // 关闭VDD_IF (1.8V接口电源)
    sunxi_lcd_power_disable(sel, AXP2101_ID_DCDC1); // 关闭VDD (1.8V数字电源)
}

/*开启背光*/
static void lcd_bl_open(u32 sel)
{
    syslog(LOG_INFO, "fisker T070S140B: Enabling PWM backlight control (ch=4, freq=10KHz, pol=1)\n");
    sunxi_lcd_pwm_enable(sel);
    sunxi_lcd_delay_ms(100);

    syslog(LOG_INFO, "fisker T070S140B: Enabling backlight power (LED+)\n");
    sunxi_lcd_backlight_enable(sel);
    sunxi_lcd_delay_ms(100);
}

/*关闭背光*/
static void lcd_bl_close(u32 sel)
{
    syslog(LOG_INFO, "fisker T070S140B: Backlight off, sel=%lu\n", sel);

    sunxi_lcd_backlight_disable(sel);
    sunxi_lcd_pwm_disable(sel);
    sunxi_lcd_delay_ms(200);
}

static void lcd_panel_init(u32 sel)
{
    struct ek79007ad2_config chip_config;
    // 获取EK79007AD2芯片级配置
    if(get_ek79007ad2_config(sel, &chip_config) != 0) {
        syslog(LOG_ERR, "fisker T070S140B: Failed to get chip config!\n");
        return;
    }

    // R01h - 软件复位
    syslog(LOG_INFO, "fisker T070S140B: Sending software reset command (0x01)\n");
    sunxi_lcd_dsi_dcs_write_0para(sel, DSI_DCS_SOFT_RESET);
    sunxi_lcd_delay_ms(120);

    // R11h - EXIT_SLEEP_MODE
    syslog(LOG_INFO, "fisker T070S140B: Sending EXIT_SLEEP_MODE (0x11) - vendor mandatory\n");
    printf("[FISKER T070S140B] Sending EXIT_SLEEP_MODE command (0x11)\n");
    sunxi_lcd_dsi_dcs_write_0para(sel, DSI_DCS_EXIT_SLEEP_MODE);

    sunxi_lcd_delay_ms(30);
      sunxi_lcd_dsi_gen_write_1para(sel, 0xB0, chip_config.power_enable);
    sunxi_lcd_delay_ms(10);
        sunxi_lcd_dsi_gen_write_1para(sel, 0xB1, chip_config.res_config);
    sunxi_lcd_delay_ms(10);

    // RB2h - Lane控制
    syslog(LOG_INFO, "fisker T070S140B: Setting Lane config (0xB2) = 0x30 (4-lane MIPI)\n");
    sunxi_lcd_dsi_gen_write_1para(sel, 0xB2, 0x30);
    sunxi_lcd_delay_ms(10);

    // Gamma校正电压设置 (R80h~R86h)
    syslog(LOG_INFO, "fisker T070S140B: Setting Gamma correction registers (vendor mandatory)\n");
    printf("[FISKER T070S140B] Setting Gamma correction registers\n");
    sunxi_lcd_dsi_gen_write_1para(sel, 0x80, 0xAC);
    sunxi_lcd_dsi_gen_write_1para(sel, 0x81, 0xB8);
    sunxi_lcd_dsi_gen_write_1para(sel, 0x82, 0x09);
    sunxi_lcd_dsi_gen_write_1para(sel, 0x83, 0x78);
    sunxi_lcd_dsi_gen_write_1para(sel, 0x84, 0x7F);
    sunxi_lcd_dsi_gen_write_1para(sel, 0x85, 0xBB);
    sunxi_lcd_dsi_gen_write_1para(sel, 0x86, 0x70);
    // R29h - Display On
    sunxi_lcd_dsi_dcs_write_0para(sel, DSI_DCS_SET_DISPLAY_ON);
    sunxi_lcd_delay_ms(200);
}

static void lcd_panel_exit(u32 sel)
{
    sunxi_lcd_dsi_dcs_write_0para(sel, DSI_DCS_ENTER_SLEEP_MODE); // Sleep In
    sunxi_lcd_delay_ms(80);
    sunxi_lcd_dsi_dcs_write_0para(sel, DSI_DCS_SET_DISPLAY_OFF); // Display Off
    sunxi_lcd_delay_ms(50);
}

/* 用户自定义函数*/
static s32 lcd_user_defined_func(u32 sel, u32 para1, u32 para2, u32 para3)
{
    bsp_disp_lcd_set_bright(sel,60);
    return 0;
}
struct __lcd_panel t070s140b_panel = {
    .name = "t070s140b",
    .func = {
        .cfg_panel_info =  lcd_cfg_panel_info,
        .cfg_open_flow  =  lcd_open_flow,
        .cfg_close_flow =  lcd_close_flow,
        .lcd_user_defined_func = lcd_user_defined_func,
    },
};


