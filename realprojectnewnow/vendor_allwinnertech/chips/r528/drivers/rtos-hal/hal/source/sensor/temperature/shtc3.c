/****************************************************************************
 * SHTC3 温湿度传感器驱动
 ****************************************************************************/

#include <nuttx/config.h>

#ifdef CONFIG_SENSORS_SHTC3

#include <debug.h>
#include <errno.h>
#include <nuttx/arch.h>
#include <nuttx/clock.h>
#include <nuttx/compiler.h>
#include <nuttx/fs/fs.h>
#include <nuttx/i2c/i2c_master.h>
#include <nuttx/kmalloc.h>
#include <nuttx/sensors/sensor.h>
#include <nuttx/wqueue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <uORB/uORB.h>
/****************************************************************************
 * 预处理定义
 ****************************************************************************/

/* container_of宏定义 */
#ifndef container_of
#define container_of(ptr, type, member)                                        \
  ((type *)(((char *)(ptr)) - offsetof(type, member)))
#endif

/* SHTC3 寄存器和命令 */
#define SHTC3_I2C_ADDR 0x70

/* 命令字 */
#define SHTC3_CMD_WAKEUP 0x3517
#define SHTC3_CMD_SLEEP 0xB098
#define SHTC3_CMD_SOFT_RESET 0x805D
#define SHTC3_CMD_MEAS_T_RH 0x7CA2 /* 高精度测量，温度优先，时钟拉伸禁用 */
#define SHTC3_CMD_READ_ID 0xEFC8   /* 读取ID命令 */

/* ID 掩码 */
#define SHTC3_ID_MASK 0x083F
#define SHTC3_ID_VALUE 0x0807

/* 测量延时 (ms) */
#define SHTC3_MEAS_DELAY_MS 15 /* 标准精度模式下典型 12.1ms, 设置 15ms 余量 */

/* CRC8 多项式 (x^8 + x^5 + x^4 + 1) = 0x31 */
#define SHTC3_CRC_POLY 0x31
#define SHTC3_CRC_INIT 0xFF

/* 配置参数 */
#define SHTC3_DEFAULT_INTERVAL_US 1000000 /* 默认轮询间隔: 1秒 */
#define SHTC3_MIN_INTERVAL_US 50000       /* 最小采样间隔: 50ms */
#define SHTC3_MAX_INTERVAL_US 10000000    /* 最大采样间隔: 10秒 */

/* 传感器类型定义 */
#define SHTC3_SENSOR_TEMP 0 /* 温度传感器 */
#define SHTC3_SENSOR_HUMI 1 /* 湿度传感器 */
#define SHTC3_SENSOR_MAX 2  /* 传感器总数 */

struct shtc3_data_s {
  float temperature; /* 温度，单位：0.01°C */
  float humidity;    /* 湿度，单位：0.01% */
};
/****************************************************************************
 * 私有类型
 ****************************************************************************/
/* 字符设备接口函数 */
static int shtc3_open(FAR struct file *filep);
static int shtc3_close(FAR struct file *filep);
static ssize_t shtc3_read(FAR struct file *filep, FAR char *buffer, size_t len);
static int shtc3_ioctl(FAR struct file *filep, int cmd, unsigned long arg);

/* 文件操作结构体 */
static const struct file_operations g_shtc3fops __attribute__((unused)) = {
    shtc3_open,  /* open */
    shtc3_close, /* close */
    shtc3_read,  /* read */
    NULL,        /* write */
    NULL,        /* seek */
    shtc3_ioctl, /* ioctl */
};

/* 传感器接口结构体 */
struct shtc3_sensor_s {
  /* 公共部分 */
  struct sensor_lowerhalf_s lower; /* 通用传感器接口 */
  bool enabled;                    /* 启用状态 */
  /* 设备特有部分 */
  FAR struct shtc3_dev_s *dev; /* 指向设备的指针 */
  uint8_t type;                /* 传感器类型(温度或湿度) */
};
/* SHTC3设备实例 */
struct shtc3_dev_s {
  /* 传感器数组 - 支持温度和湿度传感器 */
  FAR struct shtc3_sensor_s sensors[SHTC3_SENSOR_MAX];

