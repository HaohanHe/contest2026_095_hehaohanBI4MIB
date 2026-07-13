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

#ifndef __ARCH_ARM_SRC_R528_HARDWARE_R528_MEMORYMAP_H
#define __ARCH_ARM_SRC_R528_HARDWARE_R528_MEMORYMAP_H

/************************************************************************************
 * Included Files
 ************************************************************************************/

#include <nuttx/config.h>
#include <arch/chip/chip.h>

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/
/* Decimal configuration values may exceed 2Gb and, hence, overflow to negative
 * values unless we force them to unsigned long:
 */

#define __CONCAT(a,b) a ## b
#define MKULONG(a) __CONCAT(a,ul)

/* r528 physical section base addresses (aligned to 1MB boundaries) */
#define R528_INTMEM_PSECTION  0x00000000  /* Internal memory 0x0002:0000-0x0003:ffff */
#define R528_DSP_PSECTION     0x01700000  /* DSP SYS         0x0170:0000-0x0170:1fff */
#define R528_VE_PSECTION      0x01c00000  /* CE              0x0190:4000-0x0190:4fff */
#define R528_SP0_PSECTION     0x02000000  /* system          0x0300:0000-0x0302:ffff */
#define R528_SP1_PSECTION     0x02500000  /* system          0x0300:0000-0x0302:ffff */
#define R528_SH0_PSECTION     0x03000000  /* Rtc             0x0700:0000-0x0700:03ff */
#define R528_SYS_PSECTION     0x03000000
#define R528_SH2_PSECTION     0x04000000  /* Storage         0x0401:1000-0x0400:5fff */
#define R528_VIDEO_OUT_PSECTION  0x05000000  /* Peripherals     0x0500:0000-0x0549:ffff */
#define R528_VIDEO_IN_PSECTION   0x05800000  /* Ledc            0x0670:0000-0x0670:03ff */
#define R528_APBS0_PSECTION   0x07000000  /* Ledc            0x0670:0000-0x0670:03ff */
#define R528_AHBS_PSECTION    0x07090000  /* Ledc            0x0670:0000-0x0670:03ff */
#define R528_CPUX_PSECTION    0x08100000  /* Cpux            0x0810:0000-0x0902:0fff */
#define R528_DRAM_PSECTION    0x40000000  /* Dram            0x4000:0000-0xffff:ffff */
#define R528_BROM_PSECTION    0x00000000  /* BROM            0x0000:0000-0x0000:7fff */

/* R528 Offsets from the internal memory section base address */
#define R528_NBROM_OFFSET    0x00000000 /* N-BROM         0x0000:0000-0x0000:7fff 32K  */
#define R528_SBROM_OFFSET    0x00000000 /* S-BROM         0x0000:0000-0x0000:7fff 32K  */
#define R528_SRAMA1_OFFSET   0x00020000 /* SRAM A1 + local sram        0x0002:0000-0x0004:7fff 160K */

/* R528 offsets from the system section base address */
#define R528_SYS_CFG_OFFSET     0x00000000 /* SYS_CFG    0x0300:0000-0x0300:0fff 4K */
#define R528_CCMU_OFFSET        0x00001000 /* CCMU       0x0300:1000-0x0300:1fff 4K */
#define R528_DMAC_OFFSET        0x00002000 /* DMAC       0x0300:2000-0x0300:2fff 4K */
#define R528_HSTIMER_OFFSET     0x00008000 /* HSTIMER    0x0300:5000-0x0300:5fff 4K */
#define R528_SID_OFFSET         0x00006000 /* SID        0x0300:6000-0x0300:6fff 4K */
#define R528_SMC_OFFSET         0x00007000 /* SMC        0x0300:7000-0x0300:7fff 4K */
#define R528_SPC_OFFSET         0x00008000 /* SPC        0x0300:8000-0x0300:83ff 1K */
#define R528_TIMER_OFFSET       0x00050000 /* TIMER      0x0300:9000-0x0300:93ff 1K */
#define R528_PWM_OFFSET         0x0000C000 /* PWM        0x0300:A000-0x0300:A3ff 1K */
#define R528_GPIO_OFFSET        0x00000000 /* GPIO       0x0300:B000-0x0300:B3ff 1K */
#define R528_PSI_OFFSET         0x0000C000 /* PSI        0x0300:C000-0x0300:C3ff 1K */
#define R528_DCU_OFFSET         0x00010000 /* DCU        0x0301:0000-0x0301:ffff 64K */
#define R528_GIC_OFFSET         0x00020000 /* GIC        0x0302:0000-0x0301:ffff 64K */

/* R528 offsets from the rtc section base address */
#define R528_RTC_OFFSET         0x00000000 /* RTC        0x0700:0000-0x0700:03ff 1K */

/* R528 offsets from the storage section base address */
#define R528_SMHC0_OFFSET       0x00000000 /* SMHC0      0x0402:1000-0x0402:1fff 4K  */
#define R528_SMHC1_OFFSET       0x00001000 /* SMHC1      0x0402:1000-0x0402:1fff 4K  */
#define R528_SMHC2_OFFSET       0x00002000 /* SMHC2      0x0402:1000-0x0402:1fff 4K  */
#define R528_MSI_CTRL_OFFSET    0x00002000 /* MSI_CTRL   0x0400:2000-0x0400:2fff 4K  */
#define R528_DRAM_PHY_OFFSET    0x00003000 /* DRAM_PHY   0x0400:3000-0x0400:5fff 12K */

