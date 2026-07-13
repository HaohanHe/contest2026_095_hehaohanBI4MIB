

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <assert.h>
#include <errno.h>
#include <debug.h>
#include <stdio.h>
#include <stdlib.h>

#include <nuttx/kmalloc.h>
#include <nuttx/kthread.h>
#include <nuttx/signal.h>
#include <nuttx/mutex.h>
#include <nuttx/fs/fs.h>
#include <nuttx/i2c/i2c_master.h>
#include <nuttx/sensors/sensor.h>
#include <sunxi_hal_twi.h>

#if defined(CONFIG_I2C) && defined(CONFIG_SENSORS_HX3203)

#define HX3203_IIC_ADDRESS 0x44
#define CONFIG_SENSORS_HX3203_POLL_INTERVAL 100 * 1000

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
#define I2C_BUS_FREQ_HZ 400000

#define HX3203_SENSOR_MAX 1

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct hx3203_sensor_data_s {
  uint32_t data;
  uint64_t timestamp;
};

#define CONFIG_SENSORS_HX3203_POLL

struct hx3203_dev_s;
struct hx3203_sensor_s {
  struct sensor_lowerhalf_s lower; /* Common lower interface */
  unsigned int buffer_size;        /* Data size for reading */
#ifdef CONFIG_SENSORS_HX3203_POLL
  bool enabled; /* Sensor activated */
#endif
  FAR struct hx3203_dev_s *dev; /* Driver instance */
};

