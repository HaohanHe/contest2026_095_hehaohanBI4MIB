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
#include <nuttx/nuttx.h>
#include <nuttx/syslog/syslog.h>

#include <assert.h>
#include <debug.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <hal_queue.h>
#include <hal_thread.h>
#include <hal_time.h>
#include <hal_msgbox.h>

#include <nuttx/arch.h>
#include <nuttx/kthread.h>
#include <nuttx/semaphore.h>

#include <nuttx/rpmsg/rpmsg.h>
#include <nuttx/rptun/rptun.h>
#include <nuttx/drivers/addrenv.h>

#include <arch/board/board.h>

#include "dsp_boot.h"

#define R528_DSP_WDT
#ifdef R528_DSP_WDT
#include "r528_dsp_wdt.h"
#define R528_DSP_WDT_REG_BASE		(0x1700400)
#define R528_DSP_WDT_IRQ_NUM		(154)
#define R528_DSP_WDT_TIMEOUT_MS		(16 * 1000)
#endif

#define rperr  _err
#define rpwarm _warn
#define rpinfo _info

#ifdef CONFIG_AW_RPTUN_DEBUG
#define rpdbg(fmt, args...) _info
#else
#define rpdbg(fmt, args...)
#endif

/* in sun8iw20 0 ARM, 1 DSP */
#define MSGBOX_REMOTE_ID				(1)

/* rptun initialization names */
#define SUNXI_RPTUN_CPU_NAME      CONFIG_AW_RPTUN_REMOTE_NAME

struct sunxi_rptun_dev_s {
	struct rptun_dev_s         rptun;
	struct msg_endpoint        ept;
	rptun_callback_t           callback;
	void                      *arg;
	bool                       master;
	char                       cpuname[RPMSG_NAME_SIZE + 1];
#ifdef R528_DSP_WDT
	struct sunxi_rproc_wdt     wdt;
	hal_queue_t                queue;
	void                      *async_thread;
	sem_t                      thread_sem;
#endif
};

/* r528 only support one rptun */
static struct sunxi_rptun_dev_s g_rptun_dev = { 0 };

static const char *sunxi_rptun_get_cpuname(struct rptun_dev_s *dev);
static const char *sunxi_rptun_get_firmware(struct rptun_dev_s *dev);
static const struct rptun_addrenv_s
*sunxi_rptun_get_addrenv(struct rptun_dev_s *dev);
static bool sunxi_rptun_is_autostart(struct rptun_dev_s *dev);
static bool sunxi_rptun_is_master(struct rptun_dev_s *dev);
static int sunxi_rptun_start(struct rptun_dev_s *dev);
static int sunxi_rptun_stop(struct rptun_dev_s *dev);
static int sunxi_rptun_notify(struct rptun_dev_s *dev, uint32_t notifyid);
static int sunxi_rptun_register_callback(struct rptun_dev_s *dev,
                                        rptun_callback_t callback,
                                        void *arg);

static const struct rptun_ops_s g_sunxi_rptun_ops = {
	.get_cpuname       = sunxi_rptun_get_cpuname,
	.get_firmware      = sunxi_rptun_get_firmware,
	.get_addrenv       = sunxi_rptun_get_addrenv,
	.get_resource      = NULL, /* master get resource from elf */
	.is_autostart      = sunxi_rptun_is_autostart,
	.is_master         = sunxi_rptun_is_master,
	.start             = sunxi_rptun_start,
	.stop              = sunxi_rptun_stop,
	.notify            = sunxi_rptun_notify,
	.register_callback = sunxi_rptun_register_callback,
};

static void msgbox_recv_callback(uint32_t data, void *priv)
{
	int ret = 0;
	struct sunxi_rptun_dev_s *dev = priv;

	rpdbg("%s: receive kick:%d\r\n", dev->cpuname, data);

	if (dev != NULL && dev->callback != NULL)
		dev->callback(dev->arg, data);
}

