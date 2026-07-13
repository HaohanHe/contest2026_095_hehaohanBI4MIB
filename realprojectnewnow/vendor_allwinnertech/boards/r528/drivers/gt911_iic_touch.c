/****************************************************************************
 * vendor/allwinnertech/boards/r528/drivers/gt911_iic_touch.c
 *
 * GT911 IIC Touchscreen Driver for R528 Platform
 * GT911 capacitive touch controller driver
 *
 ****************************************************************************/

#include <nuttx/config.h>
#ifndef OPEN_MAX
#define OPEN_MAX 256
#endif
#ifndef CLOCK_MAX
#define CLOCK_MAX 4294967295U
#endif

#include <stdbool.h>
#include <stdint.h>
#include <debug.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#include <nuttx/irq.h>
#include <nuttx/kmalloc.h>
#include <nuttx/i2c/i2c_master.h>
#include <nuttx/input/touchscreen.h>
#include <nuttx/wqueue.h>
#include <nuttx/semaphore.h>

#include <arch/board/board.h>
#include "aw_common.h"
#include "hal_gpio.h"
#include "gt911_iic_touch.h"

#define POLL_MINDELAY  (5)
#define POLL_MAXDELAY  (30)
#define POLL_INCREMENT (5)

#define RST GPIOB(4)
#define INT GPIOB(5)
#define GPIO_SET(pin, val) hal_gpio_set_data(pin,val)

#define TOUCH_INVALID (1 << 0)
#define TOUCH_VALID   (1 << 1)
#define TOUCH_EVENT_MASK (0x00)
#define TOUCH_COUNT 0x0F
#define GT911_MOVE_THRESH_PIX 4
#define HIGH true
#define LOW false
#define GT911_TOUCH_POLLMODE

enum{
  ERROR_TRANSFER  = 1,
  ERROR_READ,
  ERROR_SLAVE,
  ERROR_REGISTERED,
  ERROR_REGISTER,
  ERROR_CONTROL,
  ERROR_IRQ,
};

/* GT911 touch device instance */
struct gt911_touch_dev_s
{
  struct touch_lowerhalf_s lower;         /* Standard touch lower half */
  uint8_t touch_buf[GT911_TOUCH_DATA_LEN]; /* Raw touch data buffer */
  FAR struct i2c_master_s *i2c;           /* I2C driver instance */
  uint32_t  frequency;                    /* Current I2C frequency */
  struct work_s work;                     /* Work queue for touch processing */
  bool touch_valid;                       /* Valid touch data flag */
  uint8_t i2c_addr;                       /* Detected I2C address */
  sem_t waitsem;                          /* Semaphore for ISR synchronization */
#ifdef GT911_TOUCH_POLLMODE
  uint32_t poll_interval;                 /* Polling interval in milliseconds */
#else
  uint32_t irq;                           /* Interrupt line for GT911 */
#endif
  uint8_t last_state;
  int16_t last_x;
  int16_t last_y;
};

extern void up_udelay(useconds_t microseconds);

static int gt911_detect_controller(FAR struct gt911_touch_dev_s *priv);
/* GT911 touch device instance */
static struct gt911_touch_dev_s g_gt911_touch = {
  .lower.maxpoint = 1,
  .lower.control = NULL,
  .lower.write = NULL,
#ifdef GT911_TOUCH_POLLMODE
  .poll_interval = POLL_MINDELAY,
#endif
  .frequency = GT911_I2C_FREQUENCY,
  .last_state = TOUCH_INVALID,
  .last_x = 0,
  .last_y = 0,
};

static uint8_t data[GT911_TOUCH_DATA_LEN];
static void gt911_worker(FAR void *arg);

static int gt911_i2c_read(FAR struct gt911_touch_dev_s *priv,
                              uint16_t regaddr, FAR uint8_t *buffer, size_t buflen)
{
  struct i2c_msg_s msg[2];
  uint8_t addr_buf[2];
  int ret;

  DEBUGASSERT(priv && priv->i2c);

  if (!priv->i2c) {
    ierr("fisker: ERROR - I2C instance is NULL!\n");
    return -EINVAL;
  }

  /* GT911 uses 16-bit register addresses in big-endian format */
  addr_buf[0] = (regaddr >> 8) & 0xFF;   /* High byte */
  addr_buf[1] = regaddr & 0xFF;          /* Low byte */

  /* Set up the address write operation */
  msg[0].frequency = priv->frequency;
  msg[0].addr      = priv->i2c_addr;
  msg[0].flags     = 0;
  msg[0].buffer    = addr_buf;
  msg[0].length    = 2;

  /* Set up the data read operation */
  msg[1].frequency = priv->frequency;
  msg[1].addr      = priv->i2c_addr;
  msg[1].flags     = I2C_M_READ;
  msg[1].buffer    = buffer;
  msg[1].length    = buflen;

  ret = I2C_TRANSFER(priv->i2c, msg, 2);
  if (ret)
    {
      ierr("ERROR: GT911 I2C read failed: %d\n", ret);
      return ERROR_TRANSFER;
    }
  return OK;
}

