/**
 *******************************************************************************
 * Copyright(c) 2023 Realtek Semiconductor Corporation. All rights reserved.
 *******************************************************************************
 */

#include "include/rtk_hci_board.h"
#include "include/rtk_coex.h"

#include <assert.h>
#include <debug.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <nuttx/fs/fs.h>
#include <nuttx/kthread.h>
#include <nuttx/nuttx.h>
#include <nuttx/queue.h>
#include <nuttx/serial/serial.h>
#include <nuttx/serial/tioctl.h>
#include <nuttx/serial/uart_bth4.h>
#include <nuttx/serial/uart_bth5.h>
#include <nuttx/wireless/bluetooth/bt_bridge.h>
#include <nuttx/wireless/bluetooth/bt_driver.h>
#include <nuttx/wireless/bluetooth/bt_slip.h>
#include <nuttx/wireless/bluetooth/bt_uart.h>
#include <nuttx/wqueue.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
#define RTK_BLUETOOTH_COEX

#define H5_ACK_PKT 0x00
#define HCI_COMMAND_PKT 0x01
#define HCI_ACLDATA_PKT 0x02
#define HCI_SCODATA_PKT 0x03
#define HCI_EVENT_PKT 0x04

#ifndef CONFIG_UART_HCI_RXBUFSIZE
#define CONFIG_UART_HCI_RXBUFSIZE 2048
#endif
#define SLIP_DELIMITER 0xc0

#ifdef CONFIG_RTK_BT_DUMP
#define bt_dump(m, b, s) lib_dumpbuffer(m, b, s)
#else
#define bt_dump(m, b, s)
#endif

/****************************************************************************
 * Private Types
 ****************************************************************************/

enum
{
  RTKHCI_STATE_CLOSED,
  RTKHCI_STATE_OPENNING,
  RTKHCI_STATE_OPENNED,
};

struct rtk_bt_priv_s
{
  struct bt_driver_s drv;
#if defined(CONFIG_BLUETOOTH_BRIDGE)
  struct bt_driver_s bridge;
#endif

  uint8_t id;
  volatile uint8_t state;
  size_t rxlen;

  struct work_s work;
  struct file filep_h5;
  struct file filep_uart;

  uint8_t rxbuf[CONFIG_UART_HCI_RXBUFSIZE];
};

struct bt_hci_evt_hdr
{
  uint8_t evt;
  uint8_t len;
};


/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct rtk_bt_priv_s g_bthci_dev = { 0 };

static FAR struct rtk_bt_priv_s *bthci_from_drv(FAR struct bt_driver_s *drv)
{
  if (drv != &g_bthci_dev.drv)
    {
      wlerr("invalid drv:%p expect:%p", drv, &g_bthci_dev.drv);
      return NULL;
    }

  return &g_bthci_dev;
}

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int bthci_open(FAR struct bt_driver_s *drv);

static int bthci_send(FAR struct bt_driver_s *drv,
                      enum bt_buf_type_e type,
                      FAR void *data, size_t len);

static int bthci_ioctl(FAR struct bt_driver_s *driver, int cmd,
                       unsigned long arg);

static void bthci_close(FAR struct bt_driver_s *drv);

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static uint64_t get_timestamp_us(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_BOOTTIME, &ts);

    return (uint64_t)(((uint64_t)ts.tv_sec * 1000000L) + ((uint64_t)ts.tv_nsec / 1000));
}

static void poll_cb(FAR struct pollfd *fds)
{
  int semcount = 0;
  FAR sem_t *pollsem;

  if (fds->arg != NULL)
    {
      pollsem = (FAR sem_t *)fds->arg;
      nxsem_get_value(pollsem, &semcount);
      if (semcount < 1)
        {
          nxsem_post(pollsem);
        }
    }
}

