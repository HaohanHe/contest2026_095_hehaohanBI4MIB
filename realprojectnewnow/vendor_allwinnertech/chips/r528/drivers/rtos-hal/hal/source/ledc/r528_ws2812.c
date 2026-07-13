#include "r528_ws2812.h"
#include "platform/ledc_sun8iw20.h"
#include <fcntl.h>
#include <hal_cache.h>
#include <hal_clk.h>
#include <hal_dma.h>
#include <hal_gpio.h>
#include <nuttx/kmalloc.h>
#include <nuttx/leds/ws2812.h>
#include <nuttx/signal.h>
#include <stdlib.h>
#include <string.h>

#ifdef CONFIG_DRIVERS_LEDC

#define LEDC_PIN GPIOG(16)
#define LEDC_PINMUXSEL 7

static unsigned int base_addr = LEDC_BASE;

struct sunxi_dma_chan *dma_chan = NULL;

static void ledc_set_reset_ns(unsigned int reset_ns) {
  unsigned int n, reg_val;
  unsigned int mask = 0x1FFF;
  unsigned int min = LEDC_RESET_TIME_MIN_NS;
  unsigned int max = LEDC_RESET_TIME_MAX_NS;

  if (reset_ns < min || reset_ns > max) {
    lederr("ledc_set_reset_ns: nvalid parameter, reset_ns should be %u-%u!\n",
         min, max);
    return;
  }

  n = (reset_ns - 42) / 42;
  reg_val = hal_readl(base_addr + LED_RST_TIMING_CTRL_REG);
  reg_val &= ~(mask << 16);
  reg_val |= (n << 16);
  hal_writel(reg_val, base_addr + LED_RST_TIMING_CTRL_REG);
}

static void ledc_set_t1h_ns(unsigned int t1h_ns) {
  unsigned int n, reg_val;
  unsigned int mask = 0x3F;
  unsigned int shift = 21;
  unsigned int min = LEDC_T1H_MIN_NS;
  unsigned int max = LEDC_T1H_MAX_NS;

  if (t1h_ns < min || t1h_ns > max) {
    lederr("ledc_set_t1h_ns: nvalid parameter, t1h_ns should be %u-%u!\n", min,
         max);
    return;
  }

  n = (t1h_ns - 42) / 42;
  reg_val = hal_readl(base_addr + LED_T01_TIMING_CTRL_REG);
  reg_val &= ~(mask << shift);
  reg_val |= n << shift;
  hal_writel(reg_val, base_addr + LED_T01_TIMING_CTRL_REG);
}

static void ledc_set_t1l_ns(unsigned int t1l_ns) {
  unsigned int n, reg_val;
  unsigned int mask = 0x1F;
  unsigned int shift = 16;
  unsigned int min = LEDC_T1L_MIN_NS;
  unsigned int max = LEDC_T1L_MAX_NS;

  if (t1l_ns < min || t1l_ns > max) {
    lederr("ledc_set_t1l_ns: nvalid parameter, t1l_ns should be %u-%u!\n", min,
         max);
    return;
  }

  n = (t1l_ns - 42) / 42;
  reg_val = hal_readl(base_addr + LED_T01_TIMING_CTRL_REG);
  reg_val &= ~(mask << shift);
  reg_val |= n << shift;
  hal_writel(reg_val, base_addr + LED_T01_TIMING_CTRL_REG);
}

static void ledc_set_t0h_ns(unsigned int t0h_ns) {
  unsigned int n, reg_val;
  unsigned int mask = 0x1F;
  unsigned int shift = 6;
  unsigned int min = LEDC_T0H_MIN_NS;
  unsigned int max = LEDC_T0H_MAX_NS;

  if (t0h_ns < min || t0h_ns > max) {
    lederr("ledc_set_t0h_ns: nvalid parameter, t0h_ns should be %u-%u!\n", min,
         max);
    return;
  }

  n = (t0h_ns - 42) / 42;
  reg_val = hal_readl(base_addr + LED_T01_TIMING_CTRL_REG);
  reg_val &= ~(mask << shift);
  reg_val |= n << shift;
  hal_writel(reg_val, base_addr + LED_T01_TIMING_CTRL_REG);
}

static void ledc_set_t0l_ns(unsigned int t0l_ns) {
  unsigned int n, reg_val;
  unsigned int min = LEDC_T0L_MIN_NS;
  unsigned int max = LEDC_T0L_MAX_NS;

  if (t0l_ns < min || t0l_ns > max) {
    lederr("ledc_set_t0l_ns: nvalid parameter, t0l_ns should be %u-%u!\n", min,
         max);
    return;
  }

  n = (t0l_ns - 42) / 42;
  reg_val = hal_readl(base_addr + LED_T01_TIMING_CTRL_REG);
  reg_val &= ~0x3F;
  reg_val |= n;
  hal_writel(reg_val, base_addr + LED_T01_TIMING_CTRL_REG);
}