static int gt911_i2c_write(FAR struct gt911_touch_dev_s *priv,
                               uint16_t regaddr, uint8_t value)
{
  struct i2c_msg_s msg;
  uint8_t dat[3];
  int ret;

  DEBUGASSERT(priv && priv->i2c);

  /* GT911 uses 16-bit register addresses in big-endian format */
  dat[0] = (regaddr >> 8) & 0xFF;   /* High byte */
  dat[1] = regaddr & 0xFF;          /* Low byte */
  dat[2] = value;                   /* Data byte */

  /* Set up the I2C message */
  msg.frequency = priv->frequency;
  msg.addr      = priv->i2c_addr;
  msg.flags     = 0;
  msg.buffer    = dat;
  msg.length    = 3;

  ret = I2C_TRANSFER(priv->i2c, &msg, 1);
  if (ret < 0)
    {
      ierr("ERROR: GT911 I2C write failed: %d\n", ret);
      return ERROR_TRANSFER;
    }

  return ret;
}

static int gt911_detect_controller(FAR struct gt911_touch_dev_s *priv)
{
  uint8_t product_id[4];
  int ret;

  memset(product_id, 0, sizeof(product_id));
  ret = gt911_i2c_read(priv, GT911_REG_PRODUCT_ID, product_id, 4);
  if(strcmp((char*)product_id,"911")){
    ierr("fisker: Failed to read GT911 product ID: %d\n", ret);
    return ERROR_READ;
  }
  return OK;
}

static void gt911_touch_process_event_one(uint8_t touch_state, FAR struct gt911_touch_dev_s *priv, FAR struct touch_sample_s *sample){

  sample->npoints = 1;
  sample->point[0].flags &= TOUCH_EVENT_MASK;

  if(touch_state == TOUCH_VALID){

    switch(priv->last_state){

      case TOUCH_INVALID:

        sample->point[0].flags |= TOUCH_DOWN;
        priv->last_state = TOUCH_VALID;
        touch_event(priv->lower.priv, sample);
        break;

      case TOUCH_VALID:

        if(abs(sample->point[0].x - priv->last_x) > GT911_MOVE_THRESH_PIX ||
           abs(sample->point[0].y - priv->last_y) > GT911_MOVE_THRESH_PIX)
        {
          sample->point[0].flags |= TOUCH_MOVE;
          priv->last_x = sample->point[0].x;
          priv->last_y = sample->point[0].y;
        }
        else
        {
          sample->point[0].flags |= TOUCH_DOWN;
          priv->last_x = sample->point[0].x;
          priv->last_y = sample->point[0].y;
        }
        touch_event(priv->lower.priv, sample);
        break;
    }
  }
  else{

    switch(priv->last_state){

      case TOUCH_INVALID:

       sample->npoints = 0;
       memset(&sample->point[0], 0, sizeof(sample->point[0]));
       break;

      case TOUCH_VALID:

       sample->point[0].flags |= TOUCH_UP;
       priv->last_state = TOUCH_INVALID;
       priv->last_x = 0;
       priv->last_y = 0;
       touch_event(priv->lower.priv, sample);
       break;
    }
  }
}

