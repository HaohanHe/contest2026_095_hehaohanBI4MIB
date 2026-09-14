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
#include <stdbool.h>
#include <debug.h>

#include <nuttx/board.h>
#include <arch/board/board.h>

#include "chip.h"
#
#include "r528_board.h"

/* The pcDuino v1 has four green LEDs; three can be controlled from software.
 * Two are tied to ground and, hence, illuminated by driving the output pins
 * to a high value:
 *
 *  1. LED1 SPI0_CLK  SPI0_CLK/UART5_RX/EINT23/PI11
 *  2. LED5 IPSOUT    From the PMU (not controllable by software)
 *
 * And two are pull high and, hence, illuminated by grounding the output:
 *
 *   3. LED3 RX_LED    LCD1_D16/ATAD12/KP_IN6/SMC_DET/EINT16/CSI1_D16/PH16
 *   4. LED4 TX_LED    LCD1_D15/ATAD11/KP_IN5/SMC_VPPPP/EINT15/CSI1_D15/PH15

 * These LEDs are not used by the board port unless CONFIG_ARCH_LEDS is
 * defined.  In that case, the usage by the board port is defined in
 * include/board.h and src/stm32_leds.c.
 * The LEDs are used to encode OS-related events as follows:
 *
 *   SYMBOL            Meaning                      LED state
 *                                              LED1 LED3 LED4
 *   ----------------- -----------------------  ---- ---- ---- ------------
 *   LED_STARTED       NuttX has been started   ON   OFF  OFF
 *   LED_HEAPALLOCATE  Heap has been allocated  OFF  ON   OFF
 *   LED_IRQSENABLED   Interrupts enabled       ON   ON   OFF
 *   LED_STACKCREATED  Idle stack created       ON   ON   OFF
 *   LED_INIRQ         In an interrupt          N/C  N/C  Soft glow
 *   LED_SIGNAL        In a signal handler      N/C  N/C  Soft glow
 *   LED_ASSERTION     An assertion failed      N/C  N/C  Soft glow
 *   LED_PANIC         The system has crashed   N/C  N/C  2Hz Flashing
 *   LED_IDLE          MCU is is sleep mode         Not used
 *
 * After booting, LED1 and 3 are not longer used by the system and can be used
 * for other purposes by the application (Of course, all LEDs are available to
 * the application if CONFIG_ARCH_LEDS is not defined.
 */

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: r528_led_initialize
 *
 * Description:
 *   Configure LEDs.  LEDs are left in the OFF state.
 *
 ****************************************************************************/

void r528_led_initialize(void)
{
}

/****************************************************************************
 * Name: board_autoled_on
 *
 * Description:
 *   Select the "logical" ON state:
 *
 *   SYMBOL            Value Meaning                    LED state
 *                                                    LED1 LED3 LED4
 *   ----------------- ----- -----------------------  ---- ---- ------------
 *   LED_STARTED         0   NuttX has been started   ON   OFF  OFF
 *   LED_HEAPALLOCATE    1   Heap has been allocated  OFF  ON   OFF
 *   LED_IRQSENABLED     2   Interrupts enabled       ON   ON   OFF
 *   LED_STACKCREATED    2   Idle stack created       ON   ON   OFF
 *   LED_INIRQ           3   In an interrupt          N/C  N/C  Soft glow
 *   LED_SIGNAL          3   In a signal handler      N/C  N/C  Soft glow
 *   LED_ASSERTION       3   An assertion failed      N/C  N/C  Soft glow
 *   LED_PANIC           3   The system has crashed   N/C  N/C  2Hz Flashing
 *   LED_IDLE           ---  MCU is is sleep mode         Not used
 *
 *   LED1 is illuminated by driving the output pins to a high value
 *   LED3 and LED 4 are illuminated by taking the output to ground.
 *
 ****************************************************************************/

#ifdef CONFIG_ARCH_LEDS
void board_autoled_on(int led)
{
}
#endif

/****************************************************************************
 * Name: board_autoled_off
 *
 * Description:
 *   Select the "logical" OFF state:
 *
 *   SYMBOL            Value Meaning                    LED state
 *                                                    LED1 LED3 LED4
 *   ----------------- ----- -----------------------  ---- ---- ------------
 *   LED_STARTED         0   NuttX has been started   ON   OFF  OFF
 *   LED_HEAPALLOCATE    1   Heap has been allocated  OFF  ON   OFF
 *   LED_IRQSENABLED     2   Interrupts enabled       ON   ON   OFF
 *   LED_STACKCREATED    2   Idle stack created       ON   ON   OFF
 *   LED_INIRQ           3   In an interrupt          N/C  N/C  Soft glow
 *   LED_SIGNAL          3   In a signal handler      N/C  N/C  Soft glow
 *   LED_ASSERTION       3   An assertion failed      N/C  N/C  Soft glow
 *   LED_PANIC           3   The system has crashed   N/C  N/C  2Hz Flashing
 *   LED_IDLE           ---  MCU is is sleep mode         Not used
 *
 *   LED1 is illuminated by driving the output pins to a high value
 *   LED3 and LED 4 are illuminated by taking the output to ground.
 *
 ****************************************************************************/

#ifdef CONFIG_ARCH_LEDS
void board_autoled_off(int led)
{
}
#endif

/****************************************************************************
 * Name:  board_userled_initialize, board_userled, and board_userled_all
 *
 * Description:
 *   These interfaces allow user control of the board LEDs.
 *
 *   If CONFIG_ARCH_LEDS is defined, then NuttX will control both on-board
 *   LEDs up until the completion of boot.
 *   The it will continue to control LED2; LED1 is available for application
 *   use.
 *
 *   If CONFIG_ARCH_LEDS is not defined, then both LEDs are available for
 *   application use.
 *
 ****************************************************************************/

uint32_t board_userled_initialize(void)
{
  /* Initialization already performed in a1x_led_initialize */
	return 0;
}

void board_userled(int led, bool ledon)
{
}

void board_userled_all(uint32_t ledset)
{
}
