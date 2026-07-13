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


#include "g2d_rcq.h"
#include "g2d_driver_i.h"

__s32 g2d_top_mem_pool_alloc(struct g2d_rcq_mem_info *p_rcq_info)
{
	int ret = 0;

	p_rcq_info->rcq_byte_used =
	    p_rcq_info->alloc_num * sizeof(*(p_rcq_info->vir_addr));
	p_rcq_info->vir_addr = g2d_malloc(p_rcq_info->rcq_reg_mem_size,
					  (__u32 *)&p_rcq_info->phy_addr);
	if (!p_rcq_info->vir_addr)
		ret = -1;

	return ret;
}

void *g2d_top_reg_memory_alloc(__u32 size, void *phy_addr,
			       struct g2d_rcq_mem_info *p_rcq_info)
{
	void *viraddr = NULL;
	if (p_rcq_info->vir_addr) {

		*(__u32 *)phy_addr = (unsigned long)p_rcq_info->phy_addr +
					  p_rcq_info->rcq_byte_used;

		viraddr =
		    (void *)p_rcq_info->vir_addr + p_rcq_info->rcq_byte_used;

		p_rcq_info->rcq_byte_used += G2D_RCQ_BYTE_ALIGN(size);

		if (p_rcq_info->rcq_byte_used > p_rcq_info->rcq_reg_mem_size) {
			G2D_ERR_MSG("Malloc %ld byte fail, out of total "
				    "memory %ld bytes, current used byte:%ld\n",
				    G2D_RCQ_BYTE_ALIGN(size),
				    p_rcq_info->rcq_reg_mem_size,
				    p_rcq_info->rcq_byte_used);
			viraddr = NULL;
			*(__u32 *)phy_addr = (unsigned long)NULL;
		}
		return viraddr;
	} else {
		G2D_ERR_MSG("Null pointer!\n");
		*(__u32 *)phy_addr = (unsigned long)NULL;
		return NULL;
	}
}

void g2d_top_mem_pool_free(struct g2d_rcq_mem_info *p_rcq_info)
{
	if (p_rcq_info->vir_addr) {
		g2d_free((void *)p_rcq_info->vir_addr,
			 (void *)p_rcq_info->phy_addr,
			 p_rcq_info->rcq_reg_mem_size);
	}
}