/* R528 offsets from the periph section base address */
#define R528_UART0_OFFSET       0x00000000 /* UART0      0x0500:0000-0x0500:03ff 1K  */
#define R528_UART1_OFFSET       0x00000400 /* UART1      0x0500:0400-0x0500:07ff 1K  */
#define R528_UART2_OFFSET       0x00000800 /* UART2      0x0500:0800-0x0500:0bff 1K  */
#define R528_UART3_OFFSET       0x00000C00 /* UART3      0x0500:0C00-0x0500:0fff 1K  */
#define R528_TWI0_OFFSET        0x00002000 /* TWI0       0x0500:2000-0x0500:23ff 1K  */
#define R528_TWI1_OFFSET        0x00002400 /* TWI1       0x0500:2400-0x0500:27ff 1K  */
#define R528_SPI0_OFFSET        0x00010000 /* SPI0       0x0501:0000-0x0501:0fff 4K  */
#define R528_SPI1_OFFSET        0x00011000 /* SPI1       0x0501:1000-0x0501:1fff 4K  */
#define R528_GPADC_OFFSET       0x00070000 /* GPADC      0x0507:0000-0x0507:03ff 1K  */
#define R528_THS_OFFSET         0x00070400 /* THS        0x0507:0400-0x0507:07ff 1K  */
#define R528_LRADC_OFFSET       0x00070800 /* LRADC      0x0507:0800-0x0507:0bff 1K  */
#define R528_I2S0_OFFSET        0x00090000 /* I2S0       0x0509:0000-0x0509:0fff 4K  */
#define R528_I2S1_OFFSET        0x00091000 /* I2S1       0x0509:1000-0x0509:1fff 4K  */
#define R528_I2S2_OFFSET        0x00092000 /* I2S1       0x0509:2000-0x0509:2fff 4K  */
#define R528_SPDIF_OFFSET       0x00093000 /* SPDIF      0x0509:3000-0x0509:33ff 1K  */
#define R528_DMIC_OFFSET        0x00095000 /* DMIC       0x0509:5000-0x0509:53ff 1K  */
#define R528_AUDIOCODEC_OFFSET  0x00096000 /* AUDIOCODEC 0x0509:6000-0x0509:6fff 4K  */
#define R528_USB0_OFFSET        0x00100000 /* USB0       0x0510:0000-0x051f:ffff 1M  */
#define R528_MAD_OFFSET         0x00400000 /* USB0       0x0540:0000-0x0540:0fff 4K  */
#define R528_MAD_SRAM_OFFSET    0x00480000 /* MAD_SRAM   0x0548:0000-0x0549:ffff 128K  */

/* R528 offsets from the ledc section base address */
#define R528_LEDC_OFFSET       0x00000000  /* LEDC       0x0670:0000-0x0670:3ff0 1K  */

/* R528 offsets from the cpux section base address */
#define R528_CPU_SYS_OFFSET    0x00000000  /* CPU_SYS    0x0810:0000-0x0810:03ff 1K  */
#define R528_TIMESTAMPS_OFFSET 0x00010000  /* TIMESTAMPS 0x0811:0000-0x0811:0fff 4K  */
#define R528_TIMESTAMPC_OFFSET 0x00020000  /* TIMESTAMPS 0x0812:0000-0x0812:0fff 4K  */
#define R528_IDC_OFFSET        0x00030000  /* IDC        0x0813:0000-0x0813:0fff 4K  */

/* R528 offsets from the dram section base address */
#define R528_DRAM_OFFSET       0x00000000  /* DRAM       0x4000:0000-0xffff:ffff 3G  */


/* R528 internal memory physical base address */
#define R528_NBROM_PADDR    (R528_INTMEM_PSECTION + R528_NBROM_OFFSET)
#define R528_SBROM_PADDR    (R528_INTMEM_PSECTION + R528_SBROM_OFFSET)
#define R528_SRAMA1_PADDR   (R528_INTMEM_PSECTION + R528_SRAMA1_OFFSET)


/* R528 system physical base address */
#define R528_SYS_CFG_PADDR  (R528_SYS_PSECTION + R528_SYS_CFG_OFFSET)
#define R528_CCMU_PADDR     (R528_SP0_PSECTION + R528_CCMU_OFFSET)
#define R528_DMAC_PADDR     (R528_SH0_PSECTION + R528_DMAC_OFFSET)
#define R528_HSTIMER_PADDR  (R528_SH0_PSECTION + R528_HSTIMER_OFFSET)
#define R528_SID_PADDR      (R528_SH0_PSECTION + R528_SID_OFFSET)
#define R528_SMC_PADDR      (R528_SH0_PSECTION + R528_SMC_OFFSET)
#define R528_SPC_PADDR      (R528_SP0_PSECTION + R528_SPC_OFFSET)
#define R528_TIMER_PADDR    (R528_SP0_PSECTION + R528_TIMER_OFFSET)
#define R528_PWM_PADDR      (R528_SP0_PSECTION + R528_PWM_OFFSET)
#define R528_GPIO_PADDR     (R528_SP0_PSECTION + R528_GPIO_OFFSET)
#define R528_PSI_PADDR      (R528_SYS_PSECTION + R528_PSI_OFFSET)
#define R528_DCU_PADDR      (R528_SH0_PSECTION + R528_DCU_OFFSET)
#define R528_GIC_PADDR      (R528_SH0_PSECTION + R528_GIC_OFFSET)

/* R528 rtc physical base address */
#define R528_RTC_PADDR      (R528_AHBS_PSECTION + R528_RTC_OFFSET)


