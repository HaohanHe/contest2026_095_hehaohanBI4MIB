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

#ifndef _G2D_OVL_H
#define _G2D_OVL_H
#include "g2d_rcq.h"
#include "g2d_mixer_type.h"
#include "g2d_mixer.h"

struct g2d_mixer_frame;
struct ovl_v_submodule {
	struct g2d_reg_block *reg_blks;
	__u32 reg_blk_num;
	struct g2d_reg_mem_info *reg_info;
	__s32 (*destory)(struct ovl_v_submodule *p_ovl_v);
	__s32 (*apply)(struct ovl_v_submodule *p_ovl_v, g2d_image_enh *p_image);
	int (*rcq_setup)(struct ovl_v_submodule *p_ovl_v, u8 *base,
			   struct g2d_rcq_mem_info *p_rcq_info);
	__u32 (*get_reg_block_num)(struct ovl_v_submodule *p_ovl_v);
	__u32 (*get_rcq_mem_size)(struct ovl_v_submodule *p_ovl_v);
	__s32 (*get_reg_block)(struct ovl_v_submodule *p_ovl_v, struct g2d_reg_block **blks);
	struct g2d_mixer_ovl_v_reg  *(*get_reg)(struct ovl_v_submodule *p_ovl_v);
	void (*set_block_dirty)(struct ovl_v_submodule *p_ovl_v, __u32 blk_id, __u32 dirty);
};
__s32 g2d_ovl_v_fc_set(struct ovl_v_submodule *p_ovl_v, __u32 color_value);
__s32 g2d_vlayer_set(struct ovl_v_submodule *p_ovl_v, __u32 sel,
		     g2d_image_enh *p_image, u8 evasion_flag);

__s32 g2d_vlayer_overlay_set(struct ovl_v_submodule *p_ovl_v, __u32 sel,
					g2d_coor *coor,  __u32 w, __u32 h);
struct ovl_v_submodule *
g2d_ovl_v_submodule_setup(struct g2d_mixer_frame *p_frame);
__s32 g2d_ovl_v_calc_coarse(struct ovl_v_submodule *p_ovl_v, __u32 format, __u32 inw,
		      __u32 inh, __u32 outw, __u32 outh, __u32 *midw,
		      __u32 *midh);

#endif /*End of file*/