static const char *sunxi_rptun_get_cpuname(struct rptun_dev_s *dev)
{
	struct sunxi_rptun_dev_s *priv = container_of(dev,
										 struct sunxi_rptun_dev_s,
										 rptun);

	return priv->cpuname;
}

static const char *sunxi_rptun_get_firmware(struct rptun_dev_s *dev)
{
	return CONFIG_AW_RPTUN_FIRMWARE;
}

static const struct rptun_addrenv_s dsp_memapping[] = {
	/* cacheable */
	/* local SRAM - IRAM */
	{ .da = 0x20028000, .pa = 0x28000, .size = 0x10000 },
	/* local SRAM - DRAM0 */
	{ .da = 0x20038000, .pa = 0x38000, .size = 0x8000 },
	/* local SRAM - DRAM1 */
	{ .da = 0x20040000, .pa = 0x40000, .size = 0x8000 },
	/* DDR front 256MB */
	{ .da = 0x30000000, .pa = 0x40000000, .size = 0x10000000 },
	/* DDR front 1GB */
	{ .da = 0xC0000000, .pa = 0x40000000, .size = 0x40000000 },

	/* non-cacheable */
	/* local SRAM via external bus */
	{ .da = 0x28000, .pa = 0x28000, .size = 0x20000 },
	/* local SRAM via internal bus */
	{ .da = 0x400000, .pa = 0x28000, .size = 0x10000 },
	{ .da = 0x420000, .pa = 0x38000, .size = 0x8000 },
	{ .da = 0x440000, .pa = 0x40000, .size = 0x8000 },
	/* DDR front 256MB */
	{ .da = 0x10000000, .pa = 0x40000000, .size = 0x10000000 },
	/* DDR front 1GB */
	{ .da = 0x40000000, .pa = 0x40000000, .size = 0x40000000 },
	/*
	 * DDR front 1GB (cacheable configurable)
	 *  In init-sun8iw20.c it's configured non-cacheable.
	 */
	{ .da = 0xC0000000, .pa = 0x40000000, .size = 0x40000000 },

	/* end */
	{ .da = 0, .pa = 0, .size = 0},
};

static const struct rptun_addrenv_s *
sunxi_rptun_get_addrenv(struct rptun_dev_s *dev)
{
	return dsp_memapping;
}

static bool sunxi_rptun_is_autostart(struct rptun_dev_s *dev)
{
	return false;
}

static bool sunxi_rptun_is_master(struct rptun_dev_s *dev)
{
	struct sunxi_rptun_dev_s *priv = container_of(dev,
											 struct sunxi_rptun_dev_s,
											 rptun);
	return priv->master;
}

static int sunxi_rptun_start(struct rptun_dev_s *dev)
{
#ifdef R528_DSP_WDT
	int ret;
	struct sunxi_rptun_dev_s *priv = container_of(dev,
											 struct sunxi_rptun_dev_s,
											 rptun);
	rpinfo("%s: sunxi_rproc_wdt_start\r\n", priv->cpuname);
	ret = sunxi_rproc_wdt_start(&priv->wdt);
	if (ret) {
		rperr("%s: sunxi_rproc_wdt_start failed\r\n", priv->cpuname);
	}
#endif

	rpdbg("start dsp...(0x400660)\r\n");
	sunxi_dsp_start(0x400660);
	return 0;
}

static int sunxi_rptun_stop(struct rptun_dev_s *dev)
{
#ifdef R528_DSP_WDT
	struct sunxi_rptun_dev_s *priv = container_of(dev,
											 struct sunxi_rptun_dev_s,
											 rptun);
#endif

	rpinfo("stop dsp...\r\n");
	sunxi_dsp_stop();

#ifdef R528_DSP_WDT
	rpinfo("%s: sunxi_rproc_wdt_stop\r\n", priv->cpuname);
	sunxi_rproc_wdt_stop(&priv->wdt);
#endif

	return 0;
}

