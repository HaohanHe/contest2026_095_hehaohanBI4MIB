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

#ifndef _PLATFORM_RESOURCE_H
#define _PLATFORM_RESOURCE_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <hal_clk.h>
#include <hal_reset.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * clk_info
 */
struct clk_info_t {
	char name[20];
	hal_clk_id_t clk_id;
	hal_clk_id_t clk_parent_id;
	hal_reset_id_t rst_id;
	hal_clk_t clk;
	hal_clk_t clk_parent;
	struct reset_control *rst;
};

s32 plat_get_reg_base(u32 index, u32 *data);

s32 plat_get_irq_no(u32 index, u32 *data);

#ifndef CONFIG_ARCH_SUN8IW19
s32 plat_get_clk(char *name, hal_clk_id_t *data);
#else
s32 plat_get_clk(u32 index, hal_clk_id_t *data);
#endif

s32 plat_get_clk_parent(hal_clk_id_t clk, hal_clk_id_t * parent);

s32 plat_get_clk_from_id(hal_clk_id_t clk_id, hal_clk_t *clk, struct reset_control **rst);

s32 plat_get_rst_by_name(char *name, struct reset_control **rst);
#ifdef __cplusplus
}
#endif

#endif /*End of file*/
