/**
 *******************************************************************************
 * Copyright(c) 2023 Realtek Semiconductor Corporation. All rights reserved.
 *******************************************************************************
 */

#include "include/rtk_hci_board.h"

#include <debug.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include <nuttx/ioexpander/gpio.h>

#include "sunxi_secure_storage_warpper.h"
#include "sunxi_secure_storage.h"
#include "hal_mem.h"

#define BT_DIS_PIN_GPIO "/dev/gpio2"

#define CONFIG_BTADDR "/etc/bt/8723fs_btaddr.txt"
#ifndef PATCH_BIN_NAME
#define PATCH_BIN_NAME "/etc/bt/8723fs_fw_C-cut.bin"
#endif
#ifndef CONFIG_BIN_NAME
#define CONFIG_BIN_NAME "/etc/bt/8723fs_config_C-cut.bin"
#endif

#define MAX_CONFIG_SIZE 100
#define RESERVED_CONFIG_SIZE 18
#define MAX_FW_SIZE 51200

static uint16_t rtl_config_size = 0;
static uint32_t rtl_fw_size = 0;
static uint8_t *rtl_config = NULL;
static uint8_t *rtl_fw = NULL;

void rtkbt_free_fwc_buf(void)
{
  if (rtl_config)
  {
    free(rtl_config);
    rtl_config = NULL;
    rtl_config_size = 0;
  }
  if (rtl_fw)
  {
    free(rtl_fw);
    rtl_fw = NULL;
    rtl_fw_size = 0;
  }
  return;
}

int
rtkbt_board_get_baudrate(uint32_t *bt_baudrate, uint32_t uart_baudrate)
{
  typedef struct
  {
    uint32_t bt_baudrate;
    uint32_t uart_baudrate;
  } baudrate_map;

  const baudrate_map maps[] = {
    { 0x0000701d, 115200 },  { 0x0252c00a, 230400 },  { 0x05f75004, 921600 },
    { 0x00005004, 1000000 }, { 0x04928002, 1500000 }, { 0x00005002, 2000000 },
    { 0x0000b001, 2500000 }, { 0x04928001, 3000000 }, { 0x052a6001, 3500000 },
    { 0x00005001, 4000000 },
  };

  uint32_t i;

  for (i = 0; i < sizeof(maps) / sizeof(maps[0]); i++)
    {
      if (uart_baudrate == maps[i].uart_baudrate)
        {
          break;
        }
    }

  if (i == sizeof(maps) / sizeof(maps[0]))
    {
      return -EINVAL;
    }

  *bt_baudrate = maps[i].bt_baudrate;
  return 0;
}

uint8_t*
rtb_read_firmware(uint32_t *fw_len)
{
  struct stat st;
  int fd = -1;
  size_t fwsize;
  ssize_t result;
  uint8_t *fw_buf_temp = NULL;

  if (!fw_len)
    {
      wlerr("%s: Invalid parameter", __func__);
      return NULL;
    }

  if ((fd = open(PATCH_BIN_NAME, O_RDONLY)) < 0)
    {
      wlerr("Can't open firmware, %s", strerror(errno));
      return NULL;
    }

  if (stat(PATCH_BIN_NAME, &st) < 0)
    {
      wlerr("Can't access firmware %s, %s", PATCH_BIN_NAME,
                  strerror(errno));
      close(fd);
      return NULL;
    }

  fwsize = st.st_size;
  if (fwsize >= MAX_FW_SIZE)
    {
      wlerr("fw size(%d) is larger than max fw size(%d)", fwsize, MAX_FW_SIZE);
      close(fd);
      return NULL;
    }

  fw_buf_temp = malloc(fwsize);
  if (!fw_buf_temp)
    {
      wlerr("Can't allocate memory for fw, %s", strerror(errno));
      close(fd);
      return NULL;
    }

  result = read(fd, fw_buf_temp, fwsize);
  if (result != (ssize_t) fwsize)
    {
      wlerr("Read FW %s error, %s", PATCH_BIN_NAME, strerror(errno));
      free(fw_buf_temp);
      close(fd);
      return NULL;
    }
  *fw_len = (uint32_t)result;

  close(fd);
  wlinfo("Load FW %s OK, size %zd", PATCH_BIN_NAME, result);
  return fw_buf_temp;
}