#ifdef R528_DSP_WDT

#define RPTUN_CRASH_REPORT	(0x1<<0)
#define RPTUN_DEINIT		(0x1<<1)

struct r528_rptun_msg_type {
	int flags;
};

static void r528_dsp_wdt_callback(struct sunxi_rproc_wdt *wdt, void *cb_priv)
{
	struct sunxi_rptun_dev_s *priv = container_of(wdt,
										struct sunxi_rptun_dev_s,
										wdt);
	struct r528_rptun_msg_type msg;
	rperr("%s: crash detected\r\n", priv->cpuname);
	msg.flags = RPTUN_CRASH_REPORT;
	if (hal_queue_send_wait(priv->queue, &msg, HAL_WAIT_FOREVER))
		rperr("%s: crash report failed!\r\n", priv->cpuname);

	syslog_flush();
	// TODO
}

static void r528_rptun_async_thread(void *arg)
{
	struct sunxi_rptun_dev_s *dev = (struct sunxi_rptun_dev_s *)arg;
	hal_queue_t queue = dev->queue;
	struct r528_rptun_msg_type msg;
	int ret;

	while (1) {
		if ((ret = hal_queue_recv(queue, &msg, HAL_WAIT_FOREVER))) {
			//printf("%s: hal_queue_recv return %d\n", __func__, ret);
			continue;
		}

		if (msg.flags & RPTUN_CRASH_REPORT) {
			rperr("%s: crash detected, reload...\r\n", dev->cpuname);
			int rptun_boot(FAR const char *cpuname);
			rptun_boot(dev->cpuname);
			rperr("%s: reload finish\r\n", dev->cpuname);
			continue;
		}

		if (msg.flags & RPTUN_DEINIT) {
			rperr("%s: %s exit\r\n", dev->cpuname, __func__);
			break;
		}
	}

	sem_post(&dev->thread_sem);
	hal_thread_stop(dev->async_thread);
}

#endif

static int sunxi_rptun_notify(struct rptun_dev_s *dev, uint32_t notifyid)
{
	int ret;
	struct sunxi_rptun_dev_s *priv = container_of(dev,
										struct sunxi_rptun_dev_s,
										rptun);

	rpdbg("%s: notify:%d\r\n", priv->cpuname, notifyid);

	ret = hal_msgbox_channel_send(&priv->ept,
					(uint8_t *)&notifyid, sizeof(notifyid));
	if (ret != 0) {
		rperr("%s: failed to send message to remote\n", priv->cpuname);
		return -EBUSY;
	}

	return OK;
}

static int sunxi_rptun_register_callback(struct rptun_dev_s *dev,
                                        rptun_callback_t callback,
                                        void *arg)
{
	struct sunxi_rptun_dev_s *priv = container_of(dev,
										struct sunxi_rptun_dev_s,
										rptun);

	priv->callback = callback;
	priv->arg      = arg;

	return 0;
}

int msgbox_ipi_init(struct msg_endpoint *ept,
				void (*callback)(uint32_t data, void *priv), void *priv)
{
	int ret = 0;

	ept->rec = callback;
	ept->private = priv;

	ret = hal_msgbox_alloc_channel(ept, MSGBOX_REMOTE_ID,
					CONFIG_MBOX_CHANNEL, CONFIG_MBOX_CHANNEL);
	if(ret != 0) {
		rperr("Failed to allocate msgbox channel\n");
		return -EFAULT;
	}

	return 0;
}

void msgbox_ipi_deinit(struct msg_endpoint *ept)
{
	hal_msgbox_free_channel(ept);
}

