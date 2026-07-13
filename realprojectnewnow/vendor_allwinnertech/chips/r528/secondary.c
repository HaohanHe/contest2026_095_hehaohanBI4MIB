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
#include <stdint.h>
#include <sunxi_hal_common.h>
#include <hal_time.h>

#define SUNXI_CPUXCFG_BASE    0x09010000
#define SUNXI_CPUSCFG_BASE    0x07000400
#define CPU1_SOFT_ENT_REG(x)  (0x1c4 + (x * 4))
#define C0_RST_CTRL           (0x00)
#define C0_CTRL_REG0          (0x10)
#define C0_CPU_STATUS         (0X80)
#define CORE_RESET_OFFSET     (0x00)
#define L1_RST_DISABLE_OFFSET (0x00)
#define STANDBYWFI_OFFSET     (0x10)

#define SPC_BASE		(0x03008000)
#define SUNXI_CCM_BASE		(0x03001000)
#define SUNXI_DMA_BASE  	(0x03002000)
#define SPC_STA_REG(x)		(SPC_BASE+0x10*(x)+0x0)
#define SPC_SET_REG(x)		(SPC_BASE+0x10*(x)+0x4)
#define SPC_CLR_REG(x)		(SPC_BASE+0x10*(x)+0x8)

void r528_set_cpu1_boot_entry(uint32_t entry)
{
	int cpu = 1;
    writel(entry, SUNXI_CPUSCFG_BASE + CPU1_SOFT_ENT_REG(cpu));
    hal_mdelay(10);

    isb();
    dsb();
    dmb();
}

static void r528_enable_cpu1(int cpu)
{
	unsigned int value;
    /* assert cpu core reset low */
    value = readl(SUNXI_CPUXCFG_BASE + C0_RST_CTRL);
    value &= (~(0x1 << (CORE_RESET_OFFSET + cpu)));
    writel(value, SUNXI_CPUXCFG_BASE + C0_RST_CTRL);

    hal_mdelay(10);
    /* L1RSTDISABLE hold low */
    value = readl(SUNXI_CPUXCFG_BASE + C0_CTRL_REG0);
    value &= (~(0x1 << (L1_RST_DISABLE_OFFSET + cpu)));
    writel(value, SUNXI_CPUXCFG_BASE + C0_CTRL_REG0);

    hal_mdelay(20);

    /* Deassert core reset high */
    value = readl(SUNXI_CPUXCFG_BASE + C0_RST_CTRL);
    value |= (0x1 << (CORE_RESET_OFFSET + cpu));
    writel(value, SUNXI_CPUXCFG_BASE + C0_RST_CTRL);
    hal_mdelay(10);

    isb();
    dsb();
    dmb();
}

void sencond_cpu_bootup(void)
{
    asm volatile( "NOP" );
    asm volatile( "NOP" );
    asm volatile( "NOP" );
    r528_enable_cpu1(1);
    asm volatile("isb");
    asm volatile("dmb");
    asm volatile("dmb");
}
