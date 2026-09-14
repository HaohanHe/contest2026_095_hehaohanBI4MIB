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

#include <nuttx/config.h>
#include <nuttx/arch.h>

#include <stdint.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/irq.h>
#include <nuttx/spinlock.h>
#include <nuttx/clock.h>
#include <nuttx/timers/oneshot.h>
#include <arch/board/board.h>
#include <nuttx/timers/arch_alarm.h>
#include <nuttx/lib/math32.h>

#include <hal_clk.h>
#include <hal_interrupt.h>
#include "chip.h"

#if defined(CONFIG_DRIVERS_TIMER)

//#define TIMER_OPS_LOCK
#ifdef CONFIG_X4B_AP
//#define TIMER_PERFORMANCE_DEBUG
#endif

#define TIMESTAMP_FREQ		(24) /* 24M */

#define R528_TIMESTAMP_LO					(0x08110000)
#define R528_TIMESTAMP_HI					(0x08110004)
#if 1
#ifndef unlikely
#define unlikely(x)             __builtin_expect ((x), 0)
#endif

static inline uint64_t _get_cur_timestamp(void)
{
	uint32_t cntval_l, cntval_h, cntval_h_prev;

	cntval_h_prev = hal_readl(R528_TIMESTAMP_HI);
retry:
	cntval_l = hal_readl(R528_TIMESTAMP_LO);
	cntval_h = hal_readl(R528_TIMESTAMP_HI);

	if (unlikely(cntval_h_prev != cntval_h)) {
		cntval_h_prev = cntval_h;
		goto retry;
	}

	return ((uint64_t)cntval_h << 32) | cntval_l;
}

#define GET_CUR_TIMESTAMP()	(_get_cur_timestamp())
#else
#define GET_CUR_TIMESTAMP() \
	({ uint64_t __t; \
		__t = *(volatile uint32_t *)(R528_TIMESTAMP_HI); \
		__t <<= 32; \
		__t |= *(volatile uint32_t *)(R528_TIMESTAMP_LO); \
		__t; })
#endif

#define TIMESTAMP_TO_TIMESPEC(ts, t) \
	do { \
		(ts)->tv_sec = (time_t)((t) / TIMESTAMP_FREQ / 1000000); \
		(ts)->tv_nsec = (long)((((t) / TIMESTAMP_FREQ) % 1000000) * 1000); \
	} while (0)

#define TIMESPEC_TO_TIMESTAMP(ts) \
	(((ts)->tv_sec * 1000000llu * TIMESTAMP_FREQ) + ((ts)->tv_nsec / 1000lu * TIMESTAMP_FREQ))

#define TIMER_MAX_DELAY_TIMESPEC(ts) TIMESTAMP_TO_TIMESPEC(ts, 0x54d4a800lu)

#define TIMESTAMP_TO_TIMER_INTVAL(ts) (uint32_t)(ts)
#define TIMER_INTVAL_TO_TIMESTAMP(ts) (uint64_t)(ts)

#ifndef container_of
#define container_of(ptr, type, member) \
	((type *)((char *)(ptr) - (unsigned long)(&((type *)0)->member)))
#endif

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int r528_timer_start(FAR struct oneshot_lowerhalf_s *lower, FAR const struct timespec *ts);
static int r528_timer_cancel(FAR struct oneshot_lowerhalf_s *lower, FAR struct timespec *ts);
static int r528_timer_max_delay(FAR struct oneshot_lowerhalf_s *lower, FAR struct timespec *ts);
static int r528_timer_current(FAR struct oneshot_lowerhalf_s *lower, FAR struct timespec *ts);
static int r528_tick_start(FAR struct oneshot_lowerhalf_s *lower, oneshot_callback_t callback,
			   FAR void *arg, clock_t ticks);
static int r528_tick_cancel(FAR struct oneshot_lowerhalf_s *lower, clock_t *ticks);
static int r528_tick_max_delay(FAR struct oneshot_lowerhalf_s *lower, clock_t *ticks);
static int r528_tick_current(FAR struct oneshot_lowerhalf_s *lower, clock_t *ticks);

/****************************************************************************
 * Private Data
 ****************************************************************************/
struct sunxi_timer_s {
	struct oneshot_lowerhalf_s lower;
	oneshot_callback_t callback;
	void *arg;

	unsigned int base;
	unsigned int timer_id;
	unsigned int irq;
};

static const struct oneshot_operations_s g_timerops = {
	.start		= r528_timer_start,
	.cancel		= r528_timer_cancel,
	.max_delay	= r528_timer_max_delay,
	.current	= r528_timer_current,
};