static int gt911_touch_process_data(FAR struct gt911_touch_dev_s *priv,FAR struct touch_sample_s *sample)
{
  FAR struct gt911_touch_data_s *raw = (FAR struct gt911_touch_data_s *)priv->touch_buf;
  if(!TOUCH_POINT_GET_STATUS(raw->status_id) || !TOUCH_POINT_GET_NUM(raw->status_id) || TOUCH_POINT_GET_LARGE(raw->status_id)){

    gt911_i2c_write(priv, GT911_REG_COORD_ADDR, 0x00);
    return TOUCH_INVALID;
  }
  sample->npoints = TOUCH_POINT_GET_NUM(raw->status_id);
  if (sample->npoints > GT911_MAX_TOUCH_POINTS)
    sample->npoints = GT911_MAX_TOUCH_POINTS;

  for (int i = 0; i < sample->npoints; i++)
  {
#ifdef CONFIG_SUNXI_DISP2_FB_HW_ROTATION_SUPPORT
    // sample->point[i].x        = TOUCH_POINT_GET_Y(raw->touch[i]);
    // sample->point[i].y        = T070S140B_LCD_WIDTH - TOUCH_POINT_GET_X(raw->touch[i]);
    sample->point[i].x        = TOUCH_POINT_GET_X(raw->touch[i]);
    sample->point[i].y        = TOUCH_POINT_GET_Y(raw->touch[i]);
#else
    sample->point[i].x        = T070S140B_LCD_WIDTH - TOUCH_POINT_GET_X(raw->touch[i]);
    sample->point[i].y        = T070S140B_LCD_HEIGHT - TOUCH_POINT_GET_Y(raw->touch[i]);
#endif
    sample->point[i].id       = TOUCH_POINT_GET_ID(raw->touch[i]);
    sample->point[i].h        = 0;
    sample->point[i].w        = 0;
    sample->point[i].pressure = TOUCH_POINT_GET_SIZE(raw->touch[i]);
    sample->point[i].timestamp = touch_get_time();
    memset(&raw->touch[i], 0, sizeof(raw->touch[i]));
  }

  raw->status_id = 0;
  gt911_i2c_write(priv, GT911_REG_COORD_ADDR, 0x00);
  return TOUCH_VALID;
}

static int gt911_control_initialize(void){

  int ret;
  FAR struct gt911_touch_dev_s *priv = &g_gt911_touch;

  printf("fisker: Initializing GT911 GPIO pins...\n");

  /* config GPIO Pin for RST and INT */
  hal_gpio_pinmux_set_function(RST, GPIO_MUXSEL_OUT);
  hal_gpio_set_driving_level(RST, GPIO_DRIVING_LEVEL3);
  hal_gpio_set_direction(RST, GPIO_DIRECTION_OUTPUT);

  hal_gpio_pinmux_set_function(INT, GPIO_MUXSEL_OUT);
  hal_gpio_set_driving_level(INT, GPIO_DRIVING_LEVEL3);
  hal_gpio_set_direction(INT, GPIO_DIRECTION_OUTPUT);

  /* try addr 0x5D */
  printf("fisker: Trying GT911 address 0x5D with specific reset sequence...\n");
  priv->i2c_addr = GT911_I2C_ADDR_1;

  hal_gpio_set_data(RST, LOW);
  hal_gpio_set_data(INT, LOW);
  up_udelay(20000);                 /* 复位保持20ms */
  hal_gpio_set_data(RST, HIGH);
  up_udelay(50000);                 /* 等待50ms芯片启动 */

#ifndef GT911_TOUCH_POLLMODE
  hal_gpio_set_data(INT, HIGH);
  hal_gpio_pinmux_set_function(INT, GPIO_MUXSEL_IN);
  hal_gpio_set_direction(INT, GPIO_DIRECTION_INPUT);
  hal_gpio_set_pull(INT, GPIO_PULL_DOWN_DISABLED);
  up_udelay(20000);
#endif

  printf("fisker: Testing communication with address 0x%02X...\n", priv->i2c_addr);

  ret = gt911_detect_controller(priv);

  if(ret) {
    printf("fisker: Address 0x5D failed, trying 0x14 with different sequence...\n");
    /* 尝试地址0x14的时序 */
    priv->i2c_addr = GT911_I2C_ADDR_2;
    /* 重新配置INT为输出 */
    hal_gpio_pinmux_set_function(INT, GPIO_MUXSEL_OUT);
    hal_gpio_set_direction(INT, GPIO_DIRECTION_OUTPUT);

    hal_gpio_set_data(RST, LOW);
    hal_gpio_set_data(INT, HIGH);  /* INT=HIGH选择0x14地址 */
    up_udelay(20000);

    hal_gpio_set_data(RST, HIGH);
    up_udelay(5000);
    hal_gpio_set_data(INT, LOW);
    up_udelay(50000);

    ret = gt911_detect_controller(priv);
    if(ret != OK) {
      ierr("fisker: GT911 detection failed at both addresses!\n");
      return ERROR_SLAVE;
    }
  }

  return OK;
}