static void ledc_set_wait_time0_ns(unsigned int wait_time0_ns) {
  unsigned int n, reg_val;
  unsigned int min = LEDC_WAIT_TIME0_MIN_NS;
  unsigned int max = LEDC_WAIT_TIME0_MAX_NS;

  if (wait_time0_ns < min || wait_time0_ns > max) {
    lederr("ledc_set_wait_time0_ns: nvalid parameter, wait_time0_ns should be "
         "%u-%u!\n",
         min, max);
    return;
  }

  n = (wait_time0_ns - 42) / 42;
  reg_val = (1 << 8) | n;
  hal_writel(reg_val, base_addr + LEDC_WAIT_TIME0_CTRL_REG);
}

static void ledc_set_wait_time1_ns(unsigned long long wait_time1_ns) {
  unsigned long long max = LEDC_WAIT_TIME1_MAX_NS;
  unsigned int min = LEDC_WAIT_TIME1_MIN_NS;
  unsigned int n, reg_val;

  if (wait_time1_ns < min || wait_time1_ns > max) {
    lederr("ledc_set_wait_time1_ns: nvalid parameter, wait_time1_ns should be "
         "%u-%llu!\n",
         min, max);
    return;
  }

  n = wait_time1_ns / 42;
  // tmp = wait_time1_ns;
  // n = div_u64(tmp, 42);
  n -= 1;
  reg_val = (1 << 31) | n;
  hal_writel(reg_val, base_addr + LEDC_WAIT_TIME1_CTRL_REG);
}

static void ledc_set_wait_data_time_ns(unsigned int wait_data_time_ns) {
  unsigned int mask = 0x1FFF;
  unsigned int shift = 16;
  unsigned int reg_val = 0;
  unsigned int n, min, max;

  min = LEDC_WAIT_DATA_TIME_MIN_NS;
  max = LEDC_WAIT_DATA_TIME_MAX_NS_IC;

  if (wait_data_time_ns < min || wait_data_time_ns > max) {
    lederr("ledc_set_wait_data_time_ns: nvalid parameter, wait_data_time_ns "
         "should be %u-%u!\n",
         min, max);
    return;
  }

  n = (wait_data_time_ns - 42) / 42;
  reg_val &= ~(mask << shift);
  reg_val |= (n << shift);
  hal_writel(reg_val, base_addr + LEDC_DATA_FINISH_CNT_REG);
}

static void ledc_set_length(unsigned int length) {
  unsigned int reg_val;

  if (length == 0)
    return;

  reg_val = hal_readl(base_addr + LEDC_CTRL_REG);
  reg_val &= ~(0x1FFF << 16);
  reg_val |= length << 16;
  hal_writel(reg_val, base_addr + LEDC_CTRL_REG);

  reg_val = hal_readl(base_addr + LED_RST_TIMING_CTRL_REG);
  reg_val &= ~0x3FF;
  reg_val |= length - 1;
  hal_writel(reg_val, base_addr + LED_RST_TIMING_CTRL_REG);
}

static void ledc_set_output_mode(const char *str) {
  unsigned int val = 0;
  unsigned int mask = 0x7;
  unsigned int shift = 6;
  unsigned int reg_val;
  if (str != NULL) {
    if (!strncmp(str, "GRB", 3))
      val = LEDC_OUTPUT_GRB;
    else if (!strncmp(str, "GBR", 3))
      val = LEDC_OUTPUT_GBR;
    else if (!strncmp(str, "RGB", 3))
      val = LEDC_OUTPUT_RGB;
    else if (!strncmp(str, "RBG", 3))
      val = LEDC_OUTPUT_RBG;
    else if (!strncmp(str, "BGR", 3))
      val = LEDC_OUTPUT_BGR;
    else if (!strncmp(str, "BRG", 3))
      val = LEDC_OUTPUT_BRG;
    else
      return;
  } else {
  }

  reg_val = hal_readl(base_addr + LEDC_CTRL_REG);
  reg_val &= ~(mask << shift);
  reg_val |= val;
  hal_writel(reg_val, base_addr + LEDC_CTRL_REG);
}

#if 0
static void ledc_disable_irq(unsigned int mask)
{
    unsigned int reg_val = 0;

    reg_val = hal_readl(base_addr + LEDC_INTC_REG);
    reg_val &= ~mask;
    hal_writel(reg_val, base_addr + LEDC_INTC_REG);
}
#endif