  /* I2C设备信息 */
  FAR struct i2c_master_s *i2c;
  uint8_t addr;
  int freq;
  uint32_t interval;

  bool sleeping; /* 是否处于休眠状态 */

  /* 最近一次读取的数据 */
  float temperature; /* 温度，单位：0.01°C */
  float humidity;    /* 湿度，单位：0.01% */

  /* 为温度和湿度传感器分别设置工作队列 */
  struct work_s temp_work; /* 温度传感器工作队列 */
  struct work_s humi_work; /* 湿度传感器工作队列 */
  bool temp_work_started;  /* 温度传感器工作队列是否已启动 */
  bool humi_work_started;  /* 湿度传感器工作队列是否已启动 */
  uint32_t temp_interval;  /* 温度传感器采样间隔(微秒) */
  uint32_t humi_interval;  /* 湿度传感器采样间隔(微秒) */

  bool temp_enabled; /* 温度传感器启用状态 */
  bool humi_enabled; /* 湿度传感器启用状态 */
};

/****************************************************************************
 * 私有函数声明
 ****************************************************************************/

/* I2C通信函数 */
static uint8_t shtc3_crc8(const uint8_t *data, size_t len);
static int shtc3_send_cmd(FAR struct shtc3_dev_s *priv, uint16_t cmd);

/* 传感器操作函数 */
static int shtc3_set_interval(FAR struct sensor_lowerhalf_s *lower,
                              FAR struct file *filep, FAR uint32_t *period_us);
static int shtc3_fetch(FAR struct sensor_lowerhalf_s *lower,
                       FAR struct file *filep, FAR char *buffer, size_t buflen);

/* I2C初始化函数 */
extern FAR struct i2c_master_s *r528_i2c_initialize(FAR const char *devpath,
                                                    int i2c_id);

/* 私有函数声明部分 */
static int shtc3_temp_activate(FAR struct sensor_lowerhalf_s *lower,
                               FAR struct file *filep, bool enable);
static int shtc3_humi_activate(FAR struct sensor_lowerhalf_s *lower,
                               FAR struct file *filep, bool enable);
static void shtc3_temp_worker(FAR void *arg);
static void shtc3_humi_worker(FAR void *arg);
static int shtc3_wakeup(FAR struct shtc3_dev_s *priv);
static int shtc3_sleep(FAR struct shtc3_dev_s *priv);
static int shtc3_measure(FAR struct shtc3_dev_s *priv);
static int shtc3_checkid(FAR struct shtc3_dev_s *priv);
static int shtc3_initialize(FAR struct shtc3_dev_s *priv)
    __attribute__((unused));
/****************************************************************************
 * 私有数据
 ****************************************************************************/

/* 温度传感器操作函数表 */
static const struct sensor_ops_s g_shtc3_temp_ops = {
    .activate = shtc3_temp_activate,
    .set_interval = shtc3_set_interval,
    .batch = NULL,
    .fetch = shtc3_fetch,
    .control = NULL,
};

/* 湿度传感器操作函数表 */
static const struct sensor_ops_s g_shtc3_humi_ops = {
    .activate = shtc3_humi_activate,
    .set_interval = shtc3_set_interval,
    .batch = NULL,
    .fetch = shtc3_fetch,
    .control = NULL,
};

static uint64_t g_temp_last_enable_time = 0;
static uint64_t g_humi_last_enable_time = 0;
/****************************************************************************
 * 私有函数
 ****************************************************************************/

/****************************************************************************
 * 名称: shtc3_crc8
 *
 * 描述:
 *   计算CRC8校验值
 *
 ****************************************************************************/
static uint8_t shtc3_crc8(const uint8_t *data, size_t len) {
  uint8_t crc = SHTC3_CRC_INIT;

  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int j = 0; j < 8; j++) {
      if (crc & 0x80) {
        crc = (crc << 1) ^ SHTC3_CRC_POLY;
      } else {
        crc <<= 1;
      }
    }
  }

  return crc;
}
/****************************************************************************
 * 名称: shtc3_open
 *
 * 描述:
 *   标准字符驱动打开方法
 ****************************************************************************/
