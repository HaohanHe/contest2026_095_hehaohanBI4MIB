/*
 * Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
 *
 * Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
 * the the people's Republic of China and other countries.
 * All Allwinner Technology Co.,Ltd. trademarks are used with permission.
 *
 * DISCLAIMER
 * THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
 * IF YOU NEED TO INTEGRATE THIRD PARTY’S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
 * IN ALLWINNERS’SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
 * ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
 * ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
 * COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
 * YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY’S TECHNOLOGY.
 *
 *
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
#include <debug.h>
#include "sunxi_timer.h"
#include "platform_timer.h"
#include "aw_common.h"
#include <hal_clk.h>
#include <stdlib.h>
#include <hal_interrupt.h>
#define TIMER_NUM 2
static struct sunxi_timer g_timer[SUNXI_TMR_NUM];

static hal_irqreturn_t sunxi_timer_irq_handle(void *dev)
{
	struct sunxi_timer *timer = (struct sunxi_timer *)dev;

	/* clear pending */
	if(timer->timer_id >= TIMER_NUM){
		hal_writel((0x1 << (timer->timer_id - TIMER_NUM)), (unsigned long)R_TIMER_IRQ_ST_REG);
	}else{
		hal_writel((0x1 << timer->timer_id), (unsigned long)TIMER_IRQ_ST_REG);
	}
	/*callback*/
	if (timer->callback != NULL)
	{
		timer->callback(timer->param);
	}

	return 0;
}

static void sunxi_timer_sync(uint32_t timer)
{
	if(timer >= TIMER_NUM){
		uint32_t old = hal_readl((unsigned long)R_TIMER_CNTVAL_REG(timer-TIMER_NUM));

		while ((old - hal_readl((unsigned long)R_TIMER_CNTVAL_REG(timer-TIMER_NUM))) < TIMER_SYNC_TICKS)
		{
			int i = 10;
			while (i--);
			break;
		}
	}else{
		uint32_t old = hal_readl((unsigned long)TIMER_CNTVAL_REG(timer));

		while ((old - hal_readl((unsigned long)TIMER_CNTVAL_REG(timer))) < TIMER_SYNC_TICKS)
		{
			int i = 10;
			while (i--);
			break;
		}
	}
}

void sunxi_timer_stop(uint32_t timer)
{
	if(timer >= TIMER_NUM){
		uint32_t val = hal_readl((unsigned long)R_TIMER_CTL_REG(timer-TIMER_NUM));

		hal_writel(val & ~R_TIMER_CTL_ENABLE, (unsigned long)R_TIMER_CTL_REG(timer-TIMER_NUM));

		sunxi_timer_sync(timer);
	}else{
		uint32_t val = hal_readl((unsigned long)TIMER_CTL_REG(timer));

		hal_writel(val & ~TIMER_CTL_ENABLE, (unsigned long)TIMER_CTL_REG(timer));

		sunxi_timer_sync(timer);
	}
}

void sunxi_timer_start(uint32_t timer, bool periodic)
{
	uint32_t val = 0;
	if(timer >= TIMER_NUM){
		if (!periodic)
			val |= R_TIMER_CTL_ONESHOT;

		val |= R_TIMER_CTL_CLK_PRES(0);           //24M
		val &= ~R_TIMER_CTL_CLK_SRC(0x3);
		val |= R_TIMER_CTL_CLK_SRC(R_TIMER_CTL_CLK_SRC_OSC24M);

		hal_writel(val | R_TIMER_CTL_RELOAD, (unsigned long)R_TIMER_CTL_REG(timer-TIMER_NUM));

		// wating reload bit turns to 0
		while (hal_readl(R_TIMER_CTL_REG(timer-TIMER_NUM)) >> 1 & 1);

		hal_writel(hal_readl(R_TIMER_CTL_REG(timer-TIMER_NUM)) | R_TIMER_CTL_ENABLE, R_TIMER_CTL_REG(timer-TIMER_NUM));
	}else{
		if (!periodic)
			val |= TIMER_CTL_ONESHOT;

		val |= TIMER_CTL_CLK_PRES(0);           //24M
		val &= ~TIMER_CTL_CLK_SRC(0x3);
		val |= TIMER_CTL_CLK_SRC(TIMER_CTL_CLK_SRC_OSC24M);

		hal_writel(val | TIMER_CTL_RELOAD, (unsigned long)TIMER_CTL_REG(timer));

		// wating reload bit turns to 0
		while (hal_readl(TIMER_CTL_REG(timer)) >> 1 & 1);

		hal_writel(hal_readl(TIMER_CTL_REG(timer)) | TIMER_CTL_ENABLE, TIMER_CTL_REG(timer));
	}
}

static void sunxi_timer_setup(uint32_t tick, uint32_t timer)
{
	if(timer >= TIMER_NUM){
		hal_writel(tick, (unsigned long)R_TIMER_INTVAL_REG(timer-TIMER_NUM));
	}else{
		hal_writel(tick, (unsigned long)TIMER_INTVAL_REG(timer));
	}
}

