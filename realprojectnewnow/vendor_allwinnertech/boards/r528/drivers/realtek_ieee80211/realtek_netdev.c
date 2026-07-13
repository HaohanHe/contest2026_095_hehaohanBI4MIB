#include <nuttx/config.h>
#include <string.h>
#include <nuttx/arch.h>
#include <nuttx/irq.h>
#include <nuttx/net/netdev_lowerhalf.h>
#include <nuttx/kmalloc.h>
#include <nuttx/kthread.h>
#include <nuttx/wireless/wireless.h>
#include <stdatomic.h>
#include <wireless.h>
#include <nuttx/net/net.h>
#include "net_stack_intf.h"
#include "realtek_netdev.h"
#include <stdio.h>
#include <wifi_conf.h>
#include <osdep_service.h>

#ifdef atomic_set
#undef atomic_set
#endif

#ifdef atomic_read
#undef atomic_read
#endif

/* Get index from dev pointer. */

#define DEVIDX(p) ((struct realtek_dev_s *)(p) - g_realtek_dev)
#if TX_POLL_THREAD
sem_t * tx_sema_t;
#endif
/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/
int realtek_transmit(FAR struct netdev_lowerhalf_s *dev, FAR netpkt_t *pkt);
netpkt_t *realtek_receive(struct netdev_lowerhalf_s *dev);
int realtek_ifup(FAR struct netdev_lowerhalf_s *dev);
int realtek_ifdown(FAR struct netdev_lowerhalf_s *dev);
int realtek_ioctl(FAR struct netdev_lowerhalf_s *dev, int cmd,
                 unsigned long arg);
/****************************************************************************
 * Private Data
 ****************************************************************************/
