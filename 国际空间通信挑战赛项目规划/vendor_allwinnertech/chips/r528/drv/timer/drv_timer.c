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
#include <nuttx/clock.h>
#include <nuttx/timers/oneshot.h>
#include <arch/board/board.h>
#include <nuttx/timers/arch_alarm.h>

#include <hal_clk.h>
#include "chip.h"

#include "sunxi_hal_timer.h"

#if defined(CONFIG_DRIVERS_TIMER)

#define R528_TIMESTAMP_LO					(0x08110000)
#define R528_TIMESTAMP_HI					(0x08110004)

#define TIMESTAMP_FREQ						(24) /* 24M */

#define GET_CUR_TIMESTAMP() \
			({ uint64_t __t; \
				__t = *(volatile uint32_t *)(R528_TIMESTAMP_HI); \
				__t <<= 32; \
				__t |= *(volatile uint32_t *)(R528_TIMESTAMP_LO); \
				__t; })

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

struct sunxi_oneshot_lowerhalf_s
{
	struct oneshot_lowerhalf_s oneshot_s;
	int oneshot_id :16;
	int isstart :16;
	uint64_t target_timestamp;
	oneshot_callback_t callback;
	void *arg;
};
/* "Lower half" driver methods **********************************************/

static int r528_start(FAR struct oneshot_lowerhalf_s *lower,
                      oneshot_callback_t callback, FAR void *arg,
                      FAR const struct timespec *ts);
static int r528_cancel(FAR struct oneshot_lowerhalf_s *lower,
                       FAR struct timespec *ts);
static int r528_max_delay(FAR struct oneshot_lowerhalf_s *lower,
                          FAR struct timespec *ts);
static int r528_current(FAR struct oneshot_lowerhalf_s *lower,
                        FAR struct timespec *ts);
static int r528_tick_start(FAR struct oneshot_lowerhalf_s *lower,
                           oneshot_callback_t callback, FAR void *arg,
                           clock_t ticks);
static int r528_tick_cancel(FAR struct oneshot_lowerhalf_s *lower,
                            clock_t *ticks);
static int r528_tick_max_delay(FAR struct oneshot_lowerhalf_s *lower,
                               clock_t *ticks);
static int r528_tick_current(FAR struct oneshot_lowerhalf_s *lower,
                             clock_t *ticks);

/****************************************************************************
 * Private Data
 ****************************************************************************/
static const struct oneshot_operations_s g_timerops =
{
  .start           = r528_start,
  .cancel          = r528_cancel,
  .max_delay       = r528_max_delay,
  .current         = r528_current,
  .tick_start      = r528_tick_start,
  .tick_cancel     = r528_tick_cancel,
  .tick_max_delay  = r528_tick_max_delay,
  .tick_current    = r528_tick_current,
};

/* "Lower half" driver state */

static struct sunxi_oneshot_lowerhalf_s g_oneshot_dev[2];

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void r528_setcallback(FAR struct oneshot_lowerhalf_s *lower, CODE oneshot_callback_t callback, FAR void *arg)
{
  FAR struct sunxi_oneshot_lowerhalf_s *priv = (FAR struct sunxi_oneshot_lowerhalf_s *)lower;
  DEBUGASSERT(priv);

  priv->callback = callback;
  priv->arg = arg;
}

static void sunxi_timer_callback(void *arg)
{
  FAR struct sunxi_oneshot_lowerhalf_s *priv = (FAR struct sunxi_oneshot_lowerhalf_s *)arg;

  priv->callback(&priv->oneshot_s, priv->arg);
}

static int r528_start(FAR struct oneshot_lowerhalf_s *lower,
                      oneshot_callback_t callback, FAR void *arg,
                      FAR const struct timespec *ts)
{
  FAR struct sunxi_oneshot_lowerhalf_s *priv = (FAR struct sunxi_oneshot_lowerhalf_s *)lower;
  uint32_t delay_us;

  wdinfo("Entry\n");
  DEBUGASSERT(priv);

  r528_setcallback(lower, callback, arg);
  delay_us = ts->tv_sec * 1000000 + ts->tv_nsec / 1000;

  priv->target_timestamp = GET_CUR_TIMESTAMP() + delay_us * TIMESTAMP_FREQ;

  if (hal_timer_set_oneshot(priv->oneshot_id, delay_us, sunxi_timer_callback, priv)) {
	wdinfo("timer start failed\n");
	priv->isstart = 0;
  } else {
	priv->isstart = 1;
  }
  return OK;
}

static int r528_tick_start(FAR struct oneshot_lowerhalf_s *lower,
                           oneshot_callback_t callback, FAR void *arg,
                           clock_t ticks)
{
  FAR struct sunxi_oneshot_lowerhalf_s *priv = (FAR struct sunxi_oneshot_lowerhalf_s *)lower;
  uint32_t delay_us = TICK2USEC(ticks);

  r528_setcallback(lower, callback, arg);

  priv->target_timestamp = GET_CUR_TIMESTAMP() + delay_us * TIMESTAMP_FREQ;

  if (hal_timer_set_oneshot(priv->oneshot_id, delay_us, sunxi_timer_callback, priv)) {
	priv->isstart = 0;
  } else {
	priv->isstart = 1;
  }
  return OK;
}