static void bthci_poll_cb(FAR struct rtk_bt_priv_s *dev, void *userdata)
{
  int ret;
  FAR uint8_t *pstart;
  FAR uint8_t *pend;

  if (dev->filep_uart.f_inode == NULL)
    {
      wlerr("%s: uart file closed", __func__);
      return;
    }

  if (dev->rxlen >= sizeof(dev->rxbuf))
    {
      wlerr("%s: rxlen overflow:%u", __func__, dev->rxlen);
      dev->rxlen = 0;
      return;
    }

  do
    {
      ret = file_read(&dev->filep_uart, &dev->rxbuf[dev->rxlen],
                      sizeof(dev->rxbuf) - dev->rxlen);
    }
  while (ret < 0 && errno == EINTR);

  if (ret <= 0)
    {
      wlerr("fail, %s ret:%d", __func__, ret);
      return;
    }

  bt_dump("bthci rx", &dev->rxbuf[dev->rxlen], ret);

  dev->rxlen += ret;

  do
    {
      pstart = memchr(dev->rxbuf, SLIP_DELIMITER, dev->rxlen);
      if (!pstart)
        {
          return;
        }

      pend = memchr(pstart + 1, SLIP_DELIMITER,
                    dev->rxlen - (pstart + 1 - dev->rxbuf));
      if (!pend)
        {
          return;
        }

      bt_netdev_receive(&dev->drv, 0, pstart, pend - pstart + 1);
      dev->rxlen = dev->rxbuf + dev->rxlen - (pend + 1);
      memmove(dev->rxbuf, pend + 1, dev->rxlen);
    }
  while (1);
}

static void bt_poll(FAR struct rtk_bt_priv_s *dev, FAR struct file *filep,
                    short event, bool blocked,
                    void (*cb) (FAR struct rtk_bt_priv_s *dev, void *userdata),
                    void *userdata)
{
  struct pollfd fds[1];
  sem_t sem;
  int ret = OK;

  nxsem_init(&sem, 0, 0);

  fds[0].arg = &sem;
  fds[0].revents = 0;
  fds[0].priv = NULL;
  fds[0].events = event;
  fds[0].cb = poll_cb;

  ret = file_poll(filep, fds, true);
  if (ret >= 0)
    {
      if (blocked)
        {
          nxsem_wait(&sem);
        }

      if (fds[0].revents & POLLIN)
        {
          cb(dev, userdata);
        }

      file_poll(filep, fds, false);
    }

  nxsem_destroy(&sem);
}

static int btuart_rx_task(int argc, FAR char **argv)
{
  FAR struct rtk_bt_priv_s *dev = (struct rtk_bt_priv_s *)&g_bthci_dev;
  short events = POLLERR | POLLHUP | POLLIN;

  while (dev->state != RTKHCI_STATE_CLOSED)
    {
      bt_poll(dev, &dev->filep_uart, events, true, bthci_poll_cb, NULL);
    }

  wlinfo("%s exit", __func__);
  return OK;
}

static int bthci_open(FAR struct bt_driver_s *drv)
{
  FAR struct rtk_bt_priv_s *dev = bthci_from_drv(drv);

  if (dev == NULL)
    {
      return -EINVAL;
    }

  wlinfo("%s, id:%d", __func__, dev->id);
  return OK;
}

static int bthci_send(FAR struct bt_driver_s *drv, enum bt_buf_type_e type,
                      FAR void *data, size_t len)
{
  FAR struct rtk_bt_priv_s *dev = bthci_from_drv(drv);
  int ret;

  if (dev == NULL)
    {
      return -EINVAL;
    }

  if (dev->filep_uart.f_inode == NULL)
    {
      wlerr("%s: uart file closed, state:%u", __func__, dev->state);
      return -ENODEV;
    }

  bt_dump("bthci tx", data, len);

  do
    {
      ret = file_write(&dev->filep_uart, data, len);
    }
  while (ret < 0 && errno == EINTR);

  if (ret < 0)
    {
      wlerr("failed, tx hci5 pkt ret:%d, err:%d", ret, errno);
    }

  return ret;
}