/* R528 storage physical base address */
#define R528_SMHC0_PADDR     (R528_SH2_PSECTION + R528_SMHC0_OFFSET)
#define R528_SMHC1_PADDR     (R528_SH2_PSECTION + R528_SMHC1_OFFSET)
#define R528_SMHC2_PADDR     (R528_SH2_PSECTION + R528_SMHC2_OFFSET)

/* R528 periph physical base address */
#define R528_UART0_PADDR    (R528_SP1_PSECTION + R528_UART0_OFFSET)
#define R528_UART1_PADDR    (R528_SP1_PSECTION + R528_UART1_OFFSET)
#define R528_UART2_PADDR    (R528_SP1_PSECTION + R528_UART2_OFFSET)
#define R528_UART3_PADDR    (R528_SP1_PSECTION + R528_UART3_OFFSET)
#define R528_TWI0_PADDR     (R528_SP1_PSECTION + R528_TWI0_OFFSET)
#define R528_TWI1_PADDR     (R528_SP1_PSECTION + R528_TWI1_OFFSET)
#define R528_GPADC_PADDR    (R528_PERIPH_PSECTION + R528_GPADC_OFFSET)
#define R528_THS_PADDR      (R528_PERIPH_PSECTION + R528_THS_OFFSET)
#define R528_LRADC_PADDR    (R528_PERIPH_PSECTION + R528_LRADC_OFFSET)
#define R528_I2S0_PADDR     (R528_PERIPH_PSECTION + R528_I2S0_OFFSET)
#define R528_I2S1_PADDR     (R528_PERIPH_PSECTION + R528_I2S1_OFFSET)
#define R528_I2S2_PADDR     (R528_PERIPH_PSECTION + R528_I2S2_OFFSET)
#define R528_SPDIF_PADDR    (R528_PERIPH_PSECTION + R528_SPDIF_OFFSET)
#define R528_DMIC_PADDR     (R528_PERIPH_PSECTION + R528_DMIC_OFFSET)
#define R528_AUDIOCODEC_PADDR    (R528_PERIPH_PSECTION + R528_AUDIOCODEC_OFFSET)
#define R528_USB0_PADDR    (R528_PERIPH_PSECTION + R528_USB0_OFFSET)
#define R528_MAD_PADDR     (R528_PERIPH_PSECTION + R528_MAD_OFFSET)
#define R528_MAD_SRAM_PADDR    (R528_PERIPH_PSECTION + R528_MAD_SRAM_OFFSET)


/* R528 ledc memory physical base addresses */
#define R528_LEDC_PADDR    (R528_LEDC_PSECTION + R528_LEDC_OFFSET)

/* R528 dram memory physical base addresses */
#define R528_DRAM_PADDR    (R528_DRAM_PSECTION + R528_DRAM_OFFSET)

/* Sizes of memory regions in bytes.
 *
 * These sizes exclude the undefined addresses at the end of the memory
 * region.  The implemented sizes of the external memory regions are
 * not known apriori and must be specified with configuration settings.
 */
#define R528_INTMEM_SIZE      0X00100000  /* Internal memory 0x0002:0000-0x0004:7fff */
#define R528_DSP_SIZE         0X00100000  /* Internal memory 0x0170:0000-0x0170:1fff */
#define R528_VE_SIZE          0x00100000  /* CE              0x0190:4000-0x0190:4fff */
#define R528_SP0_SIZE         0x00100000  /* SDHC            0x0190:4000-0x0190:4fff */
#define R528_SP1_SIZE         0x00100000  /* system          0x0300:0000-0x0302:ffff */
#define R528_SH0_SIZE         0x00400000  /* Rtc             0x0700:0000-0x0700:03ff */
#define R528_SH2_SIZE         0x00600000  /* Storage         0x0401:1000-0x0400:5fff */
#define R528_VIDEO_OUT_SIZE   0x00700000  /* Peripherals     0x0500:0000-0x0549:ffff */
#define R528_VIDEO_IN_SIZE    0x00500000  /* Ledc            0x0670:0000-0x0670:03ff */
#define R528_APBS0_SIZE       0x00100000  /* Ledc            0x0670:0000-0x0670:03ff */
#define R528_CPUX_SIZE        0x01000000  /* Cpux            0x0810:0000-0x0902:0fff */
#define R528_BROM_SIZE        0x00008000  /* BROM            0x0000:0000-0x0000:7fff */
#define R528_DDR_SIZE         0x08000000  /* DDR             0x4000:0000-0x4800:0000 */
#define R528_DSP_RESERVED     0x00500000  /* Reserved  5M of DDR for DSP */
#define R528_RAMLOG_RESERVED  0X00100000  /* Reserved 1MB of DDR for store log*/
#define R528_DISP_RESERVED    0X00C00000  /* Reserved 4.5MB of DDR for store log*/


/* Force configured sizes that might exceed 2GB to be unsigned long */

#define R528_DDR_MAPOFFSET    MKULONG(CONFIG_R528_DDR_MAPOFFSET)
#define R528_DDR_MAPSIZE      MKULONG(CONFIG_R528_DDR_MAPSIZE)
#define R528_DDR_HEAP_OFFSET  MKULONG(CONFIG_R528_DDR_HEAP_OFFSET)
#define R528_DDR_HEAP_SIZE    MKULONG(CONFIG_R528_DDR_HEAP_SIZE)

/* Convert size in bytes to number of sections (in Mb). */

#define _NSECTIONS(b)        (((b)+0x000fffff) >> 20)

