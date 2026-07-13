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


#ifndef _DISP_PRIVATE_H_
#define _DISP_PRIVATE_H_
#include "include.h"
#include "disp_features.h"
#if defined(CONFIG_ARCH_SUN50IW1)
#include "./lowlevel_sun50iw1/disp_al.h"
#elif defined(CONFIG_ARCH_SUN50IW2)
#include "./lowlevel_v2x/disp_al.h"
#elif defined(CONFIG_ARCH_SUN50IW8)
#include "./lowlevel_v2x/disp_al.h"
#elif defined(CONFIG_ARCH_SUN8IW10)
#include "./lowlevel_sun8iw10/disp_al.h"
#elif defined(CONFIG_ARCH_SUN8IW11) || defined(CONFIG_ARCH_SUN20IW2)
#include "./lowlevel_v2x/disp_al.h"
#elif defined(CONFIG_ARCH_SUN8IW12) || defined(CONFIG_ARCH_SUN8IW16)\
    || defined(CONFIG_ARCH_SUN8IW19) || defined(CONFIG_ARCH_SUN8IW20)\
    || defined(CONFIG_ARCH_SUN20IW1)
#include "disp_tv.h"
#include "./lowlevel_v2x/disp_al.h"
#elif defined(CONFIG_ARCH_SUN8IW15)
#include "./lowlevel_v2x/disp_al.h"
#elif defined(CONFIG_ARCH_SUN50IW10)
#include "./lowlevel_v2x/disp_al.h"
#elif defined(CONFIG_ARCH_SUN8IW6)
#include "./lowlevel_v2x/disp_al.h"
#elif defined(CONFIG_ARCH_SUN8IW7) || defined(CONFIG_ARCH_SUN8IW17)
#include "./lowlevel_v2x/disp_al.h"
#elif defined(CONFIG_ARCH_SUN8IW8)
#include "./lowlevel_sun8iw8/disp_al.h"
#elif defined(CONFIG_ARCH_SUN50IW3) || defined(CONFIG_ARCH_SUN50IW6)
#include "./lowlevel_v3x/disp_al.h"
#else
#error "undefined platform!!!"
#endif


extern struct disp_device *disp_get_lcd(u32 disp);
struct disp_device *disp_get_direct_lcd_compat(u32 disp, u32 index);

extern struct disp_device *disp_get_hdmi(u32 disp);

extern struct disp_manager *disp_get_layer_manager(u32 disp);

extern struct disp_layer *disp_get_layer(u32 disp, u32 chn, u32 layer_id);
extern struct disp_layer *disp_get_layer_1(u32 disp, u32 layer_id);
extern struct disp_smbl *disp_get_smbl(u32 disp);
extern struct disp_enhance *disp_get_enhance(u32 disp);
extern struct disp_capture *disp_get_capture(u32 disp);

extern s32 disp_delay_ms(u32 ms);
extern s32 disp_delay_us(u32 us);
extern s32 disp_init_lcd(struct disp_bsp_init_para *para);
extern s32 disp_exit_lcd(void);
extern s32 disp_init_hdmi(struct disp_bsp_init_para *para);
extern s32 disp_exit_hdmi(void);
extern s32 disp_init_tv(void);	/* (struct disp_bsp_init_para * para); */
extern s32 disp_exit_tv(void);	/* (struct disp_bsp_init_para * para); */
extern s32 disp_exit_vdpo(void);
extern s32 disp_tv_set_func(struct disp_device *ptv, struct disp_tv_func *func);
extern s32 disp_init_tv_para(struct disp_bsp_init_para *para);
extern s32 disp_exit_tv_para(void);
extern s32 disp_tv_set_hpd(struct disp_device *ptv, u32 state);
extern s32 disp_init_vga(void);
extern s32 disp_exit_vga(void);
extern s32 disp_init_vdpo(struct disp_bsp_init_para *para);
extern s32 disp_init_edp(struct disp_bsp_init_para *para);

extern s32 disp_init_feat(void);
extern s32 disp_exit_feat(void);
extern s32 disp_init_mgr(struct disp_bsp_init_para *para);
extern s32 disp_exit_mgr(void);
extern s32 disp_init_enhance(struct disp_bsp_init_para *para);
extern s32 disp_exit_enhance(void);
extern s32 disp_init_smbl(struct disp_bsp_init_para *para);
extern s32 disp_exit_smbl(void);
extern s32 disp_init_capture(struct disp_bsp_init_para *para);
extern s32 disp_exit_capture(void);

#ifdef CONFIG_EINK_PANEL_USED
extern s32 disp_init_eink(struct disp_bsp_init_para *para);
extern s32 disp_exit_eink(void);
extern s32 write_edma(struct disp_eink_manager *manager);
extern s32 disp_init_format_convert_manager(struct disp_bsp_init_para *para);
extern void disp_exit_format_convert_manager(void);

extern struct disp_eink_manager *disp_get_eink_manager(unsigned int disp);
extern int eink_display_one_frame(struct disp_eink_manager *manager);
#endif
extern void sync_event_proc(u32 disp, bool timeout);

#include "disp_device.h"

u32 dump_layer_config(struct disp_layer_config_data *data);

extern s32 disp_init_rotation_sw(struct disp_bsp_init_para *para);
extern struct disp_rotation_sw *disp_get_rotation_sw(u32 disp);
#endif
