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
#if defined(CONFIG_DRIVERS_SPI) && defined(CONFIG_SPI_DRIVER)
#include <debug.h>
#include <sys/param.h>
#include <sys/types.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>

#include <nuttx/arch.h>
#include <nuttx/irq.h>
#include <nuttx/clock.h>
#include <nuttx/mutex.h>
#include <nuttx/semaphore.h>
#include <nuttx/spi/spi.h>
#include <nuttx/spi/spi_transfer.h>

#include <arch/board/board.h>

#include "sunxi_hal_spi.h"

#define SPI_FREQUENCY_40M 40000000
#define SPI_FREQUENCY_10M 10000000

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct sunxi_spi_priv_s
{
  struct spi_dev_s spi_dev;
  struct hal_spi_master spim;
  enum spi_mode_e mode;
  uint32_t actual;
  uint8_t nbits;
  int refs;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int sunxi_spi_lock(struct spi_dev_s *dev, bool lock);
#ifndef CONFIG_ESP32_SPI_UDCS
static void sunxi_spi_select(struct spi_dev_s *dev,
                             uint32_t devid, bool selected);
#endif
static uint32_t sunxi_spi_setfrequency(struct spi_dev_s *dev,
                                       uint32_t frequency);
static void sunxi_spi_setmode(struct spi_dev_s *dev,
                              enum spi_mode_e mode);
static void sunxi_spi_setbits(struct spi_dev_s *dev, int nbits);
#ifdef CONFIG_SPI_HWFEATURES
static int sunxi_spi_hwfeatures(struct spi_dev_s *dev,
                                spi_hwfeatures_t features);
#endif
static uint32_t sunxi_spi_send(struct spi_dev_s *dev, uint32_t wd);
static void sunxi_spi_exchange(struct spi_dev_s *dev,
                               const void *txbuffer,
                               void *rxbuffer, size_t nwords);
#ifndef CONFIG_SPI_EXCHANGE
static void sunxi_spi_sndblock(struct spi_dev_s *dev,
                               const void *txbuffer,
                               size_t nwords);
static void sunxi_spi_recvblock(struct spi_dev_s *dev,
                                void *rxbuffer,
                                size_t nwords);
#endif
#ifdef CONFIG_SPI_TRIGGER
static int sunxi_spi_trigger(struct spi_dev_s *dev);
#endif
static void sunxi_spi_init(struct spi_dev_s *dev);
static void sunxi_spi_deinit(struct spi_dev_s *dev);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct spi_ops_s sunxi_spi_ops =
{
  .lock              = sunxi_spi_lock,
  .select            = sunxi_spi_select,
  .setfrequency      = sunxi_spi_setfrequency,
  .setmode           = sunxi_spi_setmode,
  .setbits           = sunxi_spi_setbits,
#ifdef CONFIG_SPI_HWFEATURES
  .hwfeatures        = sunxi_spi_hwfeatures,
#endif
  .send              = sunxi_spi_send,
#ifdef CONFIG_SPI_EXCHANGE
  .exchange          = sunxi_spi_exchange,
#else
  .sndblock          = sunxi_spi_sndblock,
  .recvblock         = sunxi_spi_recvblock,
#endif
#ifdef CONFIG_SPI_TRIGGER
  .trigger           = sunxi_spi_trigger,
#endif
  .registercallback  = NULL,
};

static struct sunxi_spi_priv_s sunxi_spi0_priv =
{
  .spi_dev =
    {
      .ops = &sunxi_spi_ops
    },
  .spim =
    {
      .port = 0,
      .cfg =
	    {
          .flash = 1,
	    },
    },
  .refs  = 0,
};
#ifdef CONFIG_LCD_SUPPORT_SSD1306
static struct sunxi_spi_priv_s sunxi_spi1_priv =
{
  .spi_dev =
    {
      .ops = &sunxi_spi_ops
    },
  .spim =
    {
      .port = 1,
      .cfg =
	    {
          .flash = 0,
          .bit_order = HAL_SPI_MASTER_MSB_FIRST,
          .clock_frequency = SPI_MAX_FREQUENCY,
          .cpha = HAL_SPI_MASTER_CLOCK_PHASE0,  /* SPI Mode 0: 第一个边沿采样 */
          .cpol = HAL_SPI_MASTER_CLOCK_POLARITY0,  /* SPI Mode 0: 时钟空闲为低 */
          .slave_port = 0,  /* CS0 */
	    },
    },
  .mode = SPIDEV_MODE0,  /* SPI Mode 0: CPOL=0, CPHA=0 */
  .nbits = 8,  /* 8-bit transfers for SSD1306 */
  .refs  = 0,
};
#elif defined(CONFIG_LCD_SUPPORT_ILI9341)
  static struct sunxi_spi_priv_s sunxi_spi1_priv =
  {
    .spi_dev =
      {
        .ops = &sunxi_spi_ops
      },
    .spim =
      {
        .port = 1,
        .cfg =
        {
            .flash = 0,
            .bit_order = HAL_SPI_MASTER_MSB_FIRST,
            .clock_frequency = SPI_FREQUENCY_40M,
            .cpha = HAL_SPI_MASTER_CLOCK_PHASE0,  /* SPI Mode 0: 第一个边沿采样 */
            .cpol = HAL_SPI_MASTER_CLOCK_PHASE0,  /* SPI Mode 0: 时钟空闲为低 */
            .slave_port = 0,  /* CS0 */
        },
      },
    .refs  = 0,
    .mode = SPIDEV_MODE0,  /* SPI Mode 0: CPOL=0, CPHA=0 */
    .nbits = 8,  /* 8-bit transfers for SSD1306 */
  };
#elif defined(CONFIG_LCD_SUPPORT_ST7789)
  static struct sunxi_spi_priv_s sunxi_spi1_priv =
  {
    .spi_dev =
      {
        .ops = &sunxi_spi_ops
      },
    .spim =
      {
        .port = 1,
        .cfg =
        {
            .flash = 0,
            .bit_order = HAL_SPI_MASTER_MSB_FIRST,
            .clock_frequency = SPI_FREQUENCY_40M,
            .cpha = HAL_SPI_MASTER_CLOCK_PHASE0,  /* SPI Mode 0: 第一个边沿采样 */
            .cpol = HAL_SPI_MASTER_CLOCK_PHASE0,  /* SPI Mode 0: 时钟空闲为低 */
            .slave_port = 0,  /* CS0 */
        },
      },
    .refs  = 0,
    .mode = SPIDEV_MODE0,  /* SPI Mode 0: CPOL=0, CPHA=0 */
    .nbits = 8,  /* 8-bit transfers for SSD1306 */
  };
#else
  static struct sunxi_spi_priv_s sunxi_spi1_priv =
  {
    .spi_dev =
      {
        .ops = &sunxi_spi_ops
      },
    .spim =
      {
        .port = 1,
        .cfg =
        {
            .flash = 0,
            .bit_order = HAL_SPI_MASTER_MSB_FIRST,
            .clock_frequency = SPI_MAX_FREQUENCY,
            .cpha = HAL_SPI_MASTER_CLOCK_PHASE0,  /* SPI Mode 0: 第一个边沿采样 */
            .cpol = HAL_SPI_MASTER_CLOCK_PHASE0,  /* SPI Mode 0: 时钟空闲为低 */
            .slave_port = 0,  /* CS0 */
        },
      },
    .refs  = 0,
    .mode = SPIDEV_MODE0,  /* SPI Mode 0: CPOL=0, CPHA=0 */
    .nbits = 8,  /* 8-bit transfers for SSD1306 */
  };
#endif
/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int sunxi_spi_lock(struct spi_dev_s *dev, bool lock)
{
  return OK;
}

static void sunxi_spi_select(struct spi_dev_s *dev,
                             uint32_t devid, bool selected)
{
  struct sunxi_spi_priv_s *priv = (struct sunxi_spi_priv_s *)dev;
  priv->spim.cfg.slave_port = devid;
  hal_spi_set_cs(priv->spim.port, priv->spim.cfg.slave_port);
}

static uint32_t sunxi_spi_setfrequency(struct spi_dev_s *dev,
                                       uint32_t frequency)
{
  struct sunxi_spi_priv_s *priv = (struct sunxi_spi_priv_s *)dev;
  priv->spim.cfg.clock_frequency = frequency;

  hal_spi_hw_config(priv->spim.port, &priv->spim.cfg);

  priv->actual = hal_spi_get_rate(priv->spim.port);

  return priv->actual;
}

static void sunxi_spi_setmode(struct spi_dev_s *dev,
                              enum spi_mode_e mode)
{
  struct sunxi_spi_priv_s *priv = (struct sunxi_spi_priv_s *)dev;

  spiinfo("mode=%d\n", mode);

  /* Has the mode changed? */
	if (mode != priv->mode)
    {
      switch (mode)
        {
        case SPIDEV_MODE0: /* CPOL=0; CPHA=0 */
          priv->spim.cfg.cpha = HAL_SPI_MASTER_CLOCK_PHASE0;
          priv->spim.cfg.cpol = HAL_SPI_MASTER_CLOCK_POLARITY0;
          break;

        case SPIDEV_MODE1: /* CPOL=0; CPHA=1 */
          priv->spim.cfg.cpha = HAL_SPI_MASTER_CLOCK_PHASE1;
          priv->spim.cfg.cpol = HAL_SPI_MASTER_CLOCK_POLARITY0;
          break;

        case SPIDEV_MODE2: /* CPOL=1; CPHA=0 */
          priv->spim.cfg.cpha = HAL_SPI_MASTER_CLOCK_PHASE0;
          priv->spim.cfg.cpol = HAL_SPI_MASTER_CLOCK_POLARITY1;
          break;

        case SPIDEV_MODE3: /* CPOL=1; CPHA=1 */
          priv->spim.cfg.cpha = HAL_SPI_MASTER_CLOCK_PHASE1;
          priv->spim.cfg.cpol = HAL_SPI_MASTER_CLOCK_POLARITY1;
          break;

        default:
          return;
        }
        hal_spi_hw_config(priv->spim.port, &priv->spim.cfg);
    }
}

static void sunxi_spi_setbits(struct spi_dev_s *dev, int nbits)
{
  struct sunxi_spi_priv_s *priv = (struct sunxi_spi_priv_s *)dev;
  spiinfo("nbits=%d\n", nbits);
  priv->nbits = nbits;
}

#ifdef CONFIG_SPI_HWFEATURES
static int sunxi_spi_hwfeatures(struct spi_dev_s *dev,
                                spi_hwfeatures_t features)
{
  /* Other H/W features are not supported */

  return (features == 0) ? OK : -ENOSYS;
}
#endif

static uint32_t sunxi_spi_poll_send(struct sunxi_spi_priv_s *priv,
                                    uint32_t wd)
{
  if (hal_spi_write(priv->spim.port, &wd, sizeof(wd)) < 0) {
	  return -1;
  }
  return OK;
}

static uint32_t sunxi_spi_send(struct spi_dev_s *dev, uint32_t wd)
{
  struct sunxi_spi_priv_s *priv = (struct sunxi_spi_priv_s *)dev;

  return sunxi_spi_poll_send(priv, wd);
}

static void sunxi_spi_exchange(struct spi_dev_s *dev,
                               const void *txbuffer,
                               void *rxbuffer,
                               size_t nwords)
{
  struct sunxi_spi_priv_s *priv = (struct sunxi_spi_priv_s *)dev;

	hal_spi_master_transfer_t tr;

	memset(&tr, 0, sizeof(tr));

	tr.tx_buf = (uint8_t *)txbuffer;
	tr.rx_buf = rxbuffer;
	tr.tx_len = nwords;
	tr.rx_len = nwords;
    tr.rx_nbits = tr.tx_nbits = priv->nbits;
	tr.tx_single_len = nwords;
	tr.dummy_byte = 0;

	hal_spi_xfer(priv->spim.port, &tr);
}

#ifndef CONFIG_SPI_EXCHANGE

static void sunxi_spi_sndblock(struct spi_dev_s *dev,
                               const void *txbuffer,
                               size_t nwords)
{
  spiinfo("txbuffer=%p nwords=%d\n", txbuffer, nwords);

  sunxi_spi_exchange(dev, txbuffer, NULL, nwords);
}

static void sunxi_spi_recvblock(struct spi_dev_s *dev,
                                void *rxbuffer,
                                size_t nwords)
{
  spiinfo("rxbuffer=%p nwords=%d\n", rxbuffer, nwords);
  sunxi_spi_exchange(dev, NULL, rxbuffer, nwords);
}
#endif

#ifdef CONFIG_SPI_TRIGGER
static int sunxi_spi_trigger(struct spi_dev_s *dev)
{
  return -ENOSYS;
}
#endif

static void sunxi_spi_init(struct spi_dev_s *dev)
{
  struct sunxi_spi_priv_s *priv = (struct sunxi_spi_priv_s *)dev;
  hal_spi_init(priv->spim.port, &priv->spim.cfg);
}

static void sunxi_spi_deinit(struct spi_dev_s *dev)
{
  struct sunxi_spi_priv_s *priv = (struct sunxi_spi_priv_s *)dev;
  hal_spi_deinit(priv->spim.port);
}

struct spi_dev_s *sunxi_spibus_initialize(int port)
{
  struct spi_dev_s *spi_dev;
  struct sunxi_spi_priv_s *priv;

  switch (port)
    {
      case 0:
        priv = &sunxi_spi0_priv;
        break;
      case 1:
        priv = &sunxi_spi1_priv;
        break;
      default:
        return NULL;
    }

  spi_dev = (struct spi_dev_s *)priv;

  if (priv->refs != 0)
    {
      priv->refs++;
      return spi_dev;
    }

  sunxi_spi_init(spi_dev);
  priv->refs++;

  if (spi_register(spi_dev, port) < 0) {
	  priv->refs--;
      sunxi_spi_deinit(spi_dev);
	  spi_dev = NULL;
  }
  return spi_dev;
}

int sunxi_spibus_uninitialize(struct spi_dev_s *dev)
{
  struct sunxi_spi_priv_s *priv = (struct sunxi_spi_priv_s *)dev;

  DEBUGASSERT(dev);

  if (priv->refs == 0)
    {
      return ERROR;
    }

  if (--priv->refs != 0)
    {
      return OK;
    }

  sunxi_spi_deinit(dev);

  return OK;
}

#endif /* CONFIG_DRIVERS_SPI */