static void ledc_enable_irq(unsigned int mask) {
  unsigned int reg_val = 0;

  reg_val = hal_readl(base_addr + LEDC_INTC_REG);
  reg_val |= mask;
  hal_writel(reg_val, base_addr + LEDC_INTC_REG);
}

unused_code static void ledc_set_dma_mode(void) {
  unsigned int reg_val = 0;

  reg_val |= 1 << 5;
  hal_writel(reg_val, base_addr + LEDC_DMA_CTRL_REG);
}

static void ledc_set_cpu_mode(void) {
  unsigned int reg_val = 0;

  reg_val &= ~(1 << 5);
  hal_writel(reg_val, base_addr + LEDC_DMA_CTRL_REG);
}

#if 0
static void ledc_clear_all_irq(void)
{
    unsigned int reg_val;

    reg_val = hal_readl(base_addr + LEDC_INTS_REG);
    reg_val |= 0x1F;
    hal_writel(reg_val, base_addr + LEDC_INTS_REG);
}

static unsigned int ledc_get_irq_status(void)
{
    return hal_readl(base_addr + LEDC_INTS_REG);
}

static void ledc_soft_reset(void)
{
    unsigned int reg_val;

    reg_val = hal_readl(base_addr + LEDC_CTRL_REG);
    reg_val |= 1 << 1;
    hal_writel(reg_val, base_addr + LEDC_CTRL_REG);
}
#endif

unused_code static void ledc_reset_en(void) {
  unsigned int reg_val;

  reg_val = hal_readl(base_addr + LEDC_CTRL_REG);
  reg_val |= 1 << 10;
  hal_writel(reg_val, base_addr + LEDC_CTRL_REG);
}

unused_code static void ledc_set_byte(const int data, const char *addr) {
  hal_writeb(data, addr);
}

static void ledc_set_data(const uint32_t data) {
  unsigned char *addr = (unsigned char *)base_addr + LEDC_DATA_REG;
  hal_writel(data, addr);
}

static void ledc_enable(void) {
  unsigned int reg_val;

  reg_val = hal_readl(base_addr + LEDC_CTRL_REG);
  reg_val |= 1;
  hal_writel(reg_val, base_addr + LEDC_CTRL_REG);
}

unused_code static void ledc_set_time(struct ledc_config *ledc) {
  ledc_set_reset_ns(ledc->reset_ns);
  ledc_set_t1h_ns(ledc->t1h_ns);
  ledc_set_t1l_ns(ledc->t1l_ns);
  ledc_set_t0h_ns(ledc->t0h_ns);
  ledc_set_t0l_ns(ledc->t0l_ns);
  ledc_set_wait_time0_ns(ledc->wait_time0_ns);
  ledc_set_wait_time1_ns(ledc->wait_time1_ns);
  ledc_set_wait_data_time_ns(ledc->wait_data_time_ns);
}

void ledc_dma_callback(void *para) {
  ledinfo("ledc_dma_callback: do callback!\n");
}

#if 0
void hal_ledc_clear_all_irq(void)
{
    ledc_clear_all_irq();
}

unsigned int hal_ledc_get_irq_status(void)
{
    return ledc_get_irq_status();
}

void hal_ledc_reset(void)
{
    ledc_disable_irq(LEDC_TRANS_FINISH_INT_EN | LEDC_WAITDATA_TIMEOUT_INT_EN
    | LEDC_FIFO_OVERFLOW_INT_EN | LEDC_GLOBAL_INT_EN | LEDC_GLOBAL_INT_EN);

    if (dma_chan)
    {
        hal_dma_stop(dma_chan);
    }
    ledc_soft_reset();
}
#endif

void sunxi_ledc_clk_init(void) {

  struct sunxi_ccmu_reg *clk = (struct sunxi_ccmu_reg *)(LEDC_CLOCK_ADDR);

  clk->model_clk = (1 << 31) | (0 << 24) | (0 << 8) | (0);
  clk->bus_gate |= (0 << 16);
  clk->bus_gate |= (1 << 16);
  clk->bus_gate |= (1);
}

void ws2812_init(struct ledc_config *priv) {
  irqstate_t flags;
  flags = enter_critical_section();
  sunxi_ledc_clk_init();
  // gpio init
  hal_gpio_pinmux_set_function(LEDC_PIN, LEDC_PINMUXSEL);
  // 初始化全局结构体
  memset(priv, 0, sizeof(struct ledc_config));
  // 设置默认配置
  priv->led_count = 1;
  priv->reset_ns = 84;
  priv->t1h_ns = 1000;
  priv->t1l_ns = 1000;
  priv->t0h_ns = 580;
  priv->t0l_ns = 1000;
  priv->wait_time0_ns = 84;
  priv->wait_time1_ns = 84;
  priv->wait_data_time_ns = 600000;
  priv->output_mode = "GRB";
  leave_critical_section(flags);
}

