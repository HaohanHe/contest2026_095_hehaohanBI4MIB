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

#include <sunxi_hal_common.h>
#include <hal_clk.h>
#include <hal_time.h>

#include "dsp_reg.h"

#define readl_dsp(addr)			readl((const volatile void*)(addr))
#define writel_dsp(val, addr)	writel((u32)(val), (volatile void*)(addr))

#define ROUND_DOWN(a, b) ((a) & ~((b)-1))
#define ROUND_UP(a,   b) (((a) + (b)-1) & ~((b)-1))

#define ROUND_DOWN_CACHE(a) ROUND_DOWN(a, CONFIG_SYS_CACHELINE_SIZE)
#define ROUND_UP_CACHE(a)   ROUND_UP(a, CONFIG_SYS_CACHELINE_SIZE)

static void sunxi_dsp_set_runstall(u32 value)
{
	u32 reg_val;

	/* DSP0 */
	reg_val = readl_dsp(DSP0_CFG_BASE + DSP_CTRL_REG0);
	reg_val &= ~(1 << BIT_RUN_STALL);
	reg_val |= (value << BIT_RUN_STALL);
	writel_dsp(reg_val, (DSP0_CFG_BASE + DSP_CTRL_REG0));
}

static void dsp_freq_default_set(void)
{
	u32 val = 0;

	val = DSP_CLK_SRC_PERI2X | DSP_CLK_FACTOR_M(2)
		| (1 << BIT_DSP_SCLK_GATING);
	writel_dsp(val, (SUNXI_CCU_BASE + CCMU_DSP_CLK_REG));
}

static void sram_remap_set(int value)
{
	u32 val = 0;

	val = readl_dsp(SUNXI_SRAMC_BASE + SRAMC_SRAM_REMAP_REG);
	val &= ~(1 << BIT_SRAM_REMAP_ENABLE);
	val |= (value << BIT_SRAM_REMAP_ENABLE);
	writel_dsp(val, SUNXI_SRAMC_BASE + SRAMC_SRAM_REMAP_REG);
}

int sunxi_dsp_start(uint32_t run_addr)
{
	u32 reg_val;

	/* set arm use local ram */
	sram_remap_set(1);

	dsp_freq_default_set();

	/* DSP0 */
	/* clock gating */
	reg_val = readl_dsp(SUNXI_CCU_BASE + CCMU_DSP_BGR_REG);
	reg_val |= (1 << BIT_DSP0_CFG_GATING);
	writel_dsp(reg_val, SUNXI_CCU_BASE + CCMU_DSP_BGR_REG);

	/* reset */
	reg_val = readl_dsp(SUNXI_CCU_BASE + CCMU_DSP_BGR_REG);
	reg_val |= (1 << BIT_DSP0_CFG_RST);
	reg_val |= (1 << BIT_DSP0_DBG_RST);
	writel_dsp(reg_val, SUNXI_CCU_BASE + CCMU_DSP_BGR_REG);

	/* set external Reset Vector*/
	writel_dsp(run_addr, DSP0_CFG_BASE + DSP_ALT_RESET_VEC_REG);

	reg_val = readl_dsp(DSP0_CFG_BASE + DSP_CTRL_REG0);
	reg_val |= (1 << BIT_START_VEC_SEL);
	writel_dsp(reg_val, DSP0_CFG_BASE + DSP_CTRL_REG0);

	/* set runstall */
	sunxi_dsp_set_runstall(1);

	/* set dsp clken */
	reg_val = readl_dsp(DSP0_CFG_BASE + DSP_CTRL_REG0);
	reg_val |= (1 << BIT_DSP_CLKEN);
	writel_dsp(reg_val, DSP0_CFG_BASE + DSP_CTRL_REG0);

	/* de-assert dsp0 */
	reg_val = readl_dsp(SUNXI_CCU_BASE + CCMU_DSP_BGR_REG);
	reg_val |= (1 << BIT_DSP0_RST);
	writel_dsp(reg_val, SUNXI_CCU_BASE + CCMU_DSP_BGR_REG);

	/* set dsp use local ram */
	sram_remap_set(0);

	/* clear runstall */
	sunxi_dsp_set_runstall(0);

	return 0;
}

int sunxi_dsp_stop(void)
{
	u32 reg_val;

	/* set runstall */
	sunxi_dsp_set_runstall(1);

	hal_msleep(5);

	/* set dsp clken */
	reg_val = readl_dsp(DSP0_CFG_BASE + DSP_CTRL_REG0);
	reg_val &= ~(1 << BIT_DSP_CLKEN);
	writel_dsp(reg_val, DSP0_CFG_BASE + DSP_CTRL_REG0);

	/* assert dsp0 */
	reg_val = readl_dsp(SUNXI_CCU_BASE + CCMU_DSP_BGR_REG);
	reg_val &= ~(1 << BIT_DSP0_RST);
	writel_dsp(reg_val, SUNXI_CCU_BASE + CCMU_DSP_BGR_REG);

	hal_msleep(5);

	/* clock gating */
	reg_val = readl_dsp(SUNXI_CCU_BASE + CCMU_DSP_BGR_REG);
	reg_val &= ~(1 << BIT_DSP0_CFG_GATING);
	writel_dsp(reg_val, SUNXI_CCU_BASE + CCMU_DSP_BGR_REG);

	/* reset */
	reg_val = readl_dsp(SUNXI_CCU_BASE + CCMU_DSP_BGR_REG);
	reg_val &= ~(1 << BIT_DSP0_CFG_RST);
	reg_val &= ~(1 << BIT_DSP0_DBG_RST);
	writel_dsp(reg_val, SUNXI_CCU_BASE + CCMU_DSP_BGR_REG);

	return 0;
}
