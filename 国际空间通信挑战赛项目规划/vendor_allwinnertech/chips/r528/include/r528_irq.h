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

/* This file should never be included directed but, rather, only indirectly through
 * nuttx/irq.h
 */
#ifndef __ARCH_ARM_INCLUDE_R528_R528_IRQ_H
#define __ARCH_ARM_INCLUDE_R528_R528_IRQ_H

/****************************************************************************************
 * Included Files
 ****************************************************************************************/

/****************************************************************************************
 * Pre-processor Definitions
 ****************************************************************************************/

/* External interrupts numbers */

#define R528_IRQ_NMI         0 /* External Non-Mask Interrupt */
#  define R528_IRQ_POWER     0 /* Power module */
#  define R528_IRQ_BATTERY   0 /* Brownout detect */
#  define R528_IRQ_BROWNOUT  0 /* Brownout */
#define R528_IRQ_UART0       34 /* UART 0 interrupt */
#define R528_IRQ_UART1       35 /* UART 1 interrupt */
#define R528_IRQ_UART2       36 /* UART 2 interrupt */
#define R528_IRQ_UART3       37 /* UART 3 interrupt */
#define R528_IRQ_UART4       38 /* UART 3 interrupt */
#define R528_IRQ_UART5       39 /* UART 3 interrupt */
#define R528_IRQ_TWI0        41 /* TWI 0 interrupt */
#define R528_IRQ_TWI1        42 /* TWI 1 interrupt */
#define R528_IRQ_TWI2        43 /* TWI 2 interrupt */
#define R528_IRQ_TWI3        44 /* TWI 3 interrupt */
#define R528_IRQ_SPI0        47 /* SPI 0 interrupt */
#define R528_IRQ_SPI1        48 /* SPI 1 interrupt */
#define R528_IRQ_PWM         50 /* PWM interrupt */
#define R528_IRQ_IR_TX       51 /* IR_TX interrupt */
#define R528_IRQ_LEDC        52 /* LEDC interrupt */
#define R528_IRQ_SPDIF       55 /* SPDIF interrupt */
#define R528_IRQ_DMIC        56 /* DMIC interrupt */
#define R528_IRQ_AUDIO_CODEC 57 /* AUDIO CODEC interrupt */
#define R528_IRQ_I2S_PCM0    58 /* I2S PCM0 interrupt */
#define R528_IRQ_I2S_PCM1    59 /* I2S PCM1 interrupt */
#define R528_IRQ_I2S_PCM2    60 /* I2S PCM2 interrupt */
#define R528_IRQ_USB0_DEVICE 61 /* USB0 DEVICE interrupt */
#define R528_IRQ_USB0_EHCI   62 /* USB0 EHCI interrupt */
#define R528_IRQ_USB0_OHCI   63 /* USB0 OHCI interrupt */
#define R528_IRQ_USB1_EHCI   65 /* USB0 EHCI interrupt */
#define R528_IRQ_USB1_OHCI   66 /* USB0 OHCI interrupt */

#define R528_IRQ_SD0         72 /* SD0 interrupt */
#define R528_IRQ_SD1         73 /* SD1 interrupt */
#define R528_IRQ_SD2         74 /* SD2 interrupt */
#define R528_IRQ_MSI         75 /* MSI interrupt */

#define R528_IRQ_SMC         76 /* SMC interrupt */
#define R528_IRQ_DMA_NS      82 /* DMA NS interrupt */
#define R528_IRQ_DMA_S       83 /* DMA S interrupt */
#define R528_IRQ_CE_NS       84 /* CE NS interrupt */
#define R528_IRQ_CE_S        85 /* CE S interrupt */
#define R528_IRQ_HSTIMER0    87 /* HTTIMER0 interrupt */
#define R528_IRQ_HSTIMER1    88 /* HTTIMER1 interrupt */
#define R528_IRQ_TIMER0      91 /* TIMER0 interrupt */
#define R528_IRQ_TIMER1      92 /* TIMER1 interrupt */
#define R528_IRQ_LRADC       93 /* LRADC interrupt */
#define R528_IRQ_TPADC       94 /* TPADC interrupt */
#define R528_IRQ_WATCHDOG    95 /* Watchdog interrupt */
#define R528_IRQ_VE          98 /* VE interrupt */

#define R528_IRQ_GPIOB_NS    101 /* GPIOB NS interrupt */
#define R528_IRQ_GPIOB_S     102 /* GPIOB S interrupt */
#define R528_IRQ_GPIOC_NS    103 /* GPIOC NS interrupt */
#define R528_IRQ_GPIOC_S     104 /* GPIOC S interrupt */
#define R528_IRQ_GPIOD_NS    105 /* GPIOD NS interrupt */
#define R528_IRQ_GPIOD_S     106 /* GPIOD S interrupt */
#define R528_IRQ_GPIOE_NS    107 /* GPIOE NS interrupt */
#define R528_IRQ_GPIOE_S     108 /* GPIOE S interrupt */
#define R528_IRQ_GPIOF_NS    109 /* GPIOF NS interrupt */
#define R528_IRQ_GPIOF_S     110 /* GPIOF S interrupt */
#define R528_IRQ_GPIOG_NS    111 /* GPIOG NS interrupt */
#define R528_IRQ_GPIOG_S     112 /* GPIOG S interrupt */

#define R528_IRQ_DE          119 /* DE interrupt */
#define R528_IRQ_DI          120 /* DI interrupt */
#define R528_IRQ_G2D         121 /* G2D interrupt */
#define R528_IRQ_LCD         122 /* LCD interrupt */


/* Total number of interrupts */

#define R528_IRQ_NINT       320

/* Total number of IRQ numbers */

#define NR_IRQS            (R528_IRQ_NINT)

/****************************************************************************************
 * Public Types
 ****************************************************************************************/

/****************************************************************************************
 * Inline functions
 ****************************************************************************************/

/****************************************************************************************
 * Public Data
 ****************************************************************************************/

/****************************************************************************************
 * Public Function Prototypes
 ****************************************************************************************/

#ifndef __ASSEMBLY__
#ifdef __cplusplus
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

#undef EXTERN
#ifdef __cplusplus
}
#endif
#endif

#endif /* __ARCH_ARM_INCLUDE_r528_R528_IRQ_H */