uint8_t*
rtb_read_config(uint16_t *config_len)
{
  struct stat st;
  int fd = -1;
  size_t configsize;
  ssize_t result;
  uint8_t *config_buf_temp = NULL;
  wlinfo("rtb_read_config");
  if (!config_len)
    {
      wlerr("%s: Invalid parameter", __func__);
      return NULL;
    }

  if ((fd = open(CONFIG_BIN_NAME, O_RDONLY)) < 0)
    {
      wlerr("Can't open config, %s", strerror(errno));
      return NULL;
    }

  if (stat(CONFIG_BIN_NAME, &st) < 0)
    {
      wlerr("Can't access config %s, %s", CONFIG_BIN_NAME,
                  strerror(errno));
      close(fd);
      return NULL;
    }

  configsize = st.st_size;
  if (configsize > MAX_CONFIG_SIZE)
    {
      wlerr("config size(%d) is larger than max config size(%d)", configsize, MAX_CONFIG_SIZE);
      close(fd);
      return NULL;
    }

  config_buf_temp = malloc(configsize + RESERVED_CONFIG_SIZE);
  if (!config_buf_temp)
    {
      wlerr("Can't allocate memory for config, %s", strerror(errno));
      close(fd);
      return NULL;
    }

  result = read(fd, config_buf_temp, configsize);
  if (result != (ssize_t) configsize)
    {
      wlerr("Read config %s error, %s", CONFIG_BIN_NAME, strerror(errno));
      free(config_buf_temp);
      close(fd);
      return NULL;
    }
  *config_len = (uint16_t)result;

  close(fd);
  wlinfo("Load config %s OK, size %zd", CONFIG_BIN_NAME, result);
  return config_buf_temp;
}

#define BT_MAC_ADDR_LEN 18
bool
rtb_read_ble_btaddr(uint8_t *param)
{
  int ret = -1;
#ifdef BT_MAC_FROM_SEC_STORAGE
  int cur_len = 0, data_len = 0;
  char bt_mac[BT_MAC_ADDR_LEN] = {0};
  char *content = hal_malloc(4096);
  if (!content) {
    wlerr("%s: hal_malloc failed!\n", __func__);
    goto hal_malloc_err;
  }

  ret = sunxi_secure_storage_init();
  if (ret) {
    wlinfo("%s: secure storage init err\n", __func__);
    goto secure_storage_init_err;
  }

  memset(content, 0, 4096);
  ret = sunxi_secure_storage_read("mac_bt", content, 4096, &data_len);
  if (!ret) {
    for (cur_len = 0; cur_len < data_len; cur_len++) {
      if (!isprint(*(content + cur_len))) {
        syslog(LOG_WARNING, "%s: ble_mac read invalid content!\n", __func__);
        break;
      }
    }
    if (cur_len == data_len)
      strlcpy(bt_mac, content, data_len + 1);
    else
      goto read_content_invalid;

    wlinfo("%s: ble_mac=%s\n", __func__, bt_mac);

    sscanf((const char *)bt_mac, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &param[5], &param[4], &param[3],
                  &param[2], &param[1], &param[0]);
  }
  else
    wlinfo("%s: read ble_mac failed\n", __func__);

read_content_invalid:
secure_storage_init_err:
  hal_free(content);
  content = NULL;
hal_malloc_err:
#endif
  return ret < 0 ? false : true;
}
bool
rtb_read_btaddr(uint8_t *param)
{
#ifdef BT_MAC_FROM_SEC_STORAGE
  int ret = -1;
  int cur_len = 0, data_len = 0;
  char bt_mac[BT_MAC_ADDR_LEN] = {0};
  char *content = hal_malloc(4096);
  if (!content) {
    wlerr("%s: hal_malloc failed!\n", __func__);
    goto hal_malloc_err;
  }

  ret = sunxi_secure_storage_init();
  if (ret) {
    wlinfo("%s: secure storage init err\n", __func__);
    goto secure_storage_init_err;
  }

  memset(content, 0, 4096);
  ret = sunxi_secure_storage_read("mac_wifi", content, 4096, &data_len);
  if (!ret) {
    for (cur_len = 0; cur_len < data_len; cur_len++) {
      if (!isprint(*(content + cur_len))) {
        syslog(LOG_WARNING, "%s: bt_mac read invalid content!\n", __func__);
        break;
      }
    }
    if (cur_len == data_len)
      strlcpy(bt_mac, content, data_len + 1);
    else
      goto read_content_invalid;

    wlinfo("%s: mac_bt=%s\n", __func__, bt_mac);

    sscanf((const char *)bt_mac, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &param[5], &param[4], &param[3],
                  &param[2], &param[1], &param[0]);
  }
  else
    wlinfo("%s: read mac_bt failed\n", __func__);

read_content_invalid:
secure_storage_init_err:
  hal_free(content);
  content = NULL;
hal_malloc_err:
  return ret < 0 ? false : true;
#else
  struct stat st;
  int fd = -1;
  ssize_t result;
  int8_t buf[BT_MAC_ADDR_LEN] = {0};

  if (!param)
    {
      wlerr("%s: Invalid parameter", __func__);
      return false;
    }

  if ((fd = open(CONFIG_BTADDR, O_RDONLY)) < 0)
    {
      wlerr("Can't open config, %s", strerror(errno));
      return false;
    }

  if (stat(CONFIG_BTADDR, &st) < 0)
    {
      wlerr("Can't access config %s, %s", CONFIG_BTADDR,
                  strerror(errno));
      close(fd);
      return false;
    }

  if (st.st_size < BT_MAC_ADDR_LEN)
    {
      wlerr("size of %s is less than 6 bytes", CONFIG_BTADDR);
      close(fd);
      return false;
    }
  result = read(fd, buf, BT_MAC_ADDR_LEN);
  if (result != BT_MAC_ADDR_LEN)
    {
      wlerr("Read config %s error, %s", CONFIG_BTADDR, strerror(errno));
      close(fd);
      return false;
    }
  sscanf((const char *)buf, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &param[5], &param[4], &param[3],
		  &param[2], &param[1], &param[0]);
  wlinfo("Load config %s OK, size %zd", CONFIG_BTADDR, result);
  close(fd);
  return true;
#endif
}

