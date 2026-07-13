
#include <nuttx/config.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <nuttx/board.h>
#include "sunxi_secure_storage_warpper.h"
#include "sunxi_secure_storage.h"
#include "hal_mem.h"
#define PRODUCT_SN_LEN        15
static char g_serialstr[PRODUCT_SN_LEN + 1] = "00000/000000001";
#ifdef CONFIG_BOARD_USBDEV_SERIALSTR
int usbdev_serialstr_init(void)
{
	int ret = -1;
	int cur_len = 0;
	int data_len = 0;
	char *content = hal_malloc(16);
	if (!content)
	{
		syslog(LOG_ERR, "%s: hal_malloc failed!\n", __func__);
		goto hal_malloc_err;
	}
	memset(content, 0, 16);
	ret = sunxi_secure_storage_init();
	if (ret)
	{
		syslog(LOG_INFO, "%s: secure storage init err\n", __func__);
		goto secure_storage_init_err;
	}
	ret = sunxi_secure_storage_read("sn", content, 16, &data_len);
        syslog(LOG_INFO, "sn_len: %d\n", data_len);
	if (!ret)
	{
		memset(g_serialstr, 0, sizeof(g_serialstr));
		for (cur_len = 0; cur_len < data_len; cur_len++)
		{
			if (!isprint(*(content + cur_len)))
			{
				syslog(LOG_WARNING, "%s: sn[%d] read invalid content!\n", __func__, cur_len);
				break;
			}
		}
		if (cur_len == data_len)
			strlcpy(g_serialstr, content, data_len + 1);
		syslog(LOG_INFO, "%s: sn = %s\n", __func__, g_serialstr);
	}
	else
	{
		syslog(LOG_ERR, "%s: read %s failed!\n", __func__, "sn");
	}
secure_storage_init_err:
	hal_free(content);
	content = NULL;
hal_malloc_err:
	return ret;
}
const char *board_usbdev_serialstr(void)
{
	syslog(LOG_INFO, "%s: serialstr = %s\n", __func__, g_serialstr);
	return g_serialstr;
}
#endif

