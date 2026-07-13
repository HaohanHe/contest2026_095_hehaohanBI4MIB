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

#include "r528_dsp_wdt.h"
#include <hal_interrupt.h>
#include <stdbool.h>
#include <stdbool.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

#define EXPORT_SYMBOL(func)

#define WDT_CTRL_RELOAD		((1 << 0) | (0x0a57 << 1))
#define WDT_MODE_EN		BIT(0)
#define WDT_CFG_RST_DBG		(0x0)
#define WDT_CFG_RST_SYS		(0x1)
#define WDT_CFG_ONLY_INT	(0x2)

struct sunxi_wdt_reg {
	u8 wdt_irq_en;  /* Offset to WDOG_IRQ_EN_REG */
	u8 wdt_sta;  /* Offset to WDOG_STA_REG */
	u8 wdt_ctrl;  /* Offset to WDOG_CTRL_REG */
	u8 wdt_cfg;   /* Offset to WDOG_CFG_REG */
	u8 wdt_mode;  /* Offset to WDOG_MODE_REG */
	u8 wdt_timeout_mask;  /* Bit mask of WDOG_INTV_VALUE in WDOG_MODE_REG */
	u8 wdt_timeout_shift;  /* Bit offset of WDOG_INTV_VALUE in WDOG_MODE_REG */
	u8 wdt_reset_mask;  /* Bit mask of WDOG_CONFIG in WDOG_CFG_REG */
	u32 wdt_filed_magic; /* regs->wdt_filed_magic for write watchdog data */
};

static const struct sunxi_wdt_reg sun6i_wdt_reg = {
	.wdt_irq_en = 0x0,
	.wdt_sta = 0x4,
	.wdt_ctrl = 0x10,
	.wdt_cfg = 0x14,
	.wdt_mode = 0x18,
	.wdt_timeout_mask = 0xf,
	.wdt_timeout_shift = 4,
	.wdt_reset_mask = 0x03,
	.wdt_filed_magic = 0x16AA0000,
};

static inline void sunxi_rproc_wdt_enable_irq(struct sunxi_rproc_wdt *wdt, bool enable)
{
	writel(enable ? 0x1 : 0x0, wdt->base + wdt->regs->wdt_irq_en);
}

static inline void sunxi_rproc_wdt_clear_pending(struct sunxi_rproc_wdt *wdt)
{
	writel(0x1, wdt->base + wdt->regs->wdt_sta);
}

static inline void sunxi_rproc_wdt_feed(struct sunxi_rproc_wdt *wdt)
{
	writel(WDT_CTRL_RELOAD, wdt->base + wdt->regs->wdt_ctrl);
}

static inline void sunxi_rproc_wdt_reset_cfg(struct sunxi_rproc_wdt *wdt, u32 type)
{
	u32 reg = readl(wdt->base + wdt->regs->wdt_cfg);

	reg &= ~(wdt->regs->wdt_reset_mask);
	if (type == RESET_INT)
		reg |= WDT_CFG_ONLY_INT;
	else
		reg |= WDT_CFG_RST_SYS;
	reg |= wdt->regs->wdt_filed_magic;
	writel(reg, wdt->base + wdt->regs->wdt_cfg);
}

static inline u32 sunxi_rproc_wdt_read_reset_cfg(struct sunxi_rproc_wdt *wdt)
{
	u32 reg = readl(wdt->base + wdt->regs->wdt_cfg);

	reg &= wdt->regs->wdt_reset_mask;
	return reg;
}

static inline void sunxi_rproc_wdt_enable(struct sunxi_rproc_wdt *wdt, bool enable)
{
	u32 reg = readl(wdt->base + wdt->regs->wdt_mode);

	if (enable)
		reg |= WDT_MODE_EN;
	else
		reg &= ~WDT_MODE_EN;

	reg |= wdt->regs->wdt_filed_magic;

	writel(reg, wdt->base + wdt->regs->wdt_mode);
}

static inline bool sunxi_rproc_wdt_is_enable_internal(struct sunxi_rproc_wdt *wdt)
{
	u32 reg = readl(wdt->base + wdt->regs->wdt_mode);

	if (reg & WDT_MODE_EN)
		return true;
	else
		return false;
}

