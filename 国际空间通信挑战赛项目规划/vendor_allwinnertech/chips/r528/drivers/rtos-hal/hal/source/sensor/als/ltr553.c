/****************************************************************************
 * vendor/allwinnertech/chips/r528/drivers/rtos-hal/hal/source/sensor/als/ltr553.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#ifdef CONFIG_SENSORS_LTR553

#include <debug.h>
#include <errno.h>
#include <nuttx/clock.h>
#include <nuttx/fs/fs.h>
#include <nuttx/i2c/i2c_master.h>
#include <nuttx/kmalloc.h>
#include <nuttx/kthread.h>
#include <nuttx/nuttx.h>
#include <nuttx/sensors/sensor.h>
#include <nuttx/signal.h>
#include <nuttx/wqueue.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifndef CONFIG_LTR553_I2C_FREQUENCY
#define CONFIG_LTR553_I2C_FREQUENCY 400000
#endif

#ifndef CONFIG_SENSORS_LTR553_POLL_INTERVAL
#define CONFIG_SENSORS_LTR553_POLL_INTERVAL 500000
#endif

#ifndef CONFIG_SENSORS_LTR553_THREAD_STACKSIZE
#define CONFIG_SENSORS_LTR553_THREAD_STACKSIZE 2048
#endif

/* LTR553 I2C地址 */
#define LTR553_I2C_ADDR 0x23

/* LTR553 设备ID */
#define LTR553_PART_ID 0x92

/* LTR553 寄存器地址 */
#define LTR553_ALS_CONTR 0x80      /* ALS控制寄存器 */
#define LTR553_PS_CONTR 0x81       /* PS控制寄存器 */
#define LTR553_PS_LED 0x82         /* PS LED寄存器 */
#define LTR553_PS_N_PULSES 0x83    /* PS脉冲数寄存器 */
#define LTR553_PS_MEAS_RATE 0x84   /* PS测量速率 */
#define LTR553_ALS_MEAS_RATE 0x85  /* ALS测量速率 */
#define LTR553_PART_ID_REG 0x86    /* 设备ID寄存器 */
#define LTR553_MANUFAC_ID 0x87     /* 制造商ID */
#define LTR553_ALS_DATA_CH1_0 0x88 /* ALS CH1 Low */
#define LTR553_ALS_DATA_CH1_1 0x89 /* ALS CH1 High */
#define LTR553_ALS_DATA_CH0_0 0x8A /* ALS CH0 Low */
#define LTR553_ALS_DATA_CH0_1 0x8B /* ALS CH0 High */
#define LTR553_ALS_PS_STATUS 0x8C  /* ALS和PS状态寄存器 */
#define LTR553_PS_DATA_0 0x8D      /* PS Data Low */
#define LTR553_PS_DATA_1 0x8E      /* PS Data High */

/* ALS控制寄存器位定义 */
#define LTR553_ALS_GAIN_1X (0 << 2)
#define LTR553_ALS_GAIN_2X (1 << 2)
#define LTR553_ALS_GAIN_4X (2 << 2)
#define LTR553_ALS_GAIN_8X (3 << 2)
#define LTR553_ALS_GAIN_48X (6 << 2)
#define LTR553_ALS_GAIN_96X (7 << 2)

#define LTR553_ALS_MODE_ACTIVE (1 << 0)
#define LTR553_ALS_MODE_STANDBY (0 << 0)

/* ALS测量速率 */
#define LTR553_ALS_INTEG_50MS (1 << 3)
#define LTR553_ALS_INTEG_100MS (0 << 3)
#define LTR553_ALS_INTEG_200MS (2 << 3)
#define LTR553_ALS_INTEG_400MS (3 << 3)
#define LTR553_ALS_INTEG_150MS (4 << 3)
#define LTR553_ALS_INTEG_250MS (5 << 3)
#define LTR553_ALS_INTEG_300MS (6 << 3)
#define LTR553_ALS_INTEG_350MS (7 << 3)