/* Sizes of memory regions in sections.
 *
 * The boot logic in A1X_boot.c, will select 1Mb level 1 MMU mappings to
 * span the entire physical address space.  The definitions below specify
 * the number of 1Mb entries that are required to span a particular address
 * region.
 *
 * NOTE: the size of the mapped SDRAM region depends on the configured size
 * of DRAM, not on the size of the address space assigned to DRAM.
 */
#define R528_INTMEM_NSECTIONS    _NSECTIONS(R528_INTMEM_SIZE)
#define R528_DSP_NSECTIONS       _NSECTIONS(R528_DSP_SIZE)
#define R528_VE_NSECTIONS        _NSECTIONS(R528_VE_SIZE)
#define R528_SYS_NSECTIONS       _NSECTIONS(R528_SYS_SIZE)
#define R528_SP0_NSECTIONS      _NSECTIONS(R528_SP0_SIZE)
#define R528_SP1_NSECTIONS       _NSECTIONS(R528_SP1_SIZE)
#define R528_SH0_NSECTIONS   _NSECTIONS(R528_SH0_SIZE)
#define R528_SH2_NSECTIONS    _NSECTIONS(R528_SH2_SIZE)
#define R528_VIDEO_OUT_NSECTIONS      _NSECTIONS(R528_VIDEO_OUT_SIZE)
#define R528_VIDEO_IN_NSECTIONS      _NSECTIONS(R528_VIDEO_IN_SIZE)
#define R528_APBS0_NSECTIONS      _NSECTIONS(R528_APBS0_SIZE)
#define R528_AHBS_NSECTIONS      _NSECTIONS(R528_AHBS_SIZE)
#define R528_CPUX_NSECTIONS      _NSECTIONS(R528_CPUX_SIZE)
#define R528_BROM_NSECTIONS      _NSECTIONS(R528_BROM_SIZE)

#ifdef CONFIG_AW_RPTUN
#define R528_DDR_NSECTIONS      _NSECTIONS((R528_DDR_SIZE - R528_DSP_RESERVED- R528_RAMLOG_RESERVED - R528_DISP_RESERVED))
#else
#define R528_DDR_NSECTIONS      _NSECTIONS((R528_DDR_SIZE - R528_RAMLOG_RESERVED - R528_DISP_RESERVED))
#endif
#ifdef CONFIG_AW_RPTUN
#define R528_DSP_DDR_NSECTIONS  _NSECTIONS(R528_DSP_RESERVED)
#endif
#define R528_SAVE_LOG_NSECTIONS  _NSECTIONS(R528_RAMLOG_RESERVED)
#define R528_DISP_NSECTIONS  _NSECTIONS(R528_DISP_RESERVED)
/* Section MMU Flags */

#define R528_INTMEM_MMUFLAGS      MMU_MEMFLAGS
#define R528_SYS_MMUFLAGS         MMU_IOFLAGS
#define R528_BROM_MMUFLAGS        MMU_IOFLAGS
#define R528_DDR_MMUFLAGS         MMU_MEMFLAGS
#define R528_NONCACHE_DDR_MMUFLAGS \
              (PMD_TYPE_SECT | PMD_SECT_AP_RW1 | (1 << PMD_SECT_TEX_SHIFT) | \
               PMD_SECT_S | PMD_SECT_XN | PMD_SECT_DOM(0))


/* A1X Virtual (mapped) Memory Map
 *
 * board_memorymap.h contains special mappings that are needed when a ROM
 * memory map is used.  It is included in this odd location becaue it depends
 * on some the virtual address definitions provided above.
 */

#include <arch/board/board_memorymap.h>

/* A1X Virtual (mapped) Memory Map.  These are the mappings that will
 * be created if the page table lies in RAM.  If the platform has another,
 * read-only, pre-initialized page table (perhaps in ROM), then the board.h
 * file must provide these definitions.
 */

#ifndef CONFIG_ARCH_ROMPGTABLE

/* Notice that these mappings are a simple 1-to-1 mappings */

#define R528_INTMEM_VSECTION  0x00000000  /* Internal memory 0x0002:0000-0x0003:ffff */
#define R528_DSP_VSECTION     0x01700000  /* DSP SYS         0x0170:0000-0x0190:1fff */
#define R528_VE_VSECTION      0x01c00000  /* CE              0x0190:4000-0x0190:4fff */
#define R528_SP0_VSECTION     0x02000000  /* system          0x0300:0000-0x0302:ffff */
#define R528_SP1_VSECTION     0x02500000  /* system          0x0300:0000-0x0302:ffff */
#define R528_SH0_VSECTION     0x03000000  /* Rtc             0x0700:0000-0x0700:03ff */
#define R528_SYS_VSECTION     0x03000000
#define R528_SH2_VSECTION     0x04000000  /* Storage         0x0401:1000-0x0400:5fff */
#define R528_VIDEO_OUT_VSECTION  0x05000000  /* Peripherals     0x0500:0000-0x0549:ffff */
#define R528_VIDEO_IN_VSECTION   0x05800000  /* Ledc            0x0670:0000-0x0670:03ff */
#define R528_APBS0_VSECTION   0x07000000  /* Ledc            0x0670:0000-0x0670:03ff */
#define R528_AHBS_VSECTION    0x07000000  /* Ledc            0x0670:0000-0x0670:03ff */
#define R528_CPUX_VSECTION    0x08100000  /* Cpux            0x0810:0000-0x0902:0fff */
#define R528_DRAM_VSECTION    0x40000000  /* Dram            0x4000:0000-0x47ff:ffff */
#define R528_DRAM_VSECTION2    0xc0000000  /* Dram-noncache  0xc000:0000-0xc7ff:ffff */
#define R528_BROM_VSECTION    0x00000000  /* BROM            0x0000:0000-0x0000:7fff */

