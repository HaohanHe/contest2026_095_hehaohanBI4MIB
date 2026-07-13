/* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.

 * Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
 * the the People's Republic of China and other countries.
 * All Allwinner Technology Co.,Ltd. trademarks are used with permission.

 * DISCLAIMER
 * THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
 * IF YOU NEED TO INTEGRATE THIRD PARTY’S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR
 MPEGLA, ETC.)
 * IN ALLWINNERS’SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
 * ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
 * ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT
 TO MATTERS
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

#include <assert.h>

#include <nuttx/arch.h>

#include "arm_internal.h"
#include "gic.h"
#include "sctlr.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Size of the interrupt stack allocation */

#define INTSTACK_ALLOC (CONFIG_SMP_NCPUS * INTSTACK_SIZE)

/****************************************************************************
 * Public Data
 ****************************************************************************/

#if defined(CONFIG_SMP) && CONFIG_ARCH_INTERRUPTSTACK > 7
/* In the SMP configuration, we will need custom IRQ and FIQ stacks.
 * These definitions provide the aligned stack allocations.
 */

uint64_t g_irqstack_alloc[INTSTACK_ALLOC >> 3];
uint64_t g_fiqstack_alloc[INTSTACK_ALLOC >> 3];

/* These are arrays that point to the top of each interrupt stack */

uintptr_t g_irqstack_top[CONFIG_SMP_NCPUS] = {
    (uintptr_t)g_irqstack_alloc + INTSTACK_SIZE,
#if CONFIG_SMP_NCPUS > 1
    (uintptr_t)g_irqstack_alloc + (2 * INTSTACK_SIZE),
#endif
#if CONFIG_SMP_NCPUS > 2
    (uintptr_t)g_irqstack_alloc + (3 * INTSTACK_SIZE),
#endif
#if CONFIG_SMP_NCPUS > 3
    (uintptr_t)g_irqstack_alloc + (4 * INTSTACK_SIZE)
#endif
};

uintptr_t g_fiqstack_top[CONFIG_SMP_NCPUS] = {
    (uintptr_t)g_fiqstack_alloc + INTSTACK_SIZE,
#if CONFIG_SMP_NCPUS > 1
    (uintptr_t)g_fiqstack_alloc + 2 * INTSTACK_SIZE,
#endif
#if CONFIG_SMP_NCPUS > 2
    (uintptr_t)g_fiqstack_alloc + 3 * INTSTACK_SIZE,
#endif
#if CONFIG_SMP_NCPUS > 3
    (uintptr_t)g_fiqstack_alloc + 4 * INTSTACK_SIZE
#endif
};

#endif

/* Symbols defined via the linker script */

extern uint8_t _vector_start[]; /* Beginning of vector block */
extern uint8_t _vector_end[];   /* End+1 of vector block */

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: irq_enable
 *
 * Description:
 *   This function is called by up_irqinitialize() during the TEE bring up
 *   procedure, as while TEE bring up some driver need to using irq to
 *   accomplish initialization, the following function need to enable
 *   irq and fiq in tee, and enable irq in ap only
 *
 ****************************************************************************/

static inline irqstate_t irq_enable(void) {
  unsigned int cpsr;

  __asm__ __volatile__("\tmrs    %0, cpsr\n"
#ifdef CONFIG_X4B_TEE
                       "\tcpsie  if\n"
#else
                         "\tcpsie  i\n"
#endif
                       : "=r"(cpsr)
                       :
                       : "memory");

  return cpsr;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_irqinitialize
 *
 * Description:
 *   This function is called by up_initialize() during the bring-up of the
 *   system.  It is the responsibility of this function to but the interrupt
 *   subsystem into the working and ready state.
 *
 ****************************************************************************/

void up_irqinitialize(void) {
  /* The following operations need to be atomic, but since this function is
   * called early in the initialization sequence, we expect to have exclusive
   * access to the GIC.
   */

  /* Initialize the Generic Interrupt Controller (GIC) for CPU0 */

  arm_gic0_initialize(); /* Initialization unique to CPU0 */
  arm_gic_initialize();  /* Initialization common to all CPUs */

#ifdef CONFIG_ARCH_LOWVECTORS
  /* If CONFIG_ARCH_LOWVECTORS is defined, then the vectors located at the
   * beginning of the .text region must appear at address at the address
   * specified in the VBAR.  There are two ways to accomplish this:
   *
   *   1. By explicitly mapping the beginning of .text region with a page
   *      table entry so that the virtual address zero maps to the beginning
   *      of the .text region.  VBAR == 0x0000:0000.
   *
   *   2. Set the Cortex-A5 VBAR register so that the vector table address
   *      is moved to a location other than 0x0000:0000.
   *
   *  The second method is used by this logic.
   */

  /* Set the VBAR register to the address of the vector table */

  DEBUGASSERT((((uintptr_t)_vector_start) & ~VBAR_MASK) == 0);
  cp15_wrvbar((uint32_t)_vector_start);
#endif /* CONFIG_ARCH_LOWVECTORS */

#ifndef CONFIG_SUPPRESS_INTERRUPTS
  /* And finally, enable interrupts */

  irq_enable();

#endif
}

/****************************************************************************
 * Name: up_get_intstackbase
 *
 * Description:
 *   Return a pointer to the "alloc" the correct interrupt stack allocation
 *   for the current CPU.
 *
 ****************************************************************************/

#if defined(CONFIG_SMP) && CONFIG_ARCH_INTERRUPTSTACK > 7
uintptr_t up_get_intstackbase(int cpu) {
  return g_irqstack_top[cpu] - INTSTACK_SIZE;
}
#endif