static int bthci_ioctl(FAR struct bt_driver_s *driver, int cmd, unsigned long arg)
{
  return OK;
}

static void bthci_close(struct bt_driver_s *drv)
{
  if (bthci_from_drv(drv) == NULL)
    {
      return;
    }

  wlinfo("%s", __func__);
}

static int hci_recv(struct file *filep, uint8_t *buf, size_t count)
{
  ssize_t ret;
  ssize_t nread = 0;

  while (count != nread)
    {
      ret = file_read(filep, buf + nread, count - nread);
      if (ret < 0)
        {
          if (ret == -EAGAIN)
            {
              continue;
            }
          else
            {
              return ret;
            }
        }

      nread += ret;
    }

  return nread;
}

static int hci_send(struct file *filep, uint8_t *buf, size_t count)
{
  ssize_t ret;
  ssize_t nwritten = 0;

  while (nwritten != count)
    {
      ret = file_write(filep, buf + nwritten, count - nwritten);
      if (ret < 0)
        {
          if (ret == -EAGAIN)
            {
              continue;
            }
          else
            {
              return ret;
            }
        }

      nwritten += ret;
    }

  return nwritten;
}

static int hci_send_recv(FAR struct file *filep, unsigned char *command, size_t count)
{
  if (hci_send(filep, command, count) != count)
    {
      return -EIO;
    }

  hci_recv(filep, command, 1);
  if (command[0] != HCI_EVENT_PKT)
    {
      return -EIO;
    }

  hci_recv(filep, command + 1, sizeof(struct bt_hci_evt_hdr));
  hci_recv(filep, command + 3, command[2]);

  return OK;
}

static int hci_check_local_ver(FAR struct rtk_bt_priv_s *dev)
{
  FAR struct file *filep = &dev->filep_h5;
  unsigned char command[15];
  int ret;

  /* OpCode: 0x1001, h4 buf_len: Cmd(1+3=4), Event(1+14=15) */

  command[0] = HCI_COMMAND_PKT;
  command[1] = 0x01;
  command[2] = 0x10;
  command[3] = 0;

  ret = hci_send_recv(filep, command, 4);
  if (ret != OK)
    {
      wlerr("fail, %s ret:%d", __func__, ret);
      return ret;
    }

  if (!(command[4] == 0x01 && command[5] == 0x10) || command[6] != 0x00)
    {
      return -EIO;
    }

  /* Only Check LMP Subversion */

  uint16_t lmp_sbuver
      = ((uint16_t)command[13]) | (((uint16_t)command[14]) << 8);
  if (CONFIG_BLUETOOTH_LMP_SUBVER != lmp_sbuver)
    {
      return -EALREADY;
    }

  return 0;
}

static int hci_check_local_rom_ver(FAR struct rtk_bt_priv_s *dev)
{
  FAR struct file *filep = &dev->filep_h5;
  unsigned char command[8];
  int ret;

  /* OpCode: 0xfc6d, h4 buf_len: Cmd(1+3=4), Event(1+7=8) */

  command[0] = HCI_COMMAND_PKT;
  command[1] = 0x6d;
  command[2] = 0xfc;
  command[3] = 0;

  ret = hci_send_recv(filep, command, 4);
  if (ret != OK)
    {
      wlerr("fail, %s ret:%d", __func__, ret);
      return ret;
    }

  /* Check OpCode and Status */
  if (!(command[4] == 0x6d && command[5] == 0xfc) || command[6] != 0x00)
    {
      return -EIO;
    }

  /* Get Chip Id (Rom_Ver+1) and Find Patch */

  if (rtkbt_board_find_fw_patch(command[7] + 1))
    {
      return -EIO;
    }

  return 0;
}

