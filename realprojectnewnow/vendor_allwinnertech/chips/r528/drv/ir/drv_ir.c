#include <debug.h>
#include <nuttx/arch.h>
#include <nuttx/clock.h>
#include <nuttx/config.h>
#include <nuttx/irq.h>
#include <nuttx/kmalloc.h>

#include <arch/board/board.h>
#include <nuttx/lirc.h>
#include <nuttx/rc/lirc_dev.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define R528_IR_RX_BUFFER_SIZE (512 * sizeof(unsigned int))
#define R528_IR_MAX_TIMEOUT (100000) /* 100ms in microseconds */
#define R528_IR_MIN_TIMEOUT (1000)   /* 1ms in microseconds */
#define R528_IR_NO 0
#define DRIVERS_CIR_RX_PORT 0

/****************************************************************************
 * Type Definitions (Forward declarations to avoid header conflicts)
 ****************************************************************************/

/* HAL types forward declarations */
typedef enum {
  CIR_MASTER_0 = 0,
  CIR_MASTER_NUM,
} cir_port_t;

typedef enum {
  CIR_PIN_ERR = -4,
  CIR_CLK_ERR = -3,
  CIR_IRQ_ERR = -2,
  CIR_PORT_ERR = -1,
  CIR_OK = 0,
} cir_status_t;

typedef enum {
  CIR_TX_PIN_ERR = -4,
  CIR_TX_CLK_ERR = -3,
  CIR_TX_IRQ_ERR = -2,
  CIR_TX_PORT_ERR = -1,
  CIR_TX_OK = 0,
} cir_tx_status_t;

/* Forward declarations */
struct sunxi_cir_t;
struct sunxi_cir_tx_t;

/* HAL function declarations */
extern cir_status_t sunxi_cir_init(cir_port_t port);
extern void sunxi_cir_deinit(cir_port_t port);
extern void sunxi_cir_callback_register(cir_port_t port,
                                        int (*callback)(cir_port_t port,
                                                        uint32_t data_type,
                                                        uint32_t data));
extern cir_tx_status_t hal_cir_tx_init(struct sunxi_cir_tx_t **cir_tx_ptr);
extern cir_tx_status_t hal_cir_tx_deinit(struct sunxi_cir_tx_t *cir_tx);
extern void hal_cir_tx_set_carrier(int carrier_freq);
extern void hal_cir_tx_set_duty_cycle(int duty_cycle);
extern void hal_cir_tx_xmit(unsigned int *txbuf, unsigned int count);

struct sunxi_lirc_lowerhalf_s {
  struct lirc_lowerhalf_s lower; /* NuttX standard interface */

#if defined(CONFIG_DRIVERS_CIR_RX)
  cir_port_t rx_port;
#endif

#if defined(CONFIG_DRIVERS_CIR_TX)
  struct sunxi_cir_tx_t *tx_dev; /* 全志发送设备 */
#endif
};

static FAR struct sunxi_lirc_lowerhalf_s *s_lirc = NULL;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/
static int sunxi_open(FAR struct lirc_lowerhalf_s *lower);
static void sunxi_close(FAR struct lirc_lowerhalf_s *lower);
#if defined(CONFIG_DRIVERS_CIR_RX)
static int sunxi_s_rx_carrier_range(FAR struct lirc_lowerhalf_s *lower,
                                    unsigned int min, unsigned int max);
static int sunxi_s_timeout(FAR struct lirc_lowerhalf_s *lower,
                           unsigned int timeout);
#endif
#if defined(CONFIG_DRIVERS_CIR_TX)
static int sunxi_s_tx_mask(FAR struct lirc_lowerhalf_s *lower,
                           unsigned int mask);
static int sunxi_s_tx_carrier(FAR struct lirc_lowerhalf_s *lower,
                              unsigned int carrier);
static int sunxi_s_tx_duty_cycle(FAR struct lirc_lowerhalf_s *lower,
                                 unsigned int duty_cycle);
static int sunxi_tx_ir(FAR struct lirc_lowerhalf_s *lower,
                       FAR unsigned int *txbuf, unsigned int n);
static int sunxi_tx_scancode(FAR struct lirc_lowerhalf_s *lower,
                             FAR struct lirc_scancode *txbuf);
#endif
static int sunxi_s_learning_mode(FAR struct lirc_lowerhalf_s *lower,
                                 int enable);
static int sunxi_s_carrier_report(FAR struct lirc_lowerhalf_s *lower,
                                  int enable);

static const struct lirc_ops_s g_sunxi_lirc_ops = {
    .driver_type = LIRC_DRIVER_IR_RAW,
    .open = sunxi_open,
    .close = sunxi_close,

#if defined(CONFIG_DRIVERS_CIR_RX)
    .s_rx_carrier_range = sunxi_s_rx_carrier_range,
    .s_timeout = sunxi_s_timeout,
#else
    .s_rx_carrier_range = NULL,
    .s_timeout = NULL,
#endif

#if defined(CONFIG_DRIVERS_CIR_TX)
    .tx_ir = sunxi_tx_ir,
    .tx_scancode = sunxi_tx_scancode,
    .s_tx_carrier = sunxi_s_tx_carrier,
    .s_tx_duty_cycle = sunxi_s_tx_duty_cycle,
    .s_tx_mask = sunxi_s_tx_mask,
#else
    .tx_ir = NULL,
    .tx_scancode = NULL,
    .s_tx_carrier = NULL,
    .s_tx_duty_cycle = NULL,
    .s_tx_mask = NULL,
#endif
    .s_learning_mode = sunxi_s_learning_mode,
    .s_carrier_report = sunxi_s_carrier_report,
};