#ifdef CONFIG_R528_ONESHOT0
__attribute__((aligned(64))) static struct sunxi_timer_s g_oneshot0_dev;
#endif
#ifdef CONFIG_R528_ONESHOT1
__attribute__((aligned(64))) static struct sunxi_timer_s g_oneshot1_dev;
#endif
#ifdef CONFIG_R528_ONESHOT2
__attribute__((aligned(64))) static struct sunxi_timer_s g_oneshot2_dev;
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/
#define SUNXI_TMR_PBASE			(0x02050000)
#define SUNXI_IRQ_TMR(id)		(32 + 59 + (id))
#define SUNXI_R_TMR_PBASE		(0x07020000)
#define SUNXI_IRQ_R_TMR(id)		(32 + 140 + (id))
#define TIMER_IRQ_EN_REG(base)		(unsigned long)((base) + 0x00)
#define TIMER_IRQ_EN(val)		BIT(val)
#define TIMER_IRQ_ST_REG(base)		(unsigned long)((base) + 0x04)
#define TIMER_IRQ_CLEAR(val)		BIT(val)
#define TIMER_CTL_REG(base, id)		(unsigned long)((base) + 0x10 * (id) + 0x10)
#define TIMER_CTL_ENABLE		BIT(0)
#define TIMER_CTL_RELOAD		BIT(1)
#define TIMER_CTL_CLK_SRC(val)		(((val) & 0x3) << 2)
#define TIMER_CTL_CLK_SRC_OSC24M	(1)
#define TIMER_CTL_CLK_PRES(val)		(((val) & 0x7) << 4)
#define TIMER_CTL_ONESHOT		BIT(7)
#define TIMER_INTVAL_REG(base, id)	(unsigned long)((base) + 0x10 * (id) + 0x14)
#define TIMER_CNTVAL_REG(base, id)	(unsigned long)((base) + 0x10 * (id) + 0x18)
#define TIMER_SYNC_TICKS		(3)

#define TIMER_IRQ_ENABLE(priv, en) \
	do { \
		unsigned long reg = TIMER_IRQ_EN_REG((priv)->base); \
		uint32_t val = hal_readl(reg); \
		if (en) \
			val |= (0x1 << (priv)->timer_id); \
		else \
			val &= ~(0x1 << (priv)->timer_id); \
		hal_writel(val, reg); \
	} while (0)
#define TIMER_CLEAR_PENDING(priv) \
	hal_writel(0x1 << (priv)->timer_id, TIMER_IRQ_ST_REG((priv)->base))
#define TIMER_IS_STARTED(priv) \
	(TIMER_CTL_ENABLE & hal_readl(TIMER_CTL_REG((priv)->base, (priv)->timer_id)))
#define TIMER_STOP(priv) \
	do { \
		unsigned long reg = TIMER_CTL_REG((priv)->base, (priv)->timer_id); \
		uint32_t val = hal_readl(reg); \
		val &= ~TIMER_CTL_ENABLE; \
		hal_writel(val, reg); \
	} while (0)
#define TIMER_STOP_SYNC(priv) \
	do { \
		unsigned long reg = TIMER_CTL_REG((priv)->base, (priv)->timer_id); \
		uint32_t val = hal_readl(reg); \
		uint64_t until;\
		val &= ~TIMER_CTL_ENABLE; \
		hal_writel(val, reg); \
		until = GET_CUR_TIMESTAMP() + TIMER_SYNC_TICKS; \
		while (until > GET_CUR_TIMESTAMP()); \
	} while (0)
#define TIMER_START(priv) \
	do { \
		unsigned long reg = TIMER_CTL_REG((priv)->base, (priv)->timer_id); \
		uint32_t val = TIMER_CTL_ONESHOT | TIMER_CTL_RELOAD; \
		val |= TIMER_CTL_CLK_PRES(0) | TIMER_CTL_CLK_SRC(TIMER_CTL_CLK_SRC_OSC24M); \
		hal_writel(val, reg); \
		while (TIMER_CTL_RELOAD & val) \
			val = hal_readl(reg); \
		val |= TIMER_CTL_ENABLE; \
		hal_writel(val, reg); \
	} while (0)
#define TIMER_SET_INTVAL(priv, tick) \
	hal_writel((tick), TIMER_INTVAL_REG((priv)->base, (priv)->timer_id))