static int shtc3_open(FAR struct file *filep) {
  FAR struct inode *inode = filep->f_inode;
  FAR struct shtc3_dev_s *priv = inode->i_private;
  int ret;

  /* 唤醒设备 */
  ret = shtc3_wakeup(priv);
  if (ret < 0) {
    return ret;
  }

  /* 初始化测量 */
  ret = shtc3_measure(priv);
  if (ret < 0) {
    return ret;
  }

  return OK;
}

/****************************************************************************
 * 名称: shtc3_close
 *
 * 描述:
 *   标准字符驱动关闭方法
 ****************************************************************************/
static int shtc3_close(FAR struct file *filep) {
  FAR struct inode *inode = filep->f_inode;
  FAR struct shtc3_dev_s *priv = inode->i_private;

  /* 休眠设备节省功耗 */
  shtc3_sleep(priv);

  return OK;
}

/****************************************************************************
 * 名称: shtc3_read
 *
 * 描述:
 *   标准字符驱动读取方法
 ****************************************************************************/
static ssize_t shtc3_read(FAR struct file *filep, FAR char *buffer,
                          size_t len) {
  FAR struct inode *inode = filep->f_inode;
  FAR struct shtc3_dev_s *priv = inode->i_private;
  FAR struct shtc3_data_s *data = (FAR struct shtc3_data_s *)buffer;
  int ret;

  if (len < sizeof(struct shtc3_data_s)) {
    syslog(LOG_ERR, "Buffer size is insufficient, requires %d bytes\n",
           sizeof(struct shtc3_data_s));
    return -EINVAL;
  }

  /* 执行测量 */
  ret = shtc3_measure(priv);
  if (ret < 0) {
    return ret;
  }

  /* 填充数据 */
  data->temperature = priv->temperature;
  data->humidity = priv->humidity;

  return sizeof(struct shtc3_data_s);
}

/****************************************************************************
 * 名称: shtc3_ioctl
 *
 * 描述:
 *   标准字符驱动ioctl方法
 ****************************************************************************/
static int shtc3_ioctl(FAR struct file *filep, int cmd, unsigned long arg) {
  FAR struct inode *inode = filep->f_inode;
  FAR struct shtc3_dev_s *priv __attribute__((unused)) = inode->i_private;
  int ret = OK;

  switch (cmd) {
  default:
    ret = -ENOTTY;
    break;
  }

  return ret;
}
/****************************************************************************
 * 名称: shtc3_send_cmd
 *
 * 描述:
 *   发送命令到SHTC3
 *
 ****************************************************************************/
static int shtc3_send_cmd(FAR struct shtc3_dev_s *priv, uint16_t cmd) {

  struct i2c_msg_s msg;
  uint8_t txbuffer[2];
  int ret;
  int retries = 3;

  up_mdelay(20);                       /* 增加到20ms */
  txbuffer[0] = (uint8_t)(cmd >> 8);   /* 高字节先发 */
  txbuffer[1] = (uint8_t)(cmd & 0xFF); /* 低字节后发 */

  msg.frequency = priv->freq;
  msg.addr = priv->addr;
  msg.flags = 0;
  msg.buffer = txbuffer;
  msg.length = 2;

  /* 添加重试机制 */
  while (retries--) {
    ret = I2C_TRANSFER(priv->i2c, &msg, 1);
    if (ret >= 0) {

      return OK;
    }
    syslog(LOG_ERR,
           "Failed to send command 0x%04X, retrying (%d attempts left)\n", cmd,
           retries);
    /* 失败后短暂延时再重试 */
    //
    if (retries > 0) {
      /* 短暂延时后重试 */
      up_mdelay(10);
    }
  }

  syslog(LOG_ERR, "Failed to send command 0x%04X: %d\n", cmd, ret);
  return -EIO;
}

/****************************************************************************
 * 名称: shtc3_wakeup
 *
 * 描述:
 *   唤醒SHTC3
 *
 ****************************************************************************/
static int shtc3_wakeup(FAR struct shtc3_dev_s *priv) {
  int ret;

  if (!priv->sleeping) {

    return OK; /* 已经唤醒状态 */
  }

  ret = shtc3_send_cmd(priv, SHTC3_CMD_WAKEUP);

  if (ret == OK) {
    up_mdelay(2); /* 唤醒延时 */
    priv->sleeping = false;
  }

  return ret;
}

