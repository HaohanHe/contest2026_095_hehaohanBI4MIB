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
#ifndef __ARCH_ARM_SRC_R528_CHIP_H
#define __ARCH_ARM_SRC_R528_CHIP_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#ifndef __ASSEMBLY__
#  include <nuttx/arch.h>
#endif

#include "r528_irq.h"
#include "hardware/r528_memorymap.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* MPCore registers are memory mapped and accessed through a processor
 * specific private address space via the SCU.  The Cortex-A9 MCU chip.h
 * header file must provide the definition CHIP_MPCORE_VBASE to access this
 * the registers in this memory region.
 */

/****************************************************************************
 * Public Types
 ****************************************************************************/

/****************************************************************************
 * Public Data
 ****************************************************************************/

#ifdef __ASSEMBLY__

#if defined(CONFIG_SMP) && CONFIG_ARCH_INTERRUPTSTACK > 7
	.globl	g_irqstack_top
	.globl	g_fiqstack_top
#endif /* CONFIG_SMP && CONFIG_ARCH_INTERRUPTSTACK > 7 */

#endif /* __ASSEMBLY__ */

/****************************************************************************
 * Macro Definitions
 ****************************************************************************/

#ifdef __ASSEMBLY__

/***************************************************************************
 * Name: cpuindex
 *
 * Description:
 *   Return an index idenifying the current CPU.
 *
 ****************************************************************************/

#if defined(CONFIG_SMP) && CONFIG_ARCH_INTERRUPTSTACK > 7
	.macro	cpuindex, index
	mrc		p15, 0, \index, c0, c0, 5	/* Read the MPIDR */
	and		\index, \index, #3			/* Bits 0-1=CPU ID */
	.endm
#endif

/***************************************************************************
 * Name: setirqstack
 *
 * Description:
 *   Set the current stack pointer to the  -"top" of the IRQ interrupt
 *   stack for the current CPU.
 *
 ***************************************************************************/

#if defined(CONFIG_SMP) && CONFIG_ARCH_INTERRUPTSTACK > 7
	.macro	setirqstack, tmp1, tmp2
	mrc		p15, 0, \tmp1, c0, c0, 5	/* tmp1=MPIDR */
	and		\tmp1, \tmp1, #3			/* Bits 0-1=CPU ID */
	ldr		\tmp2, =g_irqstack_top		/* tmp2=Array of IRQ stack pointers */
	lsls	\tmp1, \tmp1, #2			/* tmp1=Array byte offset */
	add		\tmp2, \tmp2, \tmp1			/* tmp2=Offset address into array */
	ldr		sp, [\tmp2, #0]				/* sp=Address in stack allocation */
	.endm
#endif

/****************************************************************************
 * Name: setfiqstack
 *
 * Description:
 *   Set the current stack pointer to the  -"top" of the FIQ interrupt
 *   stack for the current CPU.
 *
 ****************************************************************************/

#if defined(CONFIG_SMP) && CONFIG_ARCH_INTERRUPTSTACK > 7
	.macro	setfiqstack, tmp1, tmp2
	mrc		p15, 0, \tmp1, c0, c0, 5	/* tmp1=MPIDR */
	and		\tmp1, \tmp1, #3			/* Bits 0-1=CPU ID */
	ldr		\tmp2, =g_fiqstack_top		/* tmp2=Array of FIQ stack pointers */
	lsls	\tmp1, \tmp1, #2			/* tmp1=Array byte offset */
	add		\tmp2, \tmp2, \tmp1			/* tmp2=Offset address into array */
	ldr		sp, [\tmp2, #0]				/* sp=Address in stack allocation */
	.endm
#endif

#endif /* __ASSEMBLY__ */

/****************************************************************************
 * Inline Functions
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

#endif /* __ARCH_ARM_SRC_R528_CHIP_H */