static int hci_update_baudrate(FAR struct rtk_bt_priv_s *dev)
{
  FAR struct file *filep_uart = &dev->filep_uart;
  FAR struct file *filep_h5 = &dev->filep_h5;
  unsigned char command[8];
  struct termios toptions;
  uint32_t bt_baudrate;
  uint32_t uart_baudrate = BT_CONFIG_BAUDRATE;
  int ret;

  /* OpCode: 0xfc17, h4 buf_len: Cmd(1+7=8), Event(1+6=7) */
  command[0] = HCI_COMMAND_PKT;
  command[1] = 0x17;
  command[2] = 0xfc;
  command[3] = sizeof(uint32_t);

  if (rtkbt_board_get_baudrate(&bt_baudrate, uart_baudrate))
    {
      return -EIO;
    }

  memcpy(&command[4], &bt_baudrate, sizeof(uint32_t));
  ret = hci_send_recv(filep_h5, command, 8);
  if (ret != OK)
    {
      wlerr("fail, %s ret:%d", __func__, ret);
      return ret;
    }

  /* Check OpCode and Status */

  if (!(command[4] == 0x17 && command[5] == 0xfc) || command[6] != 0x00)
    {
      return -EIO;
    }

  /*wait for uart ready and h5 ack */

  usleep(300 * 1000);

  file_ioctl(filep_uart, TCGETS, (unsigned long)&toptions);
  cfsetispeed(&toptions, uart_baudrate);
  cfsetospeed(&toptions, uart_baudrate);
  return file_ioctl(filep_uart, TCSETS, (unsigned long)&toptions);
}

static int hci_load_firmware(FAR struct rtk_bt_priv_s *dev)
{
  FAR struct file *filep = &dev->filep_h5;
  int header_size = 4;
  uint8_t command[RTKHCI_COMMAND_FRAGMENT_SIZE + header_size + 2];
  size_t buffer_size;
  int ret;

  while (rtkbt_board_fetch_command(command) != 0)
    {
      command[0] = HCI_COMMAND_PKT;
      command[1] = 0x20;
      command[2] = 0xfc;
      buffer_size = header_size + command[3];

      assert(buffer_size <= (sizeof(command) / sizeof(command[0])));
      ret = hci_send(filep, command, buffer_size);
      if (ret != buffer_size)
        {
          return ret;
        }

      hci_recv(filep, command, 1);
      if (HCI_EVENT_PKT != command[0])
        {
          return -EIO;
        }

      hci_recv(filep, command + 1, 2);

      assert(command[2] <= (sizeof(command) / sizeof(command[0]) - 3));
      hci_recv(filep, command + 3, command[2]);

      /* Check OpCode and Status */

      if (!(command[4] == 0x20 && command[5] == 0xfc) || command[6] != 0x00)
        {
          return -EIO;
        }
    }

  return OK;
}

static int hci_reset(FAR struct rtk_bt_priv_s *dev)
{
  FAR struct file *filep = &dev->filep_h5;
  unsigned char command[15];
  int ret;

  /* OpCode: 0x1001, h4 buf_len: Cmd(1+3=4), Event(1+14=15) */

  command[0] = HCI_COMMAND_PKT;
  command[1] = 0x03;
  command[2] = 0x0c;
  command[3] = 0;

  ret = hci_send_recv(filep, command, 4);
  if (ret != OK)
    {
      wlerr("fail, %s ret:%d", __func__, ret);
      return ret;
    }

  /* Check OpCode and Status */

  if (!(command[4] == 0x03 && command[5] == 0x0c) || command[6] != 0x00)
    {
      return -EIO;
    }

  return OK;
}

