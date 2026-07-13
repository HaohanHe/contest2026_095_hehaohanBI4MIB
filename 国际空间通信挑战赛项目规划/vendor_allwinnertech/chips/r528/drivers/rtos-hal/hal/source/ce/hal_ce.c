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
#include <stdio.h>
#include <hal_mem.h>
#include <hal_osal.h>
#include <sunxi_hal_ce.h>
#include <hal_log.h>

#include "ce_common.h"
#include "ce_reg.h"
#include "platform_ce.h"

#ifdef CONFIG_ARCH_SUN20IW2
#include <hal_reset.h>
static int sunxi_ce_clk_init(bool enable)
{
    hal_clk_status_t ret;
    hal_reset_type_t reset_type = HAL_SUNXI_RESET;
    u32  reset_id;
    hal_clk_type_t clk_type = HAL_SUNXI_CCU;
    hal_clk_id_t clk_id;
    hal_clk_t clk;
    struct reset_control *reset;

    clk_id = SUNXI_CLK_CE;
    reset_id = SUNXI_RST_CE;
    if (enable)
    {
        reset = hal_reset_control_get(reset_type, reset_id);
        hal_reset_control_deassert(reset);
        hal_reset_control_put(reset);

        hal_clock_enable(hal_clock_get(clk_type, SUNXI_CLK_MBUS_CE));
        clk = hal_clock_get(clk_type, clk_id);
        ret = hal_clock_enable(clk);
        if (ret != HAL_CLK_STATUS_OK)
            CE_ERR("CE clock enable failed.\n");
    }
    else
    {
        clk = hal_clock_get(clk_type, clk_id);
        ret = hal_clock_disable(clk);
        if (ret != HAL_CLK_STATUS_OK)
            CE_ERR("CE clock disable failed.\n");
        hal_clock_disable(hal_clock_get(clk_type, SUNXI_CLK_MBUS_CE));
        hal_clock_put(clk);
    }

    return ret;
}
#endif

void hal_ce_clock_init(void)
{
#ifdef CONFIG_ARCH_SUN20IW2
	sunxi_ce_clk_init(1);
#else
	uint32_t  reg_val;

	/* close ce clk */
	reg_val = readl(MBUS_MAT_CLK_GATING_REG);
	reg_val &= ~(1 << 2);
	writel(reg_val, MBUS_MAT_CLK_GATING_REG);

	reg_val = readl(CCMU_CE_BGR_REG);
	reg_val &= ~(CE_DEASSERT << CE_RST_BIT);
	writel(reg_val, CCMU_CE_BGR_REG);

	reg_val = readl(CCMU_CE_BGR_REG);
	reg_val &= ~(CE_GATING_PASS << CE_GATING_BIT);
	writel(reg_val, CCMU_CE_BGR_REG);

	reg_val = readl(CCMU_CE_CLK_REG);
	reg_val &= ~(CE_SCLK_ON << CE_SCLK_ONOFF_BIT);
	reg_val &= ~(CE_CLK_SRC << CE_CLK_SRC_SEL_BIT);
	reg_val |= CE_CLK_SRC_MASK << CE_CLK_SRC_SEL_BIT;
	writel(reg_val, CCMU_CE_CLK_REG);

	/*set div n*/
	reg_val &= ~(CE_CLK_DIV_RATION_N_MASK << CE_CLK_DIV_RATION_N_BIT);
	reg_val |= CE_CLK_DIV_RATION_N << CE_CLK_DIV_RATION_N_BIT;

	/*set div m*/
	reg_val &= ~(CE_CLK_DIV_RATION_M_MASK << CE_CLK_DIV_RATION_M_BIT);
	reg_val |= CE_CLK_DIV_RATION_M << CE_CLK_DIV_RATION_M_BIT;

	writel(reg_val, CCMU_CE_CLK_REG);

	/*set CE src clock*/
	reg_val &= ~(CE_CLK_SRC_MASK << CE_CLK_SRC_SEL_BIT);

	/* PLL_PERI0(2X) */
	reg_val |= CE_CLK_SRC << CE_CLK_SRC_SEL_BIT;

	/*set src clock on*/
	reg_val |= CE_SCLK_ON << CE_SCLK_ONOFF_BIT;

	writel(reg_val, CCMU_CE_CLK_REG);

	/*open CE gating*/
	reg_val = readl(CCMU_CE_BGR_REG);
	reg_val |= CE_GATING_PASS << CE_GATING_BIT;
	writel(reg_val, CCMU_CE_BGR_REG);

	/*de-assert*/
	reg_val = readl(CCMU_CE_BGR_REG);
	reg_val |= CE_DEASSERT << CE_RST_BIT;
	writel(reg_val, CCMU_CE_BGR_REG);

	/*set mbus clock gating*/
	reg_val = readl(MBUS_MAT_CLK_GATING_REG);
	reg_val |= 1 << 2;
	writel(reg_val, MBUS_MAT_CLK_GATING_REG);
#endif
}


