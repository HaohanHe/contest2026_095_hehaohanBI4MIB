#include <wifi_structures.h>
#include <wifi_constants.h>

#include <skbuff.h>
#include <nuttx/config.h>

#include <semaphore.h>
#include <nuttx/wdog.h>
#include <nuttx/wqueue.h>
#include <nuttx/net/netdev.h>
#include <nuttx/net/netdev_lowerhalf.h>
#include <osdep_service.h>
//#include <realtek_netdev.h>

#define TX_POLL_THREAD 1
struct realtek_associate_s {
  rtw_ssid_t              ssid;
  rtw_mac_t               mac;
  unsigned int                 alg;
  unsigned int            channel;
    unsigned char 		*password;
    int 				password_len;
   int					key_id;
};
enum
{
  REALTEK_WL_STATUS_DONE = 0,
  REALTEK_WL_STATUS_DISABLED,
  REALTEK_WL_STATUS_RUN,
  REALTEK_WL_STATUS_TIMEOUT,
};
struct realtek_pkt_list
{
	struct list_head list;
  netpkt_t *pkt;
};
struct realtek_dev_s
{
  struct netdev_lowerhalf_s dev;
  int devnum;
  int status;
  struct realtek_associate_s assoc;
  unsigned int            channel;
  rtw_scan_result_t         scan_data[64];
  unsigned int scan_count;
  int                       mode;
  char country[3];
  _queue rx_buff_queue;
  int free_rx_count;
  #if TX_POLL_THREAD
  _queue tx_buff_queue;
  int free_tx_count;
  #endif
  unsigned char macaddr[18];
};

#ifdef CONFIG_WIFI_STATISTICS
struct wifi_statistics_s
{
  struct ntimer_wrapper *timer_wrap;
  struct realtek_dev_s *dev;
  int period;
};

extern struct wifi_statistics_s wifi_stats;
void realtek_dump_wifi_statistics(void *priv);
#endif

int realtek_wl_get_mode(FAR struct realtek_dev_s *priv, struct iwreq *iwr);
int realtek_wl_set_mode(FAR struct realtek_dev_s *priv, struct iwreq *iwr);
int realtek_wl_set_encode_ext(struct realtek_dev_s *priv, struct iwreq *iwr);
int realtek_wl_get_encode_ext(struct realtek_dev_s *priv, struct iwreq *iwr);
int realtek_wl_get_ssid(FAR struct realtek_dev_s *priv, struct iwreq *iwr);
int realtek_wl_set_ssid(FAR struct realtek_dev_s *priv, struct iwreq *iwr);
int realtek_wl_get_bssid(FAR struct realtek_dev_s *priv, struct iwreq *iwr);
int realtek_wl_set_bssid(FAR struct realtek_dev_s *priv, struct iwreq *iwr);
int realtek_wl_get_freq(FAR struct realtek_dev_s *priv, struct iwreq *iwr);
int realtek_wl_set_freq(FAR struct realtek_dev_s *priv, struct iwreq *iwr);
int realtek_wl_get_range(struct realtek_dev_s *priv, struct iwreq *iwr);
int realtek_wl_process_command(struct realtek_dev_s *priv, int cmd, void *req);
int realtek_wl_get_scan_results(struct realtek_dev_s *priv, struct iwreq *iwr);
int realtek_wl_start_scan(struct realtek_dev_s *priv, struct iwreq *iwr);
int realtek_wl_get_bit_rate(struct realtek_dev_s *priv, struct iwreq *iwr);
int realtek_wl_set_bit_rate(struct realtek_dev_s *priv, struct iwreq *iwr);
int realtek_wl_get_country(FAR struct realtek_dev_s *priv, struct iwreq *iwr);
int realtek_wl_set_country(FAR struct realtek_dev_s *priv, struct iwreq *iwr);
int realtek_wl_get_txpower(FAR struct realtek_dev_s *priv, struct iwreq *iwr);
int realtek_wl_set_txpower(FAR struct realtek_dev_s *priv, struct iwreq *iwr);
int realtek_wl_get_pta(FAR struct realtek_dev_s *priv, struct iwreq *iwr);
int realtek_wl_set_pta(FAR struct realtek_dev_s *priv, struct iwreq *iwr);
int realtek_wl_set_auth(FAR struct realtek_dev_s *priv, struct iwreq *iwr);
void realtek_wl_netif_info_handler(int index, void *dev, unsigned char *addr);
void realtek_wl_notify_rx_handler(int index, unsigned int len);
