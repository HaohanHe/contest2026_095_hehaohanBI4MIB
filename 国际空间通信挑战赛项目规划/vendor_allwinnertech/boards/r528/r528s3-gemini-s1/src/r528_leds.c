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
#ifndef OPEN_MAX
#define OPEN_MAX 256
#endif
#ifndef CLOCK_MAX
#define CLOCK_MAX 4294967295U
#endif

#include <debug.h>
#include <stdbool.h>
#include <stdint.h>

#include <arch/board/board.h>
#include <nuttx/board.h>

#include "gpio/gpio.h"
#include "hal_gpio.h"

// #include "r528_board.h"
// #include "r528_gpio.h"
// #include "drivers/rtos-hal/include/hal/hal_gpio.h"
// #include "drivers/rtos-hal/hal/source/gpio/gpio.h"

// 直接定义需要的宏
// #define PB_BASE 32
// #define GPIOB(n) (PB_BASE + (n))

/* The r528-gemini-s1 board has one controllable LED:
 *
 *  1. SYS_LED connected to PB10 pin, active high (illuminated by driving high)
 *
 * This LED is not used by the board port unless CONFIG_ARCH_LEDS is
 * defined.  In that case, the usage by the board port is defined in
 * include/board.h and src/r528_leds.c.
 * The LED is used to encode OS-related events as follows:
 *
 *   SYMBOL            Meaning                      LED state
 *                                              SYS_LED
 *   ----------------- -----------------------  -------
 *   LED_STARTED       NuttX has been started   ON
 *   LED_HEAPALLOCATE  Heap has been allocated  OFF
 *   LED_IRQSENABLED   Interrupts enabled       ON
 *   LED_STACKCREATED  Idle stack created       ON
 *   LED_INIRQ         In an interrupt          Soft glow
 *   LED_SIGNAL        In a signal handler      Soft glow
 *   LED_ASSERTION     An assertion failed      Soft glow
 *   LED_PANIC         The system has crashed   2Hz Flashing
 *   LED_IDLE          MCU is is sleep mode     Not used
 *
 * After booting, the SYS_LED is not longer used by the system and can be used
 * for other purposes by the application (Of course, the LED is available to
 * the application if CONFIG_ARCH_LEDS is not defined).
 */

static const int g_led_map[BOARD_NLEDS] = {
    GPIOB(10),
};

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

void r528_led_initialize(void) {
  int sunxi_pin;
  for (int i = 0; i < BOARD_NLEDS; i++) {

    sunxi_pin = (g_led_map[i]);

    if (sunxi_pin < 0) {
      continue;
    }

    hal_gpio_pinmux_set_function(sunxi_pin, GPIO_MUXSEL_OUT);
  }
}

/****************************************************************************
 * Name: board_autoled_on
 *
 * Description:
 *   Select the "logical" ON state:
 *
 ****************************************************************************/

#ifdef CONFIG_ARCH_LEDS
void board_autoled_on(int led) {}
#endif

/****************************************************************************
 * Name: board_autoled_off
 *
 * Description:
 *   Select the "logical" OFF state:
 *
 ****************************************************************************/

#ifdef CONFIG_ARCH_LEDS
void board_autoled_off(int led) {}
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

uint32_t board_userled_initialize(void) {
  /* Initialization already performed in a1x_led_initialize */
  ledinfo("Called %s\n", __func__);
  r528_led_initialize();
  return BOARD_NLEDS;
}

void board_userled(int led, bool ledon) {
  ledinfo("Called %s led: %d ledon: %d\n", __func__, led, ledon);
  int sunxi_pin;

  if (led < 0 || led >= BOARD_NLEDS) {
    return; /* Invalid LED */
  }

  sunxi_pin = (g_led_map[led]);

  if (sunxi_pin < 0) {
    return; /* Invalid pin */
  }

  /* Set the LED state. An output of '1' illuminates the LED. */

  hal_gpio_set_data(sunxi_pin, ledon);
}

void board_userled_all(uint32_t ledset) {
  ledinfo("Called %s ledset: 0x%02" PRIx32 "\n", __func__, ledset);

  int sunxi_pin;
  int index = 1;
  for (int i = 0; i < BOARD_NLEDS; i++, index <<= 1) {
    sunxi_pin = (g_led_map[i]);

    if (sunxi_pin < 0) {
      continue;
    }
    hal_gpio_set_data(sunxi_pin, (ledset & index));
  }
}

void board_userled_getall(uint32_t *ledset) {
  /* Clear the LED bits */
  ledinfo("Called %s ledset: 0x%02" PRIx32 "\n", __func__, *ledset);

  *ledset = 0;

  /* Get LED state. An output of '1' illuminates the LED. */

  for (int i = 0; i < BOARD_NLEDS; i++) {
    int sunxi_pin = (g_led_map[i]);
    if (sunxi_pin < 0) {
      continue;
    }
    gpio_data_t data;
    if (hal_gpio_get_data(sunxi_pin, &data) == 0) {
      if (data == GPIO_DATA_HIGH) {
        *ledset |= (1 << i);
      }
    }
  }
}
