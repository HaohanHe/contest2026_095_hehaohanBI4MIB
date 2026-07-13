#include <hal_cache.h>
#include <hal_clk.h>
#include <hal_dma.h>
#include <hal_gpio.h>
#include <sunxi_hal_ledc.h>

#include "platform/ledc_sun8iw20.h"



#define LEDC_PIN GPIOG(16)
#define LEDC_PINMUXSEL 7

static unsigned int base_addr = LEDC_BASE;

struct sunxi_dma_chan *dma_chan = NULL;

// 全局LEDC控制结构体
struct ledc_config g_ledc;

static void ledc_dump_reg(void) {
  _info("LEDC_CTRL_REG = %0x\n", hal_readl(base_addr + LEDC_CTRL_REG));
  _info("LED_T01_TIMING_CTRL_REG = %0x\n",
            hal_readl(base_addr + LED_T01_TIMING_CTRL_REG));
  _info("LEDC_DATA_FINISH_CNT_REG = %0x\n",
            hal_readl(base_addr + LEDC_DATA_FINISH_CNT_REG));
  _info("LED_RST_TIMING_CTRL_REG = %0x\n",
            hal_readl(base_addr + LED_RST_TIMING_CTRL_REG));
  _info("LEDC_WAIT_TIME0_CTRL_REG = %0x\n",
            hal_readl(base_addr + LEDC_WAIT_TIME0_CTRL_REG));
  _info("LEDC_DATA_REG = %0x\n", hal_readl(base_addr + LEDC_DATA_REG));
  _info("LEDC_DMA_CTRL_REG = %0x\n",
            hal_readl(base_addr + LEDC_DMA_CTRL_REG));
  _info("LEDC_INTC_REG = %0x\n", hal_readl(base_addr + LEDC_INTC_REG));
  _info("LEDC_INTS_REG = %0x\n", hal_readl(base_addr + LEDC_INTS_REG));
  _info("LEDC_WAIT_TIME1_CTRL_REG = %0x\n",
            hal_readl(base_addr + LEDC_WAIT_TIME1_CTRL_REG));
  _info("LEDC_FIFO_DATA0_REG = %0x\n",
            hal_readl(base_addr + LEDC_FIFO_DATA0_REG));
}

static void ledc_set_reset_ns(unsigned int reset_ns) {
  unsigned int n, reg_val;
  unsigned int mask = 0x1FFF;
  unsigned int min = LEDC_RESET_TIME_MIN_NS;
  unsigned int max = LEDC_RESET_TIME_MAX_NS;

  if (reset_ns < min || reset_ns > max) {
    _info("invalid parameter, reset_ns should be %u-%u!\n", min, max);
    return;
  }

  n = (reset_ns - 42) / 42;
  // reg_val = hal_readl(base_addr + LED_RST_TIMING_CTRL_REG);
  // reg_val &= ~(mask << 16);
  // reg_val |= (n << 16);
  // hal_writel(reg_val, base_addr + LED_RST_TIMING_CTRL_REG);

  struct sunxi_ledc_reg *ledc = (struct sunxi_ledc_reg *)(base_addr);
  mask = 0x3FFF;
  int shift = 16;
  //  n = 1;          //42*(6+1)

  ledc->rst_t &= ~(mask << shift);
  ledc->rst_t |= n << shift;
}

static void ledc_set_t1h_ns(unsigned int t1h_ns) {
  unsigned int n, reg_val;
  unsigned int mask = 0x3F;
  unsigned int shift = 21;
  unsigned int min = LEDC_T1H_MIN_NS;
  unsigned int max = LEDC_T1H_MAX_NS;

  if (t1h_ns < min || t1h_ns > max) {
    _info("invalid parameter, t1h_ns should be %u-%u!\n", min, max);
    return;
  }

  // n = (t1h_ns - 42) / 42;
  // reg_val = hal_readl(base_addr + LED_T01_TIMING_CTRL_REG);
  // reg_val &= ~(mask << shift);
  // reg_val |= n << shift;
  // hal_writel(reg_val, base_addr + LED_T01_TIMING_CTRL_REG);

  struct sunxi_ledc_reg *ledc = (struct sunxi_ledc_reg *)(base_addr);
  uint32_t mask0 = 0x7FF;
  int n0l = 19;
  int n0h = 8;

  uint32_t mask1 = 0x7FF;
  int shift1 = 16;
  int n1l = 9;
  int n1h = 18;

  ledc->time01 &= ~mask0;
  ledc->time01 |= ((n0h << 6) | (n0l));

  ledc->time01 &= ~(mask1 << shift1);
  ledc->time01 |= ((n1h << 21) | (n1l << 16));
}

