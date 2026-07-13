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


#include <stdlib.h>
#include <string.h>
#ifndef _G2D_RCQ_H
#define _G2D_RCQ_H

#include "g2d_bsp.h"

/*32 byte align required by rcq*/
#define G2D_RCQ_BYTE_ALIGN(x) (((x + (32 - 1)) >> 5) << 5)
/* 2 align */
#define G2D_RCQ_HEADER_ALIGN(x) (((x + 1) >> 1) << 1)


#define G2D_MIXER_RCQ_USED 1

union rcq_hd_dw0 {
	__u32 dwval;
	struct {
		__u32 len:24;
		__u32 high_addr:8;
	} bits;
};

union rcq_hd_dirty {
	__u32 dwval;
	struct {
		__u32 dirty:1;
		__u32 res0:15;
		__u32 n_header_len : 16; /*next frame header length*/
	} bits;
};

struct g2d_rcq_head {
	__u32 low_addr; /* 32 bytes align */
	union rcq_hd_dw0 dw0;
	union rcq_hd_dirty dirty;
	__u32 reg_offset; /* offset_addr based on g2d_reg_base */
};

/*
* @phy_addr: must be 32 bytes align, can not be accessed by cpu.
* @vir_addr: for cpu access.
* @size: unit: byte. must be 2 bytes align.
* @reg_addr: reg base addr of this block.
* @dirty: this block need be updated to hw reg if @dirty is true.
* @rcq_hd: pointer to rcq head of this dma_reg block at rcq mode.
* @block_id: unique id for current block
*/
struct g2d_reg_block {
	u8 *phy_addr;
	u8 *vir_addr;
	__u32 size;
	u8  *reg_addr;
	__u32 dirty;
	struct g2d_rcq_head *rcq_hd;
	__u32 block_id;
};

struct g2d_reg_mem_info {
	u8 *phy_addr; /* it is non-null at rcq mode */
	u8 *vir_addr;
	__u32 size;
};

struct g2d_rcq_mem_info {
	u8 *phy_addr;
	struct g2d_rcq_head *vir_addr;
	struct g2d_reg_block **reg_blk;
	__u32 alloc_num;
	__u32 cur_num;
	__u32 block_num_per_frame;
	__u32 alloc_num_per_frame;
	__u32 rcq_header_len;
	__u32 rcq_byte_used;
	__u32 rcq_reg_mem_size;
};
__s32 g2d_top_mem_pool_alloc(struct g2d_rcq_mem_info *p_rcq_info);
void *g2d_top_reg_memory_alloc(__u32 size, void *phy_addr,
			       struct g2d_rcq_mem_info *p_rcq_info);
void g2d_top_mem_pool_free(struct g2d_rcq_mem_info *p_rcq_info);

#endif /*End of file*/