int
rtkbt_board_find_fw_patch(uint8_t chipid)
{
  (void)chipid;
  uint8_t bdaddr[6];
  uint8_t ble_bdaddr[6];
  uint16_t config_len;
  rtl_config = rtb_read_config(&rtl_config_size);
  if (rtl_config == NULL)
  {
    wlerr("failed to load config");
    return -1;
  }

  rtl_fw = rtb_read_firmware(&rtl_fw_size);
  if (rtl_fw == NULL)
  {
    wlerr("failed to load fw");
    if (rtl_config)
    {
      free(rtl_config);
      rtl_config = NULL;
      rtl_config_size = 0;
    }
    return -1;
  }

  if (rtb_read_btaddr(bdaddr))
  {
    config_len = (rtl_config[5] << 8) + rtl_config[4] + 6;
    if ((config_len + 9) <= MAX_CONFIG_SIZE)
    {
      rtl_config[config_len + 0] = 0x30;
      rtl_config[config_len + 1] = 0x00;
      rtl_config[config_len + 2] = 6;
      rtl_config[config_len + 3] = bdaddr[0];
      rtl_config[config_len + 4] = bdaddr[1];
      rtl_config[config_len + 5] = bdaddr[2];
      rtl_config[config_len + 6] = bdaddr[3];
      rtl_config[config_len + 7] = bdaddr[4];
      rtl_config[config_len + 8] = bdaddr[5];

      config_len = (rtl_config[5] << 8) + rtl_config[4] + 9;
      rtl_config[4] = (config_len & 0x00ff);
      rtl_config[5] = (config_len >> 8);

      rtl_config_size = rtl_config_size + 9;
      wlerr("load bdaddr, buf %p, len %d(0x%x, 0x%x)", rtl_config, rtl_config_size,
            rtl_config[4], rtl_config[5]);
      wlerr("the BD ADDRESS set by vendor is %02x:%02x:%02x:%02x:%02x:%02x",
                  bdaddr[5],
                  bdaddr[4],
                  bdaddr[3],
                  bdaddr[2],
                  bdaddr[1],
                  bdaddr[0]);
    }
  }
  if (rtb_read_ble_btaddr(ble_bdaddr))
  {
    config_len = (rtl_config[5] << 8) + rtl_config[4] + 6;
    if ((config_len + 9) <= MAX_CONFIG_SIZE)
    {
      rtl_config[config_len + 0] = 0x20;//need modify
      rtl_config[config_len + 1] = 0x20;
      rtl_config[config_len + 2] = 6;
      rtl_config[config_len + 3] = ble_bdaddr[0];
      rtl_config[config_len + 4] = ble_bdaddr[1];
      rtl_config[config_len + 5] = ble_bdaddr[2];
      rtl_config[config_len + 6] = ble_bdaddr[3];
      rtl_config[config_len + 7] = ble_bdaddr[4];
      rtl_config[config_len + 8] = ble_bdaddr[5];

      config_len = (rtl_config[5] << 8) + rtl_config[4] + 9;
      rtl_config[4] = (config_len & 0x00ff);
      rtl_config[5] = (config_len >> 8);

      rtl_config_size = rtl_config_size + 9;
      wlerr("load bleaddr, buf %p, len %d(0x%x, 0x%x)", rtl_config, rtl_config_size,
            rtl_config[4], rtl_config[5]);
      wlerr("the BLE ADDRESS set by vendor is %02x:%02x:%02x:%02x:%02x:%02x",
                  ble_bdaddr[5],
                  ble_bdaddr[4],
                  ble_bdaddr[3],
                  ble_bdaddr[2],
                  ble_bdaddr[1],
                  ble_bdaddr[0]);
    }
  }
  wlerr("bt rtl_fw_size:%ld, rtl_config:%d",
          rtl_fw_size, rtl_config_size);
  return 0;
}