static void gt911_worker(FAR void *arg){

  FAR struct gt911_touch_dev_s *priv = (FAR struct gt911_touch_dev_s *)arg;
  FAR struct touch_sample_s *sample = (FAR struct touch_sample_s *)data;
  uint8_t touch_state;
  int ret;

  DEBUGASSERT(priv);
  ret = gt911_i2c_read(priv, GT911_REG_COORD_ADDR, priv->touch_buf,
                           1 + (GT911_MAX_TOUCH_POINTS * GT911_POINT_SIZE));  //读到的第一个字节是header信息
  if(ret){
    ierr("fisker: I2C read failed\n");
#ifdef GT911_TOUCH_POLLMODE
    goto error_read;
#else
    ierr("ERROR: Failed to read\n");
    return;
#endif
  }

  touch_state = gt911_touch_process_data(priv, sample);
  gt911_touch_process_event_one(touch_state, priv, sample);

#ifdef GT911_TOUCH_POLLMODE
  if(touch_state == TOUCH_VALID)
    priv->poll_interval = POLL_MINDELAY;
  else if(priv->last_state == TOUCH_INVALID)
    priv->poll_interval += POLL_INCREMENT;
  else
    priv->poll_interval = POLL_MINDELAY;

  if(priv->poll_interval < POLL_MINDELAY)
    priv->poll_interval = POLL_MINDELAY;
  if(priv->poll_interval> POLL_MAXDELAY)
    priv->poll_interval = POLL_MAXDELAY;

error_read:
  work_queue(HPWORK, &priv->work, gt911_worker, priv, priv->poll_interval);
#endif
}

#ifndef GT911_TOUCH_POLLMODE
static hal_irqreturn_t gt911_interrupt_handler(FAR void *arg){

      printf("fisker,interrupt_handler call !\n");

  int ret = 0;
  FAR struct gt911_touch_dev_s *priv = (FAR struct gt911_touch_dev_s *)arg;
  DEBUGASSERT(priv->work.worker == NULL);

  ret = work_queue(HPWORK, &priv->work, gt911_worker, priv, 0);
  if (ret != 0)
    {
      ierr("ERROR: Failed to queue work: %d\n", ret);
      return HAL_IRQ_ERR;
    }
  return HAL_IRQ_OK;
}

#endif
/*
int gt911_register(FAR struct i2c_master_s *dev){

  FAR struct gt911_touch_dev_s *priv = &g_gt911_touch;
  iinfo("GT911: mount i2c_master_s instance!\n");
  priv->i2c = dev;
  priv->is_registered = true;
  return OK;
}
int gt911_unregister(void){

  FAR struct gt911_touch_dev_s *priv = &g_gt911_touch;
  priv->is_registered = false;
  priv->i2c = NULL;
  return OK;
}
*/
int gt911_register(FAR const char *devpath,FAR struct i2c_master_s *dev)
{
  FAR struct gt911_touch_dev_s *priv = &g_gt911_touch;
  int ret;

  if(dev == NULL){
    ierr("GT911: ERROR: i2c_master_s instance is NULL\n");
    return ERROR_REGISTERED;
  }
  priv->i2c = dev;

  if(gt911_control_initialize()){
    ierr("GT911: ERROR: Failed to initialize control\n");
    return ERROR_CONTROL;
    }

#ifndef  GT911_TOUCH_POLLMODE
  hal_gpio_to_irq(INT, &priv->irq);
  hal_gpio_irq_request(priv->irq, gt911_interrupt_handler, IRQ_TYPE_EDGE_RISING, priv);
  nxsem_init(&priv->waitsem, 0, 0);
#endif
  ret = touch_register(&priv->lower, devpath, GT911_MAX_TOUCH_POINTS);
  if (ret < 0)
    {
      ierr("GT911: ERROR: Failed to register touch driver: %d\n", ret);
      goto errout;
    }
  iinfo("GT911: IIC touchscreen successfully initialized and registered\n");
#ifdef GT911_TOUCH_POLLMODE
  work_queue(HPWORK, &priv->work, gt911_worker, priv, priv->poll_interval);
#else
  hal_gpio_irq_enable(priv->irq);
#endif
  return OK;

errout:
  nxsem_destroy(&priv->waitsem);
  return ERROR_REGISTER;
}

void gt911_unregister(FAR const char *devpath){

  FAR struct gt911_touch_dev_s *priv = &g_gt911_touch;
#ifndef GT911_TOUCH_POLLMODE
  hal_gpio_irq_disable(priv->irq);
  hal_gpio_irq_free(priv->irq);
#endif
  touch_unregister(&priv->lower, devpath);
  priv->i2c = NULL;
}