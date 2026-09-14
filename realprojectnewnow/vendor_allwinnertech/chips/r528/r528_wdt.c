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
/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/arch.h>

#include <stdint.h>
#include <errno.h>
#include <debug.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/boardctl.h>
#include <sys/mount.h>

#include <nuttx/irq.h>
#include <nuttx/clock.h>
#include <nuttx/syslog/syslog.h>
#include <nuttx/timers/watchdog.h>
#include <arch/board/board.h>

#include "chip.h"
#ifdef CONFIG_DRIVERS_TIMER
#include <sunxi_hal_timer.h>
#endif
#include "sunxi_hal_wdt.h"
#include <sunxi_hal_rtc.h>

/* RTC reset flag register definition */
#define RTC_RESET_FLAG_REG  SUNXI_RTC_DATA_BASE + 0x04

#if defined(CONFIG_WATCHDOG) && defined(CONFIG_R528_WATCHDOG)
#define USE_TIMER1_AS_WATCH_DOG 1
#define THE_TIMED_TIME_OF_TIMER (11*1000*1000)
#define THE_MAX_WDT_TIMEOUT_TIME 16

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define WDT_FMIN       (1)

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* Defining an watchdog ioctl of R528
 * Used to restart the system - Argument: Ignored
 */
#define WDIOC_RESTART    _WDIOC(0x081)

/* This structure provides the private representation of the "lower-half"
 * driver state structure.  This structure must be cast-compatible with the
 * well-known watchdog_lowerhalf_s structure.
 */

