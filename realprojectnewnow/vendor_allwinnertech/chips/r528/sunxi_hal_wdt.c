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
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <hal_interrupt.h>
#include <debug.h>

#include <sunxi_hal_common.h>

#include "sunxi_hal_wdt.h"
#include "r528_dev_wdt.h"

#define DRV_NAME    "sunxi-drv-wdt"
#define DRV_VERSION "1.0"

#define WDT_IRQ_EN_FLAG         1
#define WDT_IRQ_PENDING_CLEAR   1
#define WDT_SOFT_RESET_FLAG     1
#define WDT_IRQ_NUM             95
#define WDT_MAX_TIMEOUT         16
#define WDT_MIN_TIMEOUT         1
#define WDT_TIMEOUT_MASK        0x0F

#define WDT_CTRL_RELOAD         ((1 << 0) | (0x0a57 << 1))
#define WDT_MODE_EN             (1 << 0)
#define SUNXI_WDT_DEBUG

#define USE_WDT_INTERRUPT_MODE  0
#define SUNXI_WDT_DEBUG
#ifdef CONFIG_OS_NUTTX
#ifdef SUNXI_WDT_DEBUG
#define WDT_DEBUG(format, ...)  wdinfo(format, ##__VA_ARGS__)
#else
#define WDT_DEBUG(format, ...)
#endif
#define WDT_INFO(format, ...) wdinfo(format, ##__VA_ARGS__)

#else

#ifdef SUNXI_WDT_DEBUG
#define WDT_DEBUG(format, ...)  wdinfo("SUNXI WDT:"format, ##__VA_ARGS__)
#else
#define WDT_DEBUG(format, ...)
#endif
#define WDT_INFO(format, ...) wdinfo("SUNXI WDT:"format, ##__VA_ARGS__)

#endif //CONFIG_OS_NUTTX
/*
 * wdt_timeout_map maps the watchdog timer interval value in seconds to
 * the value of the register WDT_MODE at bits .wdt_timeout_shift ~ +3
 *
 * [timeout seconds] = register value
 *
 */
static const int wdt_timeout_map[] = {
	[1] = 0x1,  /* 1s  */
	[2] = 0x2,  /* 2s  */
	[3] = 0x3,  /* 3s  */
	[4] = 0x4,  /* 4s  */
	[5] = 0x5,  /* 5s  */
	[6] = 0x6,  /* 6s  */
	[8] = 0x7,  /* 8s  */
	[10] = 0x8, /* 10s */
	[12] = 0x9, /* 12s */
	[14] = 0xA, /* 14s */
	[16] = 0xB, /* 16s */
};
#if USE_WDT_INTERRUPT_MODE
static void wdt_irq_enable(struct sunxi_wdt_dev_t *sunxi_wdt_dev){
	uint32_t reg;
	void *wdt_base = sunxi_wdt_dev->wdt_base;
	const struct sunxi_wdt_reg_t *regs = sunxi_wdt_dev->wdt_regs;
	reg = hal_readl(wdt_base + regs->wdt_irq_en);
	reg |= WDT_IRQ_EN_FLAG;
	WDT_INFO("wdt irq en reg:0x%lx\n",reg);
	hal_writel(reg, wdt_base + regs->wdt_irq_en);
}
#endif
static void wdt_irq_pending_clear(struct sunxi_wdt_dev_t *sunxi_wdt_dev){
	uint32_t reg;
	void *wdt_base = sunxi_wdt_dev->wdt_base;
	const struct sunxi_wdt_reg_t *regs = sunxi_wdt_dev->wdt_regs;
	reg = hal_readl(wdt_base + regs->wdt_status);
	reg |= WDT_IRQ_PENDING_CLEAR;
	WDT_INFO("wdt status reg:0x%lx\n",reg);
	hal_writel(reg, wdt_base + regs->wdt_status);
	WDT_INFO("wdt_irq_en:0x%lx\n",hal_readl(wdt_base+regs->wdt_irq_en));
	WDT_INFO("wdt_status:0x%lx\n",hal_readl(wdt_base+regs->wdt_status));
	WDT_INFO("wdt_soft_reset:0x%lx\n",hal_readl(wdt_base+regs->wdt_soft_reset));
	WDT_INFO("wdt_ctrl:0x%lx\n",hal_readl(wdt_base+regs->wdt_ctrl));
	WDT_INFO("wdt_cfg:0x%lx\n",hal_readl(wdt_base+regs->wdt_cfg));
	WDT_INFO("wdt_mode:0x%lx\n",hal_readl(wdt_base+regs->wdt_mode));
}
static void wdt_soft_reset(struct sunxi_wdt_dev_t *sunxi_wdt_dev){
	uint32_t reg;
	void *wdt_base = sunxi_wdt_dev->wdt_base;
	const struct sunxi_wdt_reg_t *regs = sunxi_wdt_dev->wdt_regs;

	/* set wdt mode:reset system */
	reg = hal_readl(wdt_base + regs->wdt_cfg);
	reg &= ~(regs->wdt_reset_mask);
	reg |= (regs->wdt_reset_mode | KEY_FIELD_MAGIC);
	hal_writel(reg, wdt_base + regs->wdt_cfg);

	/*disable wdt first*/
	unsigned int value = hal_readl(wdt_base + regs->wdt_mode);
	value &= ~WDT_MODE_EN;
	value |= KEY_FIELD_MAGIC;
	hal_writel(value, wdt_base + regs->wdt_mode);

	/*reset system*/
	reg = hal_readl(wdt_base + regs->wdt_soft_reset);
	reg |= (WDT_SOFT_RESET_FLAG | KEY_FIELD_MAGIC);
	WDT_INFO("wdt_irq_en:0x%lx\n",hal_readl(wdt_base+regs->wdt_irq_en));
	WDT_INFO("wdt_status:0x%lx\n",hal_readl(wdt_base+regs->wdt_status));
	WDT_INFO("wdt_soft_reset:0x%lx\n",hal_readl(wdt_base+regs->wdt_soft_reset));
	WDT_INFO("wdt_ctrl:0x%lx\n",hal_readl(wdt_base+regs->wdt_ctrl));
	WDT_INFO("wdt_cfg:0x%lx\n",hal_readl(wdt_base+regs->wdt_cfg));
	WDT_INFO("wdt_mode:0x%lx\n",hal_readl(wdt_base+regs->wdt_mode));
	hal_writel(reg, wdt_base + regs->wdt_soft_reset);
}
#if USE_WDT_INTERRUPT_MODE
static hal_irqreturn_t wdt_interupt_handler(void *dev){
	WDT_INFO("wdt_interupt_handler\n");
	struct sunxi_wdt_dev_t* sunxi_wdt_dev = (struct sunxi_wdt_dev_t *)dev;
	wdt_irq_pending_clear(sunxi_wdt_dev);
	PANIC();
	return 0;
}
#endif
int sunxi_wdt_get_info(struct sunxi_wdt_dev_t *sunxi_wdt_dev, struct sunxi_wdt_info_t **wdt_info)
{
	WDT_INFO("sunxi_wdt_get_info\n");
	*wdt_info = sunxi_wdt_dev->wdt_info;
	return 0;
}