struct timeout_map_t {
	u16 ms;
	u8 val;
};

static const struct timeout_map_t wdt_timeout_map[] = {
	{500,   0x0}, /* 0.5s */
	{1000,  0x1}, /* 1s */
	{2000,  0x2}, /* 2s */
	{3000,  0x3}, /* 3s */
	{4000,  0x4}, /* 4s */
	{5000,  0x5}, /* 5s */
	{6000,  0x6}, /* 6s */
	{8000,  0x7}, /* 8s */
	{10000, 0x8}, /* 10s */
	{12000, 0x9}, /* 12s */
	{14000, 0xA}, /* 14s */
	{16000, 0xB}, /* 16s */
};

static inline u32 timeout_to_interval_val(u32 timeout)
{
	u32 i;

	for (i = 0; i < ARRAY_SIZE(wdt_timeout_map); i++) {
		if (wdt_timeout_map[i].ms == timeout)
			return wdt_timeout_map[i].val;
	}

	return wdt_timeout_map[ARRAY_SIZE(wdt_timeout_map) - 1].val;
}

static inline u32 interval_val_to_timeout(u32 val)
{
	u32 i;

	for (i = 0; i < ARRAY_SIZE(wdt_timeout_map); i++) {
		if (wdt_timeout_map[i].val == val)
			return wdt_timeout_map[i].ms;
	}

	return 0;
}

static inline int sunxi_rproc_wdt_set_timeout_internal(struct sunxi_rproc_wdt *wdt, u32 timeout_ms)
{
	u32 reg, val = timeout_to_interval_val(timeout_ms);

	reg = readl(wdt->base + wdt->regs->wdt_mode);
	reg &= ~(wdt->regs->wdt_timeout_mask << wdt->regs->wdt_timeout_shift);
	reg |= val << wdt->regs->wdt_timeout_shift;
	reg |= wdt->regs->wdt_filed_magic;
	writel(reg, wdt->base + wdt->regs->wdt_mode);

	return 0;
}

static inline int sunxi_rproc_wdt_get_timeout_internal(struct sunxi_rproc_wdt *wdt, u32 *timeout_ms)
{
	u32 val, reg = readl(wdt->base + wdt->regs->wdt_mode);

	reg &= wdt->regs->wdt_timeout_mask << wdt->regs->wdt_timeout_shift;
	reg >>= wdt->regs->wdt_timeout_shift;

	val = interval_val_to_timeout(reg);
	if (val == 0)
		return -EIO;

	*timeout_ms = val;
	return 0;
}

static hal_irqreturn_t sunxi_rproc_wdt_handler(void *dev_id)
{
	struct sunxi_rproc_wdt *wdt = dev_id;

	sunxi_rproc_wdt_enable_irq(wdt, false);
	sunxi_rproc_wdt_clear_pending(wdt);

	if (wdt->param.cb)
		wdt->param.cb(wdt, wdt->param.cb_priv);

	return HAL_IRQ_OK;
}

static inline void sunxi_rproc_wdt_cleanup(struct sunxi_rproc_wdt *wdt)
{
	memset(wdt, 0, sizeof(*wdt));
	wdt->param.irq_num = -1;
	//spin_lock_init(&wdt->lock);
}

int sunxi_rproc_wdt_set_reset_type(struct sunxi_rproc_wdt *wdt, u32 type)
{
	switch (type) {
	case RESET_INT:
	case RESET_SYS:
		break;
	default:
		return -EINVAL;
	}

	sunxi_rproc_wdt_reset_cfg(wdt, type);
	return 0;
}
EXPORT_SYMBOL(sunxi_rproc_wdt_set_reset_type);

int sunxi_rproc_wdt_get_reset_type(struct sunxi_rproc_wdt *wdt, u32 *type)
{
	u32 val = sunxi_rproc_wdt_read_reset_cfg(wdt);

	if (val == WDT_CFG_RST_SYS) {
		*type = RESET_SYS;
		return 0;
	} else if (val == WDT_CFG_ONLY_INT) {
		*type = RESET_INT;
		return 0;
	} else if (val == WDT_CFG_RST_DBG) {
		*type = RESET_DBG;
		return 0;
	} else {
		return -EIO;
	}

	return 0;
}
EXPORT_SYMBOL(sunxi_rproc_wdt_get_reset_type);