static int sunxi_open(FAR struct lirc_lowerhalf_s *lower) {
  rcinfo("Called %s\n", __func__);
  return 0;
}

static void sunxi_close(FAR struct lirc_lowerhalf_s *lower) {
  rcinfo("Called %s\n", __func__);
}

#if defined(CONFIG_DRIVERS_CIR_RX)
static int sunxi_s_rx_carrier_range(FAR struct lirc_lowerhalf_s *lower,
                                    unsigned int min, unsigned int max) {
  struct sunxi_lirc_lowerhalf_s *slower =
      (struct sunxi_lirc_lowerhalf_s *)lower;
  UNUSED(slower); /* Avoid unused variable warning */

  rcinfo("Called %s, min:%u, max:%u\n", __func__, min, max);
  //   sunxi_cir_sample_noise_threshold(port, calculate_noise_thr(min, max));
  //   sunxi_cir_sample_active_threshold(port, calculate_active_thr(min, max));

  return 0;
}

static int sunxi_s_timeout(FAR struct lirc_lowerhalf_s *lower,
                           unsigned int timeout) {
  rcinfo("Called %s, timeout:%u\n", __func__, timeout);
  return 0;
}
#endif

#if defined(CONFIG_DRIVERS_CIR_TX)
static int sunxi_s_tx_mask(FAR struct lirc_lowerhalf_s *lower,
                           unsigned int mask) {
  rcinfo("Called %s, mask:%u\n", __func__, mask);
  return 0;
}

static int sunxi_s_tx_carrier(FAR struct lirc_lowerhalf_s *lower,
                              unsigned int carrier) {
  rcinfo("Called %s, carrier:%u\n", __func__, carrier);
  hal_cir_tx_set_carrier(carrier);
  return 0;
}

static int sunxi_s_tx_duty_cycle(FAR struct lirc_lowerhalf_s *lower,
                                 unsigned int duty_cycle) {
  rcinfo("Called %s, duty_cycle:%u\n", __func__, duty_cycle);
  hal_cir_tx_set_duty_cycle(duty_cycle);
  return 0;
}

static int sunxi_tx_ir(FAR struct lirc_lowerhalf_s *lower,
                       FAR unsigned int *txbuf, unsigned int n) {
  rcinfo("Sunxi RC send raw data:%d(size:%d) to device\n", *txbuf, n);
  hal_cir_tx_xmit(txbuf, n);
  return n * sizeof(unsigned int);
}

static int sunxi_tx_scancode(FAR struct lirc_lowerhalf_s *lower,
                             FAR struct lirc_scancode *txbuf) {
  rcinfo("Sunxi RC send scancode data: 0x%llx to device\n",
         (unsigned long long)txbuf->scancode);
  return sizeof(struct lirc_scancode);
}
#endif

static int sunxi_s_learning_mode(FAR struct lirc_lowerhalf_s *lower,
                                 int enable) {
  rcinfo("Called %s, enable:%d\n", __func__, enable);
  return 0;
}

static int sunxi_s_carrier_report(FAR struct lirc_lowerhalf_s *lower,
                                  int enable) {
  rcinfo("Called %s, enable:%d\n", __func__, enable);
  return 0;
}

#if defined(CONFIG_DRIVERS_CIR_RX)
static int r528_cir_callback(cir_port_t port, uint32_t data_type,
                             uint32_t data) {
  UNUSED(port);
  UNUSED(data_type);
  extern void lirc_sample_event(FAR struct lirc_lowerhalf_s * lower,
                                unsigned int sample);
  lirc_sample_event(&s_lirc->lower, data);
  return 0;
}
#endif

int r528_rc_initialize(void) {
  int ret;
  s_lirc = (FAR struct sunxi_lirc_lowerhalf_s *)kmm_zalloc(
      sizeof(struct sunxi_lirc_lowerhalf_s));

  if (!s_lirc) {
    rcerr("Failed to allocate memory for sunxi_lirc_lowerhalf_s\n");
    return -ENOMEM;
  }

#if defined(CONFIG_DRIVERS_CIR_RX)
  ret = sunxi_cir_init(DRIVERS_CIR_RX_PORT);
  sunxi_cir_callback_register(DRIVERS_CIR_RX_PORT, r528_cir_callback);
  s_lirc->rx_port = DRIVERS_CIR_RX_PORT;

  if (ret) {
    rcerr("Failed to initialize CIR RX port\n");
    // kmm_free(s_lirc);
    // return -EINVAL;
  }
#endif

#if defined(CONFIG_DRIVERS_CIR_TX)
  hal_cir_tx_init(&s_lirc->tx_dev);
  if (!s_lirc->tx_dev) {
    rcerr("Failed to initialize CIR TX device\n");
    // kmm_free(s_lirc);
    // return -EINVAL;
  }
#endif

  s_lirc->lower.ops = &g_sunxi_lirc_ops;
  s_lirc->lower.buffer_bytes = R528_IR_RX_BUFFER_SIZE;

  return lirc_register(&s_lirc->lower, R528_IR_NO);
}
