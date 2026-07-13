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

#ifndef __DE_HAL_H__
#define __DE_HAL_H__
#include "de_rtmx.h"
#include "de_scaler.h"
#include "de_csc.h"
#if defined(CONFIG_EINK_PANEL_USED)
#include "de_eink.h"
#include "de_wb_type.h"
#endif

extern int de_al_mgr_apply(unsigned int screen_id,
			   struct disp_manager_data *data);
extern int de_al_mgr_apply_color(unsigned int screen_id,
			   struct disp_csc_config *cfg);
extern int de_al_init(struct disp_bsp_init_para *para);
extern int de_al_exit(void);
extern int de_al_lyr_apply(unsigned int screen_id,
			   struct disp_layer_config_data *data,
			   unsigned int layer_num, bool direct_show);
extern int de_al_mgr_sync(unsigned int screen_id);
extern int de_al_mgr_update_regs(unsigned int screen_id);
extern int de_al_query_irq(unsigned int screen_id);
extern int de_al_enable_irq(unsigned int screen_id, unsigned int en);
int de_update_device_fps(unsigned int sel, u32 fps);
int de_update_clk_rate(u32 rate);
int de_get_clk_rate(void);

extern int de_enhance_set_size(unsigned int screen_id, struct disp_rect *size);
extern int de_enhance_set_format(unsigned int screen_id, unsigned int format);

#endif