static int hci_read_local_addr(FAR struct rtk_bt_priv_s *dev)
{
  FAR struct file *filep = &dev->filep_h5;
  unsigned char command[15];
  int ret;

  /* OpCode: 0x1001, h4 buf_len: Cmd(1+3=4), Event(1+14=15) */

  command[0] = HCI_COMMAND_PKT;
  command[1] = 0x09;
  command[2] = 0x10;
  command[3] = 0;

  ret = hci_send_recv(filep, command, 4);
  if (ret != OK)
    {
      wlerr("fail, %s ret:%d", __func__, ret);
      return ret;
    }

  /* Check OpCode and Status */

  if (!(command[4] == 0x09 && command[5] == 0x10) || command[6] != 0x00)
    {
      return -EIO;
    }

  wlinfo("local addr:%02x:%02x:%02x:%02x:%02x:%02x", command[12], command[11],
         command[10], command[9], command[8], command[7]);
  return OK;
}

static int bthci_borad_init(FAR struct rtk_bt_priv_s *dev)
{
  FAR struct file *filep = &dev->filep_uart;
  struct termios toptions;
  uint32_t uart_baudrate = BT_DEFAUT_BAUDRATE;
  int ret;

  rtkbt_board_reset();

  ret = file_ioctl(filep, TCGETS, (unsigned long)&toptions);
  if (ret < 0)
    {
      wlerr("%s , err %s", __func__, strerror(errno));
      return ret;
    }

  cfmakeraw(&toptions);
  toptions.c_cflag |= CLOCAL;
#ifndef CONFIG_X4B_FACTEST
#define CONFIG_X4B_FACTEST
#endif
#ifdef CONFIG_X4B_FACTEST
  toptions.c_cflag &= ~(CRTSCTS);
  wlinfo("For mp, disable hw flow control");
#else
  toptions.c_cflag |= CRTSCTS;
  wlinfo("For normal BT, enable hw flow control");
#endif
  toptions.c_cflag |= PARENB;
  toptions.c_cflag &= ~(PARODD);
  ret = file_ioctl(filep, TCSETS, (unsigned long)&toptions);
  if (ret < 0)
    {
      wlerr("%s , err1 %s", __func__, strerror(errno));
      return ret;
    }

  cfsetispeed(&toptions, uart_baudrate);
  cfsetospeed(&toptions, uart_baudrate);

  ret = file_ioctl(filep, TCSETS, (unsigned long)&toptions);
  if (ret < 0)
    {
      wlerr("%s , err2 %s", __func__, strerror(errno));
      return ret;
    }

  ret = file_ioctl(filep, TCFLSH, TCIFLUSH);
  if (ret < 0)
    {
      wlerr("%s , err3 %s", __func__, strerror(errno));
      return ret;
    }

  return ret;
}

static int btuart_fw_task(int argc, FAR char **argv)
{
  FAR struct rtk_bt_priv_s *dev = (struct rtk_bt_priv_s *)&g_bthci_dev;
  int ret;
  uint64_t time;

  ret = file_open(&dev->filep_h5, CONFIG_BT_UART_ON_DEV_NAME, O_RDWR);
  if (ret < 0)
    {
      wlerr("fail, filep_h5 open ret:%d", ret);
      goto errout_open;
    }

  ret = hci_check_local_ver(dev);
  if (ret < 0)
    {
      wlerr("fail, hci_check_local_ver ret:%d", ret);
      goto errout_init;
    }

  ret = hci_check_local_rom_ver(dev);
  if (ret < 0)
    {
      wlerr("fail, hci_check_local_rom_ver ret:%d", ret);
      goto errout_init;
    }

  ret = hci_update_baudrate(dev);
  if (ret < 0)
    {
      wlerr("fail, hci_update_baudrate ret:%d", ret);
      goto errout_init;
    }

  time = get_timestamp_us();
  ret = hci_load_firmware(dev);
  rtkbt_free_fwc_buf();
  if (ret < 0)
    {
      wlerr("fail, hci_load_firmware ret:%d", ret);
      goto errout_init;
    }

  wlwarn("bt download fw takes:%lld us", get_timestamp_us() - time);

  ret = hci_reset(dev);
  if (ret < 0)
    {
      wlerr("fail, hci_reset ret:%d", ret);
      goto errout_init;
    }

  ret = hci_read_local_addr(dev);
  wlerr("%s", "after hci_read_local_addr");
  if (ret < 0)
    {
      wlerr("fail, hci_read_local_addr ret:%d", ret);
      goto errout_init;
    }

  dev->state = RTKHCI_STATE_OPENNED;
  return ret;

errout_init:
  file_close(&dev->filep_h5);
errout_open:
  dev->state = RTKHCI_STATE_CLOSED;
  rtkbt_board_poweroff();
  file_close(&dev->filep_uart);
  return ret;
}

