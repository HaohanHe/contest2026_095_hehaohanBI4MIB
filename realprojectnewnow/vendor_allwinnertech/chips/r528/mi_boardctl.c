/****************************************************************************
 * vendor/allwinnertech/boards/r528/r528s3-evb4/src/ap.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <syslog.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <sys/boardctl.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <nuttx/board.h>
#include <nuttx/config.h>
#include <nuttx/serial/uart_rpmsg.h>

#include "triad_ca_api.h"

#include "sunxi_secure_storage_warpper.h"
#include "sunxi_secure_storage.h"
#include "hal_mem.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define MIIO_DID_LEN          9
#define MIIO_KEY_LEN          16
#define PRODUCT_SN_LEN        15
#define BOARDMISC_DATA_SIZE   32
#define BOARDIOC_USER_KEY     (BOARDIOC_USER + 1)
#define BOARDIOC_USER_SN      (BOARDIOC_USER + 2)

/****************************************************************************
 * Private Types
 ****************************************************************************/

enum board_misc_data_e
{
  BOARDIOC_DATA_SN = 0,
  BOARDIOC_DATA_WIFIMAC,
  BOARDIOC_DATA_BTMAC,
  BOARDIOC_DATA_DID,
  BOARDIOC_DATA_KEY,
  BOARDIOC_DATA_NUM,
};