/****************************************************************************
 * 名称: shtc3_sleep
 *
 * 描述:
 *   使SHTC3进入睡眠模式
 *
 ****************************************************************************/
static int shtc3_sleep(FAR struct shtc3_dev_s *priv) {
  int ret;

  if (priv->sleeping) {
    return OK; /* 已经休眠状态 */
  }

  ret = shtc3_send_cmd(priv, SHTC3_CMD_SLEEP);
  if (ret == OK) {
    priv->sleeping = true;
  }

  return ret;
}

/****************************************************************************
 * 名称: shtc3_soft_reset
 *
 * 描述:
 *   软复位SHTC3
 *
 ****************************************************************************/
static int shtc3_soft_reset(FAR struct shtc3_dev_s *priv) {
  int ret;

  ret = shtc3_send_cmd(priv, SHTC3_CMD_SOFT_RESET);
  if (ret == OK) {
    up_udelay(1000);
    priv->sleeping = false;
  }

  return ret;
}

/****************************************************************************
 * 名称: shtc3_read_id
 *
 * 描述:
 *   读取SHTC3 ID
 *
 ****************************************************************************/
static int shtc3_read_id(FAR struct shtc3_dev_s *priv, uint16_t *id) {
  struct i2c_msg_s msg[2];
  uint8_t txbuffer[2];
  uint8_t rxbuffer[3];
  int ret;

  /* 准备命令 */
  txbuffer[0] = (uint8_t)(SHTC3_CMD_READ_ID >> 8);
  txbuffer[1] = (uint8_t)(SHTC3_CMD_READ_ID & 0xFF);

  /* 发送命令 */
  msg[0].frequency = priv->freq;
  msg[0].addr = priv->addr;
  msg[0].flags = 0;
  msg[0].buffer = txbuffer;
  msg[0].length = 2;

  /* 读取数据 */
  msg[1].frequency = priv->freq;
  msg[1].addr = priv->addr;
  msg[1].flags = I2C_M_READ;
  msg[1].buffer = rxbuffer;
  msg[1].length = 3;

  ret = I2C_TRANSFER(priv->i2c, msg, 2);
  if (ret < 0) {
    syslog(LOG_ERR, "Failed to read ID: %d\n", ret);
    return -EIO;
  }

  /* 验证CRC */
  if (shtc3_crc8(rxbuffer, 2) != rxbuffer[2]) {
    syslog(LOG_ERR, "ID CRC check failed\n");
    return -EIO;
  }

  /* 组合ID */
  *id = ((uint16_t)rxbuffer[0] << 8) | rxbuffer[1];

  return OK;
}

/****************************************************************************
 * 名称: shtc3_measure
 *
 * 描述:
 *   执行一次测量并获取温湿度数据
 *
 ****************************************************************************/