static int bthci_init(FAR struct rtk_bt_priv_s *dev, uint8_t id)
{
  FAR struct bt_driver_s *drv = &dev->drv;
  int ret;
  wlerr("%s", __func__);
  ret = file_open(&dev->filep_uart, CONFIG_BLUETOOTH_UART_DEV, O_RDWR);
  if (ret < 0)
    {
      wlerr("fail, %s file_open ret:%d", __func__, ret);
      return ret;
    }

  dev->id = id;
  drv->head_reserve = 1;
  drv->open = bthci_open;
  drv->send = bthci_send;
  drv->close = bthci_close;
  drv->ioctl = bthci_ioctl;

  return OK;
}

int rtk8723FS_initialize(uint8_t id)
{
  FAR struct rtk_bt_priv_s *dev = &g_bthci_dev;
  int ret;
  char name[32];
#if defined(CONFIG_BLUETOOTH_BRIDGE)
  FAR struct bt_driver_s *btdrv;
  FAR struct bt_driver_s *bledrv;
#endif
  wlerr("%s", __func__);
  ret = bthci_init(dev, id);
  if (ret != OK)
    {
      return ret;
    }

#if defined(CONFIG_BLUETOOTH_BRIDGE) && defined(CONFIG_BLUETOOTH_SLIP)
#ifdef RTK_BLUETOOTH_COEX
  ret = bt_bridge_register(rtk_coex_register(bt_slip_register(&dev->drv)), &btdrv, &bledrv);
#else
  ret = bt_bridge_register(bt_slip_register(&dev->drv), &btdrv, &bledrv);
#endif
  if (ret < 0)
    {
      wlerr("fail, bt_bridge_register ret: %d\n", ret);
      goto failed;
    }

  snprintf(name, sizeof(name), "/dev/ttyBT%d", id);
  ret = uart_bth4_register(name, btdrv);
  if (ret < 0)
    {
      wlerr("fail, register(%s) ret: %d\n", name, ret);
      goto failed;
    }

  snprintf(name, sizeof(name), "/dev/ttyBLE%d", id);
  ret = uart_bth4_register(name, bledrv);
  if (ret < 0)
    {
      wlerr("fail, register(%s) ret: %d\n", name, ret);
      goto failed;
    }

#elif defined(CONFIG_UART_BTH5)
  snprintf(name, sizeof(name), "/dev/ttyHCI%d", id);
  ret = uart_bth5_register(name, &dev->drv);
  if (ret < 0)
    {
      wlerr("fail, register(%s) ret: %d\n", name, ret);
      goto failed;
    }

#else
#error "please enable CONFIG_UART_BTH5"
#endif

  dev->state = RTKHCI_STATE_OPENNING;

  ret = bthci_borad_init(dev);
  if (ret < 0)
    {
      wlerr("fail, bthci_borad_init ret:%d", ret);
      goto failed;
    }

  kthread_create("bt_recv", CONFIG_BLUETOOTH_HCI_RECV_PRIORITY,
                 CONFIG_BLUETOOTH_HCI_RECV_STACKSIZE, btuart_rx_task, NULL);
  kthread_create("bt_fw", CONFIG_BLUETOOTH_HCI_RECV_PRIORITY,
                 CONFIG_BLUETOOTH_HCI_RECV_STACKSIZE, btuart_fw_task, NULL);

  return ret;

failed:
  file_close(&dev->filep_uart);
  return ret;
}