#define R528_PERIPH_VSECTION  R528_SP1_VSECTION

#endif


/* R528 internal memory physical base address */
#define R528_NBROM_VADDR    (R528_INTMEM_VSECTION + R528_NBROM_OFFSET)
#define R528_SBROM_VADDR    (R528_INTMEM_VSECTION + R528_SBROM_OFFSET)
#define R528_SRAMA1_VADDR   (R528_INTMEM_VSECTION + R528_SRAMA1_OFFSET)


/* R528 system physical base address */
#define R528_SYS_CFG_VADDR  (R528_SYS_VSECTION + R528_SYS_CFG_OFFSET)
#define R528_CCMU_VADDR     (R528_SP0_VSECTION + R528_CCMU_OFFSET)
#define R528_DMAC_VADDR     (R528_SH0_VSECTION + R528_DMAC_OFFSET)
#define R528_HSTIMER_VADDR  (R528_SH0_VSECTION + R528_HSTIMER_OFFSET)
#define R528_SID_VADDR      (R528_SH0_VSECTION + R528_SID_OFFSET)
#define R528_SMC_VADDR      (R528_SH0_VSECTION + R528_SMC_OFFSET)
#define R528_SPC_VADDR      (R528_SP0_VSECTION + R528_SPC_OFFSET)
#define R528_TIMER_VADDR    (R528_SP0_VSECTION + R528_TIMER_OFFSET)
#define R528_PWM_VADDR      (R528_SP0_VSECTION + R528_PWM_OFFSET)
#define R528_GPIO_VADDR     (R528_SP0_VSECTION + R528_GPIO_OFFSET)
#define R528_PSI_VADDR      (R528_SYS_VSECTION + R528_PSI_OFFSET)
#define R528_DCU_VADDR      (R528_SH0_VSECTION + R528_DCU_OFFSET)
#define R528_GIC_VADDR      (R528_SH0_VSECTION + R528_GIC_OFFSET)

/* R528 rtc physical base address */
#define R528_RTC_VADDR      (R528_AHBS_VSECTION + R528_RTC_OFFSET)


/* R528 storage physical base address */
#define R528_SMHC0_VADDR     (R528_SH2_VSECTION + R528_SMHC0_OFFSET)
#define R528_SMHC1_VADDR     (R528_SH2_VSECTION + R528_SMHC1_OFFSET)
#define R528_SMHC2_VADDR     (R528_SH2_VSECTION + R528_SMHC2_OFFSET)

#define R528_MSI_CTRL_VADDR (R528_STORAGE_VSECTION + R528_MSI_CTRL_OFFSET)
#define R528_DRAM_PHY_VADDR (R528_STORAGE_VSECTION + R528_DRAM_PHY_OFFSET)

/* R528 periph physical base address */
#define R528_UART0_VADDR    (R528_PERIPH_VSECTION + R528_UART0_OFFSET)
#define R528_UART1_VADDR    (R528_PERIPH_VSECTION + R528_UART1_OFFSET)
#define R528_UART2_VADDR    (R528_PERIPH_VSECTION + R528_UART2_OFFSET)
#define R528_UART3_VADDR    (R528_PERIPH_VSECTION + R528_UART3_OFFSET)
#define R528_TWI0_VADDR     (R528_PERIPH_VSECTION + R528_TWI0_OFFSET)
#define R528_TWI1_VADDR     (R528_PERIPH_VSECTION + R528_TWI1_OFFSET)
#define R528_SPI0_VADDR     (R528_PERIPH_VSECTION + R528_SPI0_OFFSET)
#define R528_SPI1_VADDR     (R528_PERIPH_VSECTION + R528_SPI1_OFFSET)
#define R528_GPADC_VADDR    (R528_PERIPH_VSECTION + R528_GPADC_OFFSET)
#define R528_THS_VADDR      (R528_PERIPH_VSECTION + R528_THS_OFFSET)
#define R528_LRADC_VADDR    (R528_PERIPH_VSECTION + R528_LRADC_OFFSET)
#define R528_I2S0_VADDR     (R528_PERIPH_VSECTION + R528_I2S0_OFFSET)
#define R528_I2S1_VADDR     (R528_PERIPH_VSECTION + R528_I2S1_OFFSET)
#define R528_I2S2_VADDR     (R528_PERIPH_VSECTION + R528_I2S2_OFFSET)
#define R528_SPDIF_VADDR    (R528_PERIPH_VSECTION + R528_SPDIF_OFFSET)
#define R528_DMIC_VADDR     (R528_PERIPH_VSECTION + R528_DMIC_OFFSET)
#define R528_AUDIOCODEC_VADDR    (R528_PERIPH_VSECTION + R528_AUDIOCODEC_OFFSET)
#define R528_USB0_VADDR    (R528_PERIPH_VSECTION + R528_USB0_OFFSET)
#define R528_MAD_VADDR     (R528_PERIPH_VSECTION + R528_MAD_OFFSET)
#define R528_MAD_SRAM_VADDR    (R528_PERIPH_VSECTION + R528_MAD_SRAM_OFFSET)

#define CHIP_MPCORE_VBASE  (R528_GIC_VADDR)


/* R528 ledc memory physical base addresses */
#define R528_LEDC_VADDR    (R528_LEDC_VSECTION + R528_LEDC_OFFSET)

/* R528 dram memory physical base addresses */
#define R528_DRAM_VADDR    (R528_DRAM_VSECTION + R528_DRAM_OFFSET)

