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
#include <time.h>
#include <assert.h>
#include <hal_clk.h>
#include <nuttx/arch.h>

#include <arch/irq.h>
#include <arch/board/board.h>

#include "hardware/r528_timer.h"

#include "debug.h"

static unsigned int timer_reload_val;

static unsigned int read_cntfrq(void)
{
    unsigned int cntfrq;
    unsigned int temp=24000000;
    asm volatile ("mrc  p15, 0, %0, c14, c0, 0" : "=r"(cntfrq));
    asm volatile ("mcr p15, 0, %0, c14, c0, 0" :: "r"(temp));
    asm volatile ("mrc  p15, 0, %0, c14, c0, 0" : "=r"(cntfrq));

    return cntfrq;
}


static void write_cntp_tval(unsigned int cntp_tval)
{
    asm volatile  ("mcr p15, 0, %0, c14, c2, 0" :: "r"(cntp_tval));
}

static void write_cntp_ctl(unsigned int cntp_ctl)
{
    asm volatile ("mcr p15, 0, %0, c14, c2, 1" ::"r"(cntp_ctl));
    asm volatile ("isb");
}

static void platformSetOneshotTimer(int interval)
{
    write_cntp_tval(interval);
    write_cntp_ctl(1); /* enable timer */
}

void platform_tick(int irq, void *context, void *arg)
{
    write_cntp_ctl(0); /* disable timer */
    platformSetOneshotTimer(timer_reload_val);
    nxsched_process_timer();
}

void up_timer_initialize(void)
{
    unsigned int cntfrq;

    cntfrq = read_cntfrq(); /* default: 24MHz */
    if (!cntfrq) 
    {
		return;
    }

    timer_reload_val = cntfrq /CONFIG_USEC_PER_TICK ; /* 1ms */

    /* Attach the timer interrupt vector */
    (void)irq_attach(29, (xcpt_t)platform_tick, NULL);
    platformSetOneshotTimer(timer_reload_val);
    up_enable_irq(29);
#ifdef CONFIG_DRIVERS_CCMU
    hal_clock_init();
#endif
}
