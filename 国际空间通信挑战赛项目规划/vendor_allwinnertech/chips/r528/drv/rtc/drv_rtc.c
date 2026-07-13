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
#include <nuttx/timers/arch_rtc.h>
#include <nuttx/timers/rtc.h>
#include <arch/board/board.h>

#include "chip.h"

#include "sunxi_hal_rtc.h"
#include "hal_mem.h"

#if defined(CONFIG_DRIVERS_RTC)

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

struct sunxi_rtc_lowerhalf_s
{
	struct rtc_lowerhalf_s rtc_s;
	struct sunxi_rtc_time *config_time;
	struct rtc_wkalrm *wkalarm;
    rtc_alarm_callback_t alarm_callback;
	void *alarm_data;
	int rtc_id;
	void *arg;
	/* Periodic interrupt support */
#ifdef CONFIG_RTC_PERIODIC
	bool periodic_enabled;
	int periodic_id;
	struct timespec periodic_interval;  /* Changed from itimerspec to timespec */
	rtc_alarm_callback_t periodic_callback;
	void *periodic_data;
#endif
};
/* "Lower half" driver methods **********************************************/

static int r528_rdtime(FAR struct rtc_lowerhalf_s *lower,
                       FAR struct rtc_time *rtctime);
static int r528_settime(FAR struct rtc_lowerhalf_s *lower,
                        FAR const struct rtc_time *rtctime);
static bool r528_havesettime(FAR struct rtc_lowerhalf_s *lower);

#ifdef CONFIG_RTC_ALARM
static int r528_setalarm(FAR struct rtc_lowerhalf_s *lower,
                         FAR const struct lower_setalarm_s *alarm_info);
static int r528_setrelative(FAR struct rtc_lowerhalf_s *lower,
                            FAR const struct lower_setrelative_s *alarm_info);
static int r528_cancelalarm(FAR struct rtc_lowerhalf_s *lower, int alarm_id);
static int r528_rdalarm(FAR struct rtc_lowerhalf_s *lower,
                        FAR struct lower_rdalarm_s *alarminfo);
#endif

#ifdef CONFIG_RTC_IOCTL
static int r528_ioctl(FAR struct rtc_lowerhalf_s *lower, int cmd, unsigned long arg);
#endif

/* Forward declaration for callback */
void vela_rtc_callback(void *param);

#ifdef CONFIG_RTC_PERIODIC
/* Function declarations */
static int r528_setperiodic(FAR struct rtc_lowerhalf_s *lower,
                             FAR const struct lower_setperiodic_s *periodic_info);
static int r528_cancelperiodic(FAR struct rtc_lowerhalf_s *lower, int periodic_id);

/* Function implementations */
static int r528_setperiodic(FAR struct rtc_lowerhalf_s *lower,
                             FAR const struct lower_setperiodic_s *periodic_info)
{
  FAR struct sunxi_rtc_lowerhalf_s *priv = (FAR struct sunxi_rtc_lowerhalf_s *)lower;
  int ret = 0;
  uint32_t delay_sec;

  DEBUGASSERT(priv && periodic_info);

  /* Cancel any existing periodic interrupt */
  if (priv->periodic_enabled)
  {
    ret = r528_cancelperiodic(lower, priv->periodic_id);
    if (ret != OK)
    {
      return ret;
    }
  }

  /* Save periodic interrupt configuration */
  priv->periodic_id = periodic_info->id;
  priv->periodic_interval = periodic_info->period;
  priv->periodic_callback = periodic_info->cb;
  priv->periodic_data = periodic_info->priv;

  /* Calculate delay in seconds with higher precision */
  uint64_t total_nsec = (uint64_t)priv->periodic_interval.tv_sec * 1000000000ULL + priv->periodic_interval.tv_nsec;
  delay_sec = (uint32_t)(total_nsec / 1000000000ULL);
  if (total_nsec % 1000000000ULL > 0) {
    delay_sec += 1;
  }

  /* Register the callback */
  if (hal_rtc_register_callback_with_data(vela_rtc_callback, (void*)priv))
  {
    wdinfo("Failed to register periodic callback\n");
    return -1;
  }

  /* Set the initial alarm */
  if (hal_rtc_set_relative_alarm(delay_sec))
  {
    wdinfo("Failed to set initial periodic alarm\n");
    return -1;
  }

  /* Enable periodic interrupts */
  priv->periodic_enabled = true;
  hal_rtc_alarm_irq_enable(1);

  return OK;
}

