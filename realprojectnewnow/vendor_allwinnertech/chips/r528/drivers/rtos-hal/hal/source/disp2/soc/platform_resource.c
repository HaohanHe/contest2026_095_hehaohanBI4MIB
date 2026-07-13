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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <hal_clk.h>
#include "platform_resource.h"

extern u32 g_irq_no[];
extern u32 g_reg_base[];
extern struct clk_info_t g_clk_no[];
extern u32 g_irq_no_len;
extern u32 g_reg_base_len;
extern u32 g_clk_no_len;

s32 plat_get_reg_base(u32 index, u32 *data)
{
	if (index >= g_reg_base_len || !data)
		return -1;
	*data = g_reg_base[index];
	return 0;
}

s32 plat_get_irq_no(u32 index, u32 *data)
{
	if (index >= g_irq_no_len || !data)
		return -1;
	*data = g_irq_no[index];
	return 0;
}

#ifndef CONFIG_ARCH_SUN8IW19
s32 plat_get_clk(char *name, hal_clk_id_t *data)
{
	static int init_get_clk = 0;
	u32 i = 0;
	*data = (hal_clk_id_t)-1;
	if (!init_get_clk) {
		for (i = 0; i < g_clk_no_len; ++i) {
			g_clk_no[i].clk = hal_clock_get(HAL_SUNXI_CCU, g_clk_no[i].clk_id);
#if defined(CONFIG_ARCH_SUN20IW2)
			g_clk_no[i].clk_parent = hal_clock_get(HAL_SUNXI_AON_CCU, g_clk_no[i].clk_parent_id);
#else
			g_clk_no[i].clk_parent = hal_clock_get(HAL_SUNXI_CCU, g_clk_no[i].clk_parent_id);
#endif
			g_clk_no[i].rst = hal_reset_control_get(HAL_SUNXI_RESET, g_clk_no[i].rst_id);
		}
		init_get_clk = 1;
	}
	for (i = 0; i < g_clk_no_len; ++i) {
		if(!strcmp(name, g_clk_no[i].name))
			break;
	}
	if (i >= g_clk_no_len || !data)
		return -1;
	*data = g_clk_no[i].clk_id;
	return 0;
}
#else
s32 plat_get_clk(u32 index, hal_clk_id_t *data)
{
	if (index >= g_clk_no_len || !data)
		return -1;
	*data = g_clk_no[index].clk_id;
	return 0;
}
#endif

s32 plat_get_clk_parent(hal_clk_id_t clk, hal_clk_id_t *parent)
{
	u32 i = 0;
	*parent = (hal_clk_id_t)-1;
	for (i = 0; i < g_clk_no_len; ++i) {
		if (g_clk_no[i].clk_id == clk) {
#if defined(CONFIG_ARCH_SUN20IW2)
			*parent = g_clk_no[i].clk_parent_id + 200;
#else
			*parent = g_clk_no[i].clk_parent_id;
#endif
			break;
		}
	}

	return (i == g_clk_no_len) ? -1 : 0;
}

s32 plat_get_clk_from_id(hal_clk_id_t clk_id, hal_clk_t *clk, struct reset_control **rst)
{
	u32 i = 0;
	*rst = NULL;
#ifndef CONFIG_ARCH_SUN8IW19
	for (i = 0; i < g_clk_no_len; ++i) {
		if (g_clk_no[i].clk_id == clk_id) {
			*clk = g_clk_no[i].clk;
			*rst = g_clk_no[i].rst;
			break;
#if defined(CONFIG_ARCH_SUN20IW2)
		} else if ((g_clk_no[i].clk_parent_id + 200) == clk_id) {
#else
		} else if (g_clk_no[i].clk_parent_id == clk_id) {
#endif
			*clk = g_clk_no[i].clk_parent;
		}
	}
#else
	*clk = clk_id;
#endif

	return (i == g_clk_no_len) ? -1 : 0;

}

s32 plat_get_rst_by_name(char *name, struct reset_control **rst)
{
	int i;

	for (i = 0; i < g_clk_no_len; ++i) {
		if(!strcmp(name, g_clk_no[i].name)) {
			*rst = g_clk_no[i].rst;
			return 0;
		}
	}

	return -1;
}

