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

#ifndef _FB_G2D_ROT_H
#define _FB_G2D_ROT_H

#include "disp_sys_intf.h"
#include <g2d_driver.h>
#include "de/disp_features.h"
#include <nuttx/video/fb.h>
#include <video/sunxi_display2.h>
#include <hal_cache.h> 
#include "dev_disp.h"


enum {
	FB_ROTATION_HW_0 = 0,
	FB_ROTATION_HW_90 = 1,
	FB_ROTATION_HW_180 = 2,
	FB_ROTATION_HW_270 = 3,
};

struct fb_g2d_rot_t {
	g2d_blt_h info;
	void *dst_vir_addr;
	void *dst_phy_addr;
	unsigned char *rot_dst_overlay;
	unsigned int dst_overlay_size;
	unsigned char overlay_index;
	unsigned int dst_mem_len;
	struct fb_planeinfo_s *pinfo;
	struct fb_videoinfo_s *vinfo;
	int (*apply)(struct fb_g2d_rot_t *inst);
#ifdef CONFIG_FB_OVERLAY
	int (*apply_overlay)(struct fb_g2d_rot_t *inst, const struct fb_overlayinfo_s *oinfo);
	int (*realloc)(struct fb_g2d_rot_t *inst, const struct fb_overlayinfo_s *oinfo);
#endif
	int (*free)(struct fb_g2d_rot_t *inst);

};

struct fb_g2d_rot_t *fb_g2d_rot_create(struct fb_videoinfo_s *vinfo,
						struct fb_planeinfo_s *pinfo,
						unsigned int fb_id,
						struct disp_layer_config *config);
#endif /*End of file*/
