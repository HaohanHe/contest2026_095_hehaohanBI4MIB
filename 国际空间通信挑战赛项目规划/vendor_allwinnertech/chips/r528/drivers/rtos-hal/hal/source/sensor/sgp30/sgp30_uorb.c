/****************************************************************************
 * vendor/allwinnertech/chips/r528/drivers/rtos-hal/hal/source/sensor/sgp30_uorb.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#if defined(CONFIG_I2C) && defined(CONFIG_SENSORS_SGP30_UORB)

#include <debug.h>
#include <errno.h>
#include <fcntl.h>
#include <nuttx/clock.h>
#include <nuttx/fs/fs.h>
#include <nuttx/i2c/i2c_master.h>
#include <nuttx/kmalloc.h>
#include <nuttx/kthread.h>
#include <nuttx/nuttx.h>
#include <nuttx/sensors/sensor.h>
#include <nuttx/sensors/sgp30.h>
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

#ifndef CONFIG_SENSORS_SGP30_THREAD_STACKSIZE
#define CONFIG_SENSORS_SGP30_THREAD_STACKSIZE 2048
#endif

#ifndef CONFIG_SENSORS_SGP30_POLL_INTERVAL
#define CONFIG_SENSORS_SGP30_POLL_INTERVAL 1000000
#endif

#define SGP30_DEV_PATH  "/dev/sgp30"

#define SGP30_SENSOR_MAX 2

/* 传感器类型 */
#define SGP30_SENSOR_CO2    0
#define SGP30_SENSOR_TVOC   1
#define SGP30_SENSOR_MAX    2

/****************************************************************************
 * Private Type Definitions
 ****************************************************************************/

struct sgp30_sensor_s {
  struct sensor_lowerhalf_s lower;
  int type;
  bool enabled;
};

struct sgp30_dev_s {
  /* 传感器数组 */
  struct sgp30_sensor_s sensors[SGP30_SENSOR_MAX];
  struct file fd;

  /* 传感器状态 */
  bool sleeping;
  bool enabled;
  mutex_t dev_lock;
  sem_t run;

