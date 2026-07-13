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

u32 g_irq_no[] = {
	122,/*tcon-lcd0*/
	123,/*tcon-tv*/
	124/*dsi*/
};

u32 g_reg_base[] = {
	0x05000000,/*de0*/
	0x05460000,/*disp_if_top*/
	0x05461000,/*tcon_lcd0*/
	0x05470000,/*tcon_tv*/
	0x05450000,/*dsi0*/
};

struct clk_info_t g_clk_no[] = {
	{
		"clk_de0",
		CLK_DE0,
		CLK_PLL_PERIPH0_2X,
		RST_BUS_DE0,
		NULL,
		NULL,
		NULL,
	},
	{
		"clk_bus_de0",
		CLK_BUS_DE0,
		(hal_clk_id_t)-1,
		(hal_reset_id_t)-1,
		NULL,
		NULL,
		NULL,
	},
	{
		"clk_de1",
		CLK_DE0,
		CLK_PLL_PERIPH0_2X,
		RST_BUS_DE0,
		NULL,
		NULL,
		NULL,
	},
	{
		"clk_bus_de1",
		CLK_BUS_DE0,
		(hal_clk_id_t)-1,
		(hal_reset_id_t)-1,
		NULL,
		NULL,
		NULL,
	},
	{
		"clk_bus_dpss_top0",
		CLK_BUS_DPSS_TOP0,
		(hal_clk_id_t)(-1),/*NULL for clk_parent*/
		RST_BUS_DPSS_TOP0,
		NULL,
		NULL,
		NULL,
	},
	{
		"clk_bus_dpss_top1",
		CLK_BUS_DPSS_TOP0,
		(hal_clk_id_t)(-1),/*NULL for clk_parent*/
		RST_BUS_DPSS_TOP0,
		NULL,
		NULL,
		NULL,
	},
	{
		"clk_tcon0",
		CLK_TCON_LCD0,
		CLK_PLL_VIDEO0_4X,
		RST_BUS_TCON_LCD0,
		NULL,
		NULL,
		NULL,
	},
	{
		"clk_tcon1",
		CLK_TCON_TV,
		CLK_PLL_VIDEO1,
		RST_BUS_TCON_TV,
		NULL,
		NULL,
		NULL,
	},
	{
		"clk_bus_tcon0",
		CLK_BUS_TCON_LCD0,
		(hal_clk_id_t)-1,
		(hal_reset_id_t)-1,
		NULL,
		NULL,
		NULL,
	},
	{
		"clk_bus_tcon1",
		CLK_BUS_TCON_TV,
		(hal_clk_id_t)-1,
		(hal_reset_id_t)-1,
		NULL,
		NULL,
		NULL,
	},
	{
		"clk_mipi_dsi0",
		CLK_MIPI_DSI,
		CLK_PLL_PERIPH0,
		RST_BUS_MIPI_DSI,
		NULL,
		NULL,
		NULL,
	},
	{
		"clk_bus_mipi_dsi0",
		CLK_BUS_MIPI_DSI,
		(hal_clk_id_t)-1,
		(hal_reset_id_t)-1,
		NULL,
		NULL,
		NULL,
	},
	{
		"rst_bus_lvds0",
		(hal_clk_id_t)-1,
		(hal_clk_id_t)-1,
		RST_BUS_LVDS0,
		NULL,
		NULL,
		NULL,
	},
};

u32 g_irq_no_len = sizeof(g_irq_no) / sizeof(u32);
u32 g_reg_base_len = sizeof(g_reg_base) / sizeof(u32);
u32 g_clk_no_len = sizeof(g_clk_no) / sizeof(struct clk_info_t);
