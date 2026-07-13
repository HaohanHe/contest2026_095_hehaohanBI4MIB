/*
* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
*
* Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
* the the people's Republic of China and other countries.
* All Allwinner Technology Co.,Ltd. trademarks are used with permission.
*
* DISCLAIMER
* THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
* IF YOU NEED TO INTEGRATE THIRD PARTY'S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
* IN ALLWINNERS'SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
* ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
* ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
* COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
* YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY'S TECHNOLOGY.
*
*
* THIS SOFTWARE IS PROVIDED BY ALLWINNER"AS IS" AND TO THE MAXIMUM EXTENT
* PERMITTED BY LAW, ALLWINNER EXPRESSLY DISCLAIMS ALL WARRANTIES OF ANY KIND,
* WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING WITHOUT LIMITATION REGARDING
* THE TITLE, NON-INFRINGEMENT, ACCURACY, CONDITION, COMPLETENESS, PERFORMANCE
* OR MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
* IN NO EVENT SHALL ALLWINNER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
* NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS, OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
* OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <debug.h>
#include <nuttx/mmcsd.h>
#include <nuttx/sdio.h>
#include <syslog.h>
#include <arch/chip/realtek_wlan.h>
#include "wifi_conf.h"
#include "wifi_util.h"
#include "realtek_driver.h"

extern struct realtek_dev_s g_realtek_dev[2];

static struct sdio_dev_s* test_sdio_dev;
struct sdio_dev_s *sdio_initialize(int sdcno);

int wifi_init(void)
{
  int ret;
  test_sdio_dev = sdio_initialize(1);

  _info("sdio init\n");
  ret = realtek_wl_sdio_init(test_sdio_dev);
  if (ret != 0)
    return ret;
  _info("wlan init\n");
  ret = realtek_wl_initialize(0);
  if (ret < 0)
    return ret;
  return 0;
}

int wifi_set_scan(void)
{
  struct iw_scan_req req;
  struct iwreq wrq = {};

  int ret = 0;
  u16 flags = 0x00 | (0x02 << 8);

  memset(&req, 0, sizeof(req));

  req.scan_type       = 0;
  req.bssid.sa_family = ARPHRD_ETHER;
  memset(req.bssid.sa_data, 0xff, IFHWADDRLEN);
  wrq.u.data.pointer  = (caddr_t)&req;
  wrq.u.data.length   = sizeof(req);

  strlcpy(wrq.ifr_name, WLAN0_NAME, IFNAMSIZ);

  if(g_realtek_dev[0].dev.ops != NULL)
  {
    realtek_wl_start_scan(&g_realtek_dev[0], &wrq);
    ret = wext_set_scan(WLAN0_NAME, NULL, 0, flags);
  }
  syslog(LOG_INFO,"wlan wext_set_scan\n");
  return ret;
}

int wifi_get_scan_res(void)
{
  char buff[1024];
  int ret = wext_get_scan(WLAN0_NAME, buff, 1024);
  if( ret != -1)
  {
    syslog(LOG_INFO,"wlan wext_get_scan :iwr.u.data.flags%d \n", ret);
  }
  return 0;
}

int main(int argc, char **argv)
{
	_info("******start wifi test****** %d \n",argc);
  if(argc < 2)
  {
    _info("******start wifi wifi_sdio_init test******\n");
    wifi_init();
  }
  else
  {
    if(strcmp(argv[1],"init") == 0)
    {
      _info("******start wifi wifi_sdio_init test******\n");
      wifi_init();
    }
    else if(strcmp(argv[1],"sta") == 0)
    {
      if(argc >= 3)
      {
        if(strcmp(argv[2], "start"))
        {
          wifi_on(RTW_MODE_STA);
        }
        else if (strcmp(argv[2], "is0_running")==0) {
          rltk_wlan_running(0);
        }
        else if (strcmp(argv[2], "is1_running") == 0) {
          rltk_wlan_running(1);
        }
        else if(strcmp(argv[2], "scan") == 0)
        {
          wifi_set_scan();
        }
        else if(strcmp(argv[2], "get_scan") == 0)
        {
          wifi_get_scan_res();
        }
        else
        {
          wifi_off();
        }
      }
      else
      {
        _info("******start wifi wifi sta error: sta 0 [1] ******\n");
      }
    }
    else if(strcmp(argv[1],"ap") == 0)
    {
      if(strcmp(argv[2],"start")==0)
        wifi_on(RTW_MODE_AP);
      if(strcmp(argv[2],"run")==0)
      wifi_start_ap("WIFI_TEST",RTW_SECURITY_OPEN,"123456789",9,9,7);
    }
    else if(strcmp(argv[1],"mac") == 0)
    {
      if(strcmp(argv[2],"set") == 0)
        wifi_set_mac_address("00:11:22:33:44:55");
      else
      {
        char buf[24];
        wifi_get_mac_address(buf);
        _info("******get mac : %s******\n",buf);
      }
    }
    else if(strcmp(argv[1],"ssid") == 0)
    {
      wext_set_ap_ssid("wlan0", (__u8*)"WIFI_TEST",8);
    }
    else if(strcmp(argv[1],"PSK") == 0)
    {
      wext_set_key_ext(WLAN0_NAME, IW_ENCODE_ALG_NONE, NULL, 0, 0, 0, 0, NULL, 0);
    }
    else
    {
      _info("******error cmd : %s %s******\n",argv[0], argv[1]);
    }
  }
	_info("******wifi test finish******\n");
  return 0;
}