int sunxi_wdt_restart(struct sunxi_wdt_dev_t *sunxi_wdt_dev)
{
	WDT_INFO("sunxi_wdt_restart\n");
	wdt_soft_reset(sunxi_wdt_dev);
	return 0;
}

int sunxi_wdt_ping(struct sunxi_wdt_dev_t *sunxi_wdt_dev)
{
	void *wdt_base = sunxi_wdt_dev->wdt_base;
	const struct sunxi_wdt_reg_t *regs = sunxi_wdt_dev->wdt_regs;
	hal_writel(WDT_CTRL_RELOAD, wdt_base + regs->wdt_ctrl);
	return 0;
}

int sunxi_wdt_set_timeout(struct sunxi_wdt_dev_t *sunxi_wdt_dev, unsigned int timeout)
{
	void *wdt_base = sunxi_wdt_dev->wdt_base;
	const struct sunxi_wdt_reg_t *regs = sunxi_wdt_dev->wdt_regs;
	uint32_t reg;

	WDT_INFO("sunxi_wdt_set_timeout:%u\n",timeout);
	if (wdt_timeout_map[timeout] == 0)
		timeout++;

	sunxi_wdt_dev->timeout = timeout;

	reg = hal_readl(wdt_base + regs->wdt_mode);
	reg &= ~(WDT_TIMEOUT_MASK << regs->wdt_timeout_shift);
	reg |= wdt_timeout_map[timeout] << regs->wdt_timeout_shift;
	hal_writel(reg | KEY_FIELD_MAGIC, wdt_base + regs->wdt_mode);

	sunxi_wdt_ping(sunxi_wdt_dev);

	return 0;
}

int sunxi_wdt_stop(struct sunxi_wdt_dev_t *sunxi_wdt_dev)
{
	void *wdt_base = sunxi_wdt_dev->wdt_base;
	const struct sunxi_wdt_reg_t *regs = sunxi_wdt_dev->wdt_regs;

	WDT_INFO("sunxi_wdt_stop\n");
	unsigned int value = hal_readl(wdt_base + regs->wdt_mode);
	value &= ~WDT_MODE_EN;
	value |= KEY_FIELD_MAGIC;
	hal_writel(value, wdt_base + regs->wdt_mode);

	return 0;
}

