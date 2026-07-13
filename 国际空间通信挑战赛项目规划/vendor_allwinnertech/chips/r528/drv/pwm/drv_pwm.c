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
#include <nuttx/timers/pwm.h>
#include <arch/board/board.h>

#include "chip.h"

#include "sunxi_hal_pwm.h"
#include "hal_mem.h"

#if defined(CONFIG_DRIVERS_PWM)

#define DUTY_CONVER_BASE   65536

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

struct sunxi_pwm_lowerhalf_s
{
	struct pwm_lowerhalf_s pwm_s;
	struct pwm_config *config_pwm;
	int channel_in;
#ifdef CONFIG_PWM_PULSECOUNT
	int num;
#endif
	void *arg;
};
/* "Lower half" driver methods **********************************************/

static int r528_setup(FAR struct pwm_lowerhalf_s *lower);
static int r528_shutdown(FAR struct pwm_lowerhalf_s *lower);

#ifdef CONFIG_PWM_PULSECOUNT
static int r528_start(FAR struct pwm_lowerhalf_s *lower,
                      FAR const struct pwm_info_s *info,
					  FAR void *handle);
#else
static int r528_start(FAR struct pwm_lowerhalf_s *lower,
                      FAR const struct pwm_info_s *info);
#endif

static int r528_stop(FAR struct pwm_lowerhalf_s *lower);
static int r528_ioctl(FAR struct pwm_lowerhalf_s *lower,
                            int cmd, unsigned long arg);

/****************************************************************************
 * Private Data
 ****************************************************************************/
static const struct pwm_ops_s g_pwmops =
{
  .setup      = r528_setup,
  .shutdown   = r528_shutdown,
  .start      = r528_start,
  .stop       = r528_stop,
  .ioctl      = r528_ioctl,
};

/* "Lower half" driver state */

static struct sunxi_pwm_lowerhalf_s g_pwmdev[8];

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int r528_setup(FAR struct pwm_lowerhalf_s *lower)
{
  FAR struct sunxi_pwm_lowerhalf_s *priv = (FAR struct sunxi_pwm_lowerhalf_s *)lower;
  DEBUGASSERT(priv);

  if(hal_pwm_pin_init(priv->channel_in))
    return -1;

  return OK;
}


static int r528_shutdown(FAR struct pwm_lowerhalf_s *lower)
{
  FAR struct sunxi_pwm_lowerhalf_s *priv = (FAR struct sunxi_pwm_lowerhalf_s *)lower;
  DEBUGASSERT(priv);

  if(hal_pwm_channel_stop(priv->channel_in))
    return -1;

  hal_pwm_pin_deinit(priv->channel_in);

  return OK;
}

#ifdef CONFIG_PWM_PULSECOUNT
static int r528_start(FAR struct pwm_lowerhalf_s *lower,
                      FAR const struct pwm_info_s *info,
					  FAR void *handle);
{
  int ret;
  FAR struct sunxi_pwm_lowerhalf_s *priv = (FAR struct sunxi_pwm_lowerhalf_s *)lower;

  priv->config_pwm->period_ns = 1000000000 / info->frequency ;
  priv->config_pwm->duty_ns = priv->config_pwm->period_ns * (1.0F * info->duty / DUTY_CONVER_BASE);
  priv->config_pwm->polarity = PWM_POLARITY_NORMAL;

  priv->num = info->count;

  wdinfo("Entry\n");
  DEBUGASSERT(priv);

  if (priv->num == 0)
    ret = hal_pwm_control(priv->channel_in, priv->config_pwm);
  else
    ret = hal_pwm_pulse_control_single( priv->channel_in, priv->config_pwm, priv->count, pwm_expired, handle);

  if (ret)
    return -1;
  return OK;
}
#else
static int r528_start(FAR struct pwm_lowerhalf_s *lower,
                      FAR const struct pwm_info_s *info)
{
  int ret;
  FAR struct sunxi_pwm_lowerhalf_s *priv = (FAR struct sunxi_pwm_lowerhalf_s *)lower;

  priv->config_pwm->period_ns = 1000000000 / info->frequency ;
  priv->config_pwm->duty_ns = priv->config_pwm->period_ns * (1.0F * info->duty / DUTY_CONVER_BASE);
  priv->config_pwm->polarity = PWM_POLARITY_NORMAL;

  wdinfo("Entry\n");
  DEBUGASSERT(priv);

  ret = hal_pwm_control(priv->channel_in, priv->config_pwm);

  if (ret)
    return -1;
  return OK;
}
#endif

static int r528_stop(FAR struct pwm_lowerhalf_s *lower)
{
  FAR struct sunxi_pwm_lowerhalf_s *priv = (FAR struct sunxi_pwm_lowerhalf_s *)lower;

  if(hal_pwm_channel_stop(priv->channel_in))
    return -1;
  return OK;
}

static int r528_ioctl(FAR struct pwm_lowerhalf_s *lower, int cmd, unsigned long arg)
{
  FAR struct sunxi_pwm_lowerhalf_s *priv = (FAR struct sunxi_pwm_lowerhalf_s *)lower;

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

int r528_pwm_initialize(FAR const char *devpath, int channel_id)
{
  FAR struct sunxi_pwm_lowerhalf_s *priv = &g_pwmdev[channel_id];
  FAR int handle;

  wdinfo("Entry: devpath=%s\n", devpath);

  hal_pwm_init();

  priv->channel_in = channel_id;
  priv->pwm_s.ops = &g_pwmops;
  priv->config_pwm = (struct pwm_config *)hal_malloc(sizeof(struct pwm_config));
  memset(priv->config_pwm, 0, sizeof(struct pwm_config));

  handle = pwm_register(devpath, (FAR struct pwm_lowerhalf_s *)priv);

  if (!handle) {
      wdinfo("pwm%d register success!\n", channel_id);
  } else {
      wdinfo("pwm%d register faile\n", channel_id);
      return -ENODEV;
  }
  return (handle == 0) ? OK : -ENODEV;
}

#endif