#define LTR553_ALS_RATE_1000MS (4 << 0)
#define LTR553_ALS_RATE_500MS (3 << 0)
#define LTR553_ALS_RATE_100MS (2 << 0)
#define LTR553_ALS_RATE_200MS (1 << 0)
#define LTR553_ALS_RATE_50MS (0 << 0)

#define LTR553_PS_RATE_2000MS (6 << 0)
#define LTR553_PS_RATE_1000MS (5 << 0)
#define LTR553_PS_RATE_500MS (4 << 0)
#define LTR553_PS_RATE_200MS (3 << 0)
#define LTR553_PS_RATE_100MS (2 << 0)
#define LTR553_PS_RATE_70MS (1 << 0)
#define LTR553_PS_RATE_50MS (0 << 0)
#define LTR553_PS_RATE_10MS (8 << 0)

/* PS控制寄存器位定义 */
#define LTR553_PS_GAIN_16X (0 << 2)
#define LTR553_PS_GAIN_32X (2 << 2)
#define LTR553_PS_GAIN_64X (3 << 2)

#define LTR553_PS_MODE_ACTIVE (3 << 0)
#define LTR553_PS_MODE_STANDBY (1 << 0)
#define LTR553_PS_ACTIVE_16X 0x03
/* 传感器类型 */
#define LTR553_SENSOR_ALS 0
#define LTR553_SENSOR_PS 1
#define LTR553_SENSOR_MAX 2

/****************************************************************************
 * Private Type Definitions
 ****************************************************************************/

struct ltr553_sensor_s {
  struct sensor_lowerhalf_s lower;
  int type;
  bool enabled;
};

struct ltr553_dev_s {
  /* 传感器数组 */
  struct ltr553_sensor_s sensors[LTR553_SENSOR_MAX];

  /* I2C设备信息 */
  FAR struct i2c_master_s *i2c;
  uint8_t addr;
  uint32_t freq;

  /* 传感器状态 */
  bool sleeping;
  mutex_t dev_lock;
  sem_t run;
  bool enabled;

  /* 最近一次读取的数据 */
  float lux;
  float proximity;