int
rtkbt_board_fetch_command(uint8_t *command)
{
  unsigned int config_size = rtl_config_size;
  static unsigned int command_offset;
  int fragment_size = 0;
  uint8_t index;

  if (command_offset >= config_size + rtl_fw_size)
    {
      return RTKHCI_COMMAND_DONE;
    }

  if (command_offset < rtl_fw_size)
    {
      if (command_offset + RTKHCI_COMMAND_FRAGMENT_SIZE
          > rtl_fw_size)
        {
          fragment_size = rtl_fw_size - command_offset;
        }

      else
        {
          fragment_size = RTKHCI_COMMAND_FRAGMENT_SIZE;
        }

      memcpy (command + 5, rtl_fw + command_offset, fragment_size);
      command_offset += fragment_size;
    }

  if (command_offset >= rtl_fw_size)
    {
      int config_offset = command_offset - rtl_fw_size;
      int config_len = config_size - config_offset;
      if (fragment_size < RTKHCI_COMMAND_FRAGMENT_SIZE)
        {
          int free = RTKHCI_COMMAND_FRAGMENT_SIZE - fragment_size;
          int copy_size;
          if (config_len > free)
            {
              copy_size = free;
            }

          else
            {
              copy_size = config_len;
            }

          memcpy (command + 5 + fragment_size,
                  rtl_config + config_offset, copy_size);
          command_offset += copy_size;
          fragment_size += copy_size;
        }
    }

  index = (command_offset / RTKHCI_COMMAND_FRAGMENT_SIZE) - 1;
  if (command_offset % RTKHCI_COMMAND_FRAGMENT_SIZE > 0)
    {
      index++;
    }

  // FIX ME ?
  if (index > 0x7f)
    {
      if (command_offset < (config_size + rtl_fw_size))
        {
          index = (index & 0x7f) + 0x01;
        }
      else
        {
          index = (index | 0x80) + 0x01;
        }
    }
  else
    {
        if (command_offset >= (config_size + rtl_fw_size))
        {
          index = index | 0x80;
        }
    }

  command[3] = fragment_size + 1;
  command[4] = index;
  return RTKHCI_COMMAND_VALID;
}

static bool
rtkbt_dis_pin_high(void)
{
  int fd;
  int ret = -1;
  bool outvalue = true;
  fd = open(BT_DIS_PIN_GPIO, O_RDWR);
  if (fd == -1)
    {
      wlerr("can not open BT_DIS_PIN_GPIO %s", BT_DIS_PIN_GPIO);
      return false;
    }
  ret = ioctl(fd, GPIOC_SETPINTYPE, (unsigned long)GPIO_OUTPUT_PIN);
  if (ret == -1)
    {
      wlerr("can not control BT_DIS_PIN_GPIO %s, errno %d, %s",
             BT_DIS_PIN_GPIO, errno, strerror (errno));
      close (fd);
      return false;
    }
  ret = ioctl(fd, GPIOC_WRITE, (unsigned long)outvalue);
  if (ret == -1)
    {
      wlerr("can not set BT_DIS_PIN_GPIO %s, errno %d, %s", BT_DIS_PIN_GPIO,
             errno, strerror (errno));
      close(fd);
      return false;
    }
  close(fd);
  return true;
}

static bool
rtkbt_dis_pin_low (void)
{
  int fd;
  int ret = -1;
  bool outvalue = false;
  fd = open(BT_DIS_PIN_GPIO, O_RDWR);
  if (fd == -1)
    {
      wlerr("can not open BT_DIS_PIN_GPIO %s", BT_DIS_PIN_GPIO);
      return false;
    }
  ret = ioctl(fd, GPIOC_SETPINTYPE, (unsigned long)GPIO_OUTPUT_PIN);
  if (ret == -1)
    {
      wlerr("can not control BT_DIS_PIN_GPIO %s, errno %d, %s",
             BT_DIS_PIN_GPIO, errno, strerror(errno));
      close(fd);
      return false;
    }
  ret = ioctl(fd, GPIOC_WRITE, (unsigned long)outvalue);
  if (ret == -1)
    {
      wlerr("can not set BT_DIS_PIN_GPIO %s, errno %d, %s", BT_DIS_PIN_GPIO,
             errno, strerror(errno));
      close(fd);
      return false;
    }
  close(fd);
  return true;
}

void
rtkbt_board_reset(void)
{
  rtkbt_dis_pin_low();
  usleep(220 * 1000);
  rtkbt_dis_pin_high();
  usleep(220 * 1000);
}

void
rtkbt_board_poweroff(void)
{
  rtkbt_dis_pin_low();
}