static int r528_cancelperiodic(FAR struct rtc_lowerhalf_s *lower, int periodic_id)
{
  FAR struct sunxi_rtc_lowerhalf_s *priv = (FAR struct sunxi_rtc_lowerhalf_s *)lower;

  DEBUGASSERT(priv);

  if (priv->periodic_enabled && priv->periodic_id == periodic_id)
  {
    /* Cancel the current alarm */
    if (hal_rtc_cancel_alarm())
    {
      wdinfo("Failed to cancel periodic alarm\n");
      return -1;
    }

    /* Disable periodic interrupts */
    priv->periodic_enabled = false;
    priv->periodic_callback = NULL;
    priv->periodic_data = NULL;
  }

  return OK;
}
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/
static const struct rtc_ops_s g_rtcops =
{
  .rdtime       = r528_rdtime,
  .settime      = r528_settime,
  .havesettime  = r528_havesettime,
#ifdef CONFIG_RTC_ALARM
  .setalarm     = r528_setalarm,
  .setrelative  = r528_setrelative,
  .cancelalarm  = r528_cancelalarm,
  .rdalarm      = r528_rdalarm,
#endif
#ifdef CONFIG_RTC_PERIODIC
  .setperiodic  = r528_setperiodic,
  .cancelperiodic = r528_cancelperiodic,
#endif
#ifdef CONFIG_RTC_IOCTL
  .ioctl        = r528_ioctl,
#endif
};

/* "Lower half" driver state */

static struct sunxi_rtc_lowerhalf_s g_rtcdev;

/****************************************************************************
 * VELA RTC CALLBACK
 ****************************************************************************/
void vela_rtc_callback(void *param)
{
  FAR struct sunxi_rtc_lowerhalf_s *cb_priv = (FAR struct sunxi_rtc_lowerhalf_s *)param;

  if (cb_priv->alarm_callback)
  {
    cb_priv->alarm_callback(cb_priv->alarm_data, cb_priv->rtc_id);
  }

  /* Handle periodic interrupts */
#ifdef CONFIG_RTC_PERIODIC
  if (cb_priv->periodic_enabled && cb_priv->periodic_callback)
  {
    /* Call the periodic callback */
    cb_priv->periodic_callback(cb_priv->periodic_data, cb_priv->periodic_id);

    /* Set the next periodic alarm with higher precision */
    uint64_t total_nsec = (uint64_t)cb_priv->periodic_interval.tv_sec * 1000000000ULL + cb_priv->periodic_interval.tv_nsec;
    uint32_t next_delay = (uint32_t)(total_nsec / 1000000000ULL);
    if (total_nsec % 1000000000ULL > 0) {
      next_delay += 1;
    }
    if (hal_rtc_set_relative_alarm(next_delay))
    {
      wdinfo("Failed to set next periodic alarm\n");
      cb_priv->periodic_enabled = false;
    }
  }
  else
#endif
  {
    /* Disable alarm for one-shot alarms to avoid repeated triggers */
    hal_rtc_alarm_irq_enable(0);
    hal_rtc_cancel_alarm();
  }
}

/****************************************************************************
 * Private Functions
 ****************************************************************************/
static int r528_rdtime(FAR struct rtc_lowerhalf_s *lower,
                       FAR struct rtc_time *rtctime)
{
  FAR struct sunxi_rtc_lowerhalf_s *priv = (FAR struct sunxi_rtc_lowerhalf_s *)lower;

  DEBUGASSERT(priv);

  if(hal_rtc_gettime(priv->config_time))
    return -1;

  /* Copy RTC time to output */
  rtctime->tm_sec = priv->config_time->tm_sec;
  rtctime->tm_min = priv->config_time->tm_min;
  rtctime->tm_hour = priv->config_time->tm_hour;
  rtctime->tm_mday = priv->config_time->tm_mday;
  rtctime->tm_mon = priv->config_time->tm_mon;
  rtctime->tm_year = priv->config_time->tm_year;
  rtctime->tm_wday = hal_rtc_convert_wday(priv->config_time);
  rtctime->tm_yday = hal_rtc_convert_yday(priv->config_time);
  rtctime->tm_isdst = 0;

  /* Apply software compensation to improve accuracy - temporarily disabled */
  /* Original compensation code removed due to nested comment issues */

  return OK;
}

static int r528_settime(FAR struct rtc_lowerhalf_s *lower,
                        FAR const struct rtc_time *rtctime)
{
  FAR struct sunxi_rtc_lowerhalf_s *priv = (FAR struct sunxi_rtc_lowerhalf_s *)lower;
  DEBUGASSERT(priv);

  priv->config_time->tm_sec = rtctime->tm_sec;
  priv->config_time->tm_min = rtctime->tm_min;
  priv->config_time->tm_hour = rtctime->tm_hour;
  priv->config_time->tm_mday = rtctime->tm_mday;
  priv->config_time->tm_mon = rtctime->tm_mon;
  priv->config_time->tm_year = rtctime->tm_year;

  if (hal_rtc_settime(priv->config_time))
     return -1;

  return OK;
}

static bool r528_havesettime(FAR struct rtc_lowerhalf_s *lower)
{
  FAR struct sunxi_rtc_lowerhalf_s *priv = (FAR struct sunxi_rtc_lowerhalf_s *)lower;
  DEBUGASSERT(priv);

  return true;
}