uint32_t ce_readl(uint32_t offset)
{
#if defined(CONFIG_X4B_AP) || defined(CONFIG_X4B_FACTEST)
	return readl(CE_NS_BASE_REG + offset);
#else
	return readl(CE_S_BASE_REG + offset);
#endif
}

static void ce_writel(uint32_t offset, uint32_t val)
{
#if defined(CONFIG_X4B_AP) || defined(CONFIG_X4B_FACTEST)
	writel(val, CE_NS_BASE_REG + offset);
#else
	writel(val, CE_S_BASE_REG + offset);
#endif
}

uint32_t hal_ce_reg_rd(uint32_t offset)
{
	return ce_readl(offset);
}

void hal_ce_reg_wr(uint32_t offset, uint32_t val)
{
	ce_writel(offset, val);
}

void hal_ce_keyselect_set(int select, ce_task_desc_t *task)
{
	task->sym_ctl |= select << CE_SYM_CTL_KEY_SELECT_SHIFT;
}

void hal_ce_keysize_set(int size, ce_task_desc_t *task)
{
	volatile int type = CE_AES_KEY_SIZE_128;

	switch (size) {
	case AES_KEYSIZE_16:
		type = CE_AES_KEY_SIZE_128;
		break;
	case AES_KEYSIZE_24:
		type = CE_AES_KEY_SIZE_192;
		break;
	case AES_KEYSIZE_32:
		type = CE_AES_KEY_SIZE_256;
		break;
	default:
		break;
	}

	task->sym_ctl |= (type << CE_SYM_CTL_KEY_SIZE_SHIFT);
}
#ifdef CE_SUPPORT_CE_V3_1
void hal_ce_rsa_width_set(int size, ce_task_desc_t *task)
{
	int width_type = 0;

	switch (size) {
	case 512:
		width_type = CE_RSA_PUB_MODULUS_WIDTH_512;
		break;
	case 1024:
		width_type = CE_RSA_PUB_MODULUS_WIDTH_1024;
		break;
	case 2048:
		width_type = CE_RSA_PUB_MODULUS_WIDTH_2048;
		break;
	case 3072:
		width_type = CE_RSA_PUB_MODULUS_WIDTH_3072;
		break;
	case 4096:
		width_type = CE_RSA_PUB_MODULUS_WIDTH_4096;
		break;
	default:
		break;
	}

	task->asym_ctl |= width_type << CE_ASYM_CTL_RSA_PM_WIDTH_SHIFT;
}
#endif

/* key: phsical address. */
void hal_ce_key_set(char *key, int size, ce_task_desc_t *task)
{
	int i = 0;
	int key_sel = CE_KEY_SELECT_INPUT;
	struct {
		int type;
		char desc[AES_MIN_KEY_SIZE];
	} keys[] = {
		{CE_KEY_SELECT_SSK,			CE_KS_SSK},
		{CE_KEY_SELECT_HUK,			CE_KS_HUK},
		{CE_KEY_SELECT_RSSK,		CE_KS_RSSK},
		{CE_KEY_SELECT_INTERNAL_0, CE_KS_INTERNAL_0},
		{CE_KEY_SELECT_INTERNAL_1, CE_KS_INTERNAL_1},
		{CE_KEY_SELECT_INTERNAL_2, CE_KS_INTERNAL_2},
		{CE_KEY_SELECT_INTERNAL_3, CE_KS_INTERNAL_3},
		{CE_KEY_SELECT_INTERNAL_4, CE_KS_INTERNAL_4},
		{CE_KEY_SELECT_INTERNAL_5, CE_KS_INTERNAL_5},
		{CE_KEY_SELECT_INTERNAL_6, CE_KS_INTERNAL_6},
		{CE_KEY_SELECT_INTERNAL_7, CE_KS_INTERNAL_7},
		{CE_KEY_SELECT_INPUT, ""} };

	while (keys[i].type != CE_KEY_SELECT_INPUT) {
		if (strncasecmp(key, keys[i].desc, AES_MIN_KEY_SIZE) == 0) {
			key_sel = keys[i].type;
			memset(key, 0, size);
			break;
		}
		i++;
	}
	CE_DBG("The key size: %d\n", size);

	hal_ce_keyselect_set(key_sel, task);
	hal_ce_keysize_set(size, task);
	task->key_addr = (uint32_t)__va_to_pa((intptr_t)key);
}