#define TIMER_GET_CNTVAL(priv) \
	hal_readl(TIMER_CNTVAL_REG((priv)->base, (priv)->timer_id))

static hal_irqreturn_t r528_timer_irq_handle(void *arg)
{
	FAR struct sunxi_timer_s * restrict priv = (FAR struct sunxi_timer_s *)arg;

	TIMER_CLEAR_PENDING(priv);

	priv->callback(&priv->lower, priv->arg);
	return 0;
}

static volatile int r528_timer_debug_loop = 1;
static volatile struct timespec r528_timer_debug_ts;
static int r528_timer_start(FAR struct oneshot_lowerhalf_s *lower, FAR const struct timespec *ts)
{
	FAR struct sunxi_timer_s * restrict priv = container_of(lower, struct sunxi_timer_s, lower);
	uint32_t intval = TIMESTAMP_TO_TIMER_INTVAL(TIMESPEC_TO_TIMESTAMP(ts));
#ifdef TIMER_OPS_LOCK
	irqstate_t flags;
#endif

#ifdef TIMER_OPS_LOCK
	flags = enter_critical_section();
#endif

	priv->callback = priv->lower.callback;
	priv->arg = priv->lower.arg;

	if (TIMER_IS_STARTED(priv)) {
		TIMER_STOP_SYNC(priv);
	}

	if (intval < TIMER_SYNC_TICKS)
		intval = TIMER_SYNC_TICKS;
	TIMER_SET_INTVAL(priv, intval);

	TIMER_START(priv);

#ifdef TIMER_OPS_LOCK
	leave_critical_section(flags);
#endif
	return OK;
}

static int r528_tick_start(FAR struct oneshot_lowerhalf_s *lower, oneshot_callback_t callback,
			   FAR void *arg, clock_t ticks)
{
	FAR struct sunxi_timer_s * restrict priv = container_of(lower, struct sunxi_timer_s, lower);
	uint32_t intval = TICK2USEC(ticks) * TIMESTAMP_FREQ;
#ifdef TIMER_OPS_LOCK
	irqstate_t flags;
#endif

#ifdef TIMER_OPS_LOCK
	flags = enter_critical_section();
#endif

	priv->callback = callback;
	priv->arg = arg;

	if (TIMER_IS_STARTED(priv)) {
		TIMER_STOP_SYNC(priv);
	}

	if (intval < TIMER_SYNC_TICKS)
		intval = TIMER_SYNC_TICKS;
	TIMER_SET_INTVAL(priv, intval);

	TIMER_START(priv);

#ifdef TIMER_OPS_LOCK
	leave_critical_section(flags);
#endif
	return OK;
}

static int r528_timer_cancel(FAR struct oneshot_lowerhalf_s *lower, FAR struct timespec *ts)
{
	FAR struct sunxi_timer_s * restrict priv = container_of(lower, struct sunxi_timer_s, lower);
	uint64_t timestamp;
#ifdef TIMER_OPS_LOCK
	irqstate_t flags;
#endif

#ifdef TIMER_OPS_LOCK
	flags = enter_critical_section();
#endif

	if (!TIMER_IS_STARTED(priv)) {
		ts->tv_sec = (time_t)0lu;
		ts->tv_nsec = (long)0lu;
		goto exit;
	}

	TIMER_STOP(priv);

	timestamp = TIMER_GET_CNTVAL(priv);
	TIMESTAMP_TO_TIMESPEC(ts, timestamp);

exit:
#ifdef TIMER_OPS_LOCK
	leave_critical_section(flags);
#endif
	return OK;
}

static int r528_tick_cancel(FAR struct oneshot_lowerhalf_s *lower, clock_t *ticks)
{
	FAR struct sunxi_timer_s * restrict priv = container_of(lower, struct sunxi_timer_s, lower);
	uint64_t timestamp;
#ifdef TIMER_OPS_LOCK
	irqstate_t flags;
#endif

#ifdef TIMER_OPS_LOCK
	flags = enter_critical_section();
#endif

	if (!TIMER_IS_STARTED(priv)) {
		*ticks = 0;
		goto exit;
	}

	TIMER_STOP(priv);

	timestamp = TIMER_GET_CNTVAL(priv);
	*ticks = div_const(timestamp, (TIMESTAMP_FREQ * USEC_PER_TICK));

exit:
#ifdef TIMER_OPS_LOCK
	leave_critical_section(flags);
#endif
	return OK;
}

static int r528_timer_max_delay(FAR struct oneshot_lowerhalf_s *lower, FAR struct timespec *ts)
{
	TIMER_MAX_DELAY_TIMESPEC(ts);
	return OK;
}