static void ledc_set_t1l_ns(unsigned int t1l_ns) {
  unsigned int n, reg_val;
  unsigned int mask = 0x1F;
  unsigned int shift = 16;
  unsigned int min = LEDC_T1L_MIN_NS;
  unsigned int max = LEDC_T1L_MAX_NS;

  if (t1l_ns < min || t1l_ns > max) {
    _info("invalid parameter, t1l_ns should be %u-%u!\n", min, max);
    return;
  }

  n = (t1l_ns - 42) / 42;
  reg_val = hal_readl(base_addr + LED_T01_TIMING_CTRL_REG);
  reg_val &= ~(mask << shift);
  reg_val |= n << shift;
  hal_writel(reg_val, base_addr + LED_T01_TIMING_CTRL_REG);
}

static void ledc_set_t1_ns(unsigned int t1l_ns) {
  struct sunxi_ledc_reg *ledc = (struct sunxi_ledc_reg *)(base_addr);
  uint32_t mask0 = 0x7FF;
  int n0l = 19;
  int n0h = 8;

  uint32_t mask1 = 0x7FF;
  int shift1 = 16;
  int n1l = 9;
  int n1h = 18;

  ledc->time01 &= ~mask0;
  ledc->time01 |= ((n0h << 6) | (n0l));

  ledc->time01 &= ~(mask1 << shift1);
  ledc->time01 |= ((n1h << 21) | (n1l << 16));
}

static void ledc_set_t0h_ns(unsigned int t0h_ns) {
  unsigned int n, reg_val;
  unsigned int mask = 0x1F;
  unsigned int shift = 6;
  unsigned int min = LEDC_T0H_MIN_NS;
  unsigned int max = LEDC_T0H_MAX_NS;

  if (t0h_ns < min || t0h_ns > max) {
    _info("invalid parameter, t0h_ns should be %u-%u!\n", min, max);
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
    _info("invalid parameter, t0l_ns should be %u-%u!\n", min, max);
    return;
  }

  n = (t0l_ns - 42) / 42;
  reg_val = hal_readl(base_addr + LED_T01_TIMING_CTRL_REG);
  reg_val &= ~0x3F;
  reg_val |= n;
  hal_writel(reg_val, base_addr + LED_T01_TIMING_CTRL_REG);
}

static void ledc_set_t0_ns(unsigned int t0h_ns) {}

static void ledc_set_wait_time0_ns(unsigned int wait_time0_ns) {
  unsigned int n, reg_val;
  unsigned int min = LEDC_WAIT_TIME0_MIN_NS;
  unsigned int max = LEDC_WAIT_TIME0_MAX_NS;

  if (wait_time0_ns < min || wait_time0_ns > max) {
    _info("invalid parameter, wait_time0_ns should be %u-%u!\n", min, max);
    return;
  }

  // n = (wait_time0_ns - 42) / 42;
  // reg_val = (1 << 8) | n;
  // hal_writel(reg_val, base_addr + LEDC_WAIT_TIME0_CTRL_REG);

  struct sunxi_ledc_reg *ledc = (struct sunxi_ledc_reg *)(base_addr);
  uint32_t mask = 0xFF;
  int shift = 0;
  n = 1;

  ledc->t0_ctrl &= ~(mask << shift);
  ledc->t0_ctrl |= n << shift;
}