static int r528_cancel(FAR struct oneshot_lowerhalf_s *lower,
                       FAR struct timespec *ts)
{
  FAR struct sunxi_oneshot_lowerhalf_s *priv = (FAR struct sunxi_oneshot_lowerhalf_s *)lower;
  uint64_t left_us;

  hal_timer_stop(priv->oneshot_id);

  left_us = GET_CUR_TIMESTAMP();
  if (left_us > priv->target_timestamp)
      left_us = 0;
  else
      left_us = priv->target_timestamp - left_us;
  left_us /= TIMESTAMP_FREQ;

  ts->tv_sec = (time_t)(left_us / 1000000);
  ts->tv_nsec = (long)((left_us % 1000000) * 1000);

  priv->isstart = 0;

  return OK;
}

static int r528_tick_cancel(FAR struct oneshot_lowerhalf_s *lower,
                            clock_t *ticks)
{
  FAR struct sunxi_oneshot_lowerhalf_s *priv = (FAR struct sunxi_oneshot_lowerhalf_s *)lower;
  uint64_t left_us;

  hal_timer_stop(priv->oneshot_id);

  left_us = GET_CUR_TIMESTAMP();
  if (left_us > priv->target_timestamp)
      left_us = 0;
  else
      left_us = priv->target_timestamp - left_us;
  left_us /= TIMESTAMP_FREQ;

  *ticks = USEC2TICK(left_us);

  priv->isstart = 0;

  return OK;
}

static int r528_max_delay(FAR struct oneshot_lowerhalf_s *lower,
                          FAR struct timespec *ts)
{
  FAR struct sunxi_oneshot_lowerhalf_s *priv = (FAR struct sunxi_oneshot_lowerhalf_s *)lower;
  long max_us;

  max_us = 0xffffffff / 24;

  ts->tv_sec = max_us / 1000000;
  ts->tv_nsec = (max_us % 1000000) * 1000;

  wdinfo("Entry\n");
  DEBUGASSERT(priv);

  return OK;
}

static int r528_tick_max_delay(FAR struct oneshot_lowerhalf_s *lower,
                               clock_t *ticks)
{
  FAR struct sunxi_oneshot_lowerhalf_s *priv = (FAR struct sunxi_oneshot_lowerhalf_s *)lower;

  *ticks = USEC2TICK(0xffffffff / 24);

  return OK;
}

static int r528_current(FAR struct oneshot_lowerhalf_s *lower,
                        FAR struct timespec *ts)
{
  FAR struct sunxi_oneshot_lowerhalf_s *priv = (FAR struct sunxi_oneshot_lowerhalf_s *)lower;
  uint64_t cur_us;

  cur_us = GET_CUR_TIMESTAMP() / TIMESTAMP_FREQ;
  ts->tv_sec = (time_t)(cur_us / 1000000);
  ts->tv_nsec = (long)((cur_us % 1000000) * 1000);

  //wdinfo("Entry\n");
  DEBUGASSERT(priv);

  return OK;
}

static int r528_tick_current(FAR struct oneshot_lowerhalf_s *lower,
                             clock_t *ticks)
{
  FAR struct sunxi_oneshot_lowerhalf_s *priv = (FAR struct sunxi_oneshot_lowerhalf_s *)lower;

  *ticks = USEC2TICK(GET_CUR_TIMESTAMP() / TIMESTAMP_FREQ);
  return OK;
}

int r528_oneshot_initialize(FAR const char *devpath, int oneshot_id)
{
  FAR struct sunxi_oneshot_lowerhalf_s *priv = &g_oneshot_dev[oneshot_id];
  FAR int handle = -1;

  wdinfo("Entry: devpath=%s\n", devpath);

  hal_timer_init(oneshot_id);

  priv->oneshot_s.ops = &g_timerops;
  priv->oneshot_id = oneshot_id;
#ifdef CONFIG_ONESHOT_AS_TICK
  if (CONFIG_ONESHOT_TICK_TIMER_INDEX == oneshot_id) {
    up_alarm_set_lowerhalf((FAR struct oneshot_lowerhalf_s *)priv);
    tmrinfo("oneshot%d register as tick dev!\n", CONFIG_ONESHOT_TICK_TIMER_INDEX);
    return OK;
  }
#endif

#ifdef CONFIG_ONESHOT
  int oneshot_register(FAR const char *devname,
                     FAR struct oneshot_lowerhalf_s *lower);
  handle = oneshot_register(devpath, (FAR struct oneshot_lowerhalf_s *)priv);
#endif
  if (!handle) {
      wdinfo("oneshot register success!\n");
  } else {
      wdinfo("oneshot register faile\n");
      return -ENODEV;
  }
  return (handle == 0) ? OK : -ENODEV;
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
    return;
}
#endif

#endif
