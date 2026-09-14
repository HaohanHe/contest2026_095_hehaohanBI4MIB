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
#include <nuttx/syslog/syslog.h>

#include <sys/boardctl.h>
#include <sys/mount.h>

#include <sunxi_hal_rtc.h>
#include <sunxi_hal_common.h>

#define KEY_FIELD_MAGIC         (0x16AA0000)

#define R528_WDT_IRQ_EN 0x0
#define R528_WDT_STATUS 0x04
#define R528_WDT_SOFT_RESET 0x08
#define R528_WDT_CTRL 0x10
#define R528_WDT_CFG  0x14
#define R528_WDT_MODE  0x18
#define R528_WDT_TIMEOUT_SHIFT  4
#define R528_WDT_RESET_MASK  0x03
#define R528_WDT_RESET_MODE  0x01
#define R528_WDT_INTERRUPT_MODE 0x02

#define R528_WDT_BASE 0x020500a0

#define WDT_SOFT_RESET_FLAG     1

#define RTC_RESET_FLAG_REG  SUNXI_RTC_DATA_BASE + 0x04

struct r528_wdt_reg_t {
	uint8_t wdt_irq_en;
	uint8_t wdt_status;
	uint8_t wdt_soft_reset;
	uint8_t wdt_ctrl;
	uint8_t wdt_cfg;
	uint8_t wdt_mode;
	uint8_t wdt_timeout_shift;
	uint8_t wdt_reset_mask;
	uint8_t wdt_reset_mode;
	uint8_t wdt_interrupt_mode;
};

struct r528_wdt_dev_t {
	void *wdt_base;
	const struct r528_wdt_reg_t *wdt_regs;
};

static struct r528_wdt_reg_t r528_wdt_reg =
{
	.wdt_irq_en = R528_WDT_IRQ_EN,
	.wdt_status = R528_WDT_STATUS,
	.wdt_soft_reset = R528_WDT_SOFT_RESET,
	.wdt_ctrl = R528_WDT_CTRL,
	.wdt_cfg = R528_WDT_CFG,
	.wdt_mode = R528_WDT_MODE,
	.wdt_timeout_shift = R528_WDT_TIMEOUT_SHIFT,
	.wdt_reset_mask = R528_WDT_RESET_MASK,
	.wdt_reset_mode = R528_WDT_RESET_MODE,
	.wdt_interrupt_mode = R528_WDT_INTERRUPT_MODE,
};

static struct r528_wdt_dev_t r528_wdt_dev = {
	.wdt_base = (void *)R528_WDT_BASE,
	.wdt_regs = &r528_wdt_reg,
};

static struct r528_wdt_dev_t* watchdog_get_dev(void)
{
	return &r528_wdt_dev;
};

/* Trigger WDT to soft reset system. */
static void wdt_soft_reset_system(void){
	uint32_t reg;
	struct r528_wdt_dev_t *r528_wdt_dev_temp = NULL;
	r528_wdt_dev_temp = watchdog_get_dev();
	void *wdt_base = r528_wdt_dev_temp->wdt_base;
	const struct r528_wdt_reg_t *regs = r528_wdt_dev_temp->wdt_regs;
    /* set wdt mode:reset system */
    reg = hal_readl(wdt_base + regs->wdt_cfg);
    reg &= ~(regs->wdt_reset_mask);
    reg |= (regs->wdt_reset_mode | KEY_FIELD_MAGIC);
    hal_writel(reg, wdt_base + regs->wdt_cfg);
    /*reset system*/
    reg = hal_readl(wdt_base + regs->wdt_soft_reset);
    reg |= (WDT_SOFT_RESET_FLAG | KEY_FIELD_MAGIC);
    hal_writel(reg, wdt_base + regs->wdt_soft_reset);
}

static uint32_t write_bootreason(uint16_t cause, uint16_t flag)
{
	uint32_t value = cause;
	value <<= 16;
	value |= flag;

	hal_writel(value, RTC_RESET_FLAG_REG);
	return hal_readl(RTC_RESET_FLAG_REG);
}

/* Read and init boot flag */
uint32_t r528_read_resetflag(void)
{
	uint32_t value = hal_readl(RTC_RESET_FLAG_REG);

	if (value == 0) {
		value = write_bootreason(BOARDIOC_RESETCAUSE_SYS_CHIPPOR, 0);
	} else {
		struct r528_wdt_dev_t *dev = watchdog_get_dev();
		uint32_t wdt_value = hal_readl(R528_WDT_BASE + R528_WDT_STATUS);
		/* Check wdt pending flag before initialize it to check if hardware wdt has happened. */
		if (wdt_value == 1) {
			value = write_bootreason(BOARDIOC_RESETCAUSE_SYS_RWDT, 0);
		}
	}

	return value;
}

#ifdef CONFIG_BOARDCTL_RESET

/* To support BOARDCTL_RESET_CAUSE */
int board_reset_cause(struct boardioc_reset_cause_s *cause)
{
	if (cause != NULL)
	{
		uint32_t reason = r528_read_resetflag();
		cause->cause = reason >> 16;
		cause->flag = reason & 0xffff;
	}

	return 0;
}

#if defined(CONFIG_X4B_OTA)
extern void hal_nand_exit(void);
#endif
int board_reset(int status)
{
	write_bootreason(BOARDIOC_RESETCAUSE_CPU_SOFT, status);

	if (up_interrupt_context() || status == BOARDIOC_SOFTRESETCAUSE_PANIC)
	{
		goto reset;
	}

	syslog(LOG_ERR,"board_reset:status:%d\n",status);
	umount("/data");
	umount("/sst");
#if defined(CONFIG_X4B_OTA)
	hal_nand_exit();
#endif

reset:
	syslog_flush();
	up_mdelay(10000);
	wdt_soft_reset_system();
	return 0;
}
#endif