static int shtc3_measure(FAR struct shtc3_dev_s *priv) {
  struct i2c_msg_s msg[2];
  uint8_t txbuffer[2];
  uint8_t rxbuffer[6]; /* T(2)+CRC + RH(2)+CRC */
  int ret;

  /* 1. 确保设备唤醒 */
  ret = shtc3_wakeup(priv);
  if (ret < 0) {
    syslog(LOG_ERR, "Failed to wakeup device: %d\n", ret);
    return ret;
  }

  /* 2. 准备测量命令 */
  txbuffer[0] = (uint8_t)(SHTC3_CMD_MEAS_T_RH >> 8);
  txbuffer[1] = (uint8_t)(SHTC3_CMD_MEAS_T_RH & 0xFF);

  /* 3. 发送命令 */
  msg[0].frequency = priv->freq;
  msg[0].addr = priv->addr;
  msg[0].flags = 0;
  msg[0].buffer = txbuffer;
  msg[0].length = 2;

  ret = I2C_TRANSFER(priv->i2c, &msg[0], 1);
  if (ret < 0) {
    syslog(LOG_ERR, "Failed to send measure command: %d\n", ret);
    /* 尝试软复位恢复 */
    syslog(LOG_INFO, "Attempting soft reset to recover...\n");
    shtc3_soft_reset(priv);
    up_mdelay(50);
    return -EIO;
  }

  /* 4. 等待测量完成 */
  up_mdelay(SHTC3_MEAS_DELAY_MS);

  /* 5. 读取结果 */
  msg[1].frequency = priv->freq;
  msg[1].addr = priv->addr;
  msg[1].flags = I2C_M_READ;
  msg[1].buffer = rxbuffer;
  msg[1].length = 6;

  ret = I2C_TRANSFER(priv->i2c, &msg[1], 1);
  if (ret < 0) {
    syslog(LOG_ERR, "Failed to read measurement data: %d\n", ret);
    return -EIO;
  }

  /* 6. 校验CRC */
  if (shtc3_crc8(rxbuffer, 2) != rxbuffer[2] ||
      shtc3_crc8(rxbuffer + 3, 2) != rxbuffer[5]) {
    syslog(LOG_ERR, "Data CRC check failed\n");
    return -EIO;
  }

  /* 7. 解析数据 */
  uint16_t rawT = ((uint16_t)rxbuffer[0] << 8) | rxbuffer[1];
  uint16_t rawRH = ((uint16_t)rxbuffer[3] << 8) | rxbuffer[4];

  /* 温度转换: T = -45 + 175 * rawT / 65535 (°C)
   * 湿度转换: RH = 100 * rawRH / 65535 (%) */
  priv->temperature = -45.0f + (175.0f * rawT / 65535.0f); // 直接计算为°C
  priv->humidity = 100.0f * rawRH / 65535.0f;              // 直接计算为%

  return OK;
}

/****************************************************************************
 * 名称: shtc3_checkid
 *
 * 描述:
 *   检查SHTC3 ID是否正确
 *
 ****************************************************************************/
static int shtc3_checkid(FAR struct shtc3_dev_s *priv) {
  uint16_t id;
  int ret;

  /* 读取设备ID */
  ret = shtc3_read_id(priv, &id);
  if (ret < 0) {
    return ret;
  }

  sninfo("SHTC3 ID: 0x%04X\n", id);

  /* 验证ID */
  if ((id & SHTC3_ID_MASK) != SHTC3_ID_VALUE) {
    snerr("Wrong Device ID! 0x%04X\n", id);
    return -ENODEV;
  }

  return OK;
}

/****************************************************************************
 * 名称: shtc3_initialize
 *
 * 描述:
 *   初始化SHTC3设备
 *
 ****************************************************************************/
static int shtc3_initialize(FAR struct shtc3_dev_s *priv) {
  int ret;
  /* 软复位 */
  ret = shtc3_soft_reset(priv);
  if (ret < 0) {
    syslog(LOG_ERR, "Failed to soft reset: %d\n", ret);
    return ret;
  }

  /* 执行一次测量测试设备功能 */
  ret = shtc3_measure(priv);
  if (ret < 0) {
    syslog(LOG_ERR, "Initial measurement failed: %d\n", ret);
    return ret;
  }

  /* 初始化完成后进入睡眠状态 */
  ret = shtc3_sleep(priv);
  if (ret < 0) {
    syslog(LOG_ERR, "Failed to enter sleep mode: %d\n", ret);
    /* 不要以此为失败条件 */
  }

  return OK;
}

/****************************************************************************
 * 名称: shtc3_temp_activate
 ****************************************************************************/
static int shtc3_temp_activate(FAR struct sensor_lowerhalf_s *lower,
                               FAR struct file *filep, bool enable) {
  FAR struct shtc3_sensor_s *sensor =
      container_of(lower, struct shtc3_sensor_s, lower);
  FAR struct shtc3_dev_s *priv = sensor->dev;
  int ret = OK;

  if (enable) {
    if (priv->temp_enabled) {
      return OK;
    }

    ret = shtc3_wakeup(priv);
    priv->temp_enabled = true;

    if (!priv->temp_work_started) {
      priv->temp_work_started = true;
      ret = work_queue(HPWORK, &priv->temp_work, shtc3_temp_worker, priv, 0);
      if (ret < 0) {
        priv->temp_work_started = false;
        priv->temp_enabled = false;
        return ret;
      }
    }

    g_temp_last_enable_time = sensor_get_timestamp();
  } else {
    uint64_t current_time = sensor_get_timestamp();

    /* 调整最小运行时间保护 - 只保护前500ms */
    if (g_temp_last_enable_time > 0 &&
        (current_time - g_temp_last_enable_time) < 500000) {
      sninfo("SHTC3: Ignoring early disable (time: %llu us)\n",
             current_time - g_temp_last_enable_time);
      return OK;
    }

    priv->temp_enabled = false;
    priv->temp_work_started = false;
    work_cancel(HPWORK, &priv->temp_work);
  }

  return OK;
}

