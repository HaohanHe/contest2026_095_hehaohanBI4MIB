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

#ifndef _G2D_BLD_H
#define _G2D_BLD_H
#include "g2d_rcq.h"
#include "g2d_mixer_type.h"
#include "g2d_mixer.h"

struct g2d_mixer_frame;
struct blender_submodule {
	struct g2d_reg_block *reg_blks;
	__u32 reg_blk_num;
	struct g2d_reg_mem_info *reg_info;
	__s32 (*destory)(struct blender_submodule *p_bld);
	int (*rcq_setup)(struct blender_submodule *p_bld, u8 *base,
		  struct g2d_rcq_mem_info *p_rcq_info);
	__u32 (*get_reg_block_num)(struct blender_submodule *p_bld);
	__u32 (*get_rcq_mem_size)(struct blender_submodule *p_bld);
	__s32 (*get_reg_block)(struct blender_submodule *p_bld, struct g2d_reg_block **blks);
	struct g2d_mixer_bld_reg  *(*get_reg)(struct blender_submodule *p_bld);
	void (*set_block_dirty)(struct blender_submodule *p_bld, __u32 blk_id, __u32 dirty);
};

__s32 bld_in_set(struct blender_submodule *p_bld, __u32 sel, g2d_rect rect,
		 int premul);
__s32 bld_ck_para_set(struct blender_submodule *p_bld, g2d_ck *para,
		      __u32 flag);
__s32 bld_bk_set(struct blender_submodule *p_bld, __u32 color);
__s32 bld_out_setting(struct blender_submodule *p_bld, g2d_image_enh *p_image);
__s32 bld_cs_set(struct blender_submodule *p_bld, __u32 format);
__s32 bld_csc_reg_set(struct blender_submodule *p_bld, __u32 csc_no,
		      g2d_csc_sel csc_sel);
__s32 bld_rop3_set(struct blender_submodule *p_bld, __u32 sel, __u32 rop3_cmd);
__s32 bld_set_rop_ctrl(struct blender_submodule *p_bld, __u32 value);
__s32 bld_rop2_set(struct blender_submodule *p_bld, __u32 rop_cmd);
struct blender_submodule *
g2d_bld_submodule_setup(struct g2d_mixer_frame *p_frame);
__s32 bld_porter_duff(struct blender_submodule *p_bld, __u32 cmd);
__s32 bld_fc_set(struct blender_submodule *p_bld, __u32 sel, g2d_rect rect,
		    int premul, __u32 color);
#endif /*End of file*/