static void ledc_set_wait_time1_ns(unsigned long long wait_time1_ns) {
  unsigned long long max = LEDC_WAIT_TIME1_MAX_NS;
  unsigned int min = LEDC_WAIT_TIME1_MIN_NS;
  unsigned int n, reg_val;

  if (wait_time1_ns < min || wait_time1_ns > max) {
    _info("invalid parameter, wait_time1_ns should be %u-%llu!\n", min,
              max);
    return;
  }

  // n = wait_time1_ns / 42;
  // // tmp = wait_time1_ns;
  // // n = div_u64(tmp, 42);
  // n -= 1;
  // reg_val = (1 << 31) | n;
  // hal_writel(reg_val, base_addr + LEDC_WAIT_TIME1_CTRL_REG);

  struct sunxi_ledc_reg *ledc = (struct sunxi_ledc_reg *)(base_addr);
  uint32_t mask = 0x7FFFFFFF;
  int shift = 0;
  n = 1;

  ledc->t1_ctrl &= ~(mask << shift);
  ledc->t1_ctrl |= n << shift;
}

static void ledc_set_wait_data_time_ns(unsigned int wait_data_time_ns) {
  unsigned int mask = 0x1FFF;
  unsigned int shift = 16;
  unsigned int reg_val = 0;
  unsigned int n, min, max;

  min = LEDC_WAIT_DATA_TIME_MIN_NS;
  max = LEDC_WAIT_DATA_TIME_MAX_NS_IC;

  if (wait_data_time_ns < min || wait_data_time_ns > max) {
    _info("invalid parameter, wait_data_time_ns should be %u-%u!\n", min,
              max);
    return;
  }

  n = (wait_data_time_ns - 42) / 42;
  reg_val &= ~(mask << shift);
  reg_val |= (n << shift);
  hal_writel(reg_val, base_addr + LEDC_DATA_FINISH_CNT_REG);
}

static void ledc_set_length(unsigned int length) {

  if (length == 0)
    return;

  struct sunxi_ledc_reg *ledc = (struct sunxi_ledc_reg *)(base_addr);
  uint32_t mask = 0x1FFF;
  int shift = 16;

  ledc->ctrl &= ~(mask << shift);
  ledc->ctrl |= length << shift; // data length

  ledc->rst_t &= ~(0x3FF);
  ledc->rst_t |= length - 1; // lights num
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
    val = 0;
  }
  struct sunxi_ledc_reg *ledc = (struct sunxi_ledc_reg *)(base_addr);
  mask = 0x7;
  shift = 6;

  ledc->ctrl &= ~(mask << shift);
  ledc->ctrl |= (val << shift);
}

static void ledc_disable_irq(unsigned int mask) {
  struct sunxi_ledc_reg *ledc = (struct sunxi_ledc_reg *)(base_addr);

  ledc->irq_ctrl &= ~mask;
}

static void ledc_enable_irq(unsigned int mask) {
  struct sunxi_ledc_reg *ledc = (struct sunxi_ledc_reg *)(base_addr);

  ledc->irq_ctrl |= mask;
}

static void ledc_set_dma_mode(void) {
  unsigned int reg_val = 0;

  reg_val |= 1 << 5;
  hal_writel(reg_val, base_addr + LEDC_DMA_CTRL_REG);
}

static void ledc_set_cpu_mode(void) {
  struct sunxi_ledc_reg *ledc = (struct sunxi_ledc_reg *)(base_addr);
  int shift = 5;

  ledc->DMA_ctrl &= ~(1 << shift);
  ledc_enable_irq(LEDC_FIFO_CPUREQ_INT_EN);
}

static void ledc_clear_all_irq(void) {
  unsigned int reg_val;

  reg_val = hal_readl(base_addr + LEDC_INTS_REG);
  reg_val |= 0x1F;
  hal_writel(reg_val, base_addr + LEDC_INTS_REG);
}

static unsigned int ledc_get_irq_status(void) {
  return hal_readl(base_addr + LEDC_INTS_REG);
}

static void ledc_soft_reset(void) {
  unsigned int reg_val;

  reg_val = hal_readl(base_addr + LEDC_CTRL_REG);
  reg_val |= 1 << 1;
  hal_writel(reg_val, base_addr + LEDC_CTRL_REG);
}

static void ledc_reset_en(void) {
  unsigned int reg_val;

  reg_val = hal_readl(base_addr + LEDC_CTRL_REG);
  reg_val |= 1 << 10;
  hal_writel(reg_val, base_addr + LEDC_CTRL_REG);
}