void hal_ce_pending_clear(int flow)
{
	int val = CE_CHAN_PENDING << flow;

	ce_writel(CE_REG_ISR, val);

	asm volatile("DSB");
	asm volatile("ISB");
    while ((ce_readl(CE_REG_ISR) & val) != 0) {
		hal_udelay(10);
		ce_writel(CE_REG_ISR, val);
		asm volatile("DSB");
		asm volatile("ISB");
        hal_udelay(1);
	}
}

int hal_ce_pending_get(void)
{
	return ce_readl(CE_REG_ISR);
}

void hal_ce_irq_enable(int flow)
{
	int val = ce_readl(CE_REG_ICR);

	val |= CE_CHAN_INT_ENABLE << flow;
	ce_writel(CE_REG_ICR, val);
}

void hal_ce_irq_disable(int flow)
{
	int val = ce_readl(CE_REG_ICR);

	val &= ~(CE_CHAN_INT_ENABLE << flow);
	ce_writel(CE_REG_ICR, val);
}

void hal_ce_md_get(char *dst, char *src, int size)
{
	memcpy(dst, src, size);
}


void hal_ce_iv_set(char *iv, int size, ce_task_desc_t *task)
{
	task->iv_addr = (uint32_t)__va_to_pa((intptr_t)iv);
}

void hal_ce_iv_mode_set(int mode, ce_task_desc_t *task)
{
	task->comm_ctl |= mode << CE_COMM_CTL_IV_MODE_SHIFT;
}

void hal_ce_cntsize_set(int size, ce_task_desc_t *task)
{
	task->sym_ctl |= size << CE_SYM_CTL_CTR_SIZE_SHIFT;
}

void hal_ce_cnt_set(char *cnt, int size, ce_task_desc_t *task)
{
	task->ctr_addr = (uint32_t)__va_to_pa((intptr_t)cnt);
	hal_ce_cntsize_set(CE_CTR_SIZE_128, task);
}

void hal_ce_cts_last(ce_task_desc_t *task)
{
	task->sym_ctl |= CE_SYM_CTL_AES_CTS_LAST;
}

#ifndef CE_SUPPORT_CE_V3_1

void hal_ce_xts_first(ce_task_desc_t *task)
{
	task->sym_ctl |= CE_SYM_CTL_AES_XTS_FIRST;
}

void hal_ce_xts_last(ce_task_desc_t *task)
{
	task->sym_ctl |= CE_SYM_CTL_AES_XTS_LAST;
}

#endif


void hal_ce_method_set(int dir, int type, ce_task_desc_t *task)
{
	if (dir != 0)
		task->comm_ctl |= 1 << CE_COMM_CTL_OP_DIR_SHIFT;
	task->comm_ctl |= type << CE_COMM_CTL_METHOD_SHIFT;
}

void hal_ce_des_mode_set(int mode, ce_task_desc_t *task)
{
	task->sym_ctl |= mode << CE_SYM_CTL_OP_MODE_SHIFT;
}

void hal_ce_aes_mode_set(int mode, ce_task_desc_t *task)
{
	task->sym_ctl |= mode << CE_SYM_CTL_OP_MODE_SHIFT;
}

void hal_ce_task_enable(ce_task_desc_t *task)
{
	task->comm_ctl |= CE_COMM_CTL_TASK_INT_MASK;
}