static void ws2812_write(char *out_mode, uint32_t data) {
  irqstate_t flags;
  flags = enter_critical_section();
  ledc_set_output_mode(out_mode);
  ledc_set_cpu_mode();
  ledc_set_length(1);
  ledc_enable_irq(LEDC_TRANS_FINISH_INT_EN | LEDC_WAITDATA_TIMEOUT_INT_EN |
                  LEDC_FIFO_OVERFLOW_INT_EN | LEDC_GLOBAL_INT_EN);
  ledc_enable();
  ledc_set_data(data);
  leave_critical_section(flags);
}

static int r528_ws2812_open(struct file *filep) {
  struct inode *inode = filep->f_inode;
  struct ws2812_dev_s *dev_data = inode->i_private;
  int ret = OK;
  ret = nxmutex_lock(&dev_data->lock);
  if (ret < 0) {
    return ret;
  }
  nxmutex_unlock(&dev_data->lock);
  return 0;
}

static int r528_ws2812_close(struct file *filep) {
  struct inode *inode = filep->f_inode;
  struct ws2812_dev_s *dev_data = inode->i_private;
  int ret = OK;
  ret = nxmutex_lock(&dev_data->lock);
  if (ret < 0) {
    return ret;
  }
  nxmutex_unlock(&dev_data->lock);
  return OK;
}

static ssize_t r528_ws2812_write(struct file *filep, const char *data,
                                 size_t len) {
  struct inode *inode = filep->f_inode;
  struct ws2812_dev_s *dev_data = inode->i_private;
  int pos = 0;
  unsigned int temp = 0;
  unsigned int *temp_str = (unsigned int *)data;
  int ret = OK;
  ret = nxmutex_lock(&dev_data->lock);
  if (ret < 0) {
    return ret;
  }
  while (pos < len) {
    ws2812_write("RGB", *temp_str);
    temp_str++;
    pos += 4;
  }
  if (pos < len) {
    switch (len - pos) {
    case 3:
      temp |= data[pos + 2] << 16;
    case 2:
      temp |= data[pos + 2] << 8;
    case 1:
      temp |= data[pos + 1];
    }
    ws2812_write("RGB", temp);
  }
  nxmutex_unlock(&dev_data->lock);
  return len;
}

struct ws2812_dev_s *r528_ws2812_setup(const char *path, uint16_t led_count,
                                       bool has_white) {
  struct ws2812_dev_s *dev;
  struct ledc_config *priv;
  int err;

  /* Allocate struct holding generic WS2812 device data */
  dev = kmm_zalloc(sizeof(struct ws2812_dev_s));
  if (dev == NULL) {
    lederr("r528_ws2812_setup: out of memory\n");
    set_errno(ENOMEM);
    return NULL;
  }

  /* Allocate struct holding R528's WS2812 device data */
  priv = kmm_zalloc(sizeof(struct ledc_config));
  if (priv == NULL) {
    lederr("r528_ws2812_open: out of memory\n");
    kmm_free(dev);
    set_errno(ENOMEM);
    return NULL;
  }

  dev->open = r528_ws2812_open;
  dev->close = r528_ws2812_close;
  dev->write = r528_ws2812_write;
  dev->private = priv;
  dev->clock = CONFIG_WS2812_FREQUENCY;
  // dev->port      = priv->rmt->minor;
  dev->nleds = led_count;
  dev->has_white = has_white;

  nxmutex_init(&dev->lock);

  ledinfo("r528_ws2812_setup: register dev: 0x%p\n", dev);
  ws2812_init(priv);
  // dma init
  int ret = hal_dma_chan_request(&dma_chan);
  if (ret == HAL_DMA_CHAN_STATUS_BUSY) {
    lederr("r528_ws2812_open: dma channel busy!\n");
  }
  /* Register the WS2812 RGB addressable LED strip device */
  err = ws2812_register(path, dev);
  if (err != OK) {
    set_errno(err);
    return NULL;
  }
  ws2812_write("RGB", 0);
  return (void *)dev;
}

int r528_ws2812_release(void *driver) {
  struct ws2812_dev_s *dev_data = driver;
  struct ledc_config *priv = (struct ledc_config *)dev_data->private;

  nxmutex_lock(&dev_data->lock);

  if (priv)

  {
    free(priv);
    priv = NULL;
  }

  nxmutex_unlock(&dev_data->lock);

  return OK;
}

#endif
