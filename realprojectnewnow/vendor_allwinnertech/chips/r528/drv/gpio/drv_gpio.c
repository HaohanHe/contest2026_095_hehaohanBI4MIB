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

#include <stdbool.h>
#include <assert.h>
#include <debug.h>

#include <nuttx/clock.h>
#include <nuttx/wdog.h>
#include <nuttx/ioexpander/gpio.h>
#include <nuttx/ioexpander/ioexpander.h>

#include <arch/board/board.h>

#include "chip.h"

#include <hal_gpio.h>

#ifdef CONFIG_IOEXPANDER
/****************************************************************************
 * Private Types
 ****************************************************************************/
typedef enum gpio_int_state
{
    GPIO_INT_NONE = 0,
    GPIO_INT_ATTACHED,
    GPIO_INT_DETACHED,
} gpio_int_state_t;

struct sunxi_gpio_dev_s;

struct sunxi_gpio_map
{
	uint16_t nuttx_pin;
	uint16_t sunxi_pin;
	enum gpio_pintype_e pin_type;
};

struct sunxi_gpio_callback_s
{
#ifdef CONFIG_IOEXPANDER_INT_ENABLE
    ioe_callback_t cbfunc;
#endif
    FAR void *cbarg;
    unsigned int flags;
    unsigned int pin;
    unsigned int state;
    struct sunxi_gpio_dev_s *priv;
};

struct sunxi_gpio_dev_s
{
  struct ioexpander_dev_s dev;
  struct work_s work;
  struct sunxi_gpio_callback_s cb[CONFIG_IOEXPANDER_NPINS];
};

/****************************************************************************
 * Private Data
 ****************************************************************************/
static struct sunxi_gpio_dev_s g_sunxi_gpio_dev;

static struct sunxi_gpio_map maps[] = {
#ifndef CONFIG_ARCH_BOARD_R528S3_GEMINI_S1
  {0, GPIOC(0), GPIO_OUTPUT_PIN},
	{1, GPIOC(1), GPIO_OUTPUT_PIN},
#elif defined (CONFIG_ARCH_BOARD_R528S3_GEMINI_XTS)
	{0, GPIOD(21), GPIO_OUTPUT_PIN},
	{1, GPIOD(22), GPIO_OUTPUT_PIN},
#endif
	{2, GPIOG(18), GPIO_OUTPUT_PIN},
	{3, GPIOB(10), GPIO_OUTPUT_PIN},
};

static int map_pin(uint8_t pin)
{
   int i;
   for(i = 0; i < sizeof(maps)/sizeof(maps[0]); i++) {
	   if (maps[i].nuttx_pin == pin) {
		   return maps[i].sunxi_pin;
	   }
   }
   return -1;
}

static int sunxi_gpio_direction(FAR struct ioexpander_dev_s *dev, uint8_t pin,
                             int direction)
{
  int ret = -EINVAL;
  int sunxi_pin;

  sunxi_pin = map_pin(pin);
  if (sunxi_pin < 0) {
    return -EINVAL;
  }

  if (direction == IOEXPANDER_DIRECTION_IN) {
    ret = hal_gpio_pinmux_set_function(sunxi_pin, GPIO_MUXSEL_IN);
  } else if (direction == IOEXPANDER_DIRECTION_IN_PULLUP){
    ret = hal_gpio_pinmux_set_function(sunxi_pin, GPIO_MUXSEL_IN);
    ret = hal_gpio_set_pull(sunxi_pin, GPIO_PULL_UP);
  } else if (direction == IOEXPANDER_DIRECTION_IN_PULLDOWN){
    ret = hal_gpio_pinmux_set_function(sunxi_pin, GPIO_MUXSEL_IN);
    ret = hal_gpio_set_pull(sunxi_pin, GPIO_PULL_DOWN);
  } else if (direction == IOEXPANDER_DIRECTION_OUT){
    ret = hal_gpio_pinmux_set_function(sunxi_pin, GPIO_MUXSEL_OUT);
  }
  return ret;
}

static int sunxi_gpio_option(FAR struct ioexpander_dev_s *dev, uint8_t pin,
                           int opt, FAR void *value)
{
  int ret = 0;

#ifdef CONFIG_IOEXPANDER_INT_ENABLE
  FAR struct sunxi_gpio_dev_s *priv = (FAR struct sunxi_gpio_dev_s *) dev;
  unsigned int ival = (unsigned int)((uintptr_t)value);

  if (opt == IOEXPANDER_OPTION_INTCFG) {
      switch (ival)
        {
          case IOEXPANDER_VAL_HIGH:    /* Interrupt on high level */
            priv->cb[pin].flags = IRQ_TYPE_LEVEL_HIGH;
            break;

          case IOEXPANDER_VAL_LOW:     /* Interrupt on low level */
            priv->cb[pin].flags = IRQ_TYPE_LEVEL_LOW;
            break;

          case IOEXPANDER_VAL_RISING:  /* Interrupt on rising edge */
            priv->cb[pin].flags = IRQ_TYPE_EDGE_RISING;
            break;

          case IOEXPANDER_VAL_FALLING: /* Interrupt on falling edge */
            priv->cb[pin].flags = IRQ_TYPE_EDGE_FALLING;
            break;

          case IOEXPANDER_VAL_BOTH:    /* Interrupt on both edges */
            priv->cb[pin].flags = IRQ_TYPE_EDGE_BOTH;
            break;

          case IOEXPANDER_VAL_DISABLE:
            priv->cb[pin].flags = IRQ_TYPE_NONE;
            break;

          default:
            ret = -EINVAL;
            break;
        }
    }
#endif

  return ret;
}