void hal_ce_reg_printf(void)
{
	syslog(LOG_INFO, "The ce control register:\n");
	syslog(LOG_INFO, "[TSK] = 0x%08lx\n", (unsigned long)ce_readl(CE_REG_TSK));
#ifdef CE_SUPPORT_CE_V3_1
	syslog(LOG_INFO, "[CTL] = 0x%08lx\n", (unsigned long)ce_readl(CE_REG_CTL));
#endif
	syslog(LOG_INFO, "[ICR] = 0x%08lx\n", (unsigned long)ce_readl(CE_REG_ICR));
	syslog(LOG_INFO, "[ISR] = 0x%08lx\n", (unsigned long)ce_readl(CE_REG_ISR));
	syslog(LOG_INFO, "[TLR] = 0x%08lx\n", (unsigned long)ce_readl(CE_REG_TLR));
	syslog(LOG_INFO, "[TSR] = 0x%08lx\n", (unsigned long)ce_readl(CE_REG_TSR));
	syslog(LOG_INFO, "[ERR] = 0x%08lx\n", (unsigned long)ce_readl(CE_REG_ERR));
	syslog(LOG_INFO, "[CSA] = 0x%08lx\n", (unsigned long)ce_readl(CE_REG_CSA));
	syslog(LOG_INFO, "[CDA] = 0x%08lx\n", (unsigned long)ce_readl(CE_REG_CDA));
	syslog(LOG_INFO, "[VER] = 0x%08lx\n", (unsigned long)ce_readl(CE_REG_VER));

}

void hal_ce_cfb_bitwidth_set(int bitwidth, ce_task_desc_t *task)
{
	int val = 0;

	switch (bitwidth) {
	case 1:
		val = CE_CFB_WIDTH_1;
		break;
	case 8:
		val = CE_CFB_WIDTH_8;
		break;
	case 64:
		val = CE_CFB_WIDTH_64;
		break;
	case 128:
		val = CE_CFB_WIDTH_128;
		break;
	default:
		break;
	}
	task->sym_ctl |= val << CE_SYM_CTL_CFB_WIDTH_SHIFT;
}

void hal_ce_set_task(uint32_t task_addr)
{
	int count = 0;
	ce_writel(CE_REG_TSK, __va_to_pa((uint32_t)task_addr));
	asm volatile("DSB");
	asm volatile("ISB");
	while (ce_readl(CE_REG_TSK) != task_addr) {
		hal_udelay(10);
		count++;
		ce_writel(CE_REG_TSK, __va_to_pa((uint32_t)task_addr));
		asm volatile("DSB");
		asm volatile("ISB");
		hal_udelay(1);
		if (count >= 1000) {
            break;
		}
	}
}

void hal_ce_ctrl_start(void)
{
	uint32_t val = ce_readl(CE_REG_TLR);
	val = val | (0x1 << 0);
	ce_writel(CE_REG_TLR, val);
	asm volatile("DSB");
	asm volatile("ISB");
}

int hal_ce_flow_err(int flow)
{
	return ce_readl(CE_REG_ERR) & CE_REG_ESR_CHAN_MASK(flow);
}


void hal_ce_data_len_set(int len, ce_task_desc_t *task)
{
#ifdef CE_SUPPORT_CE_V3_1
	task->data_len = (len >> 2);
#else
	task->data_len = len;
#endif
}

int hal_ce_wait_finish(uint32_t flow, uint32_t task_addr)
{
	uint32_t int_en ,i = 0, value;
	int_en = ce_readl(CE_REG_ICR) & 0xf;
	int_en = int_en & (0x01 << flow);
	hal_mdelay(1);
	if (int_en != 0) {
		while ((ce_readl(CE_REG_ISR) & (0x01 << flow)) == 0) {
			i++;
			if (i % 10000 == 0) {
				value = ce_readl(CE_REG_TSR);
				if (value != flow) {
					//syslog(LOG_INFO, "ce task status value before: 0x%lx, chanid:%ld\n", value, flow);
					ce_writel(CE_REG_TLR, 0x1);
					asm volatile("DSB");
					asm volatile("ISB");
					//syslog(LOG_INFO, "ce task status value after: 0x%lx\n", value);
				}
			}
			if (i % 1000000 == 0) {
				syslog(LOG_INFO, "CE wait time out, reset ce\n");
				hal_ce_reg_printf();
				hal_ce_clock_init();
				hal_ce_set_task(task_addr);
				hal_ce_irq_enable(flow);
				hal_ce_ctrl_start();
				asm volatile("DSB");
				asm volatile("ISB");
			}
			if (i % 10000000 == 0) {
				syslog(LOG_INFO, "CE wait time out, return\n");
				hal_ce_reg_printf();
				return -1;
			}
		}
	}
	return 0;
}

uint32_t hal_ce_get_erro(uint32_t flow)
{
	return (ce_readl(CE_REG_ERR) & (0xf << flow * 4));
}