/****************************************************************************
 * 名称: shtc3_humi_activate
 ****************************************************************************/
static int shtc3_humi_activate(FAR struct sensor_lowerhalf_s *lower,
                               FAR struct file *filep, bool enable) {
  FAR struct shtc3_sensor_s *sensor =
      container_of(lower, struct shtc3_sensor_s, lower);
  FAR struct shtc3_dev_s *priv = sensor->dev;
  int ret = OK;

  if (enable) {
    if (priv->humi_enabled) {
      return OK;
    }

    ret = shtc3_wakeup(priv);
    priv->humi_enabled = true;

    if (!priv->humi_work_started) {
      priv->humi_work_started = true;
      ret = work_queue(HPWORK, &priv->humi_work, shtc3_humi_worker, priv, 0);
      if (ret < 0) {
        priv->humi_work_started = false;
        priv->humi_enabled = false;
        return ret;
      }
    }

    g_humi_last_enable_time = sensor_get_timestamp();
  } else {
    uint64_t current_time = sensor_get_timestamp();

    /* 调整最小运行时间保护 */
    if (g_humi_last_enable_time > 0 &&
        (current_time - g_humi_last_enable_time) < 500000) {
      sninfo("SHTC3: Ignoring early disable (time: %llu us)\n",
             current_time - g_humi_last_enable_time);
      return OK;
    }

    priv->humi_enabled = false;
    priv->humi_work_started = false;
    work_cancel(HPWORK, &priv->humi_work);
  }

  return OK;
}

/****************************************************************************
 * 名称: shtc3_temp_worker
 ****************************************************************************/
static void shtc3_temp_worker(FAR void *arg) {
  FAR struct shtc3_dev_s *priv = (FAR struct shtc3_dev_s *)arg;
  struct sensor_temp temp_data;
  int ret;
  static int exec_count = 0;

  if (priv == NULL || !priv->temp_work_started || !priv->temp_enabled) {
    return;
  }

  exec_count++;
  sninfo("SHTC3: Temp worker #%d\n", exec_count);

  ret = shtc3_measure(priv);
  if (ret < 0) {
    temp_data.timestamp = sensor_get_timestamp();
    temp_data.temperature = -999.0f;
    snerr("SHTC3: Temp measurement failed\n");
  } else {
    temp_data.timestamp = sensor_get_timestamp();
    temp_data.temperature = priv->temperature;

    /* 显示测量值 */
    int temp_int = (int)priv->temperature;
    int temp_frac = abs((int)((priv->temperature - temp_int) * 100 + 0.5f));
    sninfo("SHTC3: Temp: %d.%02d°C\n", temp_int, temp_frac);
  }

  if (priv->sensors[SHTC3_SENSOR_TEMP].lower.push_event != NULL) {
    ret = priv->sensors[SHTC3_SENSOR_TEMP].lower.push_event(
        priv->sensors[SHTC3_SENSOR_TEMP].lower.priv, &temp_data,
        sizeof(temp_data));
  }

  /* 重新调度 */
  if (priv->temp_work_started && priv->temp_enabled) {
    work_queue(HPWORK, &priv->temp_work, shtc3_temp_worker, priv,
               USEC2TICK(priv->temp_interval));
  } else {
    sninfo("SHTC3: Worker stopping (started=%d, enabled=%d)\n",
           priv->temp_work_started, priv->temp_enabled);
  }
}

/****************************************************************************
 * 名称: shtc3_humi_worker
 ****************************************************************************/