static int sunxi_gpio_writepin(FAR struct ioexpander_dev_s *dev, uint8_t pin,
                             bool value)
{
  int ret;
  int sunxi_pin;

  sunxi_pin = map_pin(pin);
  if (sunxi_pin < 0) {
      return -EINVAL;
  }

  ret = hal_gpio_set_data(sunxi_pin, value);
  return ret;
}

static int sunxi_gpio_readpin(FAR struct ioexpander_dev_s *dev, uint8_t pin,
                             bool *value)
{
  int ret;
  int sunxi_pin;
  gpio_data_t data;

  sunxi_pin = map_pin(pin);
  if (sunxi_pin < 0) {
      return -EINVAL;
  }

  ret = hal_gpio_get_data(sunxi_pin, &data);
  *value = (bool)data;
  return ret;
}

static int sunxi_gpio_readbuf(FAR struct ioexpander_dev_s *dev, uint8_t pin,
                             bool *value)
{
  int ret;
  int sunxi_pin;
  gpio_data_t data;

  sunxi_pin = map_pin(pin);
  if (sunxi_pin < 0) {
      return -EINVAL;
  }

  ret = hal_gpio_get_data(sunxi_pin, &data);
  *value = (bool)data;
  return ret;
}

#ifdef CONFIG_IOEXPANDER_INT_ENABLE
static void sunxi_gpio_irqworker(void *args)
{
    struct sunxi_gpio_callback_s *cb = (struct sunxi_gpio_callback_s *)args;
    if (cb && cb->cbfunc)
        cb->cbfunc((FAR struct ioexpander_dev_s *)cb->priv, (1 << cb->pin), cb->cbarg);
}

static void *sunxi_gpio_interrupt(void *args)
{
	struct sunxi_gpio_callback_s *cb = (struct sunxi_gpio_callback_s *)args;

	if (work_available(&cb->priv->work)) {
		work_queue(HPWORK, &cb->priv->work, sunxi_gpio_irqworker, (FAR void *)args, 0);
	}
    return NULL;
}

static FAR void *sunxi_gpio_attach(FAR struct ioexpander_dev_s *dev, ioe_pinset_t pinset,
		ioe_callback_t callback, FAR void *arg)
{
    int i;
	uint32_t irq;
    int sunxi_pin;

    FAR struct sunxi_gpio_dev_s *priv = (struct sunxi_gpio_dev_s *) dev;

    for(i = 0; i < CONFIG_IOEXPANDER_NPINS; i++) {
        if (pinset & (1 << i)) {
            sunxi_pin = map_pin(i);
            if (sunxi_pin < 0) {
                continue;
            }
            if (hal_gpio_to_irq(sunxi_pin, &irq) < 0) {
                continue;
            }
            if (hal_gpio_irq_request(irq, (hal_irq_handler_t)sunxi_gpio_interrupt, priv->cb[i].flags, &priv->cb[i]) < 0) {
                continue;
            }
            priv->cb[i].state = GPIO_INT_ATTACHED;
            priv->cb[i].cbfunc = callback;
            priv->cb[i].cbarg = arg;
            priv->cb[i].priv = priv;
            priv->cb[i].pin = i;
            if (hal_gpio_irq_enable(irq) < 0) {
                hal_gpio_irq_free(irq);
                priv->cb[i].state = GPIO_INT_DETACHED;
                priv->cb[i].cbfunc = NULL;
                priv->cb[i].cbarg = NULL;
                continue;
            }
        }
    }
    return (void *)(uintptr_t)(pinset);
}

static int sunxi_gpio_detach(FAR struct ioexpander_dev_s *dev, FAR void *handle)
{
    int i;
    uint32_t irq;
    int sunxi_pin;

    ioe_pinset_t pinset = (ioe_pinset_t)(uintptr_t)handle;
    FAR struct sunxi_gpio_dev_s *priv = (FAR struct sunxi_gpio_dev_s *) dev;

    for(i = 0; i < CONFIG_IOEXPANDER_NPINS; i++) {
        if (pinset & (1 << i) && (priv->cb[i].state == GPIO_INT_ATTACHED)) {
            sunxi_pin = map_pin(i);
            if (sunxi_pin < 0) {
                continue;
            }
			if (hal_gpio_to_irq(sunxi_pin, &irq) < 0) {
                continue;
            }
            if (hal_gpio_irq_disable(irq) < 0) {
                continue;
            }
            if (hal_gpio_irq_free(irq) < 0) {
                continue;
            }
            priv->cb[i].state = GPIO_INT_DETACHED;
            priv->cb[i].cbfunc = NULL;
            priv->cb[i].cbarg = NULL;
        }
    }
    return OK;
}
#endif

static const struct ioexpander_ops_s g_sunxi_gpio_ops =
{
  .ioe_direction = sunxi_gpio_direction,
  .ioe_option = sunxi_gpio_option,
  .ioe_writepin = sunxi_gpio_writepin,
  .ioe_readpin = sunxi_gpio_readpin,
  .ioe_readbuf = sunxi_gpio_readbuf,
#ifdef CONFIG_IOEXPANDER_INT_ENABLE
  .ioe_attach = sunxi_gpio_attach,
  .ioe_detach = sunxi_gpio_detach,
#endif
};

FAR struct ioexpander_dev_s *r528_gpio_initialize(void)
{
  int i;
  FAR struct sunxi_gpio_dev_s *priv;

  priv = &g_sunxi_gpio_dev;
  priv->dev.ops = &g_sunxi_gpio_ops;

  for(i = 0; i < sizeof(maps)/sizeof(maps[0]); i++) {
      gpio_lower_half(&priv->dev, maps[i].nuttx_pin, maps[i].pin_type, maps[i].nuttx_pin);
  }

  return &priv->dev;
}
#endif