  /* 传感器配置 */
  uint8_t als_gain;
  uint8_t als_integration_time;
  uint8_t als_measurement_rate;
  uint8_t ps_gain;
  uint8_t ps_led_current;
  uint8_t ps_pulses;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int ltr553_set_reg8(FAR struct ltr553_dev_s *priv, uint8_t regaddr,
                           uint8_t regval);
static int ltr553_get_reg8(FAR struct ltr553_dev_s *priv, uint8_t regaddr,
                           FAR uint8_t *regval);
static int ltr553_get_reg16(FAR struct ltr553_dev_s *priv, uint8_t regaddr,
                            FAR uint16_t *val);
static int ltr553_checkid(FAR struct ltr553_dev_s *priv);
static int ltr553_activate(FAR struct sensor_lowerhalf_s *lower,
                           FAR struct file *filep, bool enabled);
static int ltr553_fetch(FAR struct sensor_lowerhalf_s *lower,
                        FAR struct file *filep, FAR char *buffer,
                        size_t buflen);

/****************************************************************************
 * Private Data
 ****************************************************************************/
static FAR struct ltr553_dev_s *g_ltr553_dev = NULL;
static const struct sensor_ops_s g_ltr553_als_ops = {
    NULL,            /* open */
    NULL,            /* close */
    ltr553_activate, /* activate */
    NULL,            /* set_interval */
    NULL,            /* batch */
    ltr553_fetch,    /* fetch */
    NULL,            /* flush */
    NULL,            /* selftest */
    NULL,            /* set_calibvalue */
    NULL,            /* calibrate */
    NULL,            /* get_info */
    NULL             /* control */
};

static const struct sensor_ops_s g_ltr553_ps_ops = {
    NULL,            /* open */
    NULL,            /* close */
    ltr553_activate, /* activate */
    NULL,            /* set_interval */
    NULL,            /* batch */
    ltr553_fetch,    /* fetch */
    NULL,            /* flush */
    NULL,            /* selftest */
    NULL,            /* set_calibvalue */
    NULL,            /* calibrate */
    NULL,            /* get_info */
    NULL             /* control */
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: ltr553_set_reg8
 ****************************************************************************/

static int ltr553_set_reg8(FAR struct ltr553_dev_s *priv, uint8_t regaddr,
                           uint8_t regval) {
  struct i2c_msg_s msg;
  uint8_t txbuffer[2];
  int ret;

  txbuffer[0] = regaddr;
  txbuffer[1] = regval;

  msg.frequency = priv->freq;
  msg.addr = priv->addr;
  msg.flags = 0;
  msg.buffer = txbuffer;
  msg.length = 2;

  ret = I2C_TRANSFER(priv->i2c, &msg, 1);
  if (ret < 0) {
    snerr("I2C_TRANSFER failed (err = %d)\n", ret);
  }

  return ret;
}

/****************************************************************************
 * Name: ltr553_get_reg8
 ****************************************************************************/

static int ltr553_get_reg8(FAR struct ltr553_dev_s *priv, uint8_t regaddr,
                           FAR uint8_t *regval) {
  struct i2c_msg_s msg[2];
  int ret;

  msg[0].frequency = priv->freq;
  msg[0].addr = priv->addr;
  msg[0].flags = 0;
  msg[0].buffer = &regaddr;
  msg[0].length = 1;

  msg[1].frequency = priv->freq;
  msg[1].addr = priv->addr;
  msg[1].flags = I2C_M_READ;
  msg[1].buffer = regval;
  msg[1].length = 1;

  ret = I2C_TRANSFER(priv->i2c, msg, 2);
  if (ret < 0) {
    snerr("ERROR: I2C_TRANSFER failed (err = %d)\n", ret);
  }

  return ret;
}

/****************************************************************************
 * Name: ltr553_get_reg16
 ****************************************************************************/

static int ltr553_get_reg16(FAR struct ltr553_dev_s *priv, uint8_t regaddr,
                            FAR uint16_t *val) {
  uint8_t buffer[2];
  int ret;

  ret = ltr553_get_reg8(priv, regaddr, &buffer[0]);
  if (ret < 0) {
    return ret;
  }

  ret = ltr553_get_reg8(priv, regaddr + 1, &buffer[1]);
  if (ret < 0) {
    return ret;
  }

  *val = ((uint16_t)buffer[1] << 8) | buffer[0];
  return ret;
}

/****************************************************************************
 * Name: ltr553_checkid
 ****************************************************************************/

static int ltr553_checkid(FAR struct ltr553_dev_s *priv) {
  uint8_t devid;
  int ret;

  ret = ltr553_get_reg8(priv, LTR553_PART_ID_REG, &devid);
  if (ret < 0) {
    return ret;
  }

  sninfo("devid: 0x%02x\n", devid);
  return (devid != LTR553_PART_ID) ? -ENODEV : OK;
}

/****************************************************************************
 * Name: ltr553_thread
 ****************************************************************************/

static int ltr553_thread(int argc, char **argv) {
  FAR struct ltr553_dev_s *priv;
  struct sensor_light light;
  struct sensor_prox prox;
  uint16_t ch0_data = 0, ch1_data = 0, ps_data = 0;
  uint8_t status = 0;
  int ret;

  priv = g_ltr553_dev;

  if (priv == NULL || priv->i2c == NULL) {
    return -EINVAL;
  }

  while (true) {
    if (!priv->enabled) {
      ret = nxsem_wait(&priv->run);
      if (ret < 0) {
        break;
      }
    }

    if (g_ltr553_dev == NULL || !priv->enabled) {
      continue;
    }

    /* 读取组合状态寄存器 */
    ret = ltr553_get_reg8(priv, LTR553_ALS_PS_STATUS, &status);
    if (ret < 0) {
      continue;
    }

    /* 读取ALS数据 */
    if (priv->sensors[LTR553_SENSOR_ALS].enabled) {
      /* 始终读取ALS数据*/
      ret = ltr553_get_reg16(priv, LTR553_ALS_DATA_CH0_0, &ch0_data);
      if (ret >= 0) {
        ret = ltr553_get_reg16(priv, LTR553_ALS_DATA_CH1_0, &ch1_data);
        if (ret >= 0) {
          /* 强制计算勒克斯值 */
          if (ch0_data > 0) {
            float ratio = (float)ch1_data / (float)ch0_data;
            if (ratio < 0.45f) {
              priv->lux = (1.7743f * ch0_data) + (1.1059f * ch1_data);
            } else if (ratio < 0.64f) {
              priv->lux = (4.2785f * ch0_data) - (1.9548f * ch1_data);
            } else if (ratio < 0.85f) {
              priv->lux = (0.5926f * ch0_data) - (0.1185f * ch1_data);
            } else {
              priv->lux = 0.0f;
            }

            if (priv->lux < 0) {
              priv->lux = 0.0f;
            }
          } else {
            priv->lux = 0.0f;
          }
        }
      }

      /* 推送ALS数据 */
      light.timestamp = sensor_get_timestamp();
      light.light = priv->lux;

      if (priv->sensors[LTR553_SENSOR_ALS].lower.push_event != NULL) {
        priv->sensors[LTR553_SENSOR_ALS].lower.push_event(
            priv->sensors[LTR553_SENSOR_ALS].lower.priv, &light,
            sizeof(struct sensor_light));

        /* 增强显示 - 显示计算过程 */
        int lux_int = (int)priv->lux;
        int lux_frac = (int)((priv->lux - lux_int) * 100 + 0.5f);
        sninfo("LTR553: ALS : %d.%02d lux (CH0=%d, CH1=%d)\n", lux_int, lux_frac,
              ch0_data, ch1_data);
      }
    }

    /* 读取PS数据 */
    if (priv->sensors[LTR553_SENSOR_PS].enabled) {
      ret = ltr553_get_reg16(priv, LTR553_PS_DATA_0, &ps_data);
      if (ret >= 0) {
        /* 计算距离 */
        if (ps_data > 1000) {
          priv->proximity = 0.0f; /* 很近 */
        } else if (ps_data > 500) {
          priv->proximity = 1.0f; /* 近 */
        } else if (ps_data > 100) {
          priv->proximity = 5.0f; /* 中等距离 */
        } else if (ps_data > 10) {
          priv->proximity = 10.0f; /* 远 */
        } else {
          priv->proximity = 20.0f; /* 很远或无物体 */
        }
      } else {
        priv->proximity = -1.0f;
      }

      /* 推送PS数据 */
      prox.timestamp = sensor_get_timestamp();
      prox.proximity = priv->proximity;

      if (priv->sensors[LTR553_SENSOR_PS].lower.push_event != NULL) {
        priv->sensors[LTR553_SENSOR_PS].lower.push_event(
            priv->sensors[LTR553_SENSOR_PS].lower.priv, &prox,
            sizeof(struct sensor_prox));

        if (priv->proximity >= 0) {
          int prox_int = (int)priv->proximity;
          int prox_frac = (int)((priv->proximity - prox_int) * 100 + 0.5f);
          sninfo("LTR553: PS : %d.%02d cm (Raw=%d)\n", prox_int, prox_frac,
                ps_data);
        }
      }
    }

    /* 延时后继续轮询 */
    nxsig_usleep(CONFIG_SENSORS_LTR553_POLL_INTERVAL);
  }

  return OK;
}

/****************************************************************************
 * Name: ltr553_activate
 ****************************************************************************/

static int ltr553_activate(FAR struct sensor_lowerhalf_s *lower,
                           FAR struct file *filep, bool enabled) {
  FAR struct ltr553_sensor_s *sensor;
  FAR struct ltr553_dev_s *priv = NULL;
  int ret = OK;

  /* 基本检查保持不变 */
  if (lower == NULL) {
    return -EINVAL;
  }

  sensor = container_of(lower, FAR struct ltr553_sensor_s, lower);

  if (sensor == NULL) {
    return -EINVAL;
  }

  /* 获取设备指针 */
  if (sensor->type == LTR553_SENSOR_ALS) {
    priv = container_of(sensor, FAR struct ltr553_dev_s,
                        sensors[LTR553_SENSOR_ALS]);
  } else if (sensor->type == LTR553_SENSOR_PS) {
    priv = container_of(sensor, FAR struct ltr553_dev_s,
                        sensors[LTR553_SENSOR_PS]);
  } else {
    return -EINVAL;
  }

  if (priv == NULL || priv != g_ltr553_dev || priv->i2c == NULL) {
    return -EINVAL;
  }

  /* 配置传感器 */
  if (sensor->type == LTR553_SENSOR_ALS) {
    if (enabled) {
      /* ALS配置保持不变 */
      ret = ltr553_set_reg8(priv, LTR553_ALS_MEAS_RATE,
                            LTR553_ALS_INTEG_50MS | LTR553_ALS_RATE_50MS);
      if (ret >= 0) {
        ret = ltr553_set_reg8(priv, LTR553_ALS_CONTR,
                              LTR553_ALS_MODE_ACTIVE | LTR553_ALS_GAIN_1X);
        if (ret >= 0) {
          nxsig_usleep(100000);
          priv->lux = 0.0f;
        }
      }
    } else {
      ret = ltr553_set_reg8(priv, LTR553_ALS_CONTR, LTR553_ALS_MODE_STANDBY);
    }
  } else /* PS传感器配置 */
  {
    if (enabled) {
      /* 步骤1: 确保PS传感器处于待机状态 */
      ret = ltr553_set_reg8(priv, LTR553_PS_CONTR, LTR553_PS_MODE_STANDBY);
      if (ret >= 0) {
        nxsig_usleep(10000); /* 等待10ms */

        /* 步骤2: 设置PS LED参数 */
        ret =
            ltr553_set_reg8(priv, LTR553_PS_LED, 0x7F); /* 60kHz, 100%, 100mA */
        if (ret >= 0) {
          /* 步骤3: 设置PS脉冲数 */
          ret = ltr553_set_reg8(priv, LTR553_PS_N_PULSES, 0x0F); /* 15个脉冲 */
          if (ret >= 0) {
            /* 步骤4: 设置PS测量速率 */
            ret = ltr553_set_reg8(priv, LTR553_PS_MEAS_RATE,
                                  LTR553_PS_RATE_50MS); /* 50ms测量速率 */
            if (ret >= 0) {
              /* 步骤5: 启用PS传感器 */
              ret = ltr553_set_reg8(priv, LTR553_PS_CONTR,
                                    LTR553_PS_MODE_ACTIVE); /* 启用PS */
              if (ret >= 0) {
                nxsig_usleep(100000); /* 等待100ms稳定 */
                priv->proximity = -1.0f;
              }
            }
          }
        }
      }
    } else {
      ret = ltr553_set_reg8(priv, LTR553_PS_CONTR, LTR553_PS_MODE_STANDBY);
    }
  }

  if (ret < 0) {
    return ret;
  }

  sensor->enabled = enabled;

  /* 更新设备状态 */
  priv->enabled = priv->sensors[LTR553_SENSOR_ALS].enabled ||
                  priv->sensors[LTR553_SENSOR_PS].enabled;

  if (enabled == true) {
    /* 唤醒轮询线程 */
    nxsem_post(&priv->run);
  }

  return OK;
}

/****************************************************************************
 * 名称: ltr553_fetch
 ****************************************************************************/
static int ltr553_fetch(FAR struct sensor_lowerhalf_s *lower,
                        FAR struct file *filep, FAR char *buffer,
                        size_t buflen) {
  FAR struct ltr553_sensor_s *sensor =
      container_of(lower, struct ltr553_sensor_s, lower);
  FAR struct ltr553_dev_s *priv = NULL;
  uint16_t ch0_data = 0, ch1_data = 0, proximity_data = 0;
  uint8_t status = 0;
  int ret;
  uint64_t timestamp;

  if (!buffer || buflen == 0) {
    return -EINVAL;
  }
  sninfo("%s:%d %d", __func__, __LINE__, sensor->type);
  /* 获取设备指针 */
  if (sensor->type == LTR553_SENSOR_ALS) {
    priv = container_of(sensor, FAR struct ltr553_dev_s,
                        sensors[LTR553_SENSOR_ALS]);
  } else if (sensor->type == LTR553_SENSOR_PS) {
    priv = container_of(sensor, FAR struct ltr553_dev_s,
                        sensors[LTR553_SENSOR_PS]);
  } else {
    return -EINVAL;
  }

  if (priv == NULL || priv != g_ltr553_dev || priv->i2c == NULL) {
    return -EINVAL;
  }

  timestamp = sensor_get_timestamp();

  /* 读取组合状态寄存器 */
  ret = ltr553_get_reg8(priv, LTR553_ALS_PS_STATUS, &status);
  if (ret < 0) {
    return ret;
  }

  /* 读取ALS数据 */
  if (sensor->type == LTR553_SENSOR_ALS) {
    if (priv->sensors[LTR553_SENSOR_ALS].enabled) {
      /* 始终读取ALS数据*/
      ret = ltr553_get_reg16(priv, LTR553_ALS_DATA_CH0_0, &ch0_data);
      if (ret >= 0) {
        ret = ltr553_get_reg16(priv, LTR553_ALS_DATA_CH1_0, &ch1_data);
        if (ret >= 0) {
          /* 强制计算勒克斯值 */
          if (ch0_data > 0) {
            float ratio = (float)ch1_data / (float)ch0_data;
            if (ratio < 0.45f) {
              priv->lux = (1.7743f * ch0_data) + (1.1059f * ch1_data);
            } else if (ratio < 0.64f) {
              priv->lux = (4.2785f * ch0_data) - (1.9548f * ch1_data);
            } else if (ratio < 0.85f) {
              priv->lux = (0.5926f * ch0_data) - (0.1185f * ch1_data);
            } else {
              priv->lux = 0.0f;
            }

            if (priv->lux < 0) {
              priv->lux = 0.0f;
            }
          } else {
            priv->lux = 0.0f;
          }
        }
      }

      if (buflen < sizeof(struct sensor_light)) {
        return -ENOMEM;
      }

      struct sensor_light *als_data = (struct sensor_light *)buffer;
      als_data->timestamp = timestamp;
      als_data->light = priv->lux;
      sninfo("%s:%d %d %llu %.02f", __func__, __LINE__, sensor->type, timestamp,
            als_data->light);
      return sizeof(struct sensor_light);
    }
  }

  /* 读取PS数据 */
  if (sensor->type == LTR553_SENSOR_PS) {
    if (priv->sensors[LTR553_SENSOR_PS].enabled) {
      ret = ltr553_get_reg16(priv, LTR553_PS_DATA_0, &proximity_data);
      if (ret >= 0) {
        /* 计算距离 */
        if (proximity_data > 1000) {
          priv->proximity = 0.0f; /* 很近 */
        } else if (proximity_data > 500) {
          priv->proximity = 1.0f; /* 近 */
        } else if (proximity_data > 100) {
          priv->proximity = 5.0f; /* 中等距离 */
        } else if (proximity_data > 10) {
          priv->proximity = 10.0f; /* 远 */
        } else {
          priv->proximity = 20.0f; /* 很远或无物体 */
        }
      } else {
        priv->proximity = -1.0f;
      }

      if (buflen < sizeof(struct sensor_prox)) {
        return -ENOMEM;
      }

      struct sensor_prox *ps_data = (struct sensor_prox *)buffer;
      ps_data->timestamp = timestamp;
      ps_data->proximity = priv->proximity;
      sninfo("%s:%d %d %llu %.02f", __func__, __LINE__, sensor->type, timestamp,
            ps_data->proximity);
      return sizeof(struct sensor_prox);
    }
  }

  return -EINVAL;
}

/****************************************************************************
 * Name: ltr553_register
 ****************************************************************************/

int ltr553_register(int devno, FAR struct i2c_master_s *i2c) {
  FAR struct ltr553_dev_s *priv;
  int ret = OK;

  DEBUGASSERT(i2c != NULL);

  /* Initialize the LTR553 device structure */
  priv = kmm_zalloc(sizeof(struct ltr553_dev_s));
  if (priv == NULL) {
    return -ENOMEM;
  }

  /* 设置全局指针 */
  g_ltr553_dev = priv;

  priv->i2c = i2c;
  priv->addr = LTR553_I2C_ADDR;
  priv->freq = CONFIG_LTR553_I2C_FREQUENCY;
  priv->enabled = false;
  priv->sleeping = true;

  /* 设置默认配置 */
  priv->als_gain = LTR553_ALS_GAIN_1X;
  priv->als_integration_time = LTR553_ALS_INTEG_100MS;
  priv->als_measurement_rate = LTR553_ALS_RATE_100MS;
  priv->ps_gain = LTR553_PS_GAIN_16X;
  priv->ps_led_current = 0x7F;
  priv->ps_pulses = 1;

  nxmutex_init(&priv->dev_lock);
  nxsem_init(&priv->run, 0, 0);

  /* Check Device ID */
  ret = ltr553_checkid(priv);
  if (ret < 0) {
    goto err_init;
  }

  /* 软件复位传感器 */
  ret = ltr553_set_reg8(priv, LTR553_ALS_CONTR, 0x02); /* 软件复位位 */
  if (ret >= 0) {
    nxsig_usleep(10000);                                 /* 等待10ms */
    ret = ltr553_set_reg8(priv, LTR553_ALS_CONTR, 0x00); /* 清除复位位 */
  }

  if (ret < 0) {
    goto err_init;
  }

  /* 初始化ALS传感器 */
  priv->sensors[LTR553_SENSOR_ALS].type = LTR553_SENSOR_ALS;
  priv->sensors[LTR553_SENSOR_ALS].enabled = false;
  priv->sensors[LTR553_SENSOR_ALS].lower.ops = &g_ltr553_als_ops;
  priv->sensors[LTR553_SENSOR_ALS].lower.type = SENSOR_TYPE_LIGHT;

  ret = sensor_register(&priv->sensors[LTR553_SENSOR_ALS].lower, devno);
  if (ret < 0) {
    goto err_init;
  }

  /* 初始化PS传感器 */
  priv->sensors[LTR553_SENSOR_PS].type = LTR553_SENSOR_PS;
  priv->sensors[LTR553_SENSOR_PS].enabled = false;
  priv->sensors[LTR553_SENSOR_PS].lower.ops = &g_ltr553_ps_ops;
  priv->sensors[LTR553_SENSOR_PS].lower.type = SENSOR_TYPE_PROXIMITY;

  ret = sensor_register(&priv->sensors[LTR553_SENSOR_PS].lower, devno);
  if (ret < 0) {
    goto err_register;
  }

  /* 创建轮询线程 */
  ret = kthread_create("ltr553_thread", SCHED_PRIORITY_DEFAULT,
                       CONFIG_SENSORS_LTR553_THREAD_STACKSIZE, ltr553_thread,
                       NULL);
  if (ret < 0) {
    goto err_register_ps;
  }

  return OK;

err_register_ps:
  sensor_unregister(&priv->sensors[LTR553_SENSOR_PS].lower, devno);
err_register:
  sensor_unregister(&priv->sensors[LTR553_SENSOR_ALS].lower, devno);
err_init:
  g_ltr553_dev = NULL;
  nxsem_destroy(&priv->run);
  nxmutex_destroy(&priv->dev_lock);
  kmm_free(priv);
  return ret;
}

#endif /* CONFIG_SENSORS_LTR553 */