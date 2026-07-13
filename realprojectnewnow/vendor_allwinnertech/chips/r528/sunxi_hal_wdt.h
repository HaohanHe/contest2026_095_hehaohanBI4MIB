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

#ifndef __ARCH_ARM_SRC_SUNXI_HAL_WDT_H
#define __ARCH_ARM_SRC_SUNXI_HAL_WDT_H

struct sunxi_wdt_reg_t {
	uint8_t wdt_irq_en;
	uint8_t wdt_status;
	uint8_t wdt_soft_reset;
	uint8_t wdt_ctrl;
	uint8_t wdt_cfg;
	uint8_t wdt_mode;
	uint8_t wdt_out_cfg;
	uint8_t wdt_timeout_shift;
	uint8_t wdt_reset_mask;
	uint8_t wdt_reset_mode;
	uint8_t wdt_interrupt_mode;
};

struct sunxi_wdt_info_t {
	char dev_name[64];
	char drv_name[64];
	char drv_version[64];
};

struct sunxi_wdt_dev_t {
	void *wdt_base;
	const struct sunxi_wdt_reg_t *wdt_regs;
	unsigned int timeout;
	unsigned int max_timeout;
	unsigned int min_timeout;
	unsigned int irq_num;
	struct sunxi_wdt_info_t *wdt_info;
	struct sunxi_hal_driver_watchdog *wdt_ops;
};

struct sunxi_hal_driver_watchdog {
	int (*get_info)(struct sunxi_wdt_dev_t *sunxi_wdt_dev, struct sunxi_wdt_info_t **wdt_info);
	int (*start)(struct sunxi_wdt_dev_t *sunxi_wdt_dev);
	int (*stop)(struct sunxi_wdt_dev_t *sunxi_wdt_dev);
	int (*ping)(struct sunxi_wdt_dev_t *sunxi_wdt_dev);
	int (*set_timeout)(struct sunxi_wdt_dev_t *sunxi_wdt_dev, unsigned int timeout);
	int (*restart)(struct sunxi_wdt_dev_t *sunxi_wdt_dev);
};

int sunxi_wdt_get_info(struct sunxi_wdt_dev_t *sunxi_wdt_dev, struct sunxi_wdt_info_t **wdt_info);
int sunxi_wdt_start(struct sunxi_wdt_dev_t *sunxi_wdt_dev);
int sunxi_wdt_stop(struct sunxi_wdt_dev_t *sunxi_wdt_dev);
int sunxi_wdt_ping(struct sunxi_wdt_dev_t *sunxi_wdt_dev);
int sunxi_wdt_set_timeout(struct sunxi_wdt_dev_t *sunxi_wdt_dev, unsigned int timeout);
int sunxi_wdt_restart(struct sunxi_wdt_dev_t *sunxi_wdt_dev);
int sunxi_wdt_init(struct sunxi_wdt_dev_t **sunxi_wdt_dev, int init_mode);
int sunxi_wdt_exit(struct sunxi_wdt_dev_t *sunxi_wdt_dev);

#define KEY_FIELD_MAGIC         (0x16AA0000)

#endif /* __ARCH_ARM_SRC_SUNXI_HAL_WDT_H */