int sunxi_rproc_wdt_set_timeout(struct sunxi_rproc_wdt *wdt, u32 timeout_ms)
{
	sunxi_rproc_wdt_set_timeout_internal(wdt, timeout_ms);
	return 0;
}
EXPORT_SYMBOL(sunxi_rproc_wdt_set_timeout);

int sunxi_rproc_wdt_get_timeout(struct sunxi_rproc_wdt *wdt, u32 *timeout_ms)
{
	return sunxi_rproc_wdt_get_timeout_internal(wdt, timeout_ms);
}
EXPORT_SYMBOL(sunxi_rproc_wdt_get_timeout);

int sunxi_rproc_wdt_is_enable(struct sunxi_rproc_wdt *wdt)
{
	if (sunxi_rproc_wdt_is_enable_internal(wdt))
		return 1;

	return 0;
}
EXPORT_SYMBOL(sunxi_rproc_wdt_is_enable);

int sunxi_rproc_wdt_start(struct sunxi_rproc_wdt *wdt)
{
	sunxi_rproc_wdt_enable(wdt, false);
	sunxi_rproc_wdt_set_timeout_internal(wdt, wdt->param.timeout_ms);
	sunxi_rproc_wdt_reset_cfg(wdt, wdt->param.reset_type);

	sunxi_rproc_wdt_clear_pending(wdt);
	sunxi_rproc_wdt_enable_irq(wdt, true);

	sunxi_rproc_wdt_enable(wdt, true);

	return 0;
}
EXPORT_SYMBOL(sunxi_rproc_wdt_start);

void sunxi_rproc_wdt_stop(struct sunxi_rproc_wdt *wdt)
{
	sunxi_rproc_wdt_enable(wdt, false);

	sunxi_rproc_wdt_enable_irq(wdt, false);
	sunxi_rproc_wdt_clear_pending(wdt);
}
EXPORT_SYMBOL(sunxi_rproc_wdt_stop);

int sunxi_rproc_wdt_suspend(struct sunxi_rproc_wdt *wdt)
{
	return 0;
}
EXPORT_SYMBOL(sunxi_rproc_wdt_suspend);

int sunxi_rproc_wdt_resume(struct sunxi_rproc_wdt *wdt)
{
	return 0;
}
EXPORT_SYMBOL(sunxi_rproc_wdt_resume);

int sunxi_rproc_wdt_deinit(struct sunxi_rproc_wdt *wdt)
{
	if (wdt->param.irq_num >= 0) {
		hal_free_irq(wdt->param.irq_num);
		wdt->param.irq_num = -1;
	}

	if (wdt->base) {
		// not need to io unmap
		wdt->base = NULL;
	}
	sunxi_rproc_wdt_cleanup(wdt);

	return 0;
}
EXPORT_SYMBOL(sunxi_rproc_wdt_deinit);

int sunxi_rproc_wdt_init(struct sunxi_rproc_wdt *wdt, struct sunxi_rproc_wdt_param *param)
{
	int ret;

	if (!wdt || !param || param->irq_num < 0)
		return -EINVAL;

	sunxi_rproc_wdt_cleanup(wdt);

	memcpy(&wdt->param, param, sizeof(wdt->param));

	// io mapped in r528_boot.c
	wdt->base = wdt->param.reg_base;
	if (!wdt->base) {
		ret = -EIO;
		goto err_out;
	}

	ret = hal_request_irq(wdt->param.irq_num, sunxi_rproc_wdt_handler, "dsp_wdt", wdt);
	if (ret) {
		wdt->param.irq_num = -1;
		goto err_out;
	}
	hal_enable_irq(wdt->param.irq_num);

	// TODO: select ip version
	wdt->regs = &sun6i_wdt_reg;

	return 0;
err_out:
	sunxi_rproc_wdt_deinit(wdt);
	return ret;
}
EXPORT_SYMBOL(sunxi_rproc_wdt_init);