int sunxi_timer_set_oneshot(uint32_t delay_us, uint32_t timer, timer_callback callback, void *callback_param)
{
	uint32_t tick = delay_us * 24;

	if (tick < g_timer[timer].min_delta_ticks)
		tick = g_timer[timer].min_delta_ticks;
	if (tick > g_timer[timer].max_delta_ticks)
	{
		return -1;
	}

	if (callback != NULL)
	{
		g_timer[timer].callback = callback;
		g_timer[timer].param = callback_param;
	}

	sunxi_timer_stop(timer);

	sunxi_timer_setup(tick, timer);

	sunxi_timer_start(timer, false);

	return 0;
}

int sunxi_timer_set_periodic(uint32_t delay_us, uint32_t timer, timer_callback callback, void *callback_param)
{
	uint32_t tick = delay_us * 24;

	if (tick < g_timer[timer].min_delta_ticks || tick > g_timer[timer].max_delta_ticks)
	{
		return -1;
	}

	if (callback != NULL)
	{
		g_timer[timer].callback = callback;
		g_timer[timer].param = callback_param;
	}

	sunxi_timer_stop(timer);

	sunxi_timer_setup(tick, timer);

	sunxi_timer_start(timer, true);

	return 0;
}

void sunxi_timer_get_status(uint32_t timer, sunxi_timer_status *status)
{
	uint32_t reg_val;
	if(timer >= TIMER_NUM){
		reg_val = hal_readl((unsigned long)R_TIMER_CTL_REG(timer-TIMER_NUM));
		status->flags = (reg_val & (0x1 << 7)) >> 7;

		reg_val = hal_readl((unsigned long)R_TIMER_INTVAL_REG(timer-TIMER_NUM));
		status->int_us = reg_val / 24;

		reg_val = hal_readl((unsigned long)R_TIMER_CNTVAL_REG(timer-TIMER_NUM));
		status->left_us = reg_val / 24;
	}else{
		reg_val = hal_readl((unsigned long)TIMER_CTL_REG(timer));
		status->flags = (reg_val & (0x1 << 7)) >> 7;

		reg_val = hal_readl((unsigned long)TIMER_INTVAL_REG(timer));
		status->int_us = reg_val / 24;

		reg_val = hal_readl((unsigned long)TIMER_CNTVAL_REG(timer));
		status->left_us = reg_val / 24;
	}
    return;
}

#ifdef CONFIG_COMPONENTS_PM
static u32 regs_addr[] = {
	TIMER_IRQ_EN_REG,
	TIMER_CTL_REG(0),
	TIMER_INTVAL_REG(0),
	TIMER_CNTVAL_REG(0),
	TIMER_CTL_REG(1),
	TIMER_INTVAL_REG(1),
	TIMER_CNTVAL_REG(1),
	R_TIMER_IRQ_EN_REG,
	R_TIMER_CTL_REG(0),
	R_TIMER_INTVAL_REG(0),
	R_TIMER_CNTVAL_REG(0),
	R_TIMER_CTL_REG(1),
	R_TIMER_INTVAL_REG(1),
	R_TIMER_CNTVAL_REG(1),
};
static u32 regs_backup[ARRAY_SIZE(regs_addr)];
static inline void save_regs(void)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(regs_addr); i++)
		regs_backup[i] = readl(regs_addr[i]);
}

static inline void restore_regs(void)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(regs_addr); i++)
		writel(regs_backup[i], regs_addr[i]);
}
static int hal_timer_suspend(struct pm_device *dev, suspend_mode_t mode)
{
	uint32_t timer = (uint32_t)((uintptr_t)dev->data);
	save_regs();
	sunxi_timer_stop(timer);
	tmrinfo("timer suspend end\n");
	return 0;
}

static int hal_timer_resume(struct pm_device *dev, suspend_mode_t mode)
{
	uint32_t timer = (uint32_t)((uintptr_t)dev->data);
	restore_regs();
	sunxi_timer_start(timer, 0);
	tmrinfo("timer resume end\n");
	return 0;
}

struct pm_devops pm_timer_ops = {
	.suspend = hal_timer_suspend,
	.resume = hal_timer_resume,
};
#endif

void sunxi_timer_irq_disable(hal_timer_id_t id)
{
	uint32_t val;
	if(id >= TIMER_NUM){
		val = hal_readl((unsigned long)R_TIMER_IRQ_EN_REG);
		hal_writel(val & (~R_TIMER_IRQ_EN(id-TIMER_NUM)), (unsigned long)R_TIMER_IRQ_EN_REG);
	}else{
		val = hal_readl((unsigned long)TIMER_IRQ_EN_REG);
		hal_writel(val & (~TIMER_IRQ_EN(id)), (unsigned long)TIMER_IRQ_EN_REG);
	}
}