struct hx3203_dev_s {
  struct hx3203_sensor_s sensor[HX3203_SENSOR_MAX]; /* Sensor types */
  FAR struct i2c_master_s *i2c;                     /* I2C interface */
  //FAR struct hx3203_bus_s *bus;                     /* Bus power interface */
  //mutex_t lock_measure_cycle;                       /* Locks measure cycle */
  uint32_t freq; /* I2C Frequency */
  //int twi_id;
#ifdef CONFIG_SENSORS_HX3203_POLL
  unsigned long interval; /* Polling interval */
  sem_t run;              /* Locks sensor thread */
#endif
  bool activated;
  uint8_t addr; /* I2C address */
  bool chip_valid;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/* Sensor functions */

static int hx3203_active(FAR struct sensor_lowerhalf_s *lower, FAR struct file *filep, bool enabled);

// static int hx3203_fetch(FAR struct sensor_lowerhalf_s *lower, FAR struct file *filep, FAR char *buffer, size_t
// buflen);

static int hx3203_control(FAR struct sensor_lowerhalf_s *lower, FAR struct file *filep, int cmd, unsigned long arg);

#ifdef CONFIG_SENSORS_HX3203_POLL
static int hx3203_set_interval(FAR struct sensor_lowerhalf_s *lower, FAR struct file *filep,
                               FAR unsigned long *period_us);
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct sensor_ops_s g_hx3203_ops = {
#ifdef CONFIG_SENSORS_HX3203_POLL
    .set_interval = hx3203_set_interval,
#endif
    .activate = hx3203_active,
    // .fetch = hx3203_fetch,
    .control = hx3203_control
};

static unsigned long hx3203_curtime(void) {
  struct timespec ts;

  clock_systime_timespec(&ts);
  return 1000000ull * ts.tv_sec + ts.tv_nsec / 1000;
}

/****************************************************************************
 * Name: hx3203_i2c_write
 *
 * Description:
 *   Send data on I2C.
 *
 * Input Parameters:
 *   dev     - Device-specific state data
 *   config  - Described the I2C configuration
 *   regaddr - A pointer to the device reg
 *   value   - The reg value
 *
 * Returned Value:
 *   0: success, <0: A negated errno
 *
 ****************************************************************************/
#if 0
static int hx3203_i2c_write(FAR struct hx3203_dev_s *data, uint8_t *writebuf, int writelen)
{
	int ret = 0;
	uint8_t i2c_port = data->twi_id;
	struct twi_msg msgs;

	if (writelen > 0) {
		msgs.addr = data->addr;
		msgs.flags = 0;
		msgs.len = writelen;
		msgs.buf = writebuf;

		ret = hal_twi_xfer(i2c_port, &msgs, 1);
		if (ret < 0) {
			lederr("[IIC]: i2c_transfer(write) error, ret=%d!!\n",
				 ret);
		}
	}

	return ret;
}
#endif
static int hx3203_i2c_write(FAR struct i2c_master_s *dev, FAR const struct i2c_config_s *config, uint8_t regaddr,
                            uint8_t value) {
  //struct i2c_msg_s msg[2];
  struct i2c_msg_s *msg;
  int ret;

  msg = kmm_malloc(sizeof(struct i2c_msg_s) * 2);
  if (msg == NULL) {
	  snerr("ERROR: Failed to allocate instance\n");
	  return -ENOMEM;
  }

  /* Setup for the transfer */

  msg[0].frequency = config->frequency;
  msg[0].addr = config->address;
  msg[0].flags = I2C_M_NOSTOP;
  msg[0].buffer = &regaddr;
  msg[0].length = 1;

  msg[1].frequency = config->frequency;
  msg[1].addr = config->address;
  msg[1].flags = I2C_M_NOSTART;
  msg[1].buffer = &value;
  msg[1].length = 1;

  /* Then perform the transfer. */

  ret = I2C_TRANSFER(dev, msg, 2);
  kmm_free(msg);
  return (ret >= 0) ? OK : ret;
}
/****************************************************************************
 * Name: hx3203_i2c_read
 *
 * Description:
 *   Send data on I2C.
 *
 * Input Parameters:
 *   dev     - Device-specific state data
 *   config  - Described the I2C configuration
 *   regaddr - A pointer to the device reg
 *   value   - The reg value
 *
 * Returned Value:
 *   0: success, <0: A negated errno
 *
 ****************************************************************************/
 #if 0
static int hx3203_i2c_read(FAR struct hx3203_dev_s *data, uint8_t *writebuf, int writelen,
			uint8_t *readbuf, int readlen)
{
	int ret = 0;
	struct twi_msg msgs[2];
	uint8_t i2c_port = data->twi_id;
	uint16_t flags = 0;

	if (readlen > 0) {
		if (writelen > 0) {
			msgs[0].addr = data->addr;
			msgs[0].flags = flags;
			msgs[0].flags &= ~(TWI_M_RD);
			msgs[0].len = writelen;
			msgs[0].buf = writebuf;

			msgs[1].addr = data->addr;
			msgs[1].flags = flags;
			msgs[1].flags |= TWI_M_RD;
			msgs[1].len = readlen;
			msgs[1].buf = readbuf;

			ret = hal_twi_xfer(i2c_port, msgs, 1);
			if (ret < 0) {
				lederr
				    ("[IIC]: i2c_transfer(1) error, addr= 0x%02x%02x!!\n",
				     writebuf[0], writebuf[1]);
				lederr
				    ("[IIC]: i2c_transfer(1) error, ret=%d, rlen=%d, wlen=%d!!\n",
				     ret, readlen, writelen);
			} else {
				ret =
				    hal_twi_xfer(i2c_port, &msgs[1], 1);
				if (ret < 0) {
					lederr
					    ("[IIC]: i2c_transfer(2) error, addr= 0x%x!!\n",
					     writebuf[0]);
					lederr
					    ("[IIC]: i2c_transfer(2) error, ret=%d, rlen=%d, wlen=%d!!\n",
					     ret, readlen, writelen);
				}
			}
		} else {
			msgs[1].addr = data->addr;
			msgs[1].flags = TWI_M_RD;
			msgs[1].len = readlen;
			msgs[1].buf = readbuf;

			ret = hal_twi_xfer(i2c_port, &msgs[1], 1);
			if (ret < 0) {
				lederr
				    ("[IIC]: i2c_transfer(read) error, ret=%d, rlen=%d, wlen=%d!!",
				     ret, readlen, writelen);
			}
		}
	}

	return ret;
}
#endif
static int hx3203_i2c_read(FAR struct i2c_master_s *dev, FAR const struct i2c_config_s *config, uint8_t regaddr) {
 //struct i2c_msg_s msg[2];
  struct i2c_msg_s *msg;
  uint8_t data;

  msg = kmm_malloc(sizeof(struct i2c_msg_s) * 2);
  if (msg == NULL) {
	  snerr("ERROR: Failed to allocate instance\n");
	  return -ENOMEM;
  }

  /* Setup for the transfer */

  msg[0].frequency = config->frequency;
  msg[0].addr = config->address;
  msg[0].flags = I2C_M_NOSTOP;
  msg[0].buffer = &regaddr;
  msg[0].length = 1;

  msg[1].frequency = config->frequency;
  msg[1].addr = config->address;
  msg[1].flags = 0x0001;  // I2C_M_RD;
  msg[1].buffer = &data;
  msg[1].length = 1;

  /* Then perform the transfer dev addr and reg addr, read reg value */

  I2C_TRANSFER(dev, msg, 2);

  kmm_free(msg);
  return data;
}
/****************************************************************************
 * Name: hx3203_i2c_write_byte
 *
 * Description:
 *   Write a single byte to one of the hx3203 configuration registers.
 *
 ****************************************************************************/
#if 0
static int hx3203_i2c_write_byte(FAR struct hx3203_dev_s *priv, uint8_t reg_addr, uint8_t reg_val) {
  int ret = OK;
  uint8_t writebuf[2];

  writebuf[0] = reg_addr;
  writebuf[1] = reg_val;
  ledinfo("i2c addr: 0x%02X regaddr: 0x%02X value: 0x%02X\n", priv->addr, reg_addr, reg_val);

  ret = hx3203_i2c_write(priv, writebuf, 2);
  if (ret != OK) {
    lederr("ERROR: i2c_write returned error code %d\n", ret);
    return ret;
  }

  return OK;
}

static int hx3203_i2c_read_byte(FAR struct hx3203_dev_s *priv, uint8_t reg_addr) {
  uint8_t regvalue;

  hx3203_i2c_read(priv, &reg_addr, 1, &regvalue, 1);

  ledinfo("i2c addr: 0x%02X regaddr: 0x%02X data: 0x%02X\n", priv->addr, reg_addr, regvalue);

  return regvalue;
}
#endif
static int hx3203_i2c_write_byte(FAR struct hx3203_dev_s *priv, uint8_t const reg_addr, uint8_t const reg_val) {
  struct i2c_config_s config;
  int ret = OK;

  /* Setup up the I2C configuration */

  config.frequency = I2C_BUS_FREQ_HZ;
  config.address = priv->addr;
  config.addrlen = 7;

  ledinfo("i2c addr: 0x%02X regaddr: 0x%02X value: 0x%02X\n", priv->addr, reg_addr, reg_val);

  ret = hx3203_i2c_write(priv->i2c, &config, reg_addr, reg_val);
  if (ret != OK) {
    lederr("ERROR: i2c_write returned error code %d\n", ret);
    return ret;
  }

  return OK;
}

static int hx3203_i2c_read_byte(FAR struct hx3203_dev_s *priv, uint8_t const reg_addr) {
  struct i2c_config_s config;
  int data_buf;

  /* Setup up the I2C configuration */

  config.frequency = I2C_BUS_FREQ_HZ;
  config.address = priv->addr;
  config.addrlen = 7;
  data_buf = hx3203_i2c_read(priv->i2c, &config, reg_addr);

  ledinfo("i2c addr: 0x%02X regaddr: 0x%02X data: 0x%02X\n", priv->addr, reg_addr, data_buf);

  return data_buf;
}

static uint16_t hx3203_i2c_read_als(FAR struct hx3203_dev_s *priv) {
  uint8_t databuf[3];
  uint16_t ch0_data = 0;
  uint16_t ch1_data = 0;
  int16_t data = 0;

  databuf[0] = hx3203_i2c_read_byte(priv, 0x09);  // addr09, bit [10:3]
  databuf[1] = hx3203_i2c_read_byte(priv, 0x0a);  // addr0a, bit [17:11]
  databuf[2] = hx3203_i2c_read_byte(priv, 0x0f);  // addr0f, bit [2:0]
  ch0_data = ((databuf[0] << 8) | ((databuf[1] & 0x0F) << 4) | (databuf[2] & 0x0F));

  databuf[0] = hx3203_i2c_read_byte(priv, 0x08);  // addr09, bit [10:3]
  databuf[1] = hx3203_i2c_read_byte(priv, 0x0d);  // addr0a, bit [17:11]
  databuf[2] = hx3203_i2c_read_byte(priv, 0x0e);  // addr0f, bit [2:0]

  ch1_data = ((databuf[0] << 3) | ((databuf[1] & 0x3F) << 11) | (databuf[2] & 0x07));

  data = ch0_data - (uint32_t)ch1_data * 120 / 100;
  if ((ch0_data > 16380) || (ch1_data > 16380)) {
    data = 16384;
  }
  if (data < 0) {
    data = 0;
  }

  return data;
}

static uint32_t adc_lux(uint32_t adc) {
  struct tab_lux_s {
    uint32_t lux;

    uint32_t adc;
  } tab_lux[] = {
      {500, 1990}, {1000, 4025}, {1500, 6015}, {2000, 8060}, {2500, 10140}, {3500, 16384},
  };
  int i;
  uint32_t lux = 0;
  for (i = 0; i < sizeof(tab_lux) / sizeof(struct tab_lux_s); i++) {
    if (adc <= tab_lux[i].adc) {
      lux = tab_lux[i].lux * adc / tab_lux[i].adc;
      break;
    }
  }

  return lux;
}

/****************************************************************************
 * Name: hx3203_chip_init
 *
 * Description:
 *   This function init hx3203 chip
 *
 ****************************************************************************/

static bool hx3203_chip_init(FAR struct hx3203_dev_s *priv) {
  uint8_t id = 0;

  id = hx3203_i2c_read_byte(priv, 0x00);

  ledinfo(" >>> hx3203_chip_init done id = 0x%x \r\n", id);  // 0x21
  if (0x21 != id) {
    lederr("ERROR: hx3203_chip_init error code %d\n", id);
    return false;
  }

  hx3203_i2c_write_byte(priv, 0x01, 0x70);  // 0x01_bits<2>=als enble cfg , bit<6:4> cfg the wait time 111 means 0mS
  hx3203_i2c_write_byte(priv, 0x02, 0x06);  // interupt cfg , bits<3:0> als relate ,bit<3>= 1 ,int enable
  // todo disable interrupts

  /* upper threshold: 0x10,5:4;0x07,7:0;0x06,7:4;0x10,3:0 */
  /* lower threshold: 0x11,5:4;0x06,3:0;0x05,7:0;0x11,3:0 */
  // hx3203_i2c_write_byte(priv, 0x05, 0x00); // als th
  // hx3203_i2c_write_byte(priv, 0x06, 0x00); // als th
  // hx3203_i2c_write_byte(priv, 0x07, 0x18); // als th
  // hx3203_i2c_write_byte(priv, 0x10, 0x00); // als th
  // hx3203_i2c_write_byte(priv, 0x11, 0x00); // als th

  hx3203_i2c_write_byte(priv, 0x0c, 0x22);  // power dwon cfg , bit<5> = 1 normal work , bit<5>=0 , sleep mode
  hx3203_i2c_write_byte(priv, 0x16, 0x36);  // bit<3:0> ADC cfg , 0110 means 14bits , data convertion time 25mS
  hx3203_i2c_write_byte(priv, 0x0b, 0x02);     // bis<2:0> als gain cfg , 001 = 2X , 010 = 4X , 011 = 8X ,other 64X
  //hx3203_i2c_write_byte(priv, 0x0b, 0x0);  // bis<2:0> als gain cfg , 001 = 2X , 010 = 4X , 011 = 8X ,other 64X

  return true;
}

/****************************************************************************
 * Name: hx3203_chip_enable
 *
 * Description:
 *   This function enable hx3203 chip
 *
 ****************************************************************************/

static void hx3203_chip_enable(FAR struct hx3203_dev_s *priv) {
  hx3203_i2c_write_byte(priv, 0x7a, 0x04);
  hx3203_i2c_write_byte(priv, 0x01, 0x74);
  hx3203_i2c_write_byte(priv, 0x0c, 0x22);
  // hx3203_i2c_write_byte(priv, 0x02, 0x0e);      // int enable
}

/****************************************************************************
 * Name: hx3203_chip_disable
 *
 * Description:
 *   This function disable hx3203 chip
 *
 ****************************************************************************/

static void hx3203_chip_disable(FAR struct hx3203_dev_s *priv) { hx3203_i2c_write_byte(priv, 0x0c, 0x02); }

/****************************************************************************
 * Name: hx3203_control
 *
 * Description: Interface function of struct sensor_ops_s.
 *
 * Return:
 *   OK - on success
 ****************************************************************************/

static int hx3203_control(FAR struct sensor_lowerhalf_s *lower, FAR struct file *filep, int cmd, unsigned long arg) {
  int ret = 0;

  return ret;
}

/****************************************************************************
 * Name: hx3203_active
 *
 * Description: Interface function of struct sensor_ops_s.
 *
 * Return:
 *   OK - on success
 ****************************************************************************/

static int hx3203_active(FAR struct sensor_lowerhalf_s *lower, FAR struct file *filep, bool enabled) {
#ifdef CONFIG_SENSORS_HX3203_POLL
  bool start_thread = false;
  struct hx3203_sensor_s *priv = (FAR struct hx3203_sensor_s *)lower;
  if (enabled) {
    if (!priv->dev->sensor[0].enabled) {
      start_thread = true;
    }
  }

  priv->enabled = enabled;

  if (start_thread == true) {
    /* Wake up the thread */

    nxsem_post(&priv->dev->run);
  }
#endif

  return OK;
}

/****************************************************************************
 * Name: hx3203_set_interval
 *
 * Description: Interface function of struct sensor_ops_s.
 *
 * Return:
 *   OK - on success
 ****************************************************************************/

#ifdef CONFIG_SENSORS_HX3203_POLL
static int hx3203_set_interval(FAR struct sensor_lowerhalf_s *lower, FAR struct file *filep,
                               FAR unsigned long *period_us) {
  FAR struct hx3203_sensor_s *priv = (FAR struct hx3203_sensor_s *)lower;
  priv->dev->interval = *period_us;
  return OK;
}
#endif

/****************************************************************************
 * Name: hx3203_thread
 *
 * Description: Thread for performing interval measurement cycle and data
 *              read.
 *
 * Parameter:
 *   argc - Number opf arguments
 *   argv - Pointer to argument list
 ****************************************************************************/

#ifdef CONFIG_SENSORS_HX3203_POLL
static int hx3203_thread(int argc, char **argv) {
  FAR struct hx3203_dev_s *priv = (FAR struct hx3203_dev_s *)((uintptr_t)strtoul(argv[1], NULL, 0));

  while (true) {
    int ret;
    struct hx3203_sensor_s *hsensor = &priv->sensor[0];

    if (!hsensor->enabled) {
      /* Reset initial read identifier */

      /* Waiting to be woken up */

      ret = nxsem_wait(&priv->run);
      if (ret < 0) {
        continue;
      }
    }

    {
      if (priv->activated != hsensor->enabled) {
        // int r=nxmutex_lock(&priv->lock_measure_cycle);
        // if(r < 0){
        //   continue;
        // }
        if (hsensor->enabled) {
          if (priv->chip_valid) {
            hx3203_chip_init(priv);
            usleep(5 * 1000);
            hx3203_chip_enable(priv);
            usleep(5 * 1000);
          }

          priv->activated = true;
        } else {
          priv->activated = false;
          if (priv->chip_valid) {
            hx3203_chip_disable(priv);
            usleep(5 * 1000);
          }
        }
        // nxmutex_unlock(&priv->lock_measure_cycle);
      }

      /* Notify upper */

      if (hsensor->enabled) {
        struct sensor_light light;
        // int r=nxmutex_lock(&priv->lock_measure_cycle);
        // if(r<0){
        //   continue;
        // }
        uint32_t adc = 8060;
        if (priv->chip_valid) {
          adc = hx3203_i2c_read_als(priv);
        }
        // nxmutex_unlock(&priv->lock_measure_cycle);
        uint32_t lux = adc_lux(adc);
        light.timestamp = hx3203_curtime();
        /* ir value is invalied in 86-panel*/
        light.ir = 0;
        light.light = lux;

        hsensor->lower.push_event(hsensor->lower.priv, &light, sizeof(struct sensor_light));
      }
      /* Store the last sensor data for later comparison */
    }

    /* Sleeping thread before fetching the next sensor data */

    nxsig_usleep(priv->interval);
  }

  return OK;
}
#endif

extern FAR struct i2c_master_s *r528_i2c_initialize(FAR const char *devpath, int i2c_id);

int hx3203_register_uorb(FAR struct i2c_master_s *i2c) {
  int ret;
  struct hx3203_sensor_s *tmp;
#ifdef CONFIG_SENSORS_HX3203_POLL
  FAR char *argv[2];
  char arg1[32];
#endif
  uint8_t id = 0;

  int devno = 0;
  int found = 0;

  if (!i2c) {
    snerr("Invalid i2c instance\n");
    return -EINVAL;
  }

  FAR struct hx3203_dev_s *priv = (FAR struct hx3203_dev_s *)kmm_malloc(sizeof(struct hx3203_dev_s));

  if (priv == NULL) {
    snerr("ERROR: Failed to allocate instance\n");
    return -ENOMEM;
  }

  //priv->twi_id = 2;
  priv->i2c = i2c;
  priv->freq = I2C_BUS_FREQ_HZ;
  priv->addr = HX3203_IIC_ADDRESS;
  priv->activated = false;

#ifdef CONFIG_SENSORS_HX3203_POLL
  priv->interval = CONFIG_SENSORS_HX3203_POLL_INTERVAL;
#endif

  // nxmutex_init(&priv->lock_measure_cycle);
#ifdef CONFIG_SENSORS_HX3203_POLL
  nxsem_init(&priv->run, 0, 0);
#endif

  /* Humidity register */

  tmp = &priv->sensor[0];
  tmp->dev = priv;
#ifdef CONFIG_SENSORS_HX3203_POLL
  tmp->enabled = false;
#endif
  tmp->buffer_size = sizeof(struct sensor_light);
  tmp->lower.ops = &g_hx3203_ops;
  tmp->lower.type = SENSOR_TYPE_LIGHT;
  tmp->lower.uncalibrated = false;
  tmp->lower.nbuffer = 1;
  ret = sensor_register(&tmp->lower, devno);
  if (ret < 0) {
    goto humi_err;
  }

#ifdef CONFIG_SENSORS_HX3203_POLL
  /* Create thread for sensor */

  snprintf(arg1, 16, "0x%" PRIxPTR, (uintptr_t)priv);
  argv[0] = arg1;
  argv[1] = NULL;

  id = hx3203_i2c_read_byte(priv, 0x00);

  if (0x21 != id) {
    id = hx3203_i2c_read_byte(priv, 0x00);
    if (0x21 == id) {
      found = 1;
    }
  } else {
    found = 1;
  }
  if (found) {
    priv->chip_valid = 1;
  } else {
    priv->chip_valid = 0;
    _err("not found sensor hx3203!!!\n");
  }

  ret = kthread_create("hx3203_thread", SCHED_PRIORITY_DEFAULT, 8192, hx3203_thread, argv);
  if (ret > 0)
#endif
  {

    return OK;
  }

humi_err:
  sensor_unregister(&priv->sensor[0].lower, devno);

  // nxmutex_destroy(&priv->lock_measure_cycle);
  kmm_free(priv);
  return ret;
}

int hx3203_initialize_uorb(void) {
  FAR struct i2c_master_s *i2c;
#ifdef CONFIG_ARCH_BOARD_R528S3_X4B
  i2c = r528_i2c_initialize("/dev/i2c0", 0);
#else
  i2c = r528_i2c_initialize("/dev/i2c2", 2);
#endif

  hx3203_register_uorb(i2c);

  return 0;
}
#endif /* CONFIG_I2C && CONFIG_SENSORS_HX3203 */