/* Offset SDRAM address */
#define R528_DDR_MAPPADDR     (R528_DRAM_PSECTION+R528_DDR_MAPOFFSET)
#define R528_DDR_MAPVADDR     (R528_DRAM_VSECTION+R528_DDR_MAPOFFSET)
#define R528_DDR_MAPVADDR2    (R528_DRAM_VSECTION2+R528_DDR_MAPOFFSET)

#ifdef CONFIG_AW_RPTUN
#define R528_DSP_DDR_MAPPADDR (R528_DRAM_PSECTION+R528_DDR_MAPOFFSET + \
                               R528_DDR_SIZE - R528_DSP_RESERVED - R528_RAMLOG_RESERVED)
#define R528_DSP_DDR_MAPVADDR (R528_DRAM_PSECTION+R528_DDR_MAPOFFSET + \
                               R528_DDR_SIZE - R528_DSP_RESERVED - R528_RAMLOG_RESERVED)
#endif
#define R528_SAVE_LOG_DDR_MAPPADDR (R528_DRAM_PSECTION+R528_DDR_MAPOFFSET + \
                               R528_DDR_SIZE - R528_RAMLOG_RESERVED)
#define R528_SAVE_LOG_DDR_MAPVADDR (R528_DRAM_PSECTION+R528_DDR_MAPOFFSET + \
                               R528_DDR_SIZE - R528_RAMLOG_RESERVED)

#define R528_DISP_DDR_MAPPADDR (R528_DRAM_PSECTION+R528_DDR_MAPOFFSET + \
                               R528_DDR_SIZE - R528_DISP_RESERVED - R528_DSP_RESERVED - R528_RAMLOG_RESERVED)
#define R528_DISP_DDR_MAPVADDR (R528_DRAM_PSECTION+R528_DDR_MAPOFFSET + \
                               R528_DDR_SIZE - R528_DISP_RESERVED - R528_DSP_RESERVED - R528_RAMLOG_RESERVED)

#define R528_UART_VADDR(n)    (R528_UART0_VADDR+ 0x400*n)

/* NuttX virtual base address
 *
 * The boot logic will create a temporarily mapping based on where NuttX is
 * executing in memory.  In this case, NuttX will be running from either
 * internal SRAM or external SDRAM.
 *
 * Setup the RAM region as the NUTTX .txt, .bss, and .data region.
 */

#define NUTTX_TEXT_VADDR     (CONFIG_RAM_VSTART & 0xfff00000)
#define NUTTX_TEXT_PADDR     (CONFIG_RAM_START & 0xfff00000)
#define NUTTX_TEXT_PEND      ((CONFIG_RAM_END + 0x000fffff) & 0xfff00000)
#define NUTTX_TEXT_SIZE      (NUTTX_TEXT_PEND - NUTTX_TEXT_PADDR)

/* MMU Page Table
 *
 * Determine the address of the MMU page table.  Regardless of the memory
 * configuration, we will keep the page table in the A1X's internal SRAM.
 */

#if defined(PGTABLE_BASE_PADDR) || defined(PGTABLE_BASE_VADDR)

  /* Sanity check.. if one is undefined, both should be undefined */

#  if !defined(PGTABLE_BASE_PADDR) || !defined(PGTABLE_BASE_VADDR)
#    error "Only one of PGTABLE_BASE_PADDR or PGTABLE_BASE_VADDR is defined"
#  endif

  /* A sanity check, if the configuration says that the page table is read-only
   * and pre-initialized (maybe ROM), then it should have also defined both of
   * the page table base addresses.
   */

#  ifdef CONFIG_ARCH_ROMPGTABLE
#    error "CONFIG_ARCH_ROMPGTABLE defined; PGTABLE_BASE_P/VADDR not defined"
#  endif

#else /* PGTABLE_BASE_PADDR || PGTABLE_BASE_VADDR */

  /* If CONFIG_PAGING is selected, then parts of the 1-to-1 virtual memory
   * map probably do not apply because paging logic will probably partition
   * the SRAM section differently.  In particular, if the page table is located
   * at the end of SRAM, then the virtual page table address defined below
   * will probably be in error.  In that case PGTABLE_BASE_VADDR is defined
   * in the file mmu.h
   *
   * We must declare the page table at the bottom or at the top of internal
   * SRAM.  We pick the bottom of internal SRAM *unless* there are vectors
   * in the way at that position.
   */

#  if defined(CONFIG_ARCH_LOWVECTORS)
  /* In this case, table must lie in SRAM A2 after the vectors in SRAM A1 offset 16KB addr*/
#ifdef CONFIG_ARCH_TRUSTZONE_SECURE
#    define PGTABLE_BASE_PADDR  (R528_SRAMA1_PADDR)
#    define PGTABLE_BASE_VADDR  (R528_SRAMA1_VADDR)
#else
#    define PGTABLE_BASE_PADDR  (R528_SRAMA1_PADDR+0x4000)
#    define PGTABLE_BASE_VADDR  (R528_SRAMA1_VADDR+0x4000)
#endif
#  else /* CONFIG_ARCH_LOWVECTORS */

  /* Otherwise, the vectors lie at another location.  The page table will
   * then be positioned at the SRAM A1 offset 16KB addr
   */

#ifdef CONFIG_ARCH_TRUSTZONE_SECURE
#    define PGTABLE_BASE_PADDR  (R528_SRAMA1_PADDR)
#    define PGTABLE_BASE_VADDR  (R528_SRAMA1_VADDR)
#else
#    define PGTABLE_BASE_PADDR  (R528_SRAMA1_PADDR+0x4000)
#    define PGTABLE_BASE_VADDR  (R528_SRAMA1_VADDR+0x4000)
#endif
#  endif /* CONFIG_ARCH_LOWVECTORS */

  /* Note that the page table does not lie in the same address space as does the
   * mapped RAM in either case.  So we will need to create a special mapping for
   * the page table at boot time.
   */