void sunxi_timer_init(hal_timer_id_t id)
{
	uint32_t val;
	char name[32];
	if(id >= TIMER_NUM){
		static int r_timer_init_flag = 0;
		wdinfo("sunxi_timer_init:id=%d,r_timer_init_flag=%d\n",id,r_timer_init_flag);
		if(!r_timer_init_flag){
			r_timer_init_flag = 1;
			uint32_t r_timer_bus_reg_val = hal_readl(SUNXI_R_CCU_BASE+0x11c);
			/*enable R_TIMER BUS GATING*/
			hal_writel(0x0,SUNXI_R_CCU_BASE+0x11c);
			r_timer_bus_reg_val |= (1<<16);
			hal_writel(r_timer_bus_reg_val,SUNXI_R_CCU_BASE+0x11c);
			r_timer_bus_reg_val |= (1);
			hal_writel(r_timer_bus_reg_val,SUNXI_R_CCU_BASE+0x11c);
		}
		/* disable all rtimer */
		val = hal_readl((unsigned long)R_TIMER_CTL_REG(id-TIMER_NUM));
		hal_writel(val & ~R_TIMER_CTL_ENABLE, (unsigned long)R_TIMER_CTL_REG(id-TIMER_NUM));

		/* clear pending */
		hal_writel((0x1 << (id-TIMER_NUM)), (unsigned long)R_TIMER_IRQ_ST_REG);

		g_timer[id].timer_id = id;
		g_timer[id].clk_rate = 24000000;      //ahb1,should get form clk driver
#ifdef TIMER_APB1_CLK
		hal_clk_t ahb1 = hal_clock_get(TIMER_APB_CLK_TYPE, TIMER_APB1_CLK);
		if (ahb1)
			g_timer[id].clk_rate = hal_clk_get_rate(ahb1);
		hal_clock_put(ahb1);
#endif
		g_timer[id].irq = SUNXI_IRQ_R_TMR(id-TIMER_NUM);
		g_timer[id].min_delta_ticks = TIMER_SYNC_TICKS;
		g_timer[id].max_delta_ticks = 0xffffffff;
		g_timer[id].callback = NULL;
		g_timer[id].param = NULL;

		snprintf(name, 32, "r_timer%d", id-TIMER_NUM);

		if (hal_request_irq(g_timer[id].irq, sunxi_timer_irq_handle,
					name, &g_timer[id]) < 0)
		{
			return ;
		}

		/*enable r_timer irq*/
		val = hal_readl((unsigned long)R_TIMER_IRQ_EN_REG);
		val |= R_TIMER_IRQ_EN(id-TIMER_NUM);
		hal_writel(val, (unsigned long)R_TIMER_IRQ_EN_REG);
#if 0
#ifdef CONFIG_ARCH_HIPRI_INTERRUPT
		if (id == SUNXI_R_TMR0)
		{
			syslog(LOG_ERR,"set timer irq as fiq");
			up_secure_irq(g_timer[id].irq, true);
			/* Must make FIQ priority be larger than IRQ, because
			 * GICC_CTLR.AckCtl bit is set to 0.
			 */
			up_prioritize_irq(g_timer[id].irq, 0);
		}
#endif
#endif
		/* enable irq */
		hal_enable_irq(g_timer[id].irq);
	}else{
		/* disable all timer */
		val = hal_readl((unsigned long)TIMER_CTL_REG(id));
		hal_writel(val & ~TIMER_CTL_ENABLE, (unsigned long)TIMER_CTL_REG(id));

		/* clear pending */
		hal_writel((0x1 << id), (unsigned long)TIMER_IRQ_ST_REG);

		g_timer[id].timer_id = id;
		g_timer[id].clk_rate = 24000000;      //ahb1,should get form clk driver
#ifdef TIMER_APB1_CLK
		hal_clk_t ahb1 = hal_clock_get(TIMER_APB_CLK_TYPE, TIMER_APB1_CLK);
		if (ahb1)
			g_timer[id].clk_rate = hal_clk_get_rate(ahb1);
		hal_clock_put(ahb1);
#endif
		g_timer[id].irq = SUNXI_IRQ_TMR(id);
		g_timer[id].min_delta_ticks = TIMER_SYNC_TICKS;
		g_timer[id].max_delta_ticks = 0xffffffff;
		g_timer[id].callback = NULL;
		g_timer[id].param = NULL;

		snprintf(name, 32, "timer%d", id);

		if (hal_request_irq(g_timer[id].irq, sunxi_timer_irq_handle,
					name, &g_timer[id]) < 0)
		{
			return ;
		}

		/*enable timer irq*/
		val = hal_readl((unsigned long)TIMER_IRQ_EN_REG);
		val |= TIMER_IRQ_EN(id);
		hal_writel(val, (unsigned long)TIMER_IRQ_EN_REG);

		/* enable irq */
		hal_enable_irq(g_timer[id].irq);
	}
#ifdef CONFIG_COMPONENTS_PM
		g_timer[id].pm.data = (void *)id;
		g_timer[id].pm.name = "sunxi_timer";
		g_timer[id].pm.ops = &pm_timer_ops;
		pm_devops_register(&g_timer[id].pm);
#endif
}

void sunxi_timer_uninit(hal_timer_id_t id)
{
#ifdef CONFIG_COMPONENTS_PM
	pm_devops_unregister(&g_timer[id].pm);
#endif

	hal_disable_irq(g_timer[id].irq);
	hal_free_irq(g_timer[id].irq);
}

