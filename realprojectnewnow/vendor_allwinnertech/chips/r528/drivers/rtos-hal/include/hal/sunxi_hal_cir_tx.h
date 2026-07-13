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


#ifndef _SUNXI_IR_TX_H
#define _SUNXI_IR_TX_H

#include <hal_clk.h>
#include <hal_gpio.h>
#include <hal_reset.h>
#include <interrupt.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef CONFIG_COMPONENTS_PM
#include <pm_devops.h>
#endif

#define IR_TX_GLR		(0x00)
#define IR_TX_MCR		(0x04)
#define IR_TX_CR		(0x08)
#define IR_TX_IDC_H		(0x0c)
#define IR_TX_IDC_L		(0x10)
#define IR_TX_ICR_H		(0x14)
#define IR_TX_ICR_L		(0x18)
#define IR_TX_TELR		(0x20)
#define IR_TX_INTC		(0x24)
#define IR_TX_TACR		(0x28)
#define IR_TX_STAR		(0x2c)
#define IR_TX_TR		(0x30)
#define IR_TX_DMAC		(0x34)
#define IR_TX_FIFO_DR		(0x80)

#define IR_TX_GL_VALUE		(0xa3)
#define IR_TX_MC_VALUE		(0x86)
#define IR_TX_CLK_VALUE		(0x05 << 1)
#define IR_TX_IDC_H_VALUE	(0x04)
#define IR_TX_IDC_L_VALUE	(0x00)
#define IR_TX_TEL_VALUE		(0x96 -  1)
#define IR_TX_INT_C_VALUE	(0x01)
#define IR_TX_STA_VALUE		(0x03)
#define IR_TX_T_VALUE		(0x64)
#define IR_TX_CLK               12000000

#define IR_TX_FIFO_SIZE		(128)

#define	IR_TX_RAW_BUF_SIZE	(256)
#define IR_TX_CYCLE_TYPE	(1)	/* 1:cycle 0:non-cycle */
#define IR_TX_CLK_Ts		(1)

#define SUNXI_IR_TX_VERSION "v1.0.0"


typedef enum {
	CIR_TX_PIN_ERR = -4,
	CIR_TX_CLK_ERR = -3,
	CIR_TX_IRQ_ERR = -2,
	CIR_TX_PORT_ERR = -1,
	CIR_TX_OK = 0,
} cir_tx_status_t;

typedef enum {
	CIR_TX_CLK_DIV64 = 0x0,
	CIR_TX_CLK_DIV128 = 0x01,
	CIR_TX_CLK_DIV256 = 0x02,
	CIR_TX_CLK_DIV512 = 0x03,
	CIR_TX_CLK = 0x04,
} cir_tx_sample_clock_t;

typedef struct {
	uint32_t gpio;
	uint8_t enable_mux;
	uint8_t disable_mux;
} cir_gpio_t;

struct cmd {
	unsigned char protocol, address, command;
};

enum {
	DEBUG_INIT    = 1U << 0,
	DEBUG_INFO    = 1U << 1,
	DEBUG_SUSPEND = 1U << 2,
};

static u32 sunxi_irtx_regs_offset[] = {
	IR_TX_MCR,
	IR_TX_CR,
	IR_TX_IDC_H,
	IR_TX_IDC_L,
	IR_TX_STAR,
	IR_TX_INTC,
	IR_TX_GLR,
	IR_TX_TR,
};

struct sunxi_cir_tx_t {
	unsigned long reg_base;
	uint32_t irq;
	uint8_t status;
	cir_gpio_t *pin;

	hal_clk_t bclk;
	hal_clk_t gclk;

	hal_clk_id_t b_clk_id;
	hal_clk_id_t g_clk_id;

	hal_clk_type_t cir_tx_clk_type;

	struct reset_control *cir_reset;
	u32 regs_backup[ARRAY_SIZE(sunxi_irtx_regs_offset)];
#ifdef CONFIG_COMPONENTS_PM
	struct pm_device pm;
#endif
};

struct cir_tx_raw_buffer {
	unsigned int tx_dcnt;
	unsigned char tx_buf[IR_TX_RAW_BUF_SIZE];
};

cir_tx_status_t hal_cir_tx_init(struct sunxi_cir_tx_t **cir_tx_ptr);
void hal_cir_tx_set_duty_cycle(int duty_cycle);
void hal_cir_tx_set_carrier(int carrier_freq);
void hal_cir_tx_xmit(unsigned int *txbuf, unsigned int count);
//void send_ir_code(struct sunxi_cir_tx_t *cir_tx);

#define IR_TX_IOCSEND _IOR(66, 1, struct cmd)

#endif /* _SUNXI_IR_TX_H */