static void ledc_set_data(unsigned int data) {
  hal_writel(data, base_addr + LEDC_DATA_REG);
}

static void ledc_enable(void) {
  unsigned int reg_val;

  reg_val = hal_readl(base_addr + LEDC_CTRL_REG);
  reg_val |= 1;
  hal_writel(reg_val, base_addr + LEDC_CTRL_REG);
}

static void hal_ledc_set_time(struct ledc_config *ledc) {
  ledc_set_reset_ns(ledc->reset_ns);
  ledc_set_t1h_ns(ledc->t1h_ns);
  ledc_set_t1l_ns(ledc->t1l_ns);
  ledc_set_t0h_ns(ledc->t0h_ns);
  ledc_set_t0l_ns(ledc->t0l_ns);
  ledc_set_wait_time0_ns(ledc->wait_time0_ns);
  ledc_set_wait_time1_ns(ledc->wait_time1_ns);
  ledc_set_wait_data_time_ns(ledc->wait_data_time_ns);
}

void hal_ledc_dma_callback(void *para) { printf("dma callback\n"); }

static void sunxi_ledc_write_data(unsigned int length, unsigned int g,
                                  unsigned int r, unsigned int b) {
  struct sunxi_ledc_reg *ledc = (struct sunxi_ledc_reg *)(base_addr);
  int i;

  for (i = 0; i < length; i++)
    ledc->data_reg = ((g << 16) | (r << 8) | (b));
}

void hal_ledc_trans_data(struct ledc_config *ledc) {
  int i;
  unsigned long int size;
  unsigned int mask = 0;
  struct dma_slave_config slave_config;

  irqstate_t flags;

  flags = enter_critical_section();
  if (ledc->length <= SUNXI_LEDC_FIFO_DEPTH) {
    ledc_set_output_mode(ledc->output_mode);
    ledc_set_cpu_mode();
    ledc_set_length(1);
    ledc_enable_irq(LEDC_TRANS_FINISH_INT_EN | LEDC_WAITDATA_TIMEOUT_INT_EN |
                    LEDC_FIFO_OVERFLOW_INT_EN | LEDC_GLOBAL_INT_EN);
    ledc_enable();
    sunxi_ledc_write_data(1, ledc->data[0] >> 16 & 0xff,
                          ledc->data[0] >> 8 & 0xff, ledc->data[0] & 0xff);

  } else {
    mask &= ~LEDC_FIFO_CPUREQ_INT_EN;
    ledc_reset_en();
    size = ledc->length * 4;
    hal_dcache_clean((unsigned long)ledc->data, sizeof(ledc->data));
    slave_config.direction = DMA_MEM_TO_DEV;
    slave_config.src_addr = (uint32_t)ledc->data;
    slave_config.dst_addr = (uint32_t)(base_addr + LEDC_DATA_REG);
    slave_config.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
    slave_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
    slave_config.src_maxburst = DMA_SLAVE_BURST_16;
    slave_config.dst_maxburst = DMA_SLAVE_BURST_16;
    slave_config.slave_id = sunxi_slave_id(DRQDST_LEDC, DRQSRC_SDRAM);
    hal_dma_slave_config(dma_chan, &slave_config);
    hal_dma_prep_device(dma_chan, slave_config.dst_addr, slave_config.src_addr,
                        size, DMA_MEM_TO_DEV);
    dma_chan->callback = hal_ledc_dma_callback;
    hal_dma_start(dma_chan);
    hal_ledc_set_time(ledc);
    ledc_set_output_mode(ledc->output_mode);
    ledc_set_length(ledc->length);
    ledc_set_dma_mode();
    ledc_enable_irq(mask);
    ledc_enable();
  }
  leave_critical_section(flags);
  // ledc_dump_reg();
}

