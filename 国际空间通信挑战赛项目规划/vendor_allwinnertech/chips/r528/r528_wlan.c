#include <debug.h>
#include "sdmmc/nuttx_sdio.h"
#include <nuttx/sdio.h>
#ifdef CONFIG_IEEE80211_REALTEK_WIFI
#include <arch/chip/realtek_wlan.h>
#endif
static struct sdio_dev_s *g_sdio_dev;

#ifdef CONFIG_IEEE80211_REALTEK_WIFI
void realtek_setup_oob_irq(struct sdio_dev_s *dev,int (*func)(void *), void *arg)
{
  sunxi_sdio_set_sdio_card_isr(dev, func, arg);
  sunxi_sdio_enable_irq(dev);
}

void realtek_sdio_irq_clear(struct sdio_dev_s *dev)
{

    sunxi_sdio_enable_irq(dev);
}

int realtek_wlan_bringup(void)
{
  int ret;
 /* ninfo("goio init\n");
  realtek_wl_set_gpio(dev,0);
  usleep(2000);
  realtek_wl_set_gpio(dev,1);
  usleep(2000);*/

  set_sdio_param(1,3,NULL);
  g_sdio_dev = sdio_initialize(1);

  ninfo("sdio init\n");
  ret = realtek_wl_sdio_init(g_sdio_dev);
  if (ret != 0)
    return ret;
  ninfo("wlan init\n");
  ret = realtek_wl_initialize(0);
  if (ret < 0)
    return ret;
  return 0;
}
#endif