struct realtek_dev_s g_realtek_dev[2];
struct netdev_ops_s g_ops =
{
  realtek_ifup,
  realtek_ifdown, 
  realtek_transmit,
  realtek_receive,
#ifdef CONFIG_NET_MCASTGROUP
  NULL,
  NULL,
#endif
#ifdef CONFIG_NETDEV_IOCTL
   realtek_ioctl
#endif
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/
int realtek_transmit(FAR struct netdev_lowerhalf_s *dev, FAR netpkt_t *pkt)
{
  struct realtek_dev_s *priv = (struct realtek_dev_s *)dev;

  UNUSED(priv);
#if TX_POLL_THREAD
    struct realtek_pkt_list *tx_pkt;
  _irqL irqL;
  tx_pkt = malloc(sizeof(struct realtek_pkt_list));
  if(tx_pkt==NULL)
  {
    DBG_INFO("malloc tx list fail \n");
    return -ENOMEM;
  }
  if(priv->free_tx_count<=0)
  {
    DBG_INFO("no free tx pkt\n");
    free(tx_pkt);
    return -ENOMEM;
  } 
  rtw_init_listhead(&(tx_pkt->list));
  tx_pkt->pkt=pkt;
  rtw_enter_critical_bh(&priv->tx_buff_queue.lock, &irqL);
  rtw_list_insert_tail(&(tx_pkt->list), get_list_head(&priv->tx_buff_queue));
	priv->free_tx_count--;
  if(priv->free_tx_count<0)
    DBG_INFO("free tx count out range \n");
  rtw_exit_critical_bh(&priv->tx_buff_queue.lock, &irqL);
  sem_post(tx_sema_t);
#else
  unsigned int         len;
  FAR struct sk_buff *skb;
  int devidx;
  _irqL irql;

  for (devidx = 0; devidx < 2; devidx++) {
    if (dev == &g_realtek_dev[devidx].dev) {
      break;
    }
  }
  save_and_cli(&irql);
	if (rltk_wlan_check_isup(devidx)) {
		rltk_wlan_tx_inc(devidx);
	} else {
		DBG_INFO("netif is DOWN");
		restore_flags(irql);
		return -1;
	}
	restore_flags(irql);
  len = netpkt_getdatalen(dev, pkt);
alloc_skb:
  skb = rltk_wlan_alloc_skb(len);
  if (!skb)
  {
    save_and_cli(&irql);
	  rltk_wlan_tx_dec(devidx);
	  restore_flags(irql);
    rtw_msleep_os(1);
    goto alloc_skb;
  }
  netpkt_copyout(dev, skb->tail, pkt, len, 0);
  skb_put(skb, len);		   
  rltk_wlan_send_skb(devidx, skb);
  netpkt_free(dev, pkt, NETPKT_TX);

  save_and_cli(&irql);
	rltk_wlan_tx_dec(devidx);
	restore_flags(irql);

  netdev_lower_txdone(dev);
#endif

  return OK;
}

netpkt_t *realtek_receive(struct netdev_lowerhalf_s *dev)
{
  struct realtek_dev_s *priv = (struct realtek_dev_s *)dev;
  netpkt_t            *pkt = NULL;
  struct realtek_pkt_list *rx_pkt;
  _irqL irqL;
  _list	*plist, *phead;
  if (rltk_wlan_running(DEVIDX(priv)))
    {

      rtw_enter_critical_bh(&priv->rx_buff_queue.lock, &irqL);
      if(rtw_queue_empty(&(priv->rx_buff_queue)) == _TRUE)
	    {
          rtw_exit_critical_bh(&priv->rx_buff_queue.lock, &irqL);
		      return NULL;
	    }
	    else
	    {
		    phead = get_list_head(&(priv->rx_buff_queue));

		    plist = get_next(phead);

		    rx_pkt = LIST_CONTAINOR(plist, struct realtek_pkt_list, list);

		    rtw_list_delete(&rx_pkt->list);
				priv->free_rx_count++;
	    }
      rtw_exit_critical_bh(&priv->rx_buff_queue.lock, &irqL);
      pkt = rx_pkt->pkt;
      free(rx_pkt);
    }

  return pkt;
}

int realtek_ifup(struct netdev_lowerhalf_s *dev)
{
  struct realtek_dev_s *priv = (struct realtek_dev_s *)dev;

  if (!IFF_IS_UP(dev->netdev.d_flags))
  {
    priv->mode = RTW_MODE_NONE;
  }
  return OK;
}

int realtek_ifdown(struct netdev_lowerhalf_s *dev)
{
  struct realtek_dev_s *priv = (struct realtek_dev_s *)dev;

  if (!rltk_wlan_running(0) && !rltk_wlan_running(1))
  {
      DBG_INFO("wlan is not running\r\n");
      if (IFF_IS_RUNNING(dev->netdev.d_flags))
      {
#ifdef CONFIG_WIFI_STATISTICS
        rtw_timerStop(wifi_stats.timer_wrap, 0 /*useless*/);
#endif
        netdev_lower_carrier_off(dev);
        priv->mode= RTW_MODE_NONE;
      }
      return OK;
  }

  if (priv->devnum == 0 && rltk_wlan_running(1))
  {
      DBG_INFO("must ifdown wlan 1 first\r\n");
      return ERROR;
  }

  if (IFF_IS_UP(dev->netdev.d_flags))
  {
    if (IFF_IS_RUNNING(dev->netdev.d_flags))
    {
#ifdef CONFIG_WIFI_STATISTICS
      rtw_timerStop(wifi_stats.timer_wrap, 0 /*useless*/);
#endif
      netdev_lower_carrier_off(dev);
    }
    if (priv->devnum == 0)
    {
      wifi_off();
      priv->mode= RTW_MODE_NONE;
    }
    else{
      wifi_off_coAP();
      while(rltk_wlan_running(1))
          rtw_msleep_os(50);
      priv->mode= RTW_MODE_NONE;
    }
  }


  return OK;
}

void realtek_netdev_notify_receive(FAR struct realtek_dev_s *priv,
                                  int index, unsigned int len)
{
  FAR struct netdev_lowerhalf_s *dev = &priv->dev;
  FAR struct sk_buff *skb;

  netpkt_t            *pkt = NULL;
  _irqL irqL;
  struct realtek_pkt_list *rx_pkt;

  skb = rltk_wlan_get_recv_skb(index);

  if (skb == NULL)
  {
     return;
  }
  if (!IFF_IS_UP(dev->netdev.d_flags))
  {
    skb_pull(skb, len);
    return;
  }

  rx_pkt = malloc(sizeof(struct realtek_pkt_list));
  if(rx_pkt==NULL)
  {
    skb_pull(skb, len);
    return;
  }

  if(priv->free_rx_count<=0)
  {
    DBG_INFO("no free rx pkt\n");
    skb_pull(skb, len);
    free(rx_pkt);
    return;
  } 
  pkt = netpkt_alloc(dev, NETPKT_RX);
  if(pkt==NULL)
  {
    DBG_INFO("dont have enough pkt 1\n");
    skb_pull(skb, len);
    free(rx_pkt);
    return;
  } 

  rtw_init_listhead(&(rx_pkt->list));
  netpkt_copyin(dev, pkt, skb->data, len , 0);
  rx_pkt->pkt=pkt;
  rtw_enter_critical_bh(&priv->rx_buff_queue.lock, &irqL);
  rtw_list_insert_tail(&(rx_pkt->list), get_list_head(&priv->rx_buff_queue));
  priv->free_rx_count--;
  if(priv->free_rx_count<0)
    DBG_INFO("free rx count out range \n");
  rtw_exit_critical_bh(&priv->rx_buff_queue.lock, &irqL);
  netdev_lower_rxready(dev);

}

int realtek_ioctl(FAR struct netdev_lowerhalf_s *dev, int cmd,
                 unsigned long arg)
{
  FAR struct realtek_dev_s *priv = (struct realtek_dev_s *)dev;
  int ret;
  DBG_INFO("%s,cmd:%x\n",__func__,cmd);
  switch (cmd)
    {
    case SIOCSIWSCAN:
      ret = realtek_wl_start_scan(priv, (void *)arg);
      break;
    case SIOCGIWSCAN:
      ret = realtek_wl_get_scan_results(priv, (void *)arg);
      break;
    case SIOCSIWENCODEEXT:
      ret = realtek_wl_set_encode_ext(priv, (void *)arg);
      break;
    case SIOCGIWENCODEEXT:
      ret = realtek_wl_get_encode_ext(priv, (void *)arg);
      break;
    case SIOCSIWESSID:
      ret = realtek_wl_set_ssid(priv, (void *)arg);
      break;
    case SIOCSIWAP:
      ret = realtek_wl_set_bssid(priv, (void *)arg);
      break;
    case SIOCSIWMODE:
      ret = realtek_wl_set_mode(priv, (void *)arg);
      break;
    case SIOCGIWCOUNTRY:
      ret = realtek_wl_get_country(priv, (void *)arg);
      break;
    case SIOCSIWCOUNTRY:
      ret = realtek_wl_set_country(priv, (void *)arg);
      break;
    case SIOCGIWRANGE:
      ret = realtek_wl_get_range(priv, (void *)arg);
    case SIOCGIWFREQ:
      ret = realtek_wl_get_freq(priv, (void *)arg);
      break;
    case SIOCSIWFREQ:
      ret = realtek_wl_set_freq(priv, (void *)arg);
      break;
    case SIOCGIWTXPOW:
      ret = realtek_wl_get_txpower(priv, (void *)arg);
      break;
    case SIOCSIWTXPOW:
      ret = realtek_wl_set_txpower(priv, (void *)arg);
      break;
    case SIOCGIWRATE:
      ret = realtek_wl_get_bit_rate(priv, (void *)arg);
      break;
    case SIOCSIWRATE:
      ret = realtek_wl_set_bit_rate(priv, (void *)arg);
      break;
    case SIOCGIWESSID:
      ret = realtek_wl_get_ssid(priv, (void *)arg);
      break;
    case SIOCGIWAP:
      ret = realtek_wl_get_bssid(priv, (void *)arg);
      break;
    case SIOCGIWMODE:  
      ret = realtek_wl_get_mode(priv, (void *)arg);
      break;   
    case SIOCSIWAUTH:
      ret = realtek_wl_set_auth(priv, (void *)arg);
    case SIOCGIWSENS: 
    case SIOCGIWAUTH:
      ret = realtek_wl_process_command(priv, cmd, (void *)arg);
      break;
    default:
      //wlwarn("ERROR: Unrecognized IOCTL command: %d\n", cmd);
      ret = -ENOTTY;  /* Special return value for this case */
      break;
    }

  return ret;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
#if TX_POLL_THREAD
int realtek_txpool_thread(int argc, char **argv);
#endif
int   realtek_netdev_init(void)
{
  struct netdev_lowerhalf_s *dev;
  int devidx;
  int rettemp;

#ifdef CONFIG_WIFI_STATISTICS
  wifi_stats.timer_wrap = rtw_timerCreate(NULL /*useless*/, 0 /*useless*/,
          0 /*useless*/, NULL /*useless*/, realtek_dump_wifi_statistics);
  if (!wifi_stats.timer_wrap) {
      return -1;
  }
#endif

  for (devidx = 0; devidx < 2; devidx++)
    {
      dev = &g_realtek_dev[devidx].dev;
      atomic_init(&dev->quota[NETPKT_TX], 100);
      atomic_init(&dev->quota[NETPKT_RX], 200);


#if TX_POLL_THREAD
      rtw_init_queue(&(g_realtek_dev[devidx].tx_buff_queue));
      g_realtek_dev[devidx].free_tx_count=512;
#endif
      rtw_init_queue(&(g_realtek_dev[devidx].rx_buff_queue));
      g_realtek_dev[devidx].free_rx_count=200;

      dev->ops = &g_ops;
      g_realtek_dev[devidx].devnum = devidx;
      g_realtek_dev[devidx].mode= RTW_MODE_NONE;
      memcpy(g_realtek_dev[devidx].country,"CN",3);

      /* Register the device with the OS so that socket IOCTLs can be
       * performed
       */

      rettemp = netdev_lower_register(dev, NET_LL_IEEE80211);
      DBG_INFO("netdev_lower_register[%d] ret is %d",devidx, rettemp);
    }

#ifdef CONFIG_WIFI_STATISTICS
    wifi_stats.dev = &g_realtek_dev[0];
#endif
#if TX_POLL_THREAD
    int ret;
	  FAR char *argv[2];
  	tx_sema_t = calloc(1, sizeof(sem_t));
  	if (!tx_sema_t)
    {
      return -1;
    }

	  if (sem_init(tx_sema_t, 0, 1))
	    {
      free(tx_sema_t);
      return -1;
    }

  	argv[0] = NULL;
  	argv[1] = NULL;
	  ret = kthread_create("realtek_txpool_thread",
                       106,
                       10240,
                       realtek_txpool_thread, argv);

	  if (ret <= 0)
    {
      DBG_INFO("kthread_create is failed");
      return -EBADE;
    }
#endif

  return OK;
}
#if TX_POLL_THREAD
extern int tx_skbdata_used_num;
int realtek_txpool_thread(int argc, char **argv)
{
  int ret;
  _queue *tx_queue = NULL;
  _list *frame_plist, *frame_phead;
  struct realtek_pkt_list *tx_pkt;
  unsigned int         len;
  FAR struct netdev_lowerhalf_s *dev;
  FAR struct sk_buff *skb;
  int devidx;
  _irqL irqL,irqL1;
  while (1)
  {
	  ret = nxsem_wait(tx_sema_t);

    if (ret == !OK)
		  break;

    for(devidx=0;devidx<2;devidx++)
    {
      dev = &g_realtek_dev[devidx].dev;
      tx_queue = &g_realtek_dev[devidx].tx_buff_queue;
      
      frame_phead = get_list_head(tx_queue);
			frame_plist = get_next(frame_phead);      
			while (1)
			{
          rtw_enter_critical_bh(&tx_queue->lock, &irqL);
          if (rtw_end_of_queue_search(frame_phead, frame_plist) == _TRUE)
          {
            rtw_exit_critical_bh(&tx_queue->lock, &irqL);
            break;
          }         
		  /*if(tx_skbdata_used_num>MAX_TX_SKB_DATA_NUM-3)
          {
            rtw_exit_critical_bh(&tx_queue->lock, &irqL);
            rtw_msleep_os(1);
            continue;
          }*/
          rtw_exit_critical_bh(&tx_queue->lock, &irqL);
          save_and_cli(&irqL1);
	        if (rltk_wlan_check_isup(devidx)) {
		        rltk_wlan_tx_inc(devidx);
	        } else {
		        DBG_INFO("netif is DOWN");
		        restore_flags(irqL1);
            rtw_enter_critical_bh(&tx_queue->lock, &irqL);
            frame_phead = get_list_head(tx_queue); 
            while (rtw_is_list_empty(frame_phead) == _FALSE)
			      {
				      frame_plist = get_next(frame_phead);
				      tx_pkt = LIST_CONTAINOR(frame_plist, struct realtek_pkt_list, list); 
              rtw_list_delete(&tx_pkt->list);
              netpkt_free(dev, tx_pkt->pkt, NETPKT_TX);
              free(tx_pkt);          
            }
            g_realtek_dev[devidx].free_tx_count=512; 
            rtw_exit_critical_bh(&tx_queue->lock, &irqL);
		        break;
	        }
	        restore_flags(irqL1);
          rtw_enter_critical_bh(&tx_queue->lock, &irqL);
          tx_pkt = LIST_CONTAINOR(frame_plist, struct realtek_pkt_list, list);  
          frame_plist = get_next(frame_plist);       
          len = netpkt_getdatalen(dev, tx_pkt->pkt);
          rtw_exit_critical_bh(&tx_queue->lock, &irqL);          
          skb = rltk_wlan_alloc_skb(len);
          rtw_enter_critical_bh(&tx_queue->lock, &irqL);
          if (!skb)
          {
            rtw_exit_critical_bh(&tx_queue->lock, &irqL);
            save_and_cli(&irqL1);
	          rltk_wlan_tx_dec(devidx);           
	          restore_flags(irqL1);
            rtw_msleep_os(1);
            continue;
          }
          rtw_list_delete(&tx_pkt->list);
          g_realtek_dev[devidx].free_tx_count++; 
          rtw_exit_critical_bh(&tx_queue->lock, &irqL);
          netpkt_copyout(dev, skb->tail, tx_pkt->pkt, len, 0);
          skb_put(skb, len);	   
          rltk_wlan_send_skb(devidx, skb);
          netpkt_free(dev, tx_pkt->pkt, NETPKT_TX);
          free(tx_pkt);
          save_and_cli(&irqL1);
	        rltk_wlan_tx_dec(devidx);
	        restore_flags(irqL1);
          netdev_lower_txdone(dev);

      }

    }
  }
  return 0;
}
#endif
int   realtek_netdev_deinit(void)
{
  struct netdev_lowerhalf_s *dev;
  int devidx;

#ifdef CONFIG_WIFI_STATISTICS
  if (wifi_stats.timer_wrap) {
      rtw_timerDelete(wifi_stats.timer_wrap, 0 /*useless*/);
      wifi_stats.dev = NULL;
  }
#endif

  for (devidx = 0; devidx < 2; devidx++)
    {
      dev = &g_realtek_dev[devidx].dev;
      g_realtek_dev[devidx].mode= RTW_MODE_NONE;
      /* Register the device with the OS so that socket IOCTLs can be
       * performed
       */

      netdev_lower_unregister(dev);
    }
  
  return OK;
}