int sunxi_set_led_brightness(int led_num, unsigned int brightness) {
  if (led_num < 0 || ((brightness >> 24) & 0xff) > 0) {
    return -1;
  }
  // _info("cacascascascascasc   %s\r\n", g_ledc.output_mode);
  unsigned int rgb_data = 0;
  if (strcmp(g_ledc.output_mode, "GRB") == 0) {
    // GRB 格式: G << 16 | R << 8 | B << 0

    // rgb_data = (brightness << 16) | (brightness << 8) | brightness;
    rgb_data = brightness;
  } else if (strcmp(g_ledc.output_mode, "GBR") == 0) {
    // GBR 格式: G << 16 | B << 8 | R << 0
    rgb_data = (brightness << 16) | (brightness << 8) | brightness;
  } else if (strcmp(g_ledc.output_mode, "RGB") == 0) {
    // RGB 格式: R << 16 | G << 8 | B << 0
    rgb_data = (brightness << 16) | (brightness << 8) | brightness;
  } else if (strcmp(g_ledc.output_mode, "RBG") == 0) {
    // RBG 格式: R << 16 | B << 8 | G << 0
    rgb_data = (brightness << 16) | (brightness << 8) | brightness;
  } else if (strcmp(g_ledc.output_mode, "BGR") == 0) {
    // BGR 格式: B << 16 | G << 8 | R << 0
    rgb_data = (brightness << 16) | (brightness << 8) | brightness;
  } else if (strcmp(g_ledc.output_mode, "BRG") == 0) {
    // BRG 格式: B << 16 | R << 8 | G << 0
    rgb_data = (brightness << 16) | (brightness << 8) | brightness;
  } else {
    // 未知输出模式
    return -1;
  }

  // 配置数据缓冲区
  g_ledc.data = malloc(sizeof(int));
  g_ledc.data[0] = brightness;
  g_ledc.length = 1;
  // 执行传输
  hal_ledc_trans_data(&g_ledc);
  free(g_ledc.data);
  return 0;
}

int sunxi_led_init(void) {
  // 初始化全局结构体
  memset(&g_ledc, 0, sizeof(struct ledc_config));

  // 设置默认配置
  g_ledc = (struct ledc_config){.led_count = 1,
                                .reset_ns = 84,
                                .t1h_ns = 1000,
                                .t1l_ns = 1000,
                                .t0h_ns = 580,
                                .t0l_ns = 1000,
                                .wait_time0_ns = 84,
                                .wait_time1_ns = 84,
                                .wait_data_time_ns = 600000,
                                .output_mode = "GRB"};

  return 0;
}

void hal_ledc_clear_all_irq(void) { ledc_clear_all_irq(); }

unsigned int hal_ledc_get_irq_status(void) { return ledc_get_irq_status(); }

void hal_ledc_reset(void) {
  ledc_disable_irq(LEDC_TRANS_FINISH_INT_EN | LEDC_WAITDATA_TIMEOUT_INT_EN |
                   LEDC_FIFO_OVERFLOW_INT_EN | LEDC_GLOBAL_INT_EN |
                   LEDC_GLOBAL_INT_EN);

  if (dma_chan) {
    hal_dma_stop(dma_chan);
  }
  ledc_soft_reset();
}

void sunxi_ledc_clk_init(void) {
  struct sunxi_ccmu_reg *clk = (struct sunxi_ccmu_reg *)(LEDC_CLOCK_ADDR);

  clk->model_clk = (1 << 31) | (0 << 24) | (0 << 8) | (0);
  clk->bus_gate |= (0 << 16);
  clk->bus_gate |= (1 << 16);
  clk->bus_gate |= (1);
}

void hal_ledc_deinit(void) {
  hal_dma_chan_free(dma_chan);

  // clk_deinit
}

void hal_ledc_init(void) {
  // int i;
  int ret = 0;
  // unsigned int reg_val = 0;
  hal_clk_t ledc_bus_clk;

  // clk_init
  // clk source : default OSC24M
  //  ledc_bus_clk = hal_clock_get(LEDC_CLK_TYPE, LEDC_CLK_ID);
  //  hal_clock_enable(ledc_bus_clk);
  irqstate_t flags;

  flags = enter_critical_section();

  sunxi_ledc_clk_init();

  // gpio init
  hal_gpio_pinmux_set_function(LEDC_PIN, LEDC_PINMUXSEL);

  sunxi_led_init();
  leave_critical_section(flags);
  // hal_dma_init();
  // dma init
  ret = hal_dma_chan_request(&dma_chan);
  if (ret == HAL_DMA_CHAN_STATUS_BUSY) {
    printf("dma channel busy!");
  }

  // ledc_dump_reg();
}