int sunxi_wdt_start(struct sunxi_wdt_dev_t *sunxi_wdt_dev)
{
	uint32_t reg;
	void *wdt_base = sunxi_wdt_dev->wdt_base;
	const struct sunxi_wdt_reg_t *regs = sunxi_wdt_dev->wdt_regs;
	int ret;

	WDT_INFO("sunxi_wdt_start\n");
	ret = sunxi_wdt_set_timeout(sunxi_wdt_dev, sunxi_wdt_dev->timeout);
	if (ret < 0)
		return ret;

	/* Set the default wdt mode:interrupt */
	reg = hal_readl(wdt_base + regs->wdt_cfg);
	reg &= ~(regs->wdt_reset_mask);
#if USE_WDT_INTERRUPT_MODE
	reg |= (regs->wdt_interrupt_mode | KEY_FIELD_MAGIC);
#else
    reg |= (regs->wdt_reset_mode | KEY_FIELD_MAGIC);
#endif
	hal_writel(reg, wdt_base + regs->wdt_cfg);

	/* Enable watchdog */
	reg = hal_readl(wdt_base + regs->wdt_mode);
	reg |= (WDT_MODE_EN | KEY_FIELD_MAGIC);
	hal_writel(reg, wdt_base + regs->wdt_mode);

	return 0;
}

static struct sunxi_hal_driver_watchdog sunxi_hal_watchdog_ops =
{
	.get_info   = sunxi_wdt_get_info,
	.start		= sunxi_wdt_start,
	.stop		= sunxi_wdt_stop,
	.ping		= sunxi_wdt_ping,
	.set_timeout	= sunxi_wdt_set_timeout,
	.restart	= sunxi_wdt_restart,
};

//init_mode:
//       0 : Initialize watchdog and stop watchdog
//       1 : Initialize watchdog and start watchdog(timeout set to max timeout)
int sunxi_wdt_init(struct sunxi_wdt_dev_t **sunxi_wdt_dev, int init_mode)
{
	struct sunxi_wdt_dev_t *sunxi_wdt_dev_temp = NULL;

	WDT_INFO("sunxi_wdt_init,init_mode:%d\n",init_mode);
	sunxi_wdt_dev_temp = watchdog_get_dev();
	if (!sunxi_wdt_dev_temp) {
		WDT_INFO("sunxi Watchdog get dev fail\n");
		return -1;
	} else {
		*sunxi_wdt_dev = sunxi_wdt_dev_temp;
	}
	wdt_irq_pending_clear(sunxi_wdt_dev_temp);
	sunxi_wdt_dev_temp->irq_num = WDT_IRQ_NUM;
	strcpy(sunxi_wdt_dev_temp->wdt_info->drv_name,DRV_NAME);
	strcpy(sunxi_wdt_dev_temp->wdt_info->drv_version,DRV_VERSION);
	sunxi_wdt_dev_temp->wdt_ops = &sunxi_hal_watchdog_ops;
	sunxi_wdt_dev_temp->timeout = WDT_MAX_TIMEOUT;
	sunxi_wdt_dev_temp->max_timeout = WDT_MAX_TIMEOUT;
	sunxi_wdt_dev_temp->min_timeout = WDT_MIN_TIMEOUT;

	struct sunxi_wdt_info_t *wdt_info = NULL;
	sunxi_wdt_get_info(sunxi_wdt_dev_temp, &wdt_info);
	WDT_DEBUG("dev name:%s\n", wdt_info->dev_name);
	WDT_DEBUG("drv name:%s\n", wdt_info->drv_name);
	WDT_DEBUG("drv version:%s\n", wdt_info->drv_version);
	hal_writel(0x0f, sunxi_wdt_dev_temp->wdt_base + sunxi_wdt_dev_temp->wdt_regs->wdt_out_cfg);
	//watchdog needs to be stop because it may have been start in phase bootloader
	sunxi_wdt_stop(sunxi_wdt_dev_temp);
#if USE_WDT_INTERRUPT_MODE
	if (hal_request_irq(sunxi_wdt_dev_temp->irq_num, wdt_interupt_handler, "wdt",(void*)sunxi_wdt_dev_temp) < 0)	{
		WDT_INFO("wdt:hal_request_irq fail\n");
		return -1;
	}
	hal_enable_irq(sunxi_wdt_dev_temp->irq_num);
	wdt_irq_enable(sunxi_wdt_dev_temp);
#endif
	if (init_mode) {
		sunxi_wdt_start(sunxi_wdt_dev_temp);
		WDT_INFO("Watchdog enabled (timeout=%d sec)\n", sunxi_wdt_dev_temp->timeout);
	}

	return 0;
}

int sunxi_wdt_exit(struct sunxi_wdt_dev_t *sunxi_wdt_dev)
{
	WDT_INFO("sunxi_wdt_exit\n");
	sunxi_wdt_stop(sunxi_wdt_dev);
	return 0;
}
