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
#include <nuttx/kmalloc.h>

#include <stdint.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/irq.h>
#include <nuttx/clock.h>
#include <nuttx/analog/adc.h>
#include <nuttx/analog/ioctl.h>
#include <arch/board/board.h>

#include "chip.h"

#include "sunxi_hal_gpadc.h"

#if defined(CONFIG_DRIVERS_GPADC)
unsigned int IRQ_test_sum;
/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

struct sunxi_gpadc_lowerhalf_s
{
	struct adc_dev_s gpadc_s;
	FAR const struct adc_callback_s *cb;	/* A reference to the outer (parent) */
	int am_channel;

	void *arg;
};


static int r528_gpadc_bind(FAR struct adc_dev_s *dev,
						   FAR const struct adc_callback_s *callback);
static void r528_gpadc_reset(FAR struct adc_dev_s *dev);
static int r528_gpadc_setup(FAR struct adc_dev_s *dev);
static void r528_gpadc_shutdown(FAR struct adc_dev_s *dev);
static void r528_gpadc_rxint(FAR struct adc_dev_s *dev, bool enable);
static int r528_gpadc_ioctl(FAR struct adc_dev_s *dev, int cmd, unsigned long arg);

static void adc_read_work(struct adc_dev_s *dev);

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static const struct adc_ops_s g_gpadcops =
{
	.ao_bind		=	r528_gpadc_bind,
	.ao_reset		=	r528_gpadc_reset,
	.ao_setup		=	r528_gpadc_setup,
	.ao_shutdown	=	r528_gpadc_shutdown,
	.ao_rxint		=	r528_gpadc_rxint,
	.ao_ioctl		=	r528_gpadc_ioctl,
};

static struct sunxi_gpadc_lowerhalf_s g_gpadcdev[2];

static int r528_gpadc_bind(FAR struct adc_dev_s *dev,
					 FAR const struct adc_callback_s *callback)
{
	FAR struct sunxi_gpadc_lowerhalf_s *priv = (FAR struct sunxi_gpadc_lowerhalf_s *)dev->ad_priv;
	DEBUGASSERT(priv);

	priv->cb = callback;

	return OK;
}

static void r528_gpadc_reset(FAR struct adc_dev_s *dev)
{

}

static int r528_gpadc_setup(FAR struct adc_dev_s *dev)
{
	int ret = -1;
	int channel;
	FAR struct sunxi_gpadc_lowerhalf_s *priv = (FAR struct sunxi_gpadc_lowerhalf_s *)dev->ad_priv;
	DEBUGASSERT(priv);

	channel = priv->am_channel;
	ret = hal_gpadc_channel_init(channel);
	if (ret) {
		hal_log_err("gpadc channel init failed!\n");
		return -1;
	}
	hal_gpadc_irq_init(channel);
	return OK;
}

static void r528_gpadc_shutdown(FAR struct adc_dev_s *dev)
{
	int channel_in;
	FAR struct sunxi_gpadc_lowerhalf_s *priv = (FAR struct sunxi_gpadc_lowerhalf_s *)dev->ad_priv;
	DEBUGASSERT(priv);

	channel_in = priv->am_channel;
	hal_gpadc_channel_exit(channel_in);
}

static void r528_gpadc_rxint(FAR struct adc_dev_s *dev, bool enable)
{
	FAR struct sunxi_gpadc_lowerhalf_s *priv = (FAR struct sunxi_gpadc_lowerhalf_s *)dev->ad_priv;
	DEBUGASSERT(priv);

	if (enable) {
		gpadc_enable_irq();
		ainfo("gpadc_enable_irq successful \n");
	} else {
		gpadc_disable_irq();
		aerr("gpadc_enable_irq fail \n");
	}
}

static void adc_read_work(struct adc_dev_s *dev)
{
	int channel_in;
	uint32_t value;
	FAR struct sunxi_gpadc_lowerhalf_s *priv = (FAR struct sunxi_gpadc_lowerhalf_s *)dev->ad_priv;
 	value = gpadc_read_channel_data(0);
	channel_in = priv->am_channel;

	priv->cb->au_receive(dev, channel_in, value);
}


static int r528_gpadc_ioctl(FAR struct adc_dev_s *dev, int cmd, unsigned long arg)
{
	FAR struct sunxi_gpadc_lowerhalf_s *priv = (FAR struct sunxi_gpadc_lowerhalf_s *)dev->ad_priv;

	DEBUGASSERT(priv);

	switch (cmd) {
	case ANIOC_TRIGGER:
	{
		/* Start sampling and read ADC value here */
		adc_read_work(dev);
	} break;

	default: {
		wdinfo("Unsupported command\n");
		return -1;
	} break;
	}

	return OK;
}

int r528_gpadc_initialize(FAR const char *devpath, int channel_id)
{
#if 1
	int ret;
	struct adc_dev_s *dev = kmm_zalloc(sizeof(struct adc_dev_s));
	g_gpadcdev[channel_id].am_channel = channel_id;
	dev->ad_ops  = &g_gpadcops;
	dev->ad_priv = &g_gpadcdev[channel_id];
	ret  = adc_register(devpath, dev);
	if (ret < 0) {
		kmm_free(dev);
	}
	return ret;
#else
	FAR struct sunxi_gpadc_lowerhalf_s *priv = &g_gpadcdev[channel_id];
	FAR void *handle = NULL;

	wdinfo("Entry: devpath=%s\n", devpath);

	hal_gpadc_init();

	priv->gpadc_s.ad_recv.af_buffer[CONFIG_DRIVERS_GPADC_CTL_NUM].am_channel = channel_id;
	priv->gpadc_s.ad_ops	= &g_gpadcops;
	priv->cb = NULL;

	handle = (void *)adc_register(devpath, (FAR struct adc_dev_s *)priv);

	if (handle != NULL) {
		wdinfo("gpadc%d init success!\n", channel_id);
	} else {
		wdinfo("gpadc%d init fail\n", channel_id);
		return -ENODEV;
	}
	return (handle != NULL) ? OK : -ENODEV;
#endif
}

#endif