#  define ARMV7A_PGTABLE_MAPPING 1

#endif /* PGTABLE_BASE_PADDR || PGTABLE_BASE_VADDR */

/* Level 2 Page table start addresses.
 *
 * 16Kb of memory is reserved hold the page table for the virtual mappings.  A
 * portion of this table is not accessible in the virtual address space (for
 * normal operation).   There are several large holes in the physical address
 * space for which there will never be level 1 mappings:
 *
 *                                    LI PAGE TABLE
 *   ADDRESS RANGE           SIZE     ENTRIES       SECTIONS
 *   ----------------------- ------- -------------- ---------
 *   0x0003:0000-0x01eb:ffff 275MB   0x0004-0x006c 26
 *                                  *(none usable) 0
 *   0x01ec:0000-0x3fff:ffff 993MB   0x0078-0x0ffc 993
 *                                  *0x0400-0x0ffc 767
 *
 * And the largest is probably from the end of SDRAM through 0xfff0:0000.
 * But the size of that region varies with the size of the installed SDRAM.
 * It is at least:
 *
 *                                    LI PAGE TABLE
 *   ADDRESS RANGE           SIZE     ENTRIES       SECTIONS
 *   ----------------------- ------- -------------- ---------
 *   0xc000:0000-0xffef:ffff 1022MB  *0x3000-0x3ff8 1022
 *
 * And probably much larger.
 *
 *   * NOTE that the L2 page table entries must be aligned 1KB address
 *     boundaries.
 *
 * These two larger regions is where L2 page tables will positioned.  Up to
 * two L2 page tables may be used:
 *
 * 1) One mapping the vector table (only when CONFIG_ARCH_LOWVECTORS is not
 *    defined).
 * 2) If on-demand paging is supported (CONFIG_PAGING=y), than an additional
 *    L2 page table is needed.
 */

#ifndef CONFIG_ARCH_LOWVECTORS
/* Vector L2 page table offset/size */

#  define VECTOR_L2_OFFSET        0x000000400
#  define VECTOR_L2_SIZE          0x000000bfc

/* Vector L2 page table base addresses */

#  define VECTOR_L2_PBASE         (PGTABLE_BASE_PADDR+VECTOR_L2_OFFSET)
#  define VECTOR_L2_VBASE         (PGTABLE_BASE_VADDR+VECTOR_L2_OFFSET)

/* Vector L2 page table end addresses */

#  define VECTOR_L2_END_PADDR     (VECTOR_L2_PBASE+VECTOR_L2_SIZE)
#  define VECTOR_L2_END_VADDR     (VECTOR_L2_VBASE+VECTOR_L2_SIZE)

#endif /* !CONFIG_ARCH_LOWVECTORS */

/* Paging L2 page table offset/size */

#define PGTABLE_L2_START_PADDR    (R528_DRAM_PSECTION+R528_DDR_MAPOFFSET+R528_DDR_MAPSIZE)
#define PGTABLE_BROM_OFFSET       0x3ffc

#define PGTABLE_L2_OFFSET         ((PGTABLE_L2_START_PADDR >> 18) & ~3)
#define PGTABLE_L2_SIZE           (PGTABLE_BROM_OFFSET - PGTABLE_L2_OFFSET)

/* Paging L2 page table base addresses
 *
 * NOTE: If CONFIG_PAGING is defined, mmu.h will re-assign the virtual
 * address of the page table.
 */

#define PGTABLE_L2_PBASE          (PGTABLE_BASE_PADDR+PGTABLE_L2_OFFSET)
#define PGTABLE_L2_VBASE          (PGTABLE_BASE_VADDR+PGTABLE_L2_OFFSET)

/* Paging L2 page table end addresses */

#define PGTABLE_L2_END_PADDR      (PGTABLE_L2_PBASE+PGTABLE_L2_SIZE)
#define PGTABLE_L2_END_VADDR      (PGTABLE_L2_VBASE+PGTABLE_L2_SIZE)

/* Base address of the interrupt vector table.
 *
 *   A1X_VECTOR_PADDR - Unmapped, physical address of vector table in SRAM
 *   A1X_VECTOR_VSRAM - Virtual address of vector table in SRAM
 *   A1X_VECTOR_VADDR - Virtual address of vector table (0x00000000 or 0xffff0000)
 *
 * NOTE: When using LOWVECTORS, the actual base of the vectors appears to be
 * offset to address 0x0000:0040
 */

#define VECTOR_TABLE_SIZE         0x00010000
#define VECTOR_TABLE_OFFSET       0x00000040

#ifdef CONFIG_ARCH_LOWVECTORS  /* Vectors located at 0x0000:0000  */

#if 0
#  define R528_VECTOR_PADDR        R528_SRAMA1_PADDR
#  define R528_VECTOR_VSRAM        R528_SRAMA1_VADDR
#  define R528_VECTOR_VADDR        0x00000000
#endif

#else  /* Vectors located at 0xffff:0000 -- this probably does not work */

