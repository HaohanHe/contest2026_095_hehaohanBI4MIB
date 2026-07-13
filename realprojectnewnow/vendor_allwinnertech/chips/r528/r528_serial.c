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

#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <semaphore.h>
#include <string.h>
#include <errno.h>
#include <debug.h>
#ifdef CONFIG_SERIAL_TERMIOS
#  include <termios.h>
#endif

#include <nuttx/irq.h>
#include <nuttx/arch.h>
#include <nuttx/fs/ioctl.h>
#include <nuttx/serial/serial.h>
#include <arch/board/board.h>

#include "arm_internal.h"

#include "chip.h"
#include "hardware/r528_uart.h"
#include "r528_serial.h"

#include "gic.h"
#include "r528_lowputc.h"

#include <sunxi_hal_common.h>
#include <hal_gpio.h>

/****************************************************************************
 * Pre-processor definitions
 ****************************************************************************/

/* If we are not using the serial driver for the console, then we still must
 * provide some minimal implementation of up_putc.
 */

/* SCLK is the UART input clock.
 *
 * Through experimentation, it has been found that the serial clock is
 * OSC24M
 */

#define R528_SCLK 24000000
/****************************************************************************
 * Private Types
 ****************************************************************************/

struct up_dev_s
{
  uint32_t uartbase;  /* Base address of UART registers */
  uint32_t baud;      /* Configured baud */
  uint32_t ier;       /* Saved IER value */
  uint8_t  irq;       /* IRQ associated with this UART */
  uint8_t  parity;    /* 0=none, 1=odd, 2=even */
  uint8_t  bits;      /* Number of bits (7 or 8) */
  bool     stopbits2; /* true: Configure with 2 stop bits instead of 1 */
  bool     hwflowctrl;
  int      port;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int  up_setup(struct uart_dev_s *dev);
static void up_shutdown(struct uart_dev_s *dev);
static int  up_attach(struct uart_dev_s *dev);
static void up_detach(struct uart_dev_s *dev);
static int  uart_interrupt(int irq, void *context, void *arg);
static int  up_ioctl(struct file *filep, int cmd, unsigned long arg);
static int  up_receive(struct uart_dev_s *dev, unsigned int *status);
static void up_rxint(struct uart_dev_s *dev, bool enable);
static bool up_rxavailable(struct uart_dev_s *dev);
static void up_send(struct uart_dev_s *dev, int ch);
static void up_txint(struct uart_dev_s *dev, bool enable);
static bool up_txready(struct uart_dev_s *dev);
static bool up_txempty(struct uart_dev_s *dev);
#ifdef CONFIG_SERIAL_IFLOWCONTROL
static bool up_rxflowcontrol(struct uart_dev_s *dev,
                             unsigned int nbuffered, bool upper);
#endif

extern int hal_gpio_pinmux_set_function_early(unsigned long gpio_membase, gpio_pin_t pin, gpio_muxsel_t function_index);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct uart_ops_s g_uart_ops =
{
  .setup          = up_setup,
  .shutdown       = up_shutdown,
  .attach         = up_attach,
  .detach         = up_detach,
  .ioctl          = up_ioctl,
  .receive        = up_receive,
  .rxint          = up_rxint,
  .rxavailable    = up_rxavailable,
#ifdef CONFIG_SERIAL_IFLOWCONTROL
  .rxflowcontrol  = up_rxflowcontrol,
#endif
  .send           = up_send,
  .txint          = up_txint,
  .txready        = up_txready,
  .txempty        = up_txempty,
};

/* I/O buffers */

#ifdef CONFIG_R528_UART0
static char g_uart0rxbuffer[CONFIG_UART0_RXBUFSIZE];
static char g_uart0txbuffer[CONFIG_UART0_TXBUFSIZE];
#endif

#ifdef CONFIG_R528_UART1
static char g_uart1rxbuffer[CONFIG_UART1_RXBUFSIZE];
static char g_uart1txbuffer[CONFIG_UART1_TXBUFSIZE];
#endif

#ifdef CONFIG_R528_UART2
static char g_uart2rxbuffer[CONFIG_UART2_RXBUFSIZE];
static char g_uart2txbuffer[CONFIG_UART2_TXBUFSIZE];
#endif

#ifdef CONFIG_R528_UART3
static char g_uart3rxbuffer[CONFIG_UART3_RXBUFSIZE];
static char g_uart3txbuffer[CONFIG_UART3_TXBUFSIZE];
#endif

/* This describes the state of the A1X UART0 port. */

#ifdef CONFIG_R528_UART0
static struct up_dev_s g_uart0priv =
{
  .uartbase       = R528_UART0_VADDR,
  .baud           = CONFIG_UART0_BAUD,
  .irq            = R528_IRQ_UART0,
  .parity         = CONFIG_UART0_PARITY,
  .bits           = CONFIG_UART0_BITS,
  .stopbits2      = CONFIG_UART0_2STOP,
  .hwflowctrl     = false,
  .port           = 0,
};

static uart_dev_t g_uart0port =
{
  .recv     =
  {
    .size   = CONFIG_UART0_RXBUFSIZE,
    .buffer = g_uart0rxbuffer,
  },
  .xmit     =
  {
    .size   = CONFIG_UART0_TXBUFSIZE,
    .buffer = g_uart0txbuffer,
  },
  .ops      = &g_uart_ops,
  .priv     = &g_uart0priv,
};
#endif

/* This describes the state of the A1X UART1 port. */

#ifdef CONFIG_R528_UART1
static struct up_dev_s g_uart1priv =
{
  .uartbase       = R528_UART1_VADDR,
  .baud           = CONFIG_UART1_BAUD,
  .irq            = R528_IRQ_UART1,
  .parity         = CONFIG_UART1_PARITY,
  .bits           = CONFIG_UART1_BITS,
  .stopbits2      = CONFIG_UART1_2STOP,
  .hwflowctrl     = true,
  .port           = 1,
};

static uart_dev_t g_uart1port =
{
  .recv     =
  {
    .size   = CONFIG_UART1_RXBUFSIZE,
    .buffer = g_uart1rxbuffer,
  },
  .xmit     =
  {
    .size   = CONFIG_UART1_TXBUFSIZE,
    .buffer = g_uart1txbuffer,
  },
  .ops      = &g_uart_ops,
  .priv     = &g_uart1priv,
};
#endif

/* This describes the state of the A1X UART2 port. */

#ifdef CONFIG_R528_UART2
static struct up_dev_s g_uart2priv =
{
  .uartbase       = R528_UART2_VADDR,
  .baud           = CONFIG_UART2_BAUD,
  .irq            = R528_IRQ_UART2,
  .parity         = CONFIG_UART2_PARITY,
  .bits           = CONFIG_UART2_BITS,
  .stopbits2      = CONFIG_UART2_2STOP,
  .hwflowctrl     = false,
  .port           = 2,
};

static uart_dev_t g_uart2port =
{
  .recv     =
  {
    .size   = CONFIG_UART2_RXBUFSIZE,
    .buffer = g_uart2rxbuffer,
  },
  .xmit     =
  {
    .size   = CONFIG_UART2_TXBUFSIZE,
    .buffer = g_uart2txbuffer,
  },
  .ops      = &g_uart_ops,
  .priv     = &g_uart2priv,
};
#endif

/* This describes the state of the A1X UART3 port. */

#ifdef CONFIG_R528_UART3
static struct up_dev_s g_uart3priv =
{
  .uartbase       = R528_UART3_VADDR,
  .baud           = CONFIG_UART3_BAUD,
  .irq            = R528_IRQ_UART3,
  .parity         = CONFIG_UART3_PARITY,
  .bits           = CONFIG_UART3_BITS,
  .stopbits2      = CONFIG_UART3_2STOP,
  .hwflowctrl     = false,
  .port           = 3,
};

static uart_dev_t g_uart3port =
{
  .recv     =
  {
    .size   = CONFIG_UART3_RXBUFSIZE,
    .buffer = g_uart3rxbuffer,
  },
  .xmit     =
  {
    .size   = CONFIG_UART3_TXBUFSIZE,
    .buffer = g_uart3txbuffer,
  },
  .ops      = &g_uart_ops,
  .priv     = &g_uart3priv,
};
#endif


/* Which UART with be tty0/console and which tty1-7?  The console will always
 * be ttyS0.  If there is no console then will use the lowest numbered UART.
 */

/* First pick the console and ttys0.  This could be any of UART0-7 */

#if defined(CONFIG_UART0_SERIAL_CONSOLE)
#    define CONSOLE_DEV         g_uart0port /* UART0 is console */
#    define TTYS0_DEV           g_uart0port /* UART0 is ttyS0 */
#    define UART0_ASSIGNED      1
#elif defined(CONFIG_UART1_SERIAL_CONSOLE)
#    define CONSOLE_DEV         g_uart1port /* UART1 is console */
#    define TTYS0_DEV           g_uart1port /* UART1 is ttyS0 */
#    define UART1_ASSIGNED      1
#elif defined(CONFIG_UART2_SERIAL_CONSOLE)
#    define CONSOLE_DEV         g_uart2port /* UART2 is console */
#    define TTYS0_DEV           g_uart2port /* UART2 is ttyS0 */
#    define UART2_ASSIGNED      1
#elif defined(CONFIG_UART3_SERIAL_CONSOLE)
#    define CONSOLE_DEV         g_uart3port /* UART3 is console */
#    define TTYS0_DEV           g_uart3port /* UART3 is ttyS0 */
#    define UART3_ASSIGNED      1
#else
#  undef CONSOLE_DEV                        /* No console */
#  if defined(CONFIG_R528_UART0)
#    define TTYS0_DEV           g_uart0port /* UART0 is ttyS0 */
#    define UART0_ASSIGNED      1
#  elif defined(CONFIG_R528_UART1)
#    define TTYS0_DEV           g_uart1port /* UART1 is ttyS0 */
#    define UART1_ASSIGNED      1
#  elif defined(CONFIG_R528_UART2)
#    define TTYS0_DEV           g_uart2port /* UART2 is ttyS0 */
#    define UART2_ASSIGNED      1
#  elif defined(CONFIG_R528_UART3)
#    define TTYS0_DEV           g_uart3port /* UART3 is ttyS0 */
#    define UART3_ASSIGNED      1
#  endif
#endif

/* Pick ttys1.  This could be any of UART0-7 excluding the console UART. */

#if defined(CONFIG_R528_UART0) && !defined(UART0_ASSIGNED)
#  define TTYS1_DEV           g_uart0port /* UART0 is ttyS1 */
#  define UART0_ASSIGNED      1
#elif defined(CONFIG_R528_UART1) && !defined(UART1_ASSIGNED)
#  define TTYS1_DEV           g_uart1port /* UART1 is ttyS1 */
#  define UART1_ASSIGNED      1
#elif defined(CONFIG_R528_UART2) && !defined(UART2_ASSIGNED)
#  define TTYS1_DEV           g_uart2port /* UART2 is ttyS1 */
#  define UART2_ASSIGNED      1
#elif defined(CONFIG_R528_UART3) && !defined(UART3_ASSIGNED)
#  define TTYS1_DEV           g_uart3port /* UART3 is ttyS1 */
#  define UART3_ASSIGNED      1
#endif

/* Pick ttys2.  This could be one of UART1-7. It can't be UART0 because that
 * was either assigned as ttyS0 or ttys1.  One of UART 1-7 could also be the
 * console.
 */

#if defined(CONFIG_R528_UART1) && !defined(UART1_ASSIGNED)
#  define TTYS2_DEV           g_uart1port /* UART1 is ttyS2 */
#  define UART1_ASSIGNED      1
#elif defined(CONFIG_R528_UART2) && !defined(UART2_ASSIGNED)
#  define TTYS2_DEV           g_uart2port /* UART2 is ttyS2 */
#  define UART2_ASSIGNED      1
#elif defined(CONFIG_R528_UART3) && !defined(UART3_ASSIGNED)
#  define TTYS2_DEV           g_uart3port /* UART3 is ttyS2 */
#  define UART3_ASSIGNED      1
#endif

/* Pick ttys3. This could be one of UART2-7. It can't be UART0-1 because
 * those have already been assigned to ttsyS0, 1, or 2.  One of
 * UART 2-7 could also be the console.
 */

#if defined(CONFIG_R528_UART2) && !defined(UART2_ASSIGNED)
#  define TTYS3_DEV           g_uart2port /* UART2 is ttyS3 */
#  define UART2_ASSIGNED      1
#elif defined(CONFIG_R528_UART3) && !defined(UART3_ASSIGNED)
#  define TTYS3_DEV           g_uart3port /* UART3 is ttyS3 */
#  define UART3_ASSIGNED      1
#endif

/****************************************************************************
 * Inline Functions
 ****************************************************************************/

/****************************************************************************
 * Name: arm_serialin
 ****************************************************************************/

static inline uint32_t arm_serialin(struct up_dev_s *priv, int offset)
{
  return getreg32(priv->uartbase + offset);
}

/****************************************************************************
 * Name: arm_serialout
 ****************************************************************************/

static inline void arm_serialout(struct up_dev_s *priv, int offset, uint32_t value)
{
  putreg32(value, priv->uartbase + offset);
}

/****************************************************************************
 * Name: up_disableuartint
 ****************************************************************************/

static inline void up_disableuartint(struct up_dev_s *priv, uint32_t *ier)
{
  if (ier)
    {
      *ier = priv->ier & UART_IER_ALLIE;
    }

  priv->ier &= ~UART_IER_ALLIE;
  arm_serialout(priv, R528_UART_IER_OFFSET, priv->ier);
}

/****************************************************************************
 * Name: up_restoreuartint
 ****************************************************************************/

static inline void up_restoreuartint(struct up_dev_s *priv, uint32_t ier)
{
  priv->ier |= ier & UART_IER_ALLIE;
  arm_serialout(priv, R528_UART_IER_OFFSET, priv->ier);
}

/****************************************************************************
 * Name: up_enablebreaks
 ****************************************************************************/

static inline void up_enablebreaks(struct up_dev_s *priv, bool enable)
{
  uint32_t lcr = arm_serialin(priv, R528_UART_LCR_OFFSET);

  if (enable)
    {
      lcr |= UART_LCR_BC;
    }
  else
    {
      lcr &= ~UART_LCR_BC;
    }

  arm_serialout(priv, R528_UART_LCR_OFFSET, lcr);
}

/************************************************************************************
 * Name: r528_uart0config, uart1config, uart2config, ..., uart7config
 *
 * Description:
 *   Configure the UART
 *
 ************************************************************************************/
void sunxi_clock_init_uart(int port)
{
	unsigned int i, reg;

#define CCMU_UART_BGR_REG         (R528_CCMU_VADDR + 0x90C)
#define CCM_UART_RST_OFFSET       (16)
#define CCM_UART_GATING_OFFSET    (0)

	/* reset */
	reg = hal_readl(CCMU_UART_BGR_REG);
	reg &= ~(1<<(CCM_UART_RST_OFFSET + port));
	hal_writel(reg, CCMU_UART_BGR_REG);
	for (i = 0; i < 100; i++)
		;
	reg |= (1 << (CCM_UART_RST_OFFSET + port));
	hal_writel(reg, CCMU_UART_BGR_REG);
	/* gate */
	reg = hal_readl(CCMU_UART_BGR_REG);
	reg &= ~(1<<(CCM_UART_GATING_OFFSET + port));
	hal_writel(reg, CCMU_UART_BGR_REG);
	for (i = 0; i < 100; i++)
		;
	reg |= (1 << (CCM_UART_GATING_OFFSET + port));
	hal_writel(reg, CCMU_UART_BGR_REG);
}

#ifdef CONFIG_R528_UART0
static inline void r528_uart0config(void)
{
  irqstate_t flags;

  flags = enter_critical_section();
  sunxi_clock_init_uart(0);

#if defined(CONFIG_ARCH_BOARD_R528S3_EVB4)
	#define UART0_TX		GPIOB(0)
	#define UART0_RX		GPIOB(1)
	#define UART0_GPIO_FUNCTION	(6)
#elif defined(CONFIG_ARCH_BOARD_R528S3_GEMINI_S1)
	#define UART0_TX		GPIOF(2)
	#define UART0_RX		GPIOF(4)
	#define UART0_GPIO_FUNCTION	(3)
#else
	#define UART0_TX		GPIOB(0)
	#define UART0_RX		GPIOB(1)
	#define UART0_GPIO_FUNCTION	(6)
#endif

  hal_gpio_pinmux_set_function_early(R528_GPIO_VADDR, UART0_TX, UART0_GPIO_FUNCTION);
  hal_gpio_pinmux_set_function_early(R528_GPIO_VADDR, UART0_RX, UART0_GPIO_FUNCTION);

  leave_critical_section(flags);
};
#endif

#ifdef CONFIG_R528_UART1
static inline void r528_uart1config(void)
{
  irqstate_t flags;

  flags = enter_critical_section();
  sunxi_clock_init_uart(1);
#define UART1_TX		GPIOG(6)
#define UART1_RX		GPIOG(7)
#define UART1_RTS		GPIOG(8)
#define UART1_CTS		GPIOG(9)
#define UART1_GPIO_FUNCTION	(2)
  hal_gpio_pinmux_set_function_early(R528_GPIO_VADDR, UART1_TX, UART1_GPIO_FUNCTION);
  hal_gpio_pinmux_set_function_early(R528_GPIO_VADDR, UART1_RX, UART1_GPIO_FUNCTION);
  hal_gpio_pinmux_set_function_early(R528_GPIO_VADDR, UART1_RTS, UART1_GPIO_FUNCTION);
  hal_gpio_pinmux_set_function_early(R528_GPIO_VADDR, UART1_CTS, UART1_GPIO_FUNCTION);
  leave_critical_section(flags);
};
#endif

#ifdef CONFIG_R528_UART2
void r528_uart2config(void)
{
  irqstate_t flags;

  flags = enter_critical_section();
  sunxi_clock_init_uart(2);
#define UART2_TX		GPIOC(0)
#define UART2_RX		GPIOC(1)
#define UART2_GPIO_FUNCTION	(2)
  hal_gpio_pinmux_set_function_early(R528_GPIO_VADDR, UART2_TX, UART2_GPIO_FUNCTION);
  hal_gpio_pinmux_set_function_early(R528_GPIO_VADDR, UART2_RX, UART2_GPIO_FUNCTION);
  leave_critical_section(flags);
};
#endif

#ifdef CONFIG_R528_UART3
static inline void r528_uart3config(void)
{
  irqstate_t flags;

  flags = enter_critical_section();
  sunxi_clock_init_uart(3);
#define UART3_TX		GPIOD(10)
#define UART3_RX		GPIOD(11)
#define UART3_GPIO_FUNCTION	(5)
  hal_gpio_pinmux_set_function_early(R528_GPIO_VADDR, UART3_TX, UART3_GPIO_FUNCTION);
  hal_gpio_pinmux_set_function_early(R528_GPIO_VADDR, UART3_RX, UART3_GPIO_FUNCTION);
  leave_critical_section(flags);
};
#endif

/************************************************************************************
 * Name: r528_uartdl
 *
 * Description:
 *   Select a divider to produce the BAUD from the UART PCLK.
 *
 *     BAUD = PCLK / (16 * DL), or
 *     DL   = PCLK / BAUD / 16
 *
 ************************************************************************************/

static inline uint32_t r528_uartdl(uint32_t baud)
{
  return R528_SCLK / (baud << 4);
}

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: set_format_and_baudrate
 *
 * Description:
 *   Configure the UART baud, bits, parity, etc. This method is
 *   called the first time that the serial port is opened.
 *
 ****************************************************************************/
static void set_format_and_baudrate(struct uart_dev_s *dev)
{
#ifndef CONFIG_SUPPRESS_UART_CONFIG
  struct up_dev_s *priv = (struct up_dev_s *)dev->priv;
  uint16_t dl;
  uint32_t lcr;
  /* Set up the LCR */

  lcr = 0;

  switch (priv->bits)
    {
    case 5:
      lcr |= UART_LCR_DLS_5BITS;
      break;

    case 6:
      lcr |= UART_LCR_DLS_6BITS;
      break;

    case 7:
      lcr |= UART_LCR_DLS_7BITS;
      break;

    case 8:
    default:
      lcr |= UART_LCR_DLS_8BITS;
      break;
    }

  if (priv->stopbits2)
    {
      lcr |= UART_LCR_STOP;
    }

  if (priv->parity == 1)
    {
      lcr |= UART_LCR_PEN;
      lcr &= ~UART_LCR_EPS;
    }
  else if (priv->parity == 2)
    {
      lcr |= (UART_LCR_PEN | UART_LCR_EPS);
    }
  else if (priv->parity == 0)
    {
      lcr &= ~UART_LCR_PEN;
    }


  dl = r528_uartdl(priv->baud);

  arm_serialout(priv, R528_UART_HALT_OFFSET, UART_HALT_HALT_TX | UART_HALT_FORCECFG);
  /* Enter DLAB=1 */

  arm_serialout(priv, R528_UART_LCR_OFFSET, (lcr | UART_LCR_DLAB));

  /* Set the BAUD divisor */
  arm_serialout(priv, R528_UART_DLH_OFFSET, dl >> 8);
  arm_serialout(priv, R528_UART_DLL_OFFSET, dl & 0xff);

  arm_serialout(priv, R528_UART_HALT_OFFSET, UART_HALT_HALT_TX | UART_HALT_FORCECFG | UART_HALT_LCRUP);
  /* FIXME: implement timeout */
  while (arm_serialin(priv, R528_UART_HALT_OFFSET) & UART_HALT_LCRUP)
	      ;

  /* Clear DLAB */

  arm_serialout(priv, R528_UART_LCR_OFFSET, lcr);

  arm_serialout(priv, R528_UART_HALT_OFFSET, UART_HALT_FORCECFG);

#endif
}

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: set_flowctrl
 *
 * Description:
 *   Configure the UART flowctrl. This method is
 *   called the first time that the serial port is opened.
 *
 ****************************************************************************/
static void set_flowctrl(struct uart_dev_s *dev){
#ifndef CONFIG_SUPPRESS_UART_CONFIG
  struct up_dev_s *priv = (struct up_dev_s *)dev->priv;
  /* Enable Auto-Flow Control in the Modem Control Register */
  if ((priv->port == 1 || priv->port == 2 || priv->port == 3) && priv->hwflowctrl) {
	uint32_t value = arm_serialin(priv, R528_UART_MCR_OFFSET);
	value |= UART_MCR_DTR | UART_MCR_RTS | UART_MCR_AFCE;
	arm_serialout(priv, R528_UART_MCR_OFFSET, value);

	/* enable with modem status interrupts */
	value = arm_serialin(priv, R528_UART_IER_OFFSET);
	value |= UART_IER_EDSSI;
	priv->ier = value;
	arm_serialout(priv, R528_UART_IER_OFFSET, value);
  }else if((priv->port == 1 || priv->port == 2 || priv->port == 3) && (priv->hwflowctrl==0)){
	/* disable with modem status interrupts */
	uint32_t value = arm_serialin(priv, R528_UART_IER_OFFSET);
	value &= (~UART_IER_EDSSI);
	priv->ier = value;
	arm_serialout(priv, R528_UART_IER_OFFSET, value);
	/* disable Auto-Flow Control in the Modem Control Register */
	value = arm_serialin(priv, R528_UART_MCR_OFFSET);
	value &= ((~UART_MCR_DTR)&(~UART_MCR_RTS)&(~UART_MCR_AFCE));
	arm_serialout(priv, R528_UART_MCR_OFFSET, value);
  }
#endif
}

/****************************************************************************
 * Name: up_setup
 *
 * Description:
 *   Configure the UART baud, bits, parity, fifos, etc. This method is
 *   called the first time that the serial port is opened.
 *
 ****************************************************************************/
static int up_setup(struct uart_dev_s *dev)
{
#ifndef CONFIG_SUPPRESS_UART_CONFIG
  struct up_dev_s *priv = (struct up_dev_s *)dev->priv;
  irqstate_t flags;

  flags = enter_critical_section();
  up_disable_irq(priv->irq);
  /* Clear fifos */

  arm_serialout(priv, R528_UART_FCR_OFFSET, (UART_FCR_RFIFOR | UART_FCR_XFIFOR));

  /* Set trigger */

  arm_serialout(priv, R528_UART_FCR_OFFSET, (UART_FCR_FIFOE | UART_FCR_RT_HALF));

  /* Set up the IER */

  priv->ier = arm_serialin(priv, R528_UART_IER_OFFSET);

  set_format_and_baudrate(dev);

  up_enable_irq(priv->irq);

  /* Configure the FIFOs */

  arm_serialout(priv, R528_UART_FCR_OFFSET,
               (UART_FCR_RT_HALF | UART_FCR_XFIFOR | UART_FCR_RFIFOR |
                UART_FCR_FIFOE));
  set_flowctrl(dev);
  leave_critical_section(flags);
#endif
  return OK;
}

/****************************************************************************
 * Name: up_shutdown
 *
 * Description:
 *   Disable the UART.  This method is called when the serial port is closed
 *
 ****************************************************************************/

static void up_shutdown(struct uart_dev_s *dev)
{
  irqstate_t flags;
  flags = enter_critical_section();
  struct up_dev_s *priv = (struct up_dev_s *)dev->priv;
  up_disableuartint(priv, NULL);
  leave_critical_section(flags);
}

/****************************************************************************
 * Name: up_attach
 *
 * Description:
 *   Configure the UART to operation in interrupt driven mode.  This method is
 *   called when the serial port is opened.  Normally, this is just after the
 *   the setup() method is called, however, the serial console may operate in
 *   a non-interrupt driven mode during the boot phase.
 *
 *   RX and TX interrupts are not enabled when by the attach method (unless the
 *   hardware supports multiple levels of interrupt enabling).  The RX and TX
 *   interrupts are not enabled until the txint() and rxint() methods are called.
 *
 ****************************************************************************/
static int up_attach(struct uart_dev_s *dev)
{
  struct up_dev_s *priv = (struct up_dev_s *)dev->priv;
  int ret;

  /* Attach and enable the IRQ */
  sinfo("--123------------> irq=%d.\n", priv->irq);

  ret = irq_attach(priv->irq, uart_interrupt, dev);
  if (ret == OK)
    {
      /* Enable the interrupt (RX and TX interrupts are still disabled
       * in the UART
       */
     sinfo("enable irq.\n");
      up_enable_irq(priv->irq);
    }

  return ret;
}

/****************************************************************************
 * Name: up_detach
 *
 * Description:
 *   Detach UART interrupts.  This method is called when the serial port is
 *   closed normally just before the shutdown method is called.  The exception is
 *   the serial console which is never shutdown.
 *
 ****************************************************************************/

static void up_detach(struct uart_dev_s *dev)
{
  struct up_dev_s *priv = (struct up_dev_s *)dev->priv;
  up_disable_irq(priv->irq);
  irq_detach(priv->irq);
}

/****************************************************************************
 * Name: uartN_interrupt and uart_interrupt
 *
 * Description:
 *   This is the UART interrupt handler.  It will be invoked when an
 *   interrupt received on the 'irq'  It should call uart_transmitchars or
 *   uart_receivechar to perform the appropriate data transfers.  The
 *   interrupt handling logic must be able to map the 'irq' number into the
 *   appropriate uart_dev_s structure in order to call these functions.
 *
 ****************************************************************************/

static int uart_interrupt(int irq, void *context, void *arg)
{
  struct uart_dev_s *dev = (struct uart_dev_s *)arg;
  struct up_dev_s *priv;
  uint32_t status;
  int passes;

  DEBUGASSERT(dev != NULL && dev->priv != NULL);
  priv = (struct up_dev_s *)dev->priv;

  /* Loop until there are no characters to be transferred or,
   * until we have been looping for a long time.
   */

  for (passes = 0; passes < 256; passes++)
    {
      /* Get the current UART status */

      status = arm_serialin(priv, R528_UART_IIR_OFFSET);

      /* Handle the interrupt by its interrupt ID field */

      switch (status & UART_IIR_IID_MASK)
        {
          /* Handle incoming, receive bytes (with or without timeout) */

          case UART_IIR_IID_RECV:
          case UART_IIR_IID_TIMEOUT:
            {
              uart_recvchars(dev);
              break;
            }

          /* Handle outgoing, transmit bytes */

          case UART_IIR_IID_TXEMPTY:
            {
              uart_xmitchars(dev);
              break;
            }

          /* Just clear modem status interrupts (UART1 only) */

          case UART_IIR_IID_MODEM:
            {
              /* Read the modem status register (MSR) to clear */

              status = arm_serialin(priv, R528_UART_MSR_OFFSET);
              _info("MSR: %02lx\n", status);
              break;
            }

          /* Just clear any line status interrupts */

          case UART_IIR_IID_LINESTATUS:
            {
              /* Read the line status register (LSR) to clear */

              status = arm_serialin(priv, R528_UART_LSR_OFFSET);
              _info("LSR: %02lx\n", status);
              break;
            }

          /* Busy detect.  Just ignore.  Cleared by reading the status register */

          case UART_IIR_IID_BUSY:
            {
              /* Read from the UART status register to clear the BUSY condition */

              status = arm_serialin(priv, R528_UART_USR_OFFSET);
              break;
            }

          /* No further interrupts pending... return now */

          case UART_IIR_IID_NONE:
            {
              return OK;
            }

            /* Otherwise we have received an interrupt that we cannot handle */

          default:
            {
              _err("ERROR: Unexpected IIR: %02lx\n", status);
              break;
            }
        }
    }

  return OK;
}

/****************************************************************************
 * Name: up_ioctl
 *
 * Description:
 *   All ioctl calls will be routed through this method
 *
 ****************************************************************************/

static int up_ioctl(struct file *filep, int cmd, unsigned long arg)
{
  struct inode      *inode = filep->f_inode;
  struct uart_dev_s *dev   = inode->i_private;
  struct up_dev_s   *priv  = (struct up_dev_s *)dev->priv;
  int                ret    = OK;

  switch (cmd)
    {
#ifdef CONFIG_SERIAL_TIOCSERGSTRUCT
    case TIOCSERGSTRUCT:
      {
        struct up_dev_s *user = (struct up_dev_s *)arg;
        if (!user)
          {
            ret = -EINVAL;
          }
        else
          {
            memcpy(user, dev, sizeof(struct up_dev_s));
          }
      }
      break;
#endif

    case TIOCSBRK:  /* BSD compatibility: Turn break on, unconditionally */
      {
        irqstate_t flags = enter_critical_section();
        up_enablebreaks(priv, true);
        leave_critical_section(flags);
      }
      break;

    case TIOCCBRK:  /* BSD compatibility: Turn break off, unconditionally */
      {
        irqstate_t flags;
        flags = enter_critical_section();
        up_enablebreaks(priv, false);
        leave_critical_section(flags);
      }
      break;

#ifdef CONFIG_SERIAL_TERMIOS
    case TCGETS:
      {
        struct termios *termiosp = (struct termios *)arg;
        irqstate_t flags;

        if (!termiosp)
          {
            ret = -EINVAL;
            break;
          }

        flags = enter_critical_section();
        /* TODO:  Other termios fields are not yet returned.
         * Note that cfsetospeed is not necessary because we have
         * knowledge that only one speed is supported.
         * Both cfset(i|o)speed() translate to cfsetspeed.
         */

        cfsetispeed(termiosp, priv->baud);
        termiosp->c_cflag = ((priv->parity != 0) ? PARENB : 0) |
                            ((priv->parity == 1) ? PARODD : 0);
        termiosp->c_cflag |= (priv->stopbits2) ? CSTOPB : 0;
#if defined(CONFIG_SERIAL_IFLOWCONTROL) || defined(CONFIG_SERIAL_OFLOWCONTROL)
        termiosp->c_cflag |= priv->hwflowctrl ? CRTSCTS : 0;
#endif

        switch (priv->bits)
          {
          case 5:
            termiosp->c_cflag |= CS5;
            break;

          case 6:
            termiosp->c_cflag |= CS6;
            break;

          case 7:
            termiosp->c_cflag |= CS7;
            break;

          case 8:
          default:
            termiosp->c_cflag |= CS8;
            break;
          }

        leave_critical_section(flags);
      }
      break;

    case TCSETS:
      {
        struct termios *termiosp = (struct termios *)arg;
        irqstate_t flags;
        //uint32_t           lcr;  /* Holds current values of line control register */
        //uint16_t           dl;   /* Divisor latch */

        if (!termiosp)
          {
            ret = -EINVAL;
            break;
          }

        flags = enter_critical_section();

        /* TODO:  Handle other termios settings.
         * Note that only cfgetispeed is used because we have knowledge
         * that only one speed is supported.
         */
        switch (termiosp->c_cflag & CSIZE)
          {
          case CS5:
            priv->bits = 5;
            break;

          case CS6:
            priv->bits = 6;
            break;

          case CS7:
            priv->bits = 7;
            break;

          case CS8:
          default:
            priv->bits = 8;
            break;
          }

        if ((termiosp->c_cflag & PARENB) != 0)
          {
            priv->parity = (termiosp->c_cflag & PARODD) ? 1 : 2;
          }
        else
          {
            priv->parity = 0;
          }

        /* Get the c_speed field in the termios struct */

        priv->baud = cfgetispeed(termiosp);
        priv->stopbits2 = (termiosp->c_cflag & CSTOPB) != 0;
#if defined(CONFIG_SERIAL_IFLOWCONTROL) || defined(CONFIG_SERIAL_OFLOWCONTROL)
        priv->hwflowctrl = (termiosp->c_cflag & CRTSCTS) != 0;
#endif
		set_format_and_baudrate(dev);
		set_flowctrl(dev);
        leave_critical_section(flags);
      }
      break;
#endif

    default:
      ret = -ENOTTY;
      break;
    }

  return ret;
}

/****************************************************************************
 * Name: up_receive
 *
 * Description:
 *   Called (usually) from the interrupt level to receive one
 *   character from the UART.  Error bits associated with the
 *   receipt are provided in the return 'status'.
 *
 ****************************************************************************/

static int up_receive(struct uart_dev_s *dev, unsigned int *status)
{
  struct up_dev_s *priv = (struct up_dev_s *)dev->priv;
  uint32_t rbr;

  *status = arm_serialin(priv, R528_UART_LSR_OFFSET);
  rbr     = arm_serialin(priv, R528_UART_RBR_OFFSET);
  return rbr;
}

/****************************************************************************
 * Name: up_rxint
 *
 * Description:
 *   Call to enable or disable RX interrupts
 *
 ****************************************************************************/

static void up_rxint(struct uart_dev_s *dev, bool enable)
{
  irqstate_t flags;
  struct up_dev_s *priv = (struct up_dev_s *)dev->priv;
  flags = enter_critical_section();
  if (enable)
    {
#ifndef CONFIG_SUPPRESS_SERIAL_INTS
      priv->ier |= UART_IER_ERBFI;
#endif
    }
  else
    {
      priv->ier &= ~UART_IER_ERBFI;
    }

  arm_serialout(priv, R528_UART_IER_OFFSET, priv->ier);
  leave_critical_section(flags);
}

/****************************************************************************
 * Name: up_rxavailable
 *
 * Description:
 *   Return true if the receive fifo is not empty
 *
 ****************************************************************************/

static bool up_rxavailable(struct uart_dev_s *dev)
{
  struct up_dev_s *priv = (struct up_dev_s *)dev->priv;
  return ((arm_serialin(priv, R528_UART_LSR_OFFSET) & UART_LSR_DR) != 0);
}

/****************************************************************************
 * Name: up_send
 *
 * Description:
 *   This method will send one byte on the UART
 *
 ****************************************************************************/

static void up_send(struct uart_dev_s *dev, int ch)
{
  struct up_dev_s *priv = (struct up_dev_s *)dev->priv;
  while((arm_serialin(priv, R528_UART_USR_OFFSET) & UART_USR_TFNF) == 0);
  arm_serialout(priv, R528_UART_THR_OFFSET, (uint32_t)ch);
}

/****************************************************************************
 * Name: up_txint
 *
 * Description:
 *   Call to enable or disable TX interrupts
 *
 ****************************************************************************/

static void up_txint(struct uart_dev_s *dev, bool enable)
{
  struct up_dev_s *priv = (struct up_dev_s *)dev->priv;
  irqstate_t flags;

  flags = enter_critical_section();
  if (enable)
    {
      priv->ier |= UART_IER_ETBEI;
      arm_serialout(priv, R528_UART_IER_OFFSET, priv->ier);

      /* Fake a TX interrupt here by just calling uart_xmitchars() with
       * interrupts disabled (note this may recurse).
       */

      uart_xmitchars(dev);
    }
  else
    {
      priv->ier &= ~UART_IER_ETBEI;
      arm_serialout(priv, R528_UART_IER_OFFSET, priv->ier);
    }

  leave_critical_section(flags);
}

/****************************************************************************
 * Name: up_txready
 *
 * Description:
 *   Return true if the tranmsit fifo is not full
 *
 ****************************************************************************/

static bool up_txready(struct uart_dev_s *dev)
{
  struct up_dev_s *priv = (struct up_dev_s *)dev->priv;
  return ((arm_serialin(priv, R528_UART_USR_OFFSET) & UART_USR_TFNF) != 0);
}

/****************************************************************************
 * Name: up_txempty
 *
 * Description:
 *   Return true if the transmit fifo is empty
 *
 ****************************************************************************/

static bool up_txempty(struct uart_dev_s *dev)
{
  struct up_dev_s *priv = (struct up_dev_s *)dev->priv;
  return ((arm_serialin(priv, R528_UART_LSR_OFFSET) & UART_LSR_THRE) != 0);
}

/****************************************************************************
 * Name: up_rxflowcontrol
 *
 * Description:
 *   Called when Rx buffer is full (or exceeds configured watermark levels
 *   if CONFIG_SERIAL_IFLOWCONTROL_WATERMARKS is defined).
 *   Return true if UART activated RX flow control to block more incoming
 ****************************************************************************/

#ifdef CONFIG_SERIAL_IFLOWCONTROL
static bool up_rxflowcontrol(struct uart_dev_s *dev,
                             unsigned int nbuffered, bool upper)
{
	struct up_dev_s *priv = (struct up_dev_s *)dev->priv;
	if(priv->port == 1 || priv->port == 2 || priv->port == 3)
		/* rts control by hw */
		return true;
	else
		return false;
}
#endif

/****************************************************************************
 * Public Funtions
 ****************************************************************************/

/****************************************************************************
 * Name: arm_serialinit
 *
 * Description:
 *   Performs the low level UART initialization early in debug so that the
 *   serial console will be available during bootup.  This must be called
 *   before arm_serialinit.
 *
 *   NOTE: Configuration of the CONSOLE UART was performed by up_lowsetup()
 *   very early in the boot sequence.
 *
 ****************************************************************************/

void up_earlyserialinit(void)
{
  /* Configure all UARTs (except the CONSOLE UART) and disable interrupts */
#if defined(CONFIG_R528_UART0)
  r528_uart0config();
#endif

#if defined(CONFIG_R528_UART1)
  r528_uart1config();
#endif

#if defined(CONFIG_R528_UART2)
  r528_uart2config();
#endif

#if defined(CONFIG_R528_UART3)
  r528_uart3config();
#endif

#ifdef CONSOLE_DEV
  CONSOLE_DEV.isconsole = true;
  up_setup(&CONSOLE_DEV);
#endif
}

/****************************************************************************
 * Name: arm_serialinit
 *
 * Description:
 *   Register serial console and serial ports.  This assumes that
 *   up_earlyserialinit was called previously.
 *
 ****************************************************************************/

void arm_serialinit(void)
{
  up_earlyserialinit();

#ifdef CONSOLE_DEV
  (void)uart_register("/dev/console", &CONSOLE_DEV);
#endif

#if defined(CONFIG_R528_UART0)
  (void)uart_register("/dev/uart0", &g_uart0port);
#endif
#if defined(CONFIG_R528_UART1)
  (void)uart_register("/dev/uart1", &g_uart1port);
#endif
#if defined(CONFIG_R528_UART2)
  (void)uart_register("/dev/uart2", &g_uart2port);
#endif
#if defined(CONFIG_R528_UART3)
  (void)uart_register("/dev/uart3", &g_uart3port);
#endif
}

/****************************************************************************
 * Name: up_putc
 *
 * Description:
 *   Provide priority, low-level access to support OS debug  writes
 *
 ****************************************************************************/

void up_putc(int ch)
{
#ifdef HAVE_SERIAL_CONSOLE
  irqstate_t flags;
  flags = enter_critical_section();
  struct up_dev_s *priv = (struct up_dev_s *)CONSOLE_DEV.priv;
  uint32_t ier;
  up_disableuartint(priv, &ier);
#endif

  /* Check for LF */

  if (ch == '\n')
    {
      /* Add CR */

      arm_lowputc('\r');
    }

  arm_lowputc(ch);
#ifdef HAVE_SERIAL_CONSOLE
  up_restoreuartint(priv, ier);
  leave_critical_section(flags);
#endif

}