static void shtc3_humi_worker(FAR void *arg) {
  FAR struct shtc3_dev_s *priv = (FAR struct shtc3_dev_s *)arg;
  struct sensor_humi humi_data;
  int ret;
  static int exec_count = 0;

  if (priv == NULL || !priv->humi_work_started || !priv->humi_enabled) {
    return;
  }

  exec_count++;
  sninfo("SHTC3: Humi worker #%d\n", exec_count);

  ret = shtc3_measure(priv);
  if (ret < 0) {
    humi_data.timestamp = sensor_get_timestamp();
    humi_data.humidity = -999.0f;
    snerr("SHTC3: Humi measurement failed\n");
  } else {
    humi_data.timestamp = sensor_get_timestamp();
    humi_data.humidity = priv->humidity;

    /* 显示测量值 */
    int humi_int = (int)priv->humidity;
    int humi_frac = abs((int)((priv->humidity - humi_int) * 100 + 0.5f));
    sninfo("SHTC3: Humidity: %d.%02d%%\n", humi_int, humi_frac);
  }

  if (priv->sensors[SHTC3_SENSOR_HUMI].lower.push_event != NULL) {
    ret = priv->sensors[SHTC3_SENSOR_HUMI].lower.push_event(
        priv->sensors[SHTC3_SENSOR_HUMI].lower.priv, &humi_data,
        sizeof(humi_data));
  }

  /* 重新调度 */
  if (priv->humi_work_started && priv->humi_enabled) {
    work_queue(HPWORK, &priv->humi_work, shtc3_humi_worker, priv,
               USEC2TICK(priv->humi_interval));
  }
}

/****************************************************************************
 * 名称: shtc3_fetch
 ****************************************************************************/
static int shtc3_fetch(FAR struct sensor_lowerhalf_s *lower,
                       FAR struct file *filep, FAR char *buffer,
                       size_t buflen) {
  FAR struct shtc3_sensor_s *sensor =
      container_of(lower, struct shtc3_sensor_s, lower);
  FAR struct shtc3_dev_s *priv = sensor->dev;
  int ret;
  uint64_t timestamp;

  if (!buffer || buflen == 0) {
    return -EINVAL;
  }

  timestamp = sensor_get_timestamp();
  ret = shtc3_measure(priv);
  if (ret < 0) {
    return ret;
  }

  if (sensor->type == SHTC3_SENSOR_TEMP) {
    if (buflen < sizeof(struct sensor_temp)) {
      return -ENOMEM;
    }

    struct sensor_temp *temp_data = (struct sensor_temp *)buffer;
    temp_data->timestamp = timestamp;
    temp_data->temperature = priv->temperature;

    return sizeof(struct sensor_temp);
  } else {
    if (buflen < sizeof(struct sensor_humi)) {
      return -ENOMEM;
    }

    struct sensor_humi *humi_data = (struct sensor_humi *)buffer;
    humi_data->timestamp = timestamp;
    humi_data->humidity = priv->humidity;

    return sizeof(struct sensor_humi);
  }
}
/****************************************************************************
 * 名称: shtc3_set_interval
 ****************************************************************************/
static int shtc3_set_interval(FAR struct sensor_lowerhalf_s *lower,
                              FAR struct file *filep, FAR uint32_t *period_us) {
  FAR struct shtc3_sensor_s *sensor =
      container_of(lower, struct shtc3_sensor_s, lower);
  FAR struct shtc3_dev_s *priv = sensor->dev;
  uint32_t interval = *period_us;

  sninfo("SHTC3: Set interval called for sensor type %d, interval=%lu us\n",
         sensor->type, interval);

  /* 检查间隔范围 */
  if (interval < SHTC3_MIN_INTERVAL_US) {
    sninfo("SHTC3: Interval too small, setting to minimum %d us\n",
           SHTC3_MIN_INTERVAL_US);
    interval = SHTC3_MIN_INTERVAL_US;
  } else if (interval > SHTC3_MAX_INTERVAL_US) {
    sninfo("SHTC3: Interval too large, setting to maximum %d us\n",
           SHTC3_MAX_INTERVAL_US);
    interval = SHTC3_MAX_INTERVAL_US;
  }

  /* 根据传感器类型设置对应的间隔 */
  if (sensor->type == SHTC3_SENSOR_TEMP) {
    priv->temp_interval = interval;
    sninfo("SHTC3: Temperature sensor interval set to %lu us\n", interval);
  } else if (sensor->type == SHTC3_SENSOR_HUMI) {
    priv->humi_interval = interval;
    sninfo("SHTC3: Humidity sensor interval set to %lu us\n", interval);
  } else {
    sninfo("SHTC3: Unknown sensor type: %d\n", sensor->type);
    return -EINVAL;
  }

  /* 更新返回值 */
  *period_us = interval;

  return OK;
}
/****************************************************************************
 * 名称: shtc3_register
 *
 * 描述:
 *   注册SHTC3传感器设备
 *
 * 参数:
 *   devno - 设备编号
 *   i2c   - I2C主设备句柄
 *
 * 返回值:
 *   成功返回0，失败返回负值
 ****************************************************************************/

