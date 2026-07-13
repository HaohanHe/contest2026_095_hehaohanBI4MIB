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

#ifndef __DISP_DISPLAY_H__
#define __DISP_DISPLAY_H__
#include "include.h"
#include "disp_private.h"
#include "disp_tv.h"

#ifdef CONFIG_KERNEL_FREERTOS
#include "FreeRTOS.h"

/* -------------------------------- jiffies -----------------------------*/
#define HZ 1000
#define jiffies ((unsigned long)xTaskGetTickCount())
#endif

struct disp_screen_t {
	bool have_cfg_reg;
	u32 cache_flag;
	u32 cfg_cnt;
	struct disp_health_info health_info;
#if SUPPORT_WORKQUEUE == 1
	hal_work tasklet;
#endif
	bool vsync_event_en;
	bool dvi_enable;
	u32 flag_lock;
};

struct disp_dev_t {
	struct disp_bsp_init_para init_para;	/* para from driver */
	struct disp_screen_t screen[3];
	u32 print_level;
	u32 lcd_registered[3];
	u32 hdmi_registered;
	u32 tv_registered;
	u32 vdpo_registered;
	u32 edp_registered;
};
extern struct disp_dev_t gdisp;

s32 disp_init_connections(struct disp_bsp_init_para *para);
s32 bsp_disp_shadow_protect(u32 disp, bool protect);
void sync_event_proc(u32 disp, bool timeout);
s32 disp_device_attached(int disp_mgr, int disp_dev,
			struct disp_device_config *config);

s32 disp_device_attached_and_enable(int disp_mgr, int disp_dev,
				    struct disp_device_config *config);
s32 disp_device_detach(int disp_mgr, int disp_dev,
		       enum disp_output_type output_type);
void LCD_OPEN_FUNC(u32 screen_id, LCD_FUNC func, u32 delay);
void LCD_CLOSE_FUNC(u32 screen_id, LCD_FUNC func, u32 delay);
s32 bsp_disp_sync_with_hw(struct disp_bsp_init_para *para);

/*s32 bsp_disp_check_device_enabled(struct disp_bsp_init_para *para);*/
s32 bsp_disp_get_fps(u32 disp);
s32 bsp_disp_get_health_info(u32 disp, struct disp_health_info *info);

s32 bsp_disp_init(struct disp_bsp_init_para *para);
s32 bsp_disp_exit(u32 mode);
s32 bsp_disp_open(void);
s32 bsp_disp_close(void);
s32 bsp_disp_feat_get_num_screens(void);
s32 bsp_disp_feat_get_num_channels(u32 disp);
s32 bsp_disp_feat_get_num_layers(u32 screen_id);
s32 bsp_disp_feat_get_num_layers_by_chn(u32 disp, u32 chn);
s32 bsp_disp_feat_is_supported_output_types(u32 screen_id, u32 output_type);
s32 bsp_disp_get_screen_physical_width(u32 disp);
s32 bsp_disp_get_screen_physical_height(u32 disp);
s32 bsp_disp_get_screen_width(u32 disp);
s32 bsp_disp_get_screen_height(u32 disp);
s32 bsp_disp_get_screen_width_from_output_type(u32 disp, u32 output_type,
					       u32 output_mode);
s32 bsp_disp_get_screen_height_from_output_type(u32 disp, u32 output_type,
						u32 output_mode);
s32 bsp_disp_get_lcd_registered(u32 disp);
s32 bsp_disp_get_hdmi_registered(void);
s32 bsp_disp_get_tv_registered(void);

s32 bsp_disp_get_output_type(u32 disp);
s32 bsp_disp_device_switch(int disp, enum disp_output_type output_type,
			   enum disp_output_type mode);
s32 bsp_disp_device_set_config(int disp, struct disp_device_config *config);
s32 bsp_disp_set_hdmi_func(struct disp_device_func *func);
s32 bsp_disp_set_vdpo_func(struct disp_tv_func *func);
s32 bsp_disp_hdmi_check_support_mode(u32 disp, enum disp_output_type mode);
s32 bsp_disp_hdmi_set_detect(bool hpd);
s32 bsp_disp_hdmi_cec_standby_request(void);
s32 bsp_disp_hdmi_cec_send_one_touch_play(void);
s32 bsp_disp_tv_register(struct disp_tv_func *func);
s32 bsp_disp_tv_set_hpd(u32 state);

/* lcd */
s32 bsp_disp_lcd_set_panel_funs(char *name, struct disp_lcd_panel_fun *lcd_cfg);
s32 bsp_disp_lcd_backlight_enable(u32 disp);
s32 bsp_disp_lcd_backlight_disable(u32 disp);
s32 bsp_disp_lcd_pwm_enable(u32 disp);
s32 bsp_disp_lcd_pwm_disable(u32 disp);
s32 bsp_disp_lcd_power_enable(u32 disp, u32 power_id);
s32 bsp_disp_lcd_power_disable(u32 disp, u32 power_id);
s32 bsp_disp_lcd_set_bright(u32 disp, u32 bright);
s32 bsp_disp_lcd_get_bright(u32 disp);
void bsp_disp_lcd_tcon_enable(u32 disp);
void bsp_disp_lcd_tcon_disable(u32 disp);
s32 bsp_disp_lcd_pin_cfg(u32 disp, u32 en);
s32 bsp_disp_lcd_gpio_set_value(u32 disp, u32 io_index, u32 value);
s32 bsp_disp_lcd_gpio_set_direction(u32 disp, u32 io_index,
				    u32 direction);
struct disp_lcd_flow *bsp_disp_lcd_get_open_flow(u32 disp);
struct disp_lcd_flow *bsp_disp_lcd_get_close_flow(u32 disp);
s32 bsp_disp_get_panel_info(u32 disp, struct disp_panel_para *info);
s32 bsp_disp_lcd_switch_compat_panel(u32 disp, u32 index);

s32 bsp_disp_vsync_event_enable(u32 disp, bool enable);
s32 bsp_disp_tv_suspend(void);
s32 bsp_disp_tv_resume(void);

int bsp_disp_get_fb_info(unsigned int disp, struct disp_layer_info *info);
int bsp_disp_get_display_size(u32 disp, unsigned int *width,
			      unsigned int *height);

s32 bsp_disp_lcd_dsi_clk_enable(u32 disp, u32 en);
s32 bsp_disp_lcd_dsi_dcs_wr(u32 disp, u8 command, u8 *para, u32 para_num);
s32 bsp_disp_lcd_dsi_gen_wr(u32 disp, u8 command, u8 *para, u32 para_num);
s32 bsp_disp_set_edp_func(struct disp_tv_func *func);


#endif