#if 0
#  ifdef A1X_ISRAM1_SIZE >= VECTOR_TABLE_SIZE
#    define A1X_VECTOR_PADDR      (A1X_SRAMA1_PADDR+A1X_ISRAM1_SIZE-VECTOR_TABLE_SIZE)
#    define A1X_VECTOR_VSRAM      (A1X_SRAMA1_VADDR+A1X_ISRAM1_SIZE-VECTOR_TABLE_SIZE)
#  else
#    define A1X_VECTOR_PADDR      (A1X_SRAMA1_PADDR+A1X_ISRAM0_SIZE-VECTOR_TABLE_SIZE)
#    define A1X_VECTOR_VSRAM      (A1X_SRAMA1_VADDR+A1X_ISRAM0_SIZE-VECTOR_TABLE_SIZE)
#  endif
#  define A1X_VECTOR_VADDR        0xffff0000
#endif

#endif


/* Level 2 Page table configuration.
 *
 */

#ifdef CONFIG_X4B_AP
#define ENABLE_L2PGTABLE
#endif

#ifdef ENABLE_L2PGTABLE

/* Mapped page size */
#define L2_PAGE_SHIFT			(12)
#define L2_PAGE_SIZE			(1 << L2_PAGE_SHIFT)  /* 4KB */
#define L2_PAGE_MASK			(L2_PAGE_SIZE - 1)

/* L2 PAGETABLE SIZE */
#define L2PGTABLE_PER_SIZE		(0x400)  /* 1M / 4K * 4B = 1024B */
#define _NPAGES(c)			(((c)+0x0000fff) >> 12)

/* MMU FLAGS */
#define PTE_XN				(1 << 0)
#define R528_MMU_L2_UDATAFLAGS		(MMU_L2_UDATAFLAGS | PTE_XN)
#define R528_MMU_L2_TEXTFLAGS		(PTE_TYPE_SMALL | PTE_WRITE_BACK | PTE_AP_R012)

#define R528_DDR_XN_MMUFLAGS         	(MMU_MEMFLAGS | PMD_SECT_XN | PMD_SECT_PXN)

/********************** 0-1M L2PAGETABLE MACROS ***********************/
//#define L2PGTABLE_BASE_PADDR_0M	(0x38000) /* now dsp will use 0x38000, so we can not use this addr*/
//#define L2PGTABLE_BASE_PADDR_0M	(0x27c00) /* fixme, this is illegal, we borrow 1k from L1 pgtable */
#define L2PGTABLE_BASE_PADDR_0M		(0x47f20000)
#define L2PGTABLE_BASE_PADDR_0M_SIZE	(0x400)

/* SRAM A1 + local sram size. 0x00020000-0x00047fff */
#define R528_SRAM_SIZE			(0x28000)
/* SRAM NPAGES */
#define R528_SRAM_NPAGES		_NPAGES(R528_SRAM_SIZE)


/********************** AP .text L2PAGETABLE MACROS ***********************/
#define L2PGTABLE_BASE_PADDR_AP_TEXT	(0X47f20400)


/*      |<-----start----->|<-----.text head----->|<-----.text----->|<-----.text tail----->|<-----end----->|
 *      |                 |                      |                 |                      |               |
 * 0x40000000    align(_stest,1MB)           _stext            _einit        align(_einit+1M-1B,1M)   0x48000000-dsp-disp-ramlog
 */
#define R528_AP_DDR_START_PADDR		(R528_DDR_MAPPADDR)
#define R528_AP_DDR_START_VADDR		(R528_DDR_MAPVADDR)
#define R528_AP_DDR_START_NSECTIONS	(_NSECTIONS(R528_AP_TEXT_HEAD_PADDR - R528_DDR_MAPPADDR))

#define R528_AP_TEXT_HEAD_PADDR		((uint32_t)_stext & (~SECTION_MASK))
#define R528_AP_TEXT_HEAD_VADDR		((uint32_t)_stext & (~SECTION_MASK))
#define R528_AP_TEXT_HEAD_NPAGES	(_NPAGES((uint32_t)_stext & SECTION_MASK))
#define R528_AP_TEXT_PADDR		((uint32_t)_stext)
#define R528_AP_TEXT_VADDR		((uint32_t)_stext)
#define R528_AP_TEXT_NPAGES		(_NPAGES((uint32_t)_einit - (uint32_t)_stext))
#define R528_AP_TEXT_TAIL_PADDR		((uint32_t)_einit)
#define R528_AP_TEXT_TAIL_VADDR		((uint32_t)_einit)
#define R528_AP_TEXT_TAIL_NPAGES	(_NPAGES(((uint32_t)_einit & SECTION_MASK) ? SECTION_SIZE - ((uint32_t)_einit & SECTION_MASK) : 0))

#define R528_AP_TEXT_ALL_NSECTIONS	(_NSECTIONS(R528_AP_TEXT_TAIL_PADDR - R528_AP_TEXT_HEAD_PADDR))

#define R528_AP_DDR_END_PADDR		((R528_AP_TEXT_TAIL_PADDR + SECTION_MASK) & (~SECTION_MASK))
#define R528_AP_DDR_END_VADDR		((R528_AP_TEXT_TAIL_VADDR + SECTION_MASK) & (~SECTION_MASK))
#define R528_AP_DDR_END_NSECTIONS	(R528_DDR_NSECTIONS - _NSECTIONS(R528_AP_TEXT_TAIL_PADDR - R528_AP_DDR_START_PADDR))

#endif /* ENABLE_L2PGTABLE */


/************************************************************************************
 * Public Types
 ************************************************************************************/

/************************************************************************************
 * Public Data
 ************************************************************************************/

/************************************************************************************
 * Public Functions
 ************************************************************************************/

#endif /* __ARCH_ARM_SRC_A1X_HARDWARE_A10_MEMORYMAP_H */