int shtc3_register(int devno, FAR struct i2c_master_s *i2c) {
  FAR struct shtc3_dev_s *priv;
  int ret;
  /* 初始化设备实例 */
  priv = (FAR struct shtc3_dev_s *)kmm_zalloc(sizeof(struct shtc3_dev_s));
  if (priv == NULL) {
    snerr("Failed to allocate instance\n");
    return -ENOMEM;
  }

  priv->i2c = i2c;
  priv->addr = SHTC3_I2C_ADDR;
  priv->freq = 100000;

  /* 初始化工作状态 */
  priv->sleeping = true;
  priv->temp_work_started = false;
  priv->humi_work_started = false;
  priv->temp_enabled = false;
  priv->humi_enabled = false;
  priv->temp_interval = SHTC3_DEFAULT_INTERVAL_US;
  priv->humi_interval = SHTC3_DEFAULT_INTERVAL_US;

  up_mdelay(100);
  /* 检查设备ID */
  ret = shtc3_checkid(priv);
  if (ret < 0) {
    kmm_free(priv);
    return ret;
  }
  /* 添加设备初始化 */
  ret = shtc3_initialize(priv);
  if (ret < 0) {
    kmm_free(priv);
    return ret;
  }
  /* 初始化温度传感器 */
  priv->sensors[SHTC3_SENSOR_TEMP].dev = priv;
  priv->sensors[SHTC3_SENSOR_TEMP].type = SHTC3_SENSOR_TEMP;
  priv->sensors[SHTC3_SENSOR_TEMP].lower.ops = &g_shtc3_temp_ops;
  priv->sensors[SHTC3_SENSOR_TEMP].lower.type = SENSOR_TYPE_AMBIENT_TEMPERATURE;
  priv->sensors[SHTC3_SENSOR_TEMP].enabled = false;
  priv->sensors[SHTC3_SENSOR_TEMP].lower.nbuffer = 1;

  /* 注册温度传感器 */
  ret = sensor_register(&priv->sensors[SHTC3_SENSOR_TEMP].lower, devno);
  if (ret < 0) {
    snerr("Failed to register temp sensor: %d\n", ret);
    kmm_free(priv);
    return ret;
  }

  /* 初始化湿度传感器 */
  priv->sensors[SHTC3_SENSOR_HUMI].dev = priv;
  priv->sensors[SHTC3_SENSOR_HUMI].type = SHTC3_SENSOR_HUMI;
  priv->sensors[SHTC3_SENSOR_HUMI].lower.ops = &g_shtc3_humi_ops;
  priv->sensors[SHTC3_SENSOR_HUMI].lower.type = SENSOR_TYPE_RELATIVE_HUMIDITY;
  priv->sensors[SHTC3_SENSOR_HUMI].enabled = false;
  priv->sensors[SHTC3_SENSOR_HUMI].lower.nbuffer = 1;

  /* 注册湿度传感器 */
  ret = sensor_register(&priv->sensors[SHTC3_SENSOR_HUMI].lower, devno);
  if (ret < 0) {
    sensor_unregister(&priv->sensors[SHTC3_SENSOR_TEMP].lower, devno);
    kmm_free(priv);
    return ret;
  }

  return OK;
}

#endif /* CONFIG_SENSORS_SHTC3 */