static struct sunxi_rptun_dev_s *
_sunxi_rptun_init(const char *cpuname)
{
	struct sunxi_rptun_dev_s *dev = &g_rptun_dev;
	int ret;
#ifdef R528_DSP_WDT
	struct sunxi_rproc_wdt_param r528_dsp_wdt_param = {
		.cb = r528_dsp_wdt_callback,
		.cb_priv = NULL,
		.reg_base = (void *)R528_DSP_WDT_REG_BASE,
		.irq_num = R528_DSP_WDT_IRQ_NUM,
		.timeout_ms = R528_DSP_WDT_TIMEOUT_MS,
		.reset_type = RESET_INT,
	};
#endif

	rpinfo("init msgbox ipi\n");
	ret = msgbox_ipi_init(&dev->ept, msgbox_recv_callback, dev);
	if (ret != 0) {
		rperr("ERROR: Not able to init msgbox irq\n");
		goto ipi_err;
	}

	dev->master = true;
	dev->rptun.ops = &g_sunxi_rptun_ops;
	strlcpy(dev->cpuname, cpuname, sizeof(dev->cpuname));

#ifdef R528_DSP_WDT
	sem_init(&dev->thread_sem, 0, 0);
	ret = sunxi_rproc_wdt_init(&dev->wdt, &r528_dsp_wdt_param);
	if (ret) {
		rperr("%s: %s sunxi_rproc_wdt_init failed\n", dev->cpuname, __func__);
		goto err_out_sem_destroy;
	}
	dev->queue = hal_queue_create("rptun_queue", sizeof(struct r528_rptun_msg_type), 4);
	if (!dev->queue) {
		rperr("%s: %s hal_queue_create failed\n", dev->cpuname, __func__);
		goto err_out_wdt_deinit;
	}
	dev->async_thread = hal_thread_create(r528_rptun_async_thread, dev, "rptun_async", 8192, 120);
	if (!dev->queue) {
		rperr("%s: %s hal_queue_create failed\n", dev->cpuname, __func__);
		goto err_out_queue_delete;
	}
#endif

	rpinfo("init rptun dev\n");
	ret = rptun_initialize(&dev->rptun);
	if (ret < 0) {
		rperr("ERROR: Not able to init rptun\n");
		goto out;
	}

	rpinfo(" OK \n");

	return dev;

#ifdef R528_DSP_WDT
err_out_queue_delete:
	hal_queue_delete(dev->queue);
err_out_wdt_deinit:
	sunxi_rproc_wdt_deinit(&dev->wdt);
err_out_sem_destroy:
	sem_destroy(&dev->thread_sem);
#endif
ipi_err:
	msgbox_ipi_deinit(&dev->ept);
out:
	return NULL;
}

void _sunxi_rptun_deinit(struct sunxi_rptun_dev_s *dev)
{
#ifdef R528_DSP_WDT
	struct r528_rptun_msg_type msg;
#endif

	msgbox_ipi_deinit(&dev->ept);
#ifdef R528_DSP_WDT
	msg.flags = RPTUN_DEINIT;
	sunxi_rproc_wdt_deinit(&dev->wdt);
	hal_queue_send_wait(dev->queue, &msg, HAL_WAIT_FOREVER);
	sem_wait(&dev->thread_sem);
	sem_destroy(&dev->thread_sem);
	hal_queue_delete(dev->queue);
#endif
}

int sunxi_rptun_init(void)
{
	struct sunxi_rptun_dev_s *dev;

	/* Initialize and wait for the master. This will block until. */
	rpinfo("init " SUNXI_RPTUN_CPU_NAME "...\n");
	dev = _sunxi_rptun_init(SUNXI_RPTUN_CPU_NAME);
	if (!dev) {
		rperr("ERROR: Not able to init sunxi_rptun\n");
		return -EFAULT;
	}
	rpinfo("init " SUNXI_RPTUN_CPU_NAME " OK\n");

	return OK;
}

uintptr_t up_addrenv_va_to_pa(void *va)
{
	return (uintptr_t)va;
}

void *up_addrenv_pa_to_va(uintptr_t pa)
{
	return (void *)pa;
}
