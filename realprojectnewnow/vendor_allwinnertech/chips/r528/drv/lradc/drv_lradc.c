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
#include <nuttx/analog/adc.h>
#include <nuttx/analog/ioctl.h>
#include <arch/board/board.h>

#include "chip.h"

#include "sunxi_hal_lradc.h"
#include "hal_gpio.h"
#include <nuttx/input/buttons.h>
#include <../../nuttx/drivers/input/button_upper.c>
#include <arch/chip/mi_hw_version.h>

#ifndef HW_VERSION_DVT3
#define HW_VERSION_DVT3 3
#endif
static int g_hw_version;

#define NUM_ADC_BUTTONS 5
#define BUTTON_1 0x1
#define BUTTON_2 0x2
#define BUTTON_3 0x4
#define BUTTON_4 0x8
#define BUTTON_5 0x10

int adckey_buttons[NUM_ADC_BUTTONS];

#if defined(CONFIG_DRIVERS_LRADC)
/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static btn_buttonset_t r528_bl_supported(FAR const struct btn_lowerhalf_s *lower);
static btn_buttonset_t r528_bl_buttons(FAR const struct btn_lowerhalf_s *lower);
static void r528_bl_enable(FAR const struct btn_lowerhalf_s *lower,
					  btn_buttonset_t press, btn_buttonset_t release,
					  btn_handler_t handler, FAR void *arg);
static void lradc_irq_callback(uint32_t irq_status, uint32_t data);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct btn_lowerhalf_s g_btnlower =
{
	.bl_supported	=	r528_bl_supported,
	.bl_buttons		=	r528_bl_buttons,
	.bl_enable		=	r528_bl_enable,
};

static btn_handler_t g_btnhandler;
static FAR void *g_btnarg;
static uint32_t gLastButton = 0;

static btn_buttonset_t r528_bl_supported(FAR const struct btn_lowerhalf_s *lower)
{
	btn_buttonset_t tmp_data;
	tmp_data = (btn_buttonset_t)((1 << NUM_ADC_BUTTONS)-1);
	return tmp_data;
}

static btn_buttonset_t r528_bl_buttons(FAR const struct btn_lowerhalf_s *lower)
{
	uint32_t retval;
	uint32_t ret=0;
	//retval = hal_lradc_register_callback(lradc_irq_callback);
	retval = hal_lradc_get_data();
	syslog(LOG_INFO,"r528_bl_buttons:retval:%lu\n",retval);
	if (HW_VERSION_DVT3 == g_hw_version) {
		if(retval>=8 && retval<=12)
			ret = BUTTON_1;
		else if(retval>=32 && retval<=36)
			ret = BUTTON_4;
		else if(retval>=40 && retval<=44)
			ret = BUTTON_5;
		else if(retval>=24 && retval<=28)
			ret = BUTTON_3;
		else if(retval>=16 && retval<=20)
			ret = BUTTON_2;
	} else {
		if(retval>=8 && retval<=12)
			ret = BUTTON_1;
		else if(retval>=17 && retval<=21)
			ret = BUTTON_2;
		else if(retval>=25 && retval<=29)
			ret = BUTTON_3;
		else if(retval>=33 && retval<=37)
			ret = BUTTON_4;
		else if(retval>=39 && retval<=43)
			ret = BUTTON_5;
	}
	return ret;
}

static void r528_bl_enable(FAR const struct btn_lowerhalf_s *lower,
						   btn_buttonset_t press, btn_buttonset_t release,
						   btn_handler_t handler, FAR void *arg)
{
	uint32_t id;
	btn_buttonset_t mask;
	btn_buttonset_t either = press | release;

	if (either && handler) {
		g_btnhandler = handler;
		g_btnarg = arg;

		for (id = 0; id < NUM_ADC_BUTTONS; id++) {
			mask = (1 << id);
			if ((either & mask) != 0) {
				hal_lradc_register_callback(lradc_irq_callback);
			}
		}
	}
}

static void lradc_irq_callback(uint32_t irq_status, uint32_t data)
{
	uint32_t notify_data = 0;
	if (HW_VERSION_DVT3 == g_hw_version) { //DVT3
		if(data>=32 && data<=36)
			notify_data = BUTTON_4;
		else if(data>=40 && data<=44)
			notify_data = BUTTON_5;
		else if(data>=24 && data<=28)
			notify_data = BUTTON_3;
		else if(data>=16 && data<=20)
			notify_data = BUTTON_2;
		else if(data>=8 && data<=12)
			notify_data = BUTTON_1;
	} else { //DVT1 DVT2
		if(data>=8 && data<=12)
			notify_data = BUTTON_1;
		else if(data>=17 && data<=21)
			notify_data = BUTTON_2;
		else if(data>=25 && data<=29)
			notify_data = BUTTON_3;
		else if(data>=33 && data<=37)
			notify_data = BUTTON_4;
		else if(data>=39 && data<=43)
			notify_data = BUTTON_5;
	}
	if (gLastButton != notify_data) {
		syslog(LOG_INFO,"gLastButton=%lu,notify_data=%lu\n",gLastButton,notify_data);
		gLastButton = notify_data;
		if (g_btnhandler)
			g_btnhandler(&g_btnlower,g_btnarg);
	}
}

int r528_button_initialize(FAR const char *devname)
{
	int ret = -1;
	g_hw_version = hw_version_get();
	syslog(LOG_INFO,"hardware version %d\n", g_hw_version);
	ret = hal_lradc_init();
	if (ret) {
		wdinfo("lradc init failed!\n");
		return -1;
	}
	ret = btn_register(devname, &g_btnlower);
	if (ret) {
		wdinfo("btn_register init fail\n");
		return -ENODEV;
	}
	wdinfo("btn_register init success!\n");

	hal_lradc_register_callback(lradc_irq_callback);

	return OK;
}

#endif