#ifdef CONFIG_RTC_ALARM
static int r528_setalarm(FAR struct rtc_lowerhalf_s *lower,
                         FAR const struct lower_setalarm_s *alarminfo)
{
  FAR struct sunxi_rtc_lowerhalf_s *priv = (FAR struct sunxi_rtc_lowerhalf_s *)lower;
  DEBUGASSERT(priv);

  priv->alarm_callback = alarminfo->cb;
  priv->alarm_data = alarminfo->priv;
  priv->wkalarm->enabled = 1;
  priv->wkalarm->time.tm_sec = alarminfo->time.tm_sec;
  priv->wkalarm->time.tm_min = alarminfo->time.tm_min;
  priv->wkalarm->time.tm_hour = alarminfo->time.tm_hour;
  priv->wkalarm->time.tm_mday = alarminfo->time.tm_mday;
  priv->wkalarm->time.tm_mon = alarminfo->time.tm_mon;
  priv->wkalarm->time.tm_year = alarminfo->time.tm_year;

  if (hal_rtc_register_callback_with_data(vela_rtc_callback, (void*)priv))
    return -1;

  if (hal_rtc_setalarm(priv->wkalarm))
    return -1;

  hal_rtc_alarm_irq_enable(1);

  return OK;
}

static int r528_setrelative(FAR struct rtc_lowerhalf_s *lower,
                            FAR const struct lower_setrelative_s *alarminfo)
{
  FAR struct sunxi_rtc_lowerhalf_s *priv = (FAR struct sunxi_rtc_lowerhalf_s *)lower;
  DEBUGASSERT(priv);

  priv->alarm_callback = alarminfo->cb;
  priv->alarm_data = alarminfo->priv;

  if (hal_rtc_register_callback_with_data(vela_rtc_callback,(void*)priv))
    return -1;

  if (hal_rtc_set_relative_alarm(alarminfo->reltime))
    return -1;

  hal_rtc_alarm_irq_enable(1);
  return OK;
}

static int r528_cancelalarm(FAR struct rtc_lowerhalf_s *lower, int alarmid)
{
  FAR struct sunxi_rtc_lowerhalf_s *priv = (FAR struct sunxi_rtc_lowerhalf_s *)lower;
  DEBUGASSERT(priv);

  if(hal_rtc_cancel_alarm())
	  return -1;
  return OK;
}

static int r528_rdalarm(FAR struct rtc_lowerhalf_s *lower,
                        FAR struct lower_rdalarm_s *alarminfo)
{
  FAR struct sunxi_rtc_lowerhalf_s *priv = (FAR struct sunxi_rtc_lowerhalf_s *)lower;
  DEBUGASSERT(priv);

  if (hal_rtc_getalarm(priv->wkalarm))
    return -1;

  alarminfo->priv = priv->alarm_data;
  alarminfo->time->tm_sec = priv->wkalarm->time.tm_sec;
  alarminfo->time->tm_min = priv->wkalarm->time.tm_min;
  alarminfo->time->tm_hour = priv->wkalarm->time.tm_hour;
  alarminfo->time->tm_mday = priv->wkalarm->time.tm_mday;
  alarminfo->time->tm_mon = priv->wkalarm->time.tm_mon;
  alarminfo->time->tm_year = priv->wkalarm->time.tm_year;

  return OK;
}

#endif

#ifdef CONFIG_RTC_IOCTL
static int r528_ioctl(FAR struct rtc_lowerhalf_s *lower, int cmd, unsigned long arg)
{
  FAR struct sunxi_rtc_lowerhalf_s *priv = (FAR struct sunxi_rtc_lowerhalf_s *)lower;

  DEBUGASSERT(priv);

  switch (cmd)
  {
      default:
        {
            wdinfo("Unsupported command\n");
            return -1;
        }
        break;
  }

  return OK;
}
#endif

int up_rtc_initialize(void)
{
  FAR struct sunxi_rtc_lowerhalf_s *priv = &g_rtcdev;
  FAR int handle;

  wdinfo("Entry: devpath=/dev/rtc0\n");

  hal_rtc_init();

  priv->rtc_id = 0;
  priv->rtc_s.ops = &g_rtcops;
  priv->config_time = (struct sunxi_rtc_time *)hal_malloc(sizeof(struct sunxi_rtc_time));
  priv->wkalarm =(struct rtc_wkalrm *)hal_malloc(sizeof(struct rtc_wkalrm));
  memset(priv->config_time, 0, sizeof(struct sunxi_rtc_time));
  memset(priv->wkalarm, 0, sizeof(struct rtc_wkalrm));

  /* Initialize periodic interrupt variables */
#ifdef CONFIG_RTC_PERIODIC
  priv->periodic_enabled = false;
  priv->periodic_id = -1;
  memset(&priv->periodic_interval, 0, sizeof(struct timespec));
  priv->periodic_callback = NULL;
  priv->periodic_data = NULL;
#endif

  handle = rtc_initialize(0, (FAR struct rtc_lowerhalf_s *)priv);

  if (!handle) {
      wdinfo("rtc%d register success!\n", 0);
  } else {
      wdinfo("rtc%d register faile\n", 0);
      return -ENODEV;
  }

  up_rtc_set_lowerhalf(&priv->rtc_s, true);

  return (handle == 0) ? OK : -ENODEV;
}

#endif