  int virtual_sensor;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int sgp30_checkid(FAR struct sgp30_dev_s *priv);
static int sgp30_activate(FAR struct sensor_lowerhalf_s *lower,
                           FAR struct file *filep, bool enabled);
static int sgp30_fetch(FAR struct sensor_lowerhalf_s *lower,
                         FAR struct file *filep, FAR char *buffer,
                         size_t buflen);

/****************************************************************************
 * Private Data
 ****************************************************************************/
static FAR struct sgp30_dev_s *g_sgp30_dev = NULL;
static const struct sensor_ops_s g_sgp30_co2_ops = {
    NULL,            /* open */
    NULL,            /* close */
    sgp30_activate,  /* activate */
    NULL,            /* set_interval */
    NULL,            /* batch */
    sgp30_fetch,     /* fetch */
    NULL,            /* flush */
    NULL,            /* selftest */
    NULL,            /* set_calibvalue */
    NULL,            /* calibrate */
    NULL,            /* get_info */
    NULL             /* control */
};

static const struct sensor_ops_s g_sgp30_tvoc_ops = {
    NULL,            /* open */
    NULL,            /* close */
    sgp30_activate,  /* activate */
    NULL,            /* set_interval */
    NULL,            /* batch */
    sgp30_fetch,     /* fetch */
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
 * Name: sgp30_checkid
 ****************************************************************************/

static int sgp30_checkid(FAR struct sgp30_dev_s *priv) {
  uint8_t devid = 0;
  sninfo("devid: 0x%02x\n", devid);
  return OK;
}

/****************************************************************************
 * Name: lsm9ds1_fetch
 ****************************************************************************/

static int sgp30_fetch(FAR struct sensor_lowerhalf_s *lower,
                         FAR struct file *filep, FAR char *buffer,
                         size_t buflen) {
  FAR struct sgp30_sensor_s *sensor;
  FAR struct sgp30_dev_s *priv = NULL;
  FAR struct sgp30_conv_data_s data;
  uint64_t timestamp;
  int ret = 0;

  sninfo("sgp30_fetch start!\n");

  if (!buffer || buflen == 0) {
    return -EINVAL;
  }

  /* 基本检查保持不变 */
  if (lower == NULL) {
    return -EINVAL;
  }

  sensor = container_of(lower, FAR struct sgp30_sensor_s, lower);

  /* 获取设备指针 */
  if (sensor->type == SGP30_SENSOR_CO2) {
    priv = container_of(sensor, FAR struct sgp30_dev_s,
                        sensors[SGP30_SENSOR_CO2]);
    sninfo("get SGP30_SENSOR_CO2 priv success!\n");
  } else if (sensor->type == SGP30_SENSOR_TVOC) {
    priv = container_of(sensor, FAR struct sgp30_dev_s,
                        sensors[SGP30_SENSOR_TVOC]);
    sninfo("get SGP30_SENSOR_TVOC priv success!\n");
  } else {
    snerr("get priv error!\n");
    return -EINVAL;
  }

  if (priv == NULL || priv != g_sgp30_dev) {
    return -EINVAL;
  }

  timestamp = sensor_get_timestamp();
  if (priv->virtual_sensor) {
    data.co2eq_ppm = 666;
    data.tvoc_ppb = 999;
  } else {
    ret = file_ioctl(&priv->fd, SNIOC_READ_CONVERT_DATA, &data);
  }

  if (ret < 0) {
    snerr("sgp30_fetch error!\n");
    return -ENODEV;
  }

  if (sensor->type == SGP30_SENSOR_CO2) {
    struct sensor_co2 *co2_data = (struct sensor_co2 *)buffer;
    co2_data->timestamp = timestamp;
    co2_data->co2 = data.co2eq_ppm;
  } else if (sensor->type == SGP30_SENSOR_TVOC) {
    struct sensor_tvoc *tvoc_data = (struct sensor_tvoc *)buffer;
    tvoc_data->timestamp = timestamp;
    tvoc_data->tvoc = data.tvoc_ppb;
  }

  return sensor->type == SGP30_SENSOR_CO2 ? sizeof(struct sensor_co2) : sizeof(struct sensor_tvoc);
}

/****************************************************************************
 * Name: sgp30_activate
 ****************************************************************************/

static int sgp30_activate(FAR struct sensor_lowerhalf_s *lower,
                           FAR struct file *filep, bool enabled) {
  FAR struct sgp30_sensor_s *sensor;
  FAR struct sgp30_dev_s *priv = NULL;
  int ret = OK;

  /* 基本检查保持不变 */
  if (lower == NULL) {
    return -EINVAL;
  }

  sensor = container_of(lower, FAR struct sgp30_sensor_s, lower);

  if (sensor == NULL) {
    return -EINVAL;
  }

  /* 获取设备指针 */
  if (sensor->type == SGP30_SENSOR_CO2) {
    priv = container_of(sensor, FAR struct sgp30_dev_s,
                        sensors[SGP30_SENSOR_CO2]);
  } else if (sensor->type == SGP30_SENSOR_TVOC) {
    priv = container_of(sensor, FAR struct sgp30_dev_s,
                        sensors[SGP30_SENSOR_TVOC]);
  } else {
    return -EINVAL;
  }

  if (priv == NULL || priv != g_sgp30_dev) {
    return -EINVAL;
  }

  /* 配置传感器 */
  if (enabled) {
    if (priv->enabled) {
      return OK;
    }

    if (ret < 0)
    {
      return -ENODEV;
    }
  } else {
    if (!priv->enabled) {
      return OK;
    }
  }

  sensor->enabled = enabled;

  /* 更新设备状态 */
  priv->enabled = priv->sensors[SGP30_SENSOR_CO2].enabled ||
                  priv->sensors[SGP30_SENSOR_TVOC].enabled;

  if (enabled == true) {
    /* 唤醒轮询线程 */
    nxsem_post(&priv->run);
  }

  return OK;
}

static int sgp30_thread(int argc, char **argv) {
  FAR struct sgp30_dev_s *priv;
  struct sensor_co2 co2_data = {};
  struct sensor_tvoc tvoc_data = {};
  FAR struct sgp30_conv_data_s data;
  uint64_t timestamp;
  int ret = 0;

  priv = g_sgp30_dev;

  if (priv == NULL) {
    return -EINVAL;
  }

  while (true) {
    if (!priv->enabled) {
      ret = nxsem_wait(&priv->run);
      if (ret < 0) {
        break;
      }
    }
    sninfo("sgp30 start!\n");
    if (g_sgp30_dev == NULL || !priv->enabled) {
      continue;
    }

    timestamp = sensor_get_timestamp();
    if (priv->virtual_sensor) {
      sninfo("virtual_sensor\n");
      data.co2eq_ppm = 400;
      data.tvoc_ppb = 0;
    } else {
      ret = file_ioctl(&priv->fd, SNIOC_READ_CONVERT_DATA, &data);
    }

    if (ret < 0) {
      snerr("sgp30_thread ioctrl error: %d try to reset\n", ret);
      file_ioctl(&priv->fd, SNIOC_RESET, 0);
      continue;
    }

    if (priv->sensors[SGP30_SENSOR_CO2].enabled) {
      co2_data.timestamp = timestamp;
      co2_data.co2 = data.co2eq_ppm;

      if (priv->sensors[SGP30_SENSOR_CO2].lower.push_event != NULL) {
        priv->sensors[SGP30_SENSOR_CO2].lower.push_event(
            priv->sensors[SGP30_SENSOR_CO2].lower.priv, &co2_data,
            sizeof(struct sensor_co2));
      }
    }

    if (priv->sensors[SGP30_SENSOR_TVOC].enabled) {
      tvoc_data.timestamp = timestamp;
      tvoc_data.tvoc =  data.tvoc_ppb;

      if (priv->sensors[SGP30_SENSOR_TVOC].lower.push_event != NULL) {
        priv->sensors[SGP30_SENSOR_TVOC].lower.push_event(
            priv->sensors[SGP30_SENSOR_TVOC].lower.priv, &tvoc_data,
            sizeof(struct sensor_tvoc));
      }
    }
    // syslog(LOG_ERR, "nxsig_usleep start!\n");
    /* 延时后继续轮询 */
    nxsig_usleep(CONFIG_SENSORS_SGP30_POLL_INTERVAL);
    // syslog(LOG_ERR, "nxsig_usleep end!\n");
  }

  return OK;
}

/****************************************************************************
 * Name: sgp30_register
 ****************************************************************************/

int sgp30_uorb_register(int devno, FAR struct i2c_master_s *i2c) {
  FAR struct sgp30_dev_s *priv;
  int ret = OK;

  DEBUGASSERT(i2c != NULL);

  /* Initialize the SGP30 device structure */
  priv = kmm_zalloc(sizeof(struct sgp30_dev_s));
  if (priv == NULL) {
    return -ENOMEM;
  }

  /* 设置全局指针 */
  g_sgp30_dev = priv;

  priv->enabled = false;
  priv->sleeping = true;
  priv->virtual_sensor = false;

  nxmutex_init(&priv->dev_lock);
  nxsem_init(&priv->run, 0, 0);

  /* Check Device ID */
  ret = sgp30_checkid(priv);
  if (ret < 0) {
    goto err_init;
  }

  if (!priv->virtual_sensor) {
    sgp30_register(SGP30_DEV_PATH, i2c, CONFIG_SGP30_ADDR);
  }

  if (!priv->virtual_sensor) {
    ret = file_open(&priv->fd, SGP30_DEV_PATH, O_RDWR | O_CLOEXEC);
    if (ret < 0) {
      snerr("sgp30 fopen failed!\n");
      goto err_init;
    }
  }

  /* 初始化CO2传感器 */
  priv->sensors[SGP30_SENSOR_CO2].type = SGP30_SENSOR_CO2;
  priv->sensors[SGP30_SENSOR_CO2].enabled = false;
  priv->sensors[SGP30_SENSOR_CO2].lower.ops = &g_sgp30_co2_ops;
  priv->sensors[SGP30_SENSOR_CO2].lower.type = SENSOR_TYPE_CO2;
  priv->sensors[SGP30_SENSOR_CO2].lower.nbuffer = 1;

  ret = sensor_register(&priv->sensors[SGP30_SENSOR_CO2].lower, devno);
  if (ret < 0) {
    goto err_init;
  }


  /* 初始化TVOC传感器 */
  priv->sensors[SGP30_SENSOR_TVOC].type = SGP30_SENSOR_TVOC;
  priv->sensors[SGP30_SENSOR_TVOC].enabled = false;
  priv->sensors[SGP30_SENSOR_TVOC].lower.ops = &g_sgp30_tvoc_ops;
  priv->sensors[SGP30_SENSOR_TVOC].lower.type = SENSOR_TYPE_TVOC;
  priv->sensors[SGP30_SENSOR_TVOC].lower.nbuffer = 1;

  ret = sensor_register(&priv->sensors[SGP30_SENSOR_TVOC].lower, devno);
  if (ret < 0) {
    goto err_register;
  }
  sninfo("sgp30 register success!\n");

    /* 创建轮询线程 */
  ret = kthread_create("sgp30_thread", SCHED_PRIORITY_DEFAULT,
                       CONFIG_SENSORS_SGP30_THREAD_STACKSIZE, sgp30_thread,
                       NULL);
  if (ret < 0) {
    goto err_register_tvoc;
  }

  return OK;

err_register_tvoc:
  sensor_unregister(&priv->sensors[SGP30_SENSOR_TVOC].lower, devno);
err_register:
  sensor_unregister(&priv->sensors[SGP30_SENSOR_CO2].lower, devno);
err_init:
  nxsem_destroy(&priv->run);
  nxmutex_destroy(&priv->dev_lock);
  g_sgp30_dev = NULL;
  kmm_free(priv);
  return ret;
}

#endif /* CONFIG_SENSORS_SGP30 */