struct board_misc_data_s
{
  const char *name;
  char data[BOARDMISC_DATA_SIZE];
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

#if (defined CONFIG_X4B_AP) || (defined CONFIG_X4B_FACTEST)

static struct board_misc_data_s g_misc_data[BOARDIOC_DATA_NUM] =
{
  { .name = "sn",         .data = "00000/000000001",   },
  { .name = "mac_wifi",   .data = "42:43:44:45:46:47", },
  { .name = "mac_bt",     .data = "42:43:44:45:46:48", },
  { .name = "miio_did",   .data = "000000001",         },
  { .name = "miio_key",   .data = "0000000000000001",  },
};

#define MAC_STR_LEN 18
#define WIFI_MAC_PATH "/data/wifi/wifimac.txt"
#define BT_MAC_PATH "/data/bt/8723fs_btaddr.txt"
#define DEVICE_INFO_PATH "/data/etc/device.info"

static void store_mac_to_file(
		const char *path, const char *mac_from_mikey,
		const char *name, const char *dirname)
{
	struct stat st;
	int fd;
	char mac_from_file[MAC_STR_LEN] = {0};
	int ret = 0;

	if (access(path, F_OK)) {
		if (access(dirname, F_OK))
			ret = mkdir(dirname, 0644);
		if (ret) {
			syslog(LOG_ERR, "Create dir %s failed\n", dirname); //Function: dirname(path) directly return "/"???
			return;
		}
		fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (fd > 0) {
			syslog(LOG_INFO, "Store %s to file\n", name);
			ret = write(fd, mac_from_mikey, MAC_STR_LEN);
			if (ret != MAC_STR_LEN) {
				syslog(LOG_ERR, "%s: write mac info error, ret %d, errno %d\n", __func__, ret, errno);
				ret = -1;
			}
		} else {
			syslog(LOG_ERR, "Open & Create %s failed\n", path);
			goto open_file_err;
		}
	} else {
		fd = open(path, O_RDWR);
		if (fd > 0) {
			read(fd, mac_from_file, MAC_STR_LEN);
			if (strcmp(mac_from_file, mac_from_mikey)) {
				syslog(LOG_INFO, "%s changed, restore it to file\n", name);
				lseek(fd, 0, SEEK_SET);
				ret = write(fd, mac_from_mikey, MAC_STR_LEN);
				if (ret != MAC_STR_LEN) {
					syslog(LOG_ERR, "%s: write mac info error, ret %d, errno %d\n", __func__, ret, errno);
					ret = -1;
				}
			}
		} else {
			syslog(LOG_ERR, "Open %s failed\n", path);
			goto open_file_err;
		}
	}

	close(fd);
open_file_err:
	if (ret < 0)
		unlink(path);
	return;
}

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int board_misc_init(void)
{
  static bool binit = false;
  bool exist_flag = false;
  char buf[64] = {0};
  int config_fd = -1;
  int ret = -1;
  int index = 0, cur_len = 0;
  int data_len = 0;

  if (binit)
  {
    syslog(LOG_INFO, "%s: already inited\n", __func__);
    return OK;
  }

  ret = mkdir("/data/etc", 0644);
  if (!ret)
  {
    syslog(LOG_INFO, "%s: create /data/etc successfully!\n", __func__);
  }
  else if (errno == EEXIST)
  {
    syslog(LOG_INFO, "%s: /data/etc is exist\n", __func__);
  }
  else
  {
    syslog(LOG_ERR, "%s: can not create dir /data/etc, errno %d\n", __func__, errno);
    goto config_fd_err;
  }

  config_fd = open(DEVICE_INFO_PATH, O_WRONLY | O_CREAT | O_EXCL, 0644);
  if (config_fd < 0)
  {
      if (errno == EEXIST)
      {
        syslog(LOG_INFO, "%s: %s is exist\n", __func__, DEVICE_INFO_PATH);
        exist_flag = true;
      }
      else
      {
        syslog(LOG_ERR, "%s: could not open %s, errno %d\n", __func__, DEVICE_INFO_PATH, errno);
        ret = -1;
        goto config_fd_err;
      }
  }

  int ret_did = -1, ret_key = -1;
  uint8_t did[MIIO_DID_LEN - 1] = {0}, key[MIIO_KEY_LEN] = {0};
  ret_did = triad_load_did(did, 8);
  ret_key = triad_load_key(key, MIIO_KEY_LEN);
  if (!ret_did)
  {
#ifdef CONFIG_ARCH_CHIP_GOLDFISH_ARM
    snprintf(g_misc_data[3].data, sizeof(g_misc_data[3].data), "%" PRIu64, *(uint64_t*)did);
#else
    snprintf(g_misc_data[3].data, sizeof(g_misc_data[3].data), "%u%u%u%u%u%u%u%02" PRIx8 "", did[0], did[1], did[2], did[3], did[4], did[5], did[6], did[7]);
#endif
    snprintf(buf, sizeof(buf), "%s = \"%s\"\n", g_misc_data[3].name, g_misc_data[3].data);
    syslog(LOG_INFO, "%s: name=%s, data=%s\n", __func__, g_misc_data[3].name, g_misc_data[3].data);

    if (!exist_flag)
    {
      ret = write(config_fd, buf, strlen(buf));
      if (ret != strlen(buf))
      {
        syslog(LOG_ERR, "%s: write device info error, ret %d errno %d\n", __func__, ret, errno);
        ret = -1;
        goto hal_malloc_err;
      }
    }
  }

  if (!ret_key)
  {
    for (index = 0; index < MIIO_KEY_LEN; index++)
    {
      g_misc_data[4].data[index] = (char)key[index];
    }
    snprintf(buf, sizeof(buf), "%s = \"%s\"\n", g_misc_data[4].name, g_misc_data[4].data);
    syslog(LOG_INFO, "%s: name=%s, data=%s\n", __func__, g_misc_data[4].name, g_misc_data[4].data);

    if (!exist_flag)
    {
      ret = write(config_fd, buf, strlen(buf));
      if (ret != strlen(buf))
      {
        syslog(LOG_ERR, "%s: write device info error, ret %d errno %d\n", __func__, ret, errno);
        ret = -1;
        goto hal_malloc_err;
      }
    }
  }

  char *content = hal_malloc(4096);
  if (!content)
  {
    syslog(LOG_ERR, "%s: hal_malloc failed!\n", __func__);
    ret = -1;
    goto hal_malloc_err;
  }

  ret = sunxi_secure_storage_init();
  if (ret)
  {
    syslog(LOG_INFO, "%s: secure storage init err\n", __func__);
    goto secure_storage_init_err;
  }

  for(index = 0; index < BOARDIOC_DATA_NUM; index++)
  {
    if ((index == BOARDIOC_DATA_DID && !ret_did) || (index == BOARDIOC_DATA_KEY && !ret_key))
      continue;
    memset(content, 0, 4096);
    memset(buf, 0, sizeof(buf));
    ret = sunxi_secure_storage_read(g_misc_data[index].name, content, 4096, &data_len);
    if (!ret)
    {
      for (cur_len = 0; cur_len < data_len; cur_len++)
      {
        if (!isprint(*(content + cur_len)))
        {
          syslog(LOG_WARNING, "%s: g_misc_data[%d] name(%s)[%d] read invalid content!\n", __func__, index, g_misc_data[index].name, cur_len);
          break;
        }
      }
      if (cur_len == data_len)
      {
        strlcpy(g_misc_data[index].data, content, data_len + 1);
	if ((BOARDIOC_DATA_WIFIMAC == index) || (BOARDIOC_DATA_BTMAC == index))
        {
          const char *path, *dirname;
          path = (BOARDIOC_DATA_WIFIMAC == index) ? WIFI_MAC_PATH : BT_MAC_PATH;
          dirname = (BOARDIOC_DATA_WIFIMAC == index) ? "/data/wifi" : "/data/bt";
          store_mac_to_file(path, g_misc_data[index].data, g_misc_data[index].name, dirname);
        }
        snprintf(buf, sizeof(buf), "%s = \"%s\"\n", g_misc_data[index].name, g_misc_data[index].data);
        syslog(LOG_INFO, "%s: name=%s, data=%s\n", __func__, g_misc_data[index].name, g_misc_data[index].data);

        if (!exist_flag)
        {
          ret = write(config_fd, buf, strlen(buf));
          if (ret != strlen(buf))
          {
            syslog(LOG_ERR, "%s: write device info error, ret %d, errno %d\n", __func__, ret, errno);
            ret = -1;
            goto read_content_invalid;
          }
        }
      }
      else
      {
        ret = -1;
        goto read_content_invalid;
      }
    }
    else
    {
      syslog(LOG_ERR, "%s: read %s failed! ret = %d\n", __func__, g_misc_data[index].name, ret);
      ret = -1;
      goto read_content_invalid;
    }
  }

  binit = true;

read_content_invalid:
secure_storage_init_err:
  hal_free(content);
  content = NULL;
hal_malloc_err:
  close(config_fd);
config_fd_err:
  return ret < 0 ? ret : OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int board_ioctl(unsigned int cmd, uintptr_t arg)
{
  int ret = 0;
  ret = board_misc_init();
  if (ret)
  {
    unlink(DEVICE_INFO_PATH);
    syslog(LOG_ERR, "ERROR: board_misc_init failed\n");
    return -EINVAL;
  }

  switch (cmd)
  {
    case BOARDIOC_USER_KEY:
    {
      memcpy((void *)arg, g_misc_data[BOARDIOC_DATA_KEY].data, 16);
    }
    break;

    case BOARDIOC_USER_SN:
    {
      memcpy((void *)arg, g_misc_data[BOARDIOC_DATA_SN].data, PRODUCT_SN_LEN);
    }
    break;

    default:
      return -EINVAL;
  }

  return OK;
}

int board_get_did(uint8_t *uniqueid)
{
  int ret = 0;
  ret = board_misc_init();
  if (ret)
  {
    unlink(DEVICE_INFO_PATH);
    syslog(LOG_ERR, "ERROR: read did failed\n");
    return -EINVAL;
  }

  *(uint64_t *)uniqueid = strtoull(g_misc_data[BOARDIOC_DATA_DID].data, NULL, 10);
  return OK;
}

#endif /* CONFIG_X4B_AP || CONFIG_X4B_FACTEST */

#ifdef CONFIG_RPMSG_UART
void rpmsg_serialinit(void)
{
#if defined(CONFIG_X4B_AP) || defined(CONFIG_X4B_FACTEST)
  uart_rpmsg_init("audio", "AP", 4096, true);
  uart_rpmsg_init("tee", "TEE", 4096, false);
#endif

#if defined(CONFIG_X4B_TEE)
  uart_rpmsg_init("ap", "TEE", 4096, true);
#endif
}
#endif

#ifdef CONFIG_BOARDCTL_UNIQUEKEY
#include <sunxi_hal_efuse.h>
int board_uniquekey(uint8_t *uniquekey)
{
  int ret = 0;
  DEBUGASSERT(CONFIG_BOARDCTL_UNIQUEKEY_SIZE >= 24);
  if (uniquekey == NULL)
    {
      return -EINVAL;
    }
#if defined(CONFIG_DRIVERS_EFUSE)
  /* r528 efuse huk length is 192 bits, and only can be read in tee */
  ret = hal_efuse_read("huk", uniquekey, CONFIG_BOARDCTL_UNIQUEKEY_SIZE * 8);
  if (ret < 0)
    {
      return ret;
    }

  /* if the provision is not provided, the value that read out are filled
   * with 0x00, in order to compat with vela, we need to change the default
   * value from 0x00 to 0xff
   */
  uint8_t no_provision[CONFIG_BOARDCTL_UNIQUEKEY_SIZE];
  memset(no_provision, 0x00, sizeof(no_provision));
  if(!memcmp(uniquekey, no_provision, CONFIG_BOARDCTL_UNIQUEKEY_SIZE))
    {
      /* set default huk */
      memset(uniquekey, 0xFF, CONFIG_BOARDCTL_UNIQUEKEY_SIZE);
    }
#else
#error "Should enable CONFIG_DRIVERS_EFUSE"
#endif
  return OK;
}
#endif

#if defined(CONFIG_BOARDCTL_UNIQUEID)

#include <sunxi_hal_efuse.h>
int board_uniqueid(FAR uint8_t *uniqueid)
{
  int ret = 0;
  DEBUGASSERT(CONFIG_BOARDCTL_UNIQUEID_SIZE >= 16);
  if (uniqueid == NULL)
    {
      return -EINVAL;
    }
#if defined(CONFIG_DRIVERS_EFUSE)
  /* r528 efuse chipid length is 128 bits, and can be read in tee/ap/ota... */
  ret = hal_efuse_read("chipid", uniqueid, CONFIG_BOARDCTL_UNIQUEID_SIZE * 8);
  if (ret < 0)
    {
      return ret;
    }
#else
#error "Should enable CONFIG_DRIVERS_EFUSE"
#endif
  return OK;
}

#endif
