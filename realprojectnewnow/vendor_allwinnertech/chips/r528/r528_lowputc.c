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

#include <stdint.h>

#include <arch/irq.h>
#include <arch/board/board.h>

#include "arm_internal.h"

#if defined (CONFIG_ARCH_BOARD_R528S3_GEMINI_S1)
#define	UART_REG_ADDR	(0x02500800)
#else
#define	UART_REG_ADDR	(0x02500000)
#endif
#define UART_REG_RBR 	(UART_REG_ADDR + 0x00)
#define UART_REG_THR 	(UART_REG_ADDR + 0x00)
#define UART_REG_DLL 	(UART_REG_ADDR + 0x00)
#define UART_REG_DLH 	(UART_REG_ADDR + 0x04)
#define UART_REG_IER 	(UART_REG_ADDR + 0x04)
#define UART_REG_IIR 	(UART_REG_ADDR + 0x08)
#define UART_REG_FCR 	(UART_REG_ADDR + 0x08)
#define UART_REG_LCR 	(UART_REG_ADDR + 0x0c)
#define UART_REG_MCR 	(UART_REG_ADDR + 0x10)
#define UART_REG_LSR 	(UART_REG_ADDR + 0x14)
#define UART_REG_MSR 	(UART_REG_ADDR + 0x18)
#define UART_REG_SCH 	(UART_REG_ADDR + 0x1c)
#define UART_REG_USR 	(UART_REG_ADDR + 0x7c)
#define UART_REG_TFL 	(UART_REG_ADDR + 0x80)
#define UART_REG_RFL 	(UART_REG_ADDR + 0x84)
#define UART_REG_HALT	(UART_REG_ADDR + 0xa4)

//uart config
#define UART_BAUDRATE	(115200)

#define uart_readb(addr)             (*((volatile unsigned char  *)(addr)))
#define uart_readw(addr)             (*((volatile unsigned short *)(addr)))
#define uart_readl(addr)             (*((volatile unsigned long  *)(addr)))
#define uart_writeb(v, addr) (*((volatile unsigned char  *)(addr)) = (unsigned char)(v))
#define uart_writew(v, addr) (*((volatile unsigned short *)(addr)) = (unsigned short)(v))
#define uart_writel(v, addr) (*((volatile unsigned long  *)(addr)) = (unsigned long)(v))


void arm_lowputc(char ch)
{
	//fifo is full, check again.
	while (!(uart_readl(UART_REG_USR) & 2)) {
		;
	}

	//write out charset to transmit fifo
	uart_writeb(ch, UART_REG_THR);
}