struct r528_wdt_lowerhalf_s
{
  FAR const struct watchdog_ops_s  *ops;  /* Lower half operations */
  struct sunxi_wdt_dev_t *wdt_dev;
  uint32_t timeout;   /* The (actual) timeout(milliseconds) */
  uint32_t lastreset; /* The last reset time */
  bool     started;   /* true: The watchdog timer has been started */
  uint16_t reload;    /* Timer reload value */
  xcpt_t    handler;  /* User Handler */
  void      *upper;   /* Pointer to watchdog_upperhalf_s */
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/* "Lower half" driver methods **********************************************/

static int r528_start(FAR struct watchdog_lowerhalf_s *lower);
static int r528_stop(FAR struct watchdog_lowerhalf_s *lower);
static int r528_keepalive(FAR struct watchdog_lowerhalf_s *lower);
static int r528_getstatus(FAR struct watchdog_lowerhalf_s *lower,
                           FAR struct watchdog_status_s *status);
static int r528_settimeout(FAR struct watchdog_lowerhalf_s *lower,
                            uint32_t timeout);
static int r528_ioctl(FAR struct watchdog_lowerhalf_s *lower,
                            int cmd, unsigned long arg);
static xcpt_t r528_wdt_capture(struct watchdog_lowerhalf_s *lower,xcpt_t handler);
/****************************************************************************
 * Private Data
 ****************************************************************************/

/* "Lower half" driver methods */

static const struct watchdog_ops_s g_wdtops =
{
  .start      = r528_start,
  .stop       = r528_stop,
  .keepalive  = r528_keepalive,
  .getstatus  = r528_getstatus,
  .settimeout = r528_settimeout,
  .capture    = r528_wdt_capture,
  .ioctl      = r528_ioctl,
};

/* "Lower half" driver state */

static struct r528_wdt_lowerhalf_s g_wdtdev;
#if USE_TIMER1_AS_WATCH_DOG
static void hal_timer_irq_callback(void *param)
{
	if(g_wdtdev.handler != NULL){
		wderr("timer timeout,handler is not null,callback\n");
		g_wdtdev.handler(SUNXI_IRQ_TMR(SUNXI_R_TMR0),NULL,g_wdtdev.upper);
		syslog_flush();
		up_mdelay(10);
	}else{
		wderr("timer timeout,call panic\n");
		/* Set reset cause to watchdog reset */
		uint32_t value = BOARDIOC_RESETCAUSE_SYS_RWDT;
		value <<= 16;
		hal_writel(value, RTC_RESET_FLAG_REG);
		/* boot flag will be set to panic, instead of WDT,
		since this is a timer irq not real WDT. */
		syslog_flush();
		up_mdelay(10);
		PANIC();
	}
}
#endif
/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: r528_start
 *
 * Description:
 *   Start the watchdog timer, resetting the time to the current timeout,
 *
 * Input Parameters:
 *   lower - A pointer the publicly visible representation of the "lower-half"
 *           driver state structure.
 *
 * Returned Values:
 *   Zero on success; a negated errno value on failure.
 *
 ****************************************************************************/

static int r528_start(FAR struct watchdog_lowerhalf_s *lower)
{
  FAR struct r528_wdt_lowerhalf_s *priv = (FAR struct r528_wdt_lowerhalf_s *)lower;

  //wdinfo("Entry\n");
  DEBUGASSERT(priv);

  if (!priv->started)
  {
      if (sunxi_wdt_start(priv->wdt_dev))
      {
          wdinfo("sunxi_wdt_start faile\n");
          return -1;
      }
      priv->lastreset = clock();
      priv->started   = true;
#if USE_TIMER1_AS_WATCH_DOG
	  /*start a timer as inttrupt wdt*/
	  hal_timer_init(SUNXI_R_TMR0);
	  if(priv->timeout != 0 && (priv->timeout != (THE_MAX_WDT_TIMEOUT_TIME*1000)))
		hal_timer_set_oneshot(SUNXI_R_TMR0, priv->timeout*1000, hal_timer_irq_callback, NULL);
	  else
		hal_timer_set_oneshot(SUNXI_R_TMR0, THE_TIMED_TIME_OF_TIMER, hal_timer_irq_callback, NULL);
#endif
  }

  return OK;
}

/****************************************************************************
 * Name: r528_stop
 *
 * Description:
 *   Stop the watchdog timer
 *
 * Input Parameters:
 *   lower - A pointer the publicly visible representation of the "lower-half"
 *           driver state structure.
 *
 * Returned Values:
 *   Zero on success; a negated errno value on failure.
 *
 ****************************************************************************/

static int r528_stop(FAR struct watchdog_lowerhalf_s *lower)
{
  FAR struct r528_wdt_lowerhalf_s *priv = (FAR struct r528_wdt_lowerhalf_s *)lower;

  wdinfo("Entry\n");
#if USE_TIMER1_AS_WATCH_DOG
  hal_timer_irq_disable(SUNXI_R_TMR0);
  hal_timer_stop(SUNXI_R_TMR0);
#endif
  if (sunxi_wdt_stop(priv->wdt_dev))
  {
      wdinfo("sunxi_wdt_stop faile\n");
      return -1;
  } else {
      priv->started = false;
      priv->lastreset = 0;
  }

  return OK;
}

/****************************************************************************
 * Name: r528_keepalive
 *
 * Description:
 *   Reset the watchdog timer to the current timeout value, prevent any
 *   imminent watchdog timeouts.  This is sometimes referred as "pinging"
 *   the watchdog timer or "petting the dog".
 *
 * Input Parameters:
 *   lower - A pointer the publicly visible representation of the "lower-half"
 *           driver state structure.
 *
 * Returned Values:
 *   Zero on success; a negated errno value on failure.
 *
 ****************************************************************************/

static int r528_keepalive(FAR struct watchdog_lowerhalf_s *lower)
{
  FAR struct r528_wdt_lowerhalf_s *priv = (FAR struct r528_wdt_lowerhalf_s *)lower;

  //wdinfo("Entry\n");
  DEBUGASSERT(priv);
#if USE_TIMER1_AS_WATCH_DOG
  int ret = -1;
  /*reload the timer*/
  if(priv->timeout != 0 && (priv->timeout != (THE_MAX_WDT_TIMEOUT_TIME*1000))){
	wdinfo("set timer intv value:%lus\n",(priv->timeout/1000));
	ret = hal_timer_set_oneshot(SUNXI_R_TMR0, priv->timeout*1000, hal_timer_irq_callback, NULL);
  }else{
	wdinfo("set timer intv value:%ds\n",(THE_TIMED_TIME_OF_TIMER/(1000*1000)));
	ret = hal_timer_set_oneshot(SUNXI_R_TMR0, THE_TIMED_TIME_OF_TIMER, hal_timer_irq_callback, NULL);
  }
  wdinfo("hal_timer_set_oneshot ret:%d\n",ret);
#endif
  /* Reload the WDT timer */
  if (sunxi_wdt_ping(priv->wdt_dev))
  {
       wdinfo("sunxi_wdt_ping failed\n");
       return -1;
  } else {
       priv->lastreset = clock();
  }
  return OK;
}

/****************************************************************************
 * Name: r528_getstatus
 *
 * Description:
 *   Get the current watchdog timer status
 *
 * Input Parameters:
 *   lower  - A pointer the publicly visible representation of the "lower-half"
 *            driver state structure.
 *   status - The location to return the watchdog status information.
 *
 * Returned Values:
 *   Zero on success; a negated errno value on failure.
 *
 ****************************************************************************/

static int r528_getstatus(FAR struct watchdog_lowerhalf_s *lower,
                           FAR struct watchdog_status_s *status)
{
  FAR struct r528_wdt_lowerhalf_s *priv = (FAR struct r528_wdt_lowerhalf_s *)lower;
  uint32_t ticks;
  uint32_t elapsed;

  //wdinfo("Entry\n");
  DEBUGASSERT(priv);

  /* Return the status bit */

  status->flags = WDFLAGS_RESET;
  if (priv->started)
  {
      status->flags |= WDFLAGS_ACTIVE;
  }

  /* Return the actual timeout in milliseconds */

  status->timeout = priv->timeout;

  /* Get the elapsed time since the last ping */
  ticks   = clock() - priv->lastreset;
  elapsed = (int32_t)TICK2MSEC(ticks);

  if (elapsed > priv->timeout)
  {
      elapsed = priv->timeout;
  }

  /* Return the approximate time until the watchdog timer expiration */

  status->timeleft = priv->timeout - elapsed;

  return OK;
}

/****************************************************************************
 * Name: r528_settimeout
 *
 * Description:
 *   Set a new timeout value (and reset the watchdog timer)
 *
 * Input Parameters:
 *   lower   - A pointer the publicly visible representation of the "lower-half"
 *             driver state structure.
 *   timeout - The new timeout value in milliseconds.
 *
 * Returned Values:
 *   Zero on success; a negated errno value on failure.
 *
 ****************************************************************************/

static int r528_settimeout(FAR struct watchdog_lowerhalf_s *lower, uint32_t timeout)
{
  FAR struct r528_wdt_lowerhalf_s *priv = (FAR struct r528_wdt_lowerhalf_s *)lower;

  DEBUGASSERT(priv);

  /* Can this timeout be represented? */

  if (timeout < (priv->wdt_dev->min_timeout * 1000) ||
		 timeout > (priv->wdt_dev->max_timeout * 1000))
  {
      return -ERANGE;
  }

  if (priv->started)
  {
      return -EBUSY;
  }

#if USE_TIMER1_AS_WATCH_DOG
  /*reload the timer*/
  if(timeout != 0 && (timeout != (THE_MAX_WDT_TIMEOUT_TIME*1000))){
	wdinfo("set timer intv value:%lus\n",timeout/1000);
	hal_timer_set_oneshot(SUNXI_R_TMR0, timeout*1000, hal_timer_irq_callback, NULL);
  }else{
	wdinfo("set timer intv value:%ds\n",(THE_TIMED_TIME_OF_TIMER/(1000*1000)));
	hal_timer_set_oneshot(SUNXI_R_TMR0, THE_TIMED_TIME_OF_TIMER, hal_timer_irq_callback, NULL);
  }
#endif
  priv->timeout = timeout;
  if(timeout != 0 && (timeout != (THE_MAX_WDT_TIMEOUT_TIME*1000)) && ((timeout+5000)<(THE_MAX_WDT_TIMEOUT_TIME*1000))){
	/*for cmocka_driver_watchdog test,let the timeout of second wdt is larger than first wdt 5s*/
	priv->wdt_dev->timeout = (timeout / 1000) + 5;
  }else{
	priv->wdt_dev->timeout = (timeout / 1000);
  }

  return OK;
}

/****************************************************************************
 * Name: wdt_capture
 *
 * Description:
 *   Don't reset on watchdog timer timeout; instead, call this user provider
 *   timeout handler.  NOTE:  Providing handler==NULL will restore the reset
 *   behavior.
 *
 * Input Parameters:
 *   lower         - A pointer the publicly visible representation of the
 *                   "lower-half" driver state structure.
 *   handler       - The new watchdog expiration function pointer. If this
 *                   function pointer is NULL, then the reset-on-expiration
 *                   behavior is restored.
 *
 * Returned Value:
 *   The previous watchdog expiration function pointer or NULL if there was
 *   no previous function pointer, i.e., if the previous behavior was
 *   reset-on-expiration (NULL is also returned if an error occurs).
 *
 ****************************************************************************/

static xcpt_t r528_wdt_capture(struct watchdog_lowerhalf_s *lower, xcpt_t handler)
{
  FAR struct r528_wdt_lowerhalf_s *priv = (FAR struct r528_wdt_lowerhalf_s *)lower;
  DEBUGASSERT(priv);
  wdinfo("r528_wdt_capture\n");
  priv->handler = handler;
  return priv->handler;
}

/****************************************************************************
 * Name: r528_ioctl
 *
 * Description:
 *   ioctl support restarting the system
 *
 * Input Parameters:
 *   lower   - A pointer the publicly visible representation of the "lower-half"
 *             driver state structure.
 *
 * Returned Values:
 *   Zero on success; a negated errno value on failure.
 *
 ****************************************************************************/

static int r528_ioctl(FAR struct watchdog_lowerhalf_s *lower, int cmd, unsigned long arg)
{
  FAR struct r528_wdt_lowerhalf_s *priv = (FAR struct r528_wdt_lowerhalf_s *)lower;

  DEBUGASSERT(priv);

  switch (cmd)
  {
      case WDIOC_RESTART:
        {
            if (sunxi_wdt_restart(priv->wdt_dev))
            {
                wdinfo("sunxi_wdt_restart faile\n");
                return -1;
            }
        }
        break;
      case WDIOC_START:
        {
            if (r528_start(lower))
            {
                wdinfo("sunxi_wdt_start faile\n");
                return -1;
            }
        }
        break;
      case WDIOC_STOP:
        {
            if (r528_stop(lower))
            {
                wdinfo("sunxi_wdt_start faile\n");
                return -1;
            }
        }
        break;
      case WDIOC_KEEPALIVE:
        {
            if (r528_keepalive(lower))
            {
                wdinfo("sunxi_wdt_start faile\n");
                return -1;
            }
        }
        break;
      default:
        {
            wdinfo("Unsupported command\n");
            return -1;
        }
        break;
  }

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: r528_wdt_initialize
 *
 * Description:
 *   Initialize the WDT watchdog time.  The watchdog timer is initialized and
 *   registers as 'devpath.  The initial state of the watchdog time is
 *   disabled.
 *
 * Input Parameters:
 *   devpath    - The full path to the watchdog.  This should be of the form
 *                /dev/watchdog0
 *
 * Returned Values:
 *   Zero (OK) is returned on success; a negated errno value is returned on
 *   any failure.
 *
 ****************************************************************************/

int r528_wdt_initialize(FAR const char *devpath)
{
  FAR struct r528_wdt_lowerhalf_s *priv = &g_wdtdev;

  wdinfo("Entry: devpath=%s\n", devpath);
  FAR struct sunxi_wdt_dev_t *sunxi_wdt_dev = NULL;
  FAR struct sunxi_wdt_info_t *sunxi_wdt_info = NULL;

#ifdef CONFIG_R528_WDOG_BOOTON
  if (sunxi_wdt_init(&sunxi_wdt_dev, 1)) {
#else
  if (sunxi_wdt_init(&sunxi_wdt_dev, 0)) {
#endif
    wdinfo("sunxi_wdt_init faile\n");
    return -ENODEV;
  }

  if (sunxi_wdt_get_info(sunxi_wdt_dev, &sunxi_wdt_info)) {
    wdinfo("sunxi_wdt_get_info faile\n");
    return -ENODEV;
  }

  /* Initialize the driver state structure. */

  priv->ops     = &g_wdtops;
  /* timeout is milliseconds */
  priv->timeout = sunxi_wdt_dev->timeout * 1000;
  priv->wdt_dev = sunxi_wdt_dev;
  priv->started = false;
  wdinfo("r528_wdt_initialize:priv->timeout:%lums",priv->timeout);
  /* Register the watchdog driver as /dev/watchdog0 */

  priv->upper = watchdog_register(devpath, (FAR struct watchdog_lowerhalf_s *)priv);

  if (priv->upper != NULL) {
      wdinfo("watchdog init success!\n");
      wdinfo("watchdog dev name: %s\n", sunxi_wdt_info->dev_name);
      wdinfo("watchdog drv name: %s\n", sunxi_wdt_info->drv_name);
      wdinfo("watchdog drv version: %s\n", sunxi_wdt_info->drv_version);
  } else {
      wdinfo("watchdog register faile\n");
      return -ENODEV;
  }
  return (priv->upper != NULL) ? OK : -ENODEV;
}

#endif /* CONFIG_WATCHDOG && CONFIG_R528_WATCHDOG */