static int r528_tick_max_delay(FAR struct oneshot_lowerhalf_s *lower, clock_t *ticks)
{
	*ticks = div_const(0x54d4a800lu, (TIMESTAMP_FREQ * USEC_PER_TICK));
	return OK;
}

static int r528_timer_current(FAR struct oneshot_lowerhalf_s *lower, FAR struct timespec *ts)
{
	uint64_t timestamp = GET_CUR_TIMESTAMP();

	TIMESTAMP_TO_TIMESPEC(ts, timestamp);
	return OK;
}

static int r528_tick_current(FAR struct oneshot_lowerhalf_s *lower, clock_t *ticks)
{
	*ticks = div_const(GET_CUR_TIMESTAMP(), (TIMESTAMP_FREQ * USEC_PER_TICK));
	return OK;
}

static inline FAR struct sunxi_timer_s *r528_timer_get_priv_by_id(int oneshot_id)
{
	FAR struct sunxi_timer_s *priv = NULL;
	int timer_id;

	if (oneshot_id < 2) {
		timer_id = oneshot_id;
#ifdef CONFIG_R528_ONESHOT0
		if (oneshot_id == 0)
			priv = &g_oneshot0_dev;
#endif
#ifdef CONFIG_R528_ONESHOT1
		if (oneshot_id == 1)
			priv = &g_oneshot1_dev;
#endif
	if (!priv)
		return NULL;

	priv->base = SUNXI_TMR_PBASE;
	priv->irq = SUNXI_IRQ_TMR(timer_id);
	} else {
		timer_id = oneshot_id - 2;
#ifdef CONFIG_R528_ONESHOT2
		if (oneshot_id == 2)
			priv = &g_oneshot2_dev;
#endif
		if (!priv)
			return NULL;

		priv->base = SUNXI_R_TMR_PBASE;
		priv->irq = SUNXI_IRQ_R_TMR(timer_id);
	}

	priv->timer_id = timer_id;
	priv->lower.ops = &g_timerops;
	return priv;
}

static inline int r528_timer_init(FAR struct sunxi_timer_s *priv)
{
	char name[32];
	int ret;

	TIMER_IRQ_ENABLE(priv, 0);

	TIMER_STOP_SYNC(priv);
	TIMER_CLEAR_PENDING(priv);

	snprintf(name, sizeof(name) - 1, "timer%d@%x", priv->timer_id, priv->base);
	ret = hal_request_irq(priv->irq, r528_timer_irq_handle, name, priv);
	if (ret < 0)
		return ret;

	TIMER_IRQ_ENABLE(priv, 1);

	hal_enable_irq(priv->irq);

	return 0;
}

int r528_oneshot_initialize(FAR const char *devpath, int oneshot_id)
{
	FAR struct sunxi_timer_s *priv = r528_timer_get_priv_by_id(oneshot_id);
	int ret;

	wdinfo("devpath=%s\n", devpath);

	if (!priv)
		return -ENODEV;

	ret = r528_timer_init(priv);
	if (ret)
		return ret;

#ifdef CONFIG_ONESHOT_AS_TICK
	if (CONFIG_ONESHOT_TICK_TIMER_INDEX == oneshot_id) {
		up_alarm_set_lowerhalf(&priv->lower);
		tmrinfo("oneshot%d register as tick dev!\n", CONFIG_ONESHOT_TICK_TIMER_INDEX);
		return OK;
	}
#endif

#ifdef CONFIG_ONESHOT
	int oneshot_register(FAR const char *devname, FAR struct oneshot_lowerhalf_s *lower);
	ret = oneshot_register(devpath, &priv->lower);
	if (ret) {
		wdinfo("oneshot register failed, ret: %d\n", ret);
		return ret;
	}
#endif

	return -ENODEV;
}

#if defined(CONFIG_ONESHOT_AS_TICK) && defined(CONFIG_ONESHOT)
void up_timer_initialize(void)
{
#ifdef CONFIG_DRIVERS_CCMU
	hal_clock_init();
#endif

#ifdef CONFIG_R528_ONESHOT0
	r528_oneshot_initialize("/dev/oneshot0", 0);
#endif
#ifdef CONFIG_R528_ONESHOT1
	r528_oneshot_initialize("/dev/oneshot1", 1);
#endif
#ifdef CONFIG_R528_ONESHOT2
	r528_oneshot_initialize("/dev/oneshot2", 2);
#endif
}
#endif

#endif
