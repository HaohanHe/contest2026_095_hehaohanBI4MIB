#include <nuttx/config.h>
#include <stdio.h>
#include <nuttx/kmalloc.h>
//#include "realtek_driver.h"
#include <wifi_conf.h>
#include <wifi_constants.h>
#include <nuttx/wireless/wireless.h>
#include <realtek_netdev.h>
#include "net_stack_intf.h"
#include "wlan_intf.h"
#include "osdep_service.h"
#include <fcntl.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
#ifdef CONFIG_WIFI_STATISTICS
struct wifi_statistics_s wifi_stats = {
  .period = CONFIG_WIFI_STATISTICS_PERIOD,
};
#endif

extern struct realtek_dev_s g_realtek_dev[2];

struct _sockaddr_t
{
  uint8_t sa_len;
  uint8_t sa_family;
  char sa_data[14];
};
int realtek_readMacAddr_From_File(char *ifname, unsigned char *macaddr)
{
#ifdef CONFIG_USER_WIFIMAC
    int ret;
    struct file filep;
    rtw_memset(macaddr, 0, 18);
    ret=file_open(&filep,WIFIMAC_PATH, O_RDWR);
    if(ret<0)
    {
      DBG_INFO("open macaddr file fail, use efuse %d\n",ret);
      return -1;
    }
    ret = file_read(&filep,macaddr,18);
    if(ret<18)
    {
      DBG_INFO("read macaddr file fail, use efuse %d\n",ret);
      rtw_memset(macaddr, 0, 18);
      file_close(&filep);
      return -1;
    }
    file_close(&filep);
    macaddr[17]=0;
    ret = rltk_set_user_mac(ifname,macaddr);
    if(ret<0)
    {
      DBG_INFO("address = NULL,ues efuse\n");
    } 
#endif
    return 0;
}

int realtek_readEfuse_From_File(unsigned char *filepath, unsigned char *buf, unsigned int len)
{
    int ret;
    unsigned char *ptmpbuf = NULL,*ptr;
    struct file filep;
    unsigned int bufsize = 4096;
    unsigned int count, i, j;
    unsigned char val8;
    int err;

    ptmpbuf = rtw_zmalloc(bufsize);
	  if (ptmpbuf == NULL)
		  return _FALSE;

#ifdef CONFIG_EFUSE_CONFIG_FILE
    if(filepath==NULL)
      ret=file_open(&filep,CONFIG_EFUSE_MAP_PATH, O_RDONLY);
    else
#endif
    ret=file_open(&filep,(const char *)filepath, O_RDONLY);

    if(ret<0)
    {
      DBG_INFO("open efuse file fail, ret=%d\n",ret);
      return -1;
    }

    count = file_read(&filep,ptmpbuf,bufsize);
    if(count<90)
    {
      DBG_INFO("read efuse file fail, only count %d\n",count);
      rtw_mfree(ptmpbuf, bufsize);
      file_close(&filep);
      return -1;
    }
    i = 0;
	  j = 0;
	  ptr = ptmpbuf;
	  while ((j < len) && (i < count)) {
		  if (ptmpbuf[i] == '\0')
			  break;
		  ptr = (unsigned char*)strpbrk((const char*)&ptmpbuf[i], " \t\n\r");
		  if (ptr) {
			  if (ptr == &ptmpbuf[i]) {
				  i++;
				  continue;
			  }
			  /* Add string terminating null */
			  *ptr = 0;
		  } else {
			  ptr = &ptmpbuf[count-1];
		  }

		  err = sscanf((const char*)&ptmpbuf[i], "%hhx", &val8);
		  if (err != 1) {
			  DBG_INFO("Something wrong to parse efuse file, string=%s\n", &ptmpbuf[i]);
		  } else {
			  buf[j] = val8;
			  //printf("i=%d, j=%d, 0x%02x\n", i, j, buf[j]);
			  j++;
		  }
		  i = ptr - ptmpbuf + 1;
	  }
	  rtw_mfree(ptmpbuf, bufsize);
    file_close(&filep);
    return 1;
}

extern unsigned int array_mp_8723f_phy_reg_pg[];
int realtek_readWL_PHY_REG_PG(void)
{
#ifdef CONFIG_USER_PHY_REG_PG
    #define MAX_PARA_FILE_BUF_LEN  49152
    #define DEFAULT_PHY_REG_PG  "/etc/wifi/PHY_REG_PG.txt"
    int ret;
    char *ptmpbuf = NULL,*ptr;
    struct file filep;
    unsigned int band=0,rf_path=0, tx_num=0, addr =0 ,bitmask = 0, data = 0;
    unsigned int count, i, j;
    char val8[5];
    unsigned char valc;
    unsigned int temp[4];

    ptmpbuf = (char *)rtw_zmalloc(MAX_PARA_FILE_BUF_LEN);
	  if (ptmpbuf == NULL)
		{
      DBG_INFO("malloc fail\n");
      return _FALSE;
    }  
    ret=file_open(&filep,CONFIG_USER_PHY_REG_PG_PATH, O_RDONLY);
    if(ret<0)
    {
      ret = file_open(&filep, DEFAULT_PHY_REG_PG, O_RDONLY);
      if (ret<0)
        return -1;
      DBG_INFO("open PHY_REG_PG.txt successfully, filepath:%s\n", DEFAULT_PHY_REG_PG);
    }
    else
    {
      DBG_INFO("open PHY_REG_PG.txt successfully, filepath:%s\n", CONFIG_USER_PHY_REG_PG_PATH);
    }
    count = file_read(&filep,ptmpbuf,MAX_PARA_FILE_BUF_LEN);
    if(count<=0)
    {
      DBG_INFO("read PHY_REG_PG.txt fail, use efuse %d\n",count);
      rtw_mfree((unsigned char *)ptmpbuf, MAX_PARA_FILE_BUF_LEN);
      file_close(&filep);
      return -1;
    }    
    ptr = strtok(ptmpbuf, "\n\r");
    ptr = strtok(NULL, "\n\r"); 
    i=0;
    j=0;

	  while (i<16) {
		  ptr = strtok(NULL, "\n\r"); 
      i++;
      if(rtw_memcmp(ptr,"#[END]#",7))
        continue;

      ret=sscanf(ptr, "#[%4s][%c]#", val8, &valc);
      if(ret>0)
      {
        if(rtw_memcmp(val8,"2.4G",1))
          band=0;
        else
          band=1;   
        if (band==1)
        {
          ret=sscanf(ptr, "#[%2s][%c]#", val8, &valc);  
        }   
               
        rf_path=valc-65;    
        valc=0;
        continue;
      } 
  
      ret=sscanf(ptr, "[%3s] %x %x		 	 %d %d %d %d %*s", val8, &addr,&bitmask,&temp[0],&temp[1],&temp[2],&temp[3]);
      if(ret>0)
      {
        data = ((temp[0]*4)<<24) + ((temp[1]*4)<<16)+((temp[2]*4)<<8)+temp[3]*4;
        if(rtw_memcmp(val8,"1Tx",4))
          tx_num=0;
      }
      array_mp_8723f_phy_reg_pg[j*6]=band;
      array_mp_8723f_phy_reg_pg[j*6+1]=rf_path;
      array_mp_8723f_phy_reg_pg[j*6+2]=tx_num;
      array_mp_8723f_phy_reg_pg[j*6+3]=addr;
      array_mp_8723f_phy_reg_pg[j*6+4]=bitmask;
      array_mp_8723f_phy_reg_pg[j*6+5]=data;
      /*printf(
		 "%s, %s, %s, 0x%X, 0x%08X, 0x%08X,",
		 (array_mp_8723f_phy_reg_pg[j*6] == 0 ? "2.4G" : "  5G"), (array_mp_8723f_phy_reg_pg[j*6+1] == 0 ? "A" : "B"),
		 (array_mp_8723f_phy_reg_pg[j*6+2] == 0 ? "1Tx" : "2Tx"), array_mp_8723f_phy_reg_pg[j*6+3], array_mp_8723f_phy_reg_pg[j*6+4],array_mp_8723f_phy_reg_pg[j*6+5]);*/
      j++;

	  }

	  rtw_mfree((unsigned char*)ptmpbuf, MAX_PARA_FILE_BUF_LEN);
    file_close(&filep);
    #undef DEFAULT_PHY_REG_PG
#endif
    return 1;
}
int realtek_wl_get_mode(FAR struct realtek_dev_s *priv, struct iwreq *iwr)
{
    int mode;
    char ifname[IFNAMSIZ];

    snprintf(ifname, IFNAMSIZ, "wlan%d", priv->devnum);
    wext_get_mode(ifname, &mode);
	  switch(mode) {
		case IW_MODE_MASTER:
			iwr->u.mode = IW_MODE_MASTER;
			break;
		case IW_MODE_INFRA:
		default:
			iwr->u.mode = IW_MODE_INFRA;
			break;
		//default:
			//DBG_INFO("%s(): Unknown mode %d\n", __func__, mode);
			//break;
	} 

    return 0;
}

int realtek_wl_set_mode(FAR struct realtek_dev_s *priv, struct iwreq *iwr)
{
    int mode = RTW_MODE_NONE, ret = 0;
    char ifname[IFNAMSIZ];
    snprintf(ifname, IFNAMSIZ, "wlan%d", priv->devnum);
    if (priv->devnum == 1)
    {
      if (iwr->u.mode == IW_MODE_MASTER)
        {
          mode = RTW_MODE_STA_AP;
        }
      else
        {
          return -EINVAL;
        }
    }
    else
    {
      if(rltk_wlan_running(1))
      {
        mode = RTW_MODE_STA_AP;
        ret = 0;
        goto errout;
      }

      if (iwr->u.mode == IW_MODE_MASTER)
      {
          mode = RTW_MODE_AP;
      }
      else if (iwr->u.mode == IW_MODE_INFRA)
      {
         mode = RTW_MODE_STA;         
      }
      else
      {
          return -EINVAL;
      }
    }
    if (priv->mode == mode)
    {
      return OK;
    }
  if (priv->mode != RTW_MODE_NONE)
    {
      if (priv->mode == RTW_MODE_STA && mode == RTW_MODE_AP)
        {
          if (!wifi_is_connected_to_ap())
            {
              return -EINVAL;
            }
            else
            {
              wifi_off();
              rtw_mdelay_os(20);
              realtek_readMacAddr_From_File(ifname, priv->macaddr);
              realtek_readWL_PHY_REG_PG();
              wext_set_traffic_busy_thres(500);
              ret = wifi_on(mode);
              if (ret < 0)
              {
                goto errout;
              }
            }
        }
      else if (priv->mode == RTW_MODE_AP && mode == RTW_MODE_STA)
        {
          return -EINVAL;
        }
    }
    
    if (priv->mode == RTW_MODE_NONE &&
      rltk_wlan_running(priv->devnum) == false)
    {
        if (priv->devnum == 1 && rltk_wlan_running(0) == true)
        {
          DBG_INFO("wifi on coAP %d\n",mode);
          ret= wifi_on_coAP(mode);
          if (ret < 0)
          {
            goto errout;
          }
        }
        else{
          DBG_INFO("wifi on %d\n",mode);
          realtek_readMacAddr_From_File(ifname, priv->macaddr);
          realtek_readWL_PHY_REG_PG();
          wext_set_traffic_busy_thres(500);
          ret = wifi_on(mode);
          if (ret < 0)
          {
            goto errout;
          }
#ifdef CONFIG_X4B_FACTEST
          wifi_set_autoreconnect(1);
#endif
        }
    }     

errout:
    if (ret)
    {
        mode = RTW_MODE_NONE;
    }

    priv->mode = mode;
    if (mode == RTW_MODE_STA_AP)
    {
      priv->mode = RTW_MODE_AP;
      g_realtek_dev[0].mode = RTW_MODE_STA;
    }  
    
    return ret;
}

int realtek_wl_set_auth(struct realtek_dev_s *priv, struct iwreq *iwr)
{
    return 0;
}

int realtek_wl_set_encode_ext(struct realtek_dev_s *priv, struct iwreq *iwr)
{
    struct iw_encode_ext *ext;
    ext = iwr->u.encoding.pointer;
    int alg ;
    switch (ext->alg)
    {
      case IW_ENCODE_ALG_NONE:
        alg = RTW_SECURITY_OPEN;
        break;

      case IW_ENCODE_ALG_WEP:
        alg = RTW_SECURITY_WEP_PSK;
        break;

      case IW_ENCODE_ALG_TKIP:
        alg = RTW_SECURITY_WPA_TKIP_PSK;
        break;

      case IW_ENCODE_ALG_CCMP:
        alg = RTW_SECURITY_WPA2_AES_PSK;
        break;

      case IW_ENCODE_ALG_AES_CMAC:
        alg = RTW_SECURITY_WPA3_AES_PSK;
        break;

      default:
        //nerr("ERROR: Unknown algorithm %d", ext->alg);
        free(ext);
        return -1;
    }

    priv->assoc.password = (unsigned char *)kmm_zalloc(ext->key_len);
    memcpy(priv->assoc.password, ext->key, ext->key_len);  
    priv->assoc.password_len = ext->key_len;
    priv->assoc.alg = alg;
  return OK;
}

int realtek_wl_get_encode_ext(struct realtek_dev_s *priv, struct iwreq *iwr)
{
  struct iw_encode_ext *ext;
  struct iwreq _iwr = {};
  int ret;

  ret = rltk_wlan_control(SIOCGIWENCODEEXT, iwr);
  if (ret < 0)
    {
      return ret;
    }

  ext = iwr->u.encoding.pointer;
  _iwr.u.data.pointer = (void *)ext->key;

  memcpy(_iwr.ifr_name, iwr->ifr_name, strlen(iwr->ifr_name));

  ret = rltk_wlan_control(SIOCGIWPRIVPASSPHRASE, &_iwr);
  if (ret < 0)
    {
      return ret;
    }

  ext->key_len = _iwr.u.data.length;
  ext->key[ext->key_len] = '\0';

  return ret;
}


int realtek_wl_get_ssid(FAR struct realtek_dev_s *priv, struct iwreq *iwr)
{
    unsigned char ssid[33];
    int ret;
    char ifname[IFNAMSIZ];
    snprintf(ifname, IFNAMSIZ, "wlan%d", priv->devnum);
    ret = wext_get_ssid(ifname, ssid);

    if (ret > 0)
    {
      iwr->u.essid.flags  = iwr->u.data.flags = 1;
      iwr->u.essid.length = iwr->u.data.length = ret+1;
      memcpy(iwr->u.essid.pointer, ssid, iwr->u.essid.length);
    }
    return ret;
}
int realtek_wl_set_ssid(FAR struct realtek_dev_s *priv, struct iwreq *iwr)
{
    void *semaphore;
	  char 				*ssid;
	  rtw_security_t	security_type;
	  char 				*password;
	  int 				ssid_len;
	  int 				password_len;
	  int 				key_id;
    int         channel;
    int flag = 1;
    int ret = 0;
    switch(iwr->u.essid.flags)
    {
      case 0:
        flag = 0;
        break;
      case 1:
        flag = 1;
        break;
      case 2:
        flag = 2;
    } 
    memcpy(priv->assoc.ssid.val, iwr->u.essid.pointer, iwr->u.essid.length);
    priv->assoc.ssid.len = iwr->u.essid.length;

    if(priv->mode == RTW_MODE_STA)
    {
      if (flag == 0)
      {  
        DBG_INFO("wifi_disconnect \n");    
        return wifi_disconnect();
      }
      ssid = (char *)priv->assoc.ssid.val;
      ssid_len = priv->assoc.ssid.len;
      password = (char *)priv->assoc.password;
      password_len = priv->assoc.password_len;
      security_type = (rtw_security_t)priv->assoc.alg;
      if(password_len == 0)
      {
        security_type = RTW_SECURITY_OPEN;
        password = NULL;
        password_len = 0;
      }

      //for wep,use default key_id, for other not use
      key_id = 0;

      semaphore = NULL;

      if(flag == 1)
      { 
        
        ret = wifi_connect(ssid, security_type, password, ssid_len, password_len, key_id, semaphore);
        if(priv->assoc.password)
            kmm_free(priv->assoc.password);
        rtw_memset(&priv->assoc,0,sizeof(struct realtek_associate_s));
        return ret;
      }
    }
    else if((priv->mode == RTW_MODE_STA_AP)||(priv->mode == RTW_MODE_AP))
    {
      if(priv->channel!=0)
        channel=priv->channel;
      else
      {
        ret = wifi_get_channel(&channel);
        if(ret!=0)
        {
          channel = 1;
        }
      }  
      ssid = (char *)priv->assoc.ssid.val;
      ssid_len = priv->assoc.ssid.len;
      password = (char *)priv->assoc.password;
      password_len = priv->assoc.password_len;
      security_type = (rtw_security_t)priv->assoc.alg;
      if(password_len == 0)
      {
        security_type = RTW_SECURITY_OPEN;
        password = NULL;
        password_len = 0;
      }

      if(flag == 1)
      {
         
        ret = wifi_start_ap(ssid, security_type, password, ssid_len, password_len, channel);
        if(priv->assoc.password)
            kmm_free(priv->assoc.password);
        rtw_memset(&priv->assoc,0,sizeof(struct realtek_associate_s));
        return ret;
      }
    }
    return 0;
}


int realtek_wl_get_bssid(FAR struct realtek_dev_s *priv, struct iwreq *iwr)
{
    unsigned char bssid[ETH_ALEN];
    int ret;
    char ifname[IFNAMSIZ];
    snprintf(ifname, IFNAMSIZ, "wlan%d", priv->devnum);
    ret = wext_get_bssid(ifname, bssid);
    if(ret == OK)
    {
      //iwr->u.ap_addr.sa_len = ETH_ALEN;
      memcpy(iwr->u.ap_addr.sa_data, bssid, ETH_ALEN);
      iwr->u.ap_addr.sa_family = ARPHRD_ETHER;
    }
  return ret;
}
int realtek_wl_set_bssid(FAR struct realtek_dev_s *priv, struct iwreq *iwr)
{
  struct _sockaddr_t *addr = (struct _sockaddr_t *)&iwr->u.ap_addr;
  unsigned char null_mac[IFHWADDRLEN] = {};
  char *data;
  unsigned char 	bssid[ETH_ALEN];
	char 			*ssid = NULL;
	rtw_security_t		security_type;
	char 			*password;
	int 				bssid_len;
	int 				ssid_len = 0;
	int 				password_len;
	int 				key_id;
	void				*semaphore;
  addr->sa_family = ARPHRD_ETHER;
  data = addr->sa_data;
  int ret;
  if (!memcmp(data, null_mac, sizeof(null_mac)))
  {
      /* set MAC address last byte to 1
       * since driver will filter the mac with all 0x00 or 0xff
       */

      data[IFHWADDRLEN - 1] = 1;

  }
  else
  {
    ssid = (char *)priv->assoc.ssid.val;
    ssid_len = priv->assoc.ssid.len;
    memcpy(priv->assoc.mac.octet, data, IFHWADDRLEN);
    memcpy(bssid, priv->assoc.mac.octet, IFHWADDRLEN);
    bssid_len = IFHWADDRLEN;
    password = (char *)priv->assoc.password;
    password_len = priv->assoc.password_len;
    security_type = (rtw_security_t)priv->assoc.alg;
    //for wep,use default key_id, for other not use
    key_id = 0;

    semaphore = NULL;

    ret = wifi_connect_bssid(bssid, ssid, security_type, password, bssid_len, 
					                    ssid_len, password_len, key_id, semaphore);
    if(priv->assoc.password)
            kmm_free(priv->assoc.password);
    rtw_memset(&priv->assoc,0,sizeof(struct realtek_associate_s));
    return ret;	
  }

  return rltk_wlan_control(SIOCSIWAP, iwr);
}

static int realtek_freq2chan(FAR int *chan, int freq)
{
  DEBUGASSERT(chan != NULL);

  if ((freq >= 1 && freq <= 14) ||
      (freq >= 36 && freq <= 165))
    {
      *chan = freq;
      return 0;
    }

  if (freq >= 2412 && freq <= 2472)
    {
      /* 1 =< chan <= 13, freq = 2407 + 5 * chan */

      *chan = (freq - 2407) / 5;
    }
  else if (freq == 2484)
    {
      /* chan = 14, freq = 2484 */

      *chan = 14;
    }
  else if (freq >= 5180 && freq <= 5825)
    {
      /* 36 =< chan <= 165, freq = 5000 + 5 * chan */

      *chan = (freq - 5000) / 5;
    }
  else
    {
      return -EDOM;
    }

  return 0;
}

static int realtek_chan2freq(int chan, FAR int *freq)
{
  DEBUGASSERT(freq != NULL);

  if (chan >= 1 && chan <= 13)
    {
      *freq = 2407 + 5 * chan;
    }
  else if (chan == 14)
    {
      *freq = 2484;
    }
  else if (chan >= 36 && chan <= 165)
    {
      *freq = 5000 + 5 * chan;
    }
  else
    {
      return -EDOM;
    }

  return 0;
}
int realtek_wl_set_freq(FAR struct realtek_dev_s *priv, struct iwreq *iwr)
{
  int ret;
  int chan;
  unsigned char *channel_list = NULL;
	unsigned char *pscan_config = NULL;
  int i = 0;
	int num_channel;
  ret = realtek_freq2chan(&chan, (int)iwr->u.freq.m);
  //wlinfo("chan:%d, freq:%d, ret:%d\n", chan, iwr->u.freq.m, ret);
  if (ret == 0)
    {
      if(priv->mode==RTW_MODE_STA)
      {
        num_channel =1;
	      if( num_channel > 0){
          channel_list = (unsigned char *)kmm_zalloc(num_channel);
          if(!channel_list){
            //DBG_INFO("ERROR: Can't malloc memory for channel list");
            ret = -1;
            goto exit;
          } 
          pscan_config = (unsigned char *)kmm_zalloc(num_channel);
          if(!pscan_config){
            //DBG_INFO("ERROR: Can't malloc memory for pscan_config");
            ret = -1;
            goto exit;
          }
          //parse command channel list
          for(i = 0; i < num_channel ; i++){
            *(channel_list + i) = chan;		    
            *(pscan_config + i) = PSCAN_ENABLE;
          }
  
          if(wifi_set_pscan_chan(channel_list, pscan_config, num_channel) < 0){
            //DBG_INFO("ERROR: wifi set partial scan channel fail");
            ret = -1;
            goto exit;
          }
        }
      }  
      priv->channel = chan;

      //priv->assoc.mask |= AMEBAZ_ASSOCIATE_CHANNEL;
    }
exit:
  if(channel_list)
		kmm_free(channel_list);
	if(pscan_config)
		kmm_free(pscan_config);
  return ret;
}

int realtek_wl_get_range(FAR struct realtek_dev_s *priv, struct iwreq *iwr)
{
    int freq;
    int ret;
    int channel=0;
    struct iw_range *range;
    ret = wifi_get_channel(&channel);
    if (ret == 0)
    {
      ret = realtek_chan2freq(channel, &freq);
      if (ret == 0)
        {
          range = (struct iw_range *)iwr->u.data.pointer;
          range->num_frequency=1;
          range->freq[0].m = freq;
          range->freq[0].i = channel;
          range->freq[0].e = 0;
        }
      else
        {
          range = (struct iw_range *)iwr->u.data.pointer;
          range->num_frequency = 0;
          range->freq[0].m = 0;
          range->freq[0].i = 0;
          range->freq[0].e = 0;
        }
    }
    return ret;
}
int realtek_wl_get_freq(FAR struct realtek_dev_s *priv, struct iwreq *iwr)
{
    int freq;
    int ret;
    int channel=0;
    ret = wifi_get_channel(&channel);
    if (ret == 0)
    {
      ret = realtek_chan2freq(channel, &freq);
      if (ret == 0)
        {
          iwr->u.freq.m = (int32_t)freq;
          iwr->u.freq.e = 0;
        }
      else
        {
          iwr->u.freq.m = 0;
          iwr->u.freq.e = 0;
        }
    }

  return ret;
}

int realtek_wl_process_command(struct realtek_dev_s *priv, int cmd, void *req)
{
  
  return rltk_wlan_control(cmd, req);
}

static char *realtek_wl_iwe_add_event(char *stream, char *stop,
                                     struct iw_event *iwe, int event_len)
{
  if (stream + event_len > stop)
  {
    return stream;
  }
  iwe->len = event_len;

  return stream + event_len;
}
int realtek_wl_get_scan_results(struct realtek_dev_s *priv, struct iwreq *iwr)
{
  rtw_scan_result_t cache;
  struct iw_event *iwe;
  int request_size;
  char *start, *stop;
  int i;
  int ret = OK;

  extern internal_scan_handler_t scan_result_handler_ptr;

  while(scan_result_handler_ptr.scan_running ==1 )
  {
    rtw_msleep_os(20);
  }

  if(priv->scan_count==0)
    return -1;

  request_size = priv->scan_count *
                  5 *
                 sizeof(struct iw_event);

  if (iwr->u.data.pointer == NULL ||
      iwr->u.data.length < request_size)
    {
      ret = -E2BIG;
      iwr->u.data.pointer = NULL;
      iwr->u.data.length = request_size;
      return ret;
    }
  start = iwr->u.data.pointer;
  stop = (char *)iwr->u.data.pointer + iwr->u.data.length;
  for (i = priv->scan_count-1; i >=0; i--)
    {
      cache = priv->scan_data[i];

      iwe = (struct iw_event *)start;
      iwe->cmd = SIOCGIWAP;
      iwe->u.ap_addr.sa_family = ARPHRD_ETHER;
      memcpy(&iwe->u.ap_addr.sa_data, cache.BSSID.octet, IFHWADDRLEN);
      start = realtek_wl_iwe_add_event(start, stop, iwe, IW_EV_LEN(ap_addr));

      iwe = (struct iw_event *)start;
      iwe->cmd = SIOCGIWFREQ;
      iwe->u.freq.e = 0;
      iwe->u.freq.m = cache.channel;
      start = realtek_wl_iwe_add_event(start, stop, iwe, IW_EV_LEN(freq));
 
      iwe = (struct iw_event *)start;
      iwe->cmd = IWEVQUAL;
      iwe->u.qual.qual = 0;
      iwe->u.qual.level = cache.signal_strength;
      iwe->u.qual.noise = 0;
      iwe->u.qual.updated |= IW_QUAL_DBM;
      start = realtek_wl_iwe_add_event(start, stop, iwe, IW_EV_LEN(qual));

      iwe = (struct iw_event *)start;
      iwe->cmd = SIOCGIWENCODE;

      if(cache.security == RTW_SECURITY_OPEN)
        iwe->u.data.flags = IW_ENCODE_DISABLED|IW_ENCODE_ALG_NONE;
      else if(cache.security == RTW_SECURITY_WEP_PSK)
        iwe->u.data.flags = IW_ENCODE_ENABLED |IW_ENCODE_NOKEY|IW_ENCODE_ALG_WEP;
      else if((cache.security == RTW_SECURITY_WPA_TKIP_PSK)||
              (cache.security == RTW_SECURITY_WPA2_TKIP_PSK) )
        iwe->u.data.flags = IW_ENCODE_ENABLED |IW_ENCODE_NOKEY|IW_ENCODE_ALG_TKIP;
      else if((cache.security == RTW_SECURITY_WPA2_AES_PSK) ||
              (cache.security == RTW_SECURITY_WPA_WPA2_MIXED_PSK)||
              (cache.security == RTW_SECURITY_WPA_AES_PSK))
        iwe->u.data.flags = IW_ENCODE_ENABLED |IW_ENCODE_NOKEY|IW_ENCODE_ALG_CCMP;
      else if(cache.security == RTW_SECURITY_WPA3_AES_PSK)
        iwe->u.data.flags = IW_ENCODE_ENABLED |IW_ENCODE_NOKEY|IW_ENCODE_ALG_AES_CMAC;
      else
        iwe->u.data.flags = 0;

      iwe->u.data.length = 0;
      iwe->u.essid.pointer = NULL;
      start = realtek_wl_iwe_add_event(start, stop, iwe, IW_EV_LEN(data));


      iwe = (struct iw_event *)start;
      iwe->cmd = SIOCGIWESSID;
      iwe->u.essid.flags = 0;
      iwe->u.essid.length = cache.SSID.len;
      iwe->u.essid.pointer = (FAR void *)sizeof(iwe->u.essid);
      memcpy(&iwe->u.essid + 1, cache.SSID.val, cache.SSID.len);
      start = realtek_wl_iwe_add_event(start, stop, iwe,
                                   IW_EV_LEN(essid) + ((cache.SSID.len + 3) & -4));

    }
  iwr->u.data.length = start - (char *)iwr->u.data.pointer;
  if (ret < 0)
    {
      iwr->u.data.length = 0;
    }

  return ret;
}

static rtw_result_t app_scan_result_handler( rtw_scan_handler_result_t* malloced_scan_result )
{
  struct realtek_dev_s *priv = (struct realtek_dev_s *)malloced_scan_result->user_data;

	if (malloced_scan_result->scan_complete != RTW_TRUE) {
		rtw_scan_result_t* record = &malloced_scan_result->ap_details;
		record->SSID.val[record->SSID.len] = 0; /* Ensure the SSID is null terminated */
    memcpy(&priv->scan_data[priv->scan_count],record,sizeof(rtw_scan_result_t));
    priv->scan_count++;

	} 
	return RTW_SUCCESS;
}

static int app_scan_result_handler_ssid(char*buf, int buflen, char *Ssid, void *user_data)
{
  struct realtek_dev_s *priv = (struct realtek_dev_s *)user_data;
  rtw_scan_result_t record;
  int i;
  int plen = 0;
			while(plen < buflen){
				int len, rssi, ssid_len, security_mode;
				int wps_password_id;
				char *mac, *ssid;
				// len
				len = (int)*(buf + plen);
				// check end
				if(len < (1 + 6 + 4 + 1 + 1 + 1)) break;
				// mac
				mac = buf + plen + 1;
        memcpy(record.BSSID.octet,mac,ETH_ALEN);
				// rssi
				rssi = *(int*)(buf + plen + 1 + 6);
        record.signal_strength = rssi;
				// security_mode
				security_mode = (int)*(buf + plen + 1 + 6 + 4);
				switch (security_mode) {
					case IW_ENCODE_ALG_NONE:
            record.security = RTW_SECURITY_OPEN;
						break;
					case IW_ENCODE_ALG_WEP:
            record.security = RTW_SECURITY_WEP_PSK;
						break;
          case IW_ENCODE_ALG_TKIP:
            record.security = RTW_SECURITY_WPA2_TKIP_PSK;
						break;
					case IW_ENCODE_ALG_CCMP:
            record.security = RTW_SECURITY_WPA_WPA2_MIXED_PSK;
						break;
          case IW_ENCODE_ALG_AES_CMAC:
            record.security = RTW_SECURITY_WPA3_AES_PSK;
 						break;
				}
        
				// password id
				wps_password_id = (int)*(buf + plen + 1 + 6 + 4 + 1);
				record.wps_type = wps_password_id;
        record.channel = *(buf + plen + 1 + 6 + 4 + 1 + 1);
				// ssid
				ssid_len = len - 1 - 6 - 4 - 1 - 1 - 1;
				ssid = buf + plen + 1 + 6 + 4 + 1 + 1 + 1;

        record.SSID.len = ssid_len;
        memcpy(record.SSID.val,ssid,ssid_len);
				plen += len;

        for(i=0; i<priv->scan_count; i++){
          if(CMP_MAC(priv->scan_data[i].BSSID.octet, record.BSSID.octet)){
            if(record.signal_strength > priv->scan_data[i].signal_strength){
				      priv->scan_data[i].signal_strength =  record.signal_strength;
            }
             break;
          }
        }          
        if(i == priv->scan_count){
          memcpy(&priv->scan_data[priv->scan_count],&record,sizeof(rtw_scan_result_t));
          priv->scan_count++;
        }
      }
	return RTW_SUCCESS;
}

int realtek_wl_start_scan(struct realtek_dev_s *priv, struct iwreq *iwr)
{
  unsigned char * ssid = NULL;
  int ssid_len;
  unsigned char *channel_list = NULL;
	unsigned char *pscan_config = NULL;
  int i = 0;
	int num_channel;
  struct iw_scan_req *req = iwr->u.essid.pointer;;
  int scan_buf_len = 500;	
  int ret;

  //wext_private_command("wlan0","dbg sdio f0",1);
  if(iwr->u.essid.flags== IW_SCAN_THIS_ESSID)
  {
    ssid = req->essid;
    ssid_len = req->essid_len;
  } 

  num_channel = req->num_channels;
	if( num_channel > 0){
    channel_list = (unsigned char *)kmm_zalloc(num_channel);
    if(!channel_list){
      //DBG_INFO("ERROR: Can't malloc memory for channel list");
      ret = -1;
      goto exit;
    }
    pscan_config = (unsigned char *)kmm_zalloc(num_channel);
    if(!pscan_config){
      //DBG_INFO("ERROR: Can't malloc memory for pscan_config");
      ret = -1;
      goto exit;
    }
    //parse command channel list
    for(i = 0; i <= num_channel ; i++){
      *(channel_list + i) = req->channel_list[i].m;		    
      *(pscan_config + i) = PSCAN_ENABLE;
    }
  
    if(wifi_set_pscan_chan(channel_list, pscan_config, num_channel) < 0){
      //DBG_INFO("ERROR: wifi set partial scan channel fail");
      ret = -1;
      goto exit;
    }
  }
  if(priv->scan_count>0)
  {
    priv->scan_count=0;
    memset(priv->scan_data,0,sizeof(priv->scan_data));
  }

  if (ssid !=NULL)
  {
    ret = wifi_scan_networks_with_ssid(app_scan_result_handler_ssid, priv, scan_buf_len, (char *)ssid, ssid_len) ;
  }
  else
  {
    ret = wifi_scan_networks(app_scan_result_handler, priv);
  }

exit:
	if(channel_list)
		kmm_free(channel_list);
	if(pscan_config)
		kmm_free(pscan_config);
  return ret;
}

int realtek_wl_get_bit_rate(struct realtek_dev_s *priv, struct iwreq *iwr)
{
  int ret;
  char *ifname;
  unsigned char prate; 
  unsigned char fixed; 
  enum realtek_bitrate_flag_e
  {
    RTW_BITRATE_AUTO,
    RTW_BITRATE_FIXED
  };
  ifname = priv->dev.netdev.d_ifname;
  ret = wext_get_max_support_data_rate(ifname, &prate, &fixed);
  if (ret !=0)
  {
    iwr->u.bitrate.disabled  = TRUE;
    iwr->u.bitrate.value = 0;
  }  
  else
  {
    if (fixed == 1)
    {
      iwr->u.bitrate.fixed = RTW_BITRATE_FIXED;
      iwr->u.bitrate.value = (int)(prate/2.0*1000);
    }
    else
    {
      iwr->u.bitrate.fixed = RTW_BITRATE_AUTO;
      iwr->u.bitrate.value = (int)(prate/2.0*1000);
    }
    iwr->u.bitrate.disabled  = false;
    
  }  
  return  0;
}

int realtek_wl_set_bit_rate(struct realtek_dev_s *priv, struct iwreq *iwr)
{
  char *ifname;
  unsigned char prate; 
  char cmd[14];
  enum realtek_bitrate_flag_e
  {
    RTW_BITRATE_AUTO,
    RTW_BITRATE_FIXED
  };
  ifname = priv->dev.netdev.d_ifname;

  if(iwr->u.bitrate.fixed!=RTW_BITRATE_FIXED)
    return  -1;

  prate = iwr->u.bitrate.value;
  snprintf(cmd,14,"dbg fixra %d",prate);
  wext_private_command(ifname,cmd,1);
  return  0;
}

int realtek_wl_get_country(FAR struct realtek_dev_s *priv, struct iwreq *iwr)
{
    char *country=iwr->u.data.pointer;
    memcpy(country,priv->country,3);
    return 0;
}
int realtek_wl_set_country(FAR struct realtek_dev_s *priv, struct iwreq *iwr)
{
  const unsigned char *country = iwr->u.essid.pointer;
  int ret;

    ret = wifi_set_country((unsigned char *)country);
    if(ret == 0)
    {
      memcpy(g_realtek_dev[0].country,country,3);
      memcpy(g_realtek_dev[1].country,country,3);
    }  
    return ret;
}


int realtek_wl_get_txpower(FAR struct realtek_dev_s *priv, struct iwreq *iwr)
{
    int ret=-1;
    unsigned char poweridx;
    char ifname[IFNAMSIZ];
    snprintf(ifname, IFNAMSIZ, "wlan%d", priv->devnum);
    ret = wext_get_txpower(ifname, &poweridx);
    if (ret == OK)
    {    
      iwr->u.txpower.value   = poweridx;
      iwr->u.txpower.fixed    = 0;
      iwr->u.txpower.disabled = 0;
      iwr->u.txpower.flags    = IW_TXPOW_RELATIVE;
    }
    return ret;
}

int realtek_wl_set_txpower(FAR struct realtek_dev_s *priv, struct iwreq *iwr)
{
  int ret=-1;
  /*char poweridx;
  char ifname[IFNAMSIZ];
  snprintf(ifname, IFNAMSIZ, "wlan%d", priv->devnum);
  if(iwr->u.txpower.flags    == IW_TXPOW_RELATIVE)
  {
    poweridx = iwr->u.txpower.value;
      ret = wext_set_txpower(ifname,poweridx);
  }*/
  return ret;
}

int realtek_wl_get_pta(FAR struct realtek_dev_s *priv, struct iwreq *iwr)
{
  iwr->u.param.value = IW_PTA_PRIORITY_WLAN_MAXIMIZED;
  return 0;
}
int realtek_wl_set_pta(FAR struct realtek_dev_s *priv, struct iwreq *iwr)
{
  return -1;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
void realtek_wl_netif_info_handler(int index, void *dev, unsigned char *addr)
{
  FAR struct realtek_dev_s *priv = &g_realtek_dev[index];
  if (!priv || index != priv->devnum)
    {
      return;
    }

  memcpy(priv->dev.netdev.d_mac.ether.ether_addr_octet, addr, IFHWADDRLEN);
}

void realtek_wl_notify_rx_handler(int index, unsigned int len)
{
  FAR struct realtek_dev_s *priv = &g_realtek_dev[index];
  if (!priv || index != priv->devnum || !len)
    {
      return;
    }

  realtek_netdev_notify_receive(priv, index, len);
}
typedef int (*rtw_efuse_file_read_ptr)(unsigned char *filepath, unsigned char *buf, unsigned int len);
extern rtw_efuse_file_read_ptr rtw_efuse_file_read;
int realtek_wl_initialize(unsigned char mode)
{
    int ret = OK;
    char ifname[IFNAMSIZ];
    snprintf(ifname, IFNAMSIZ, "wlan%d", g_realtek_dev[0].devnum);

    ret = realtek_netdev_init();   
    if(ret != OK)
    {
      DBG_INFO("realtek_netdev_init failed");
      return ret;
    }
   
    rtw_efuse_file_read = realtek_readEfuse_From_File;
    if(mode == RTW_MODE_STA_AP)
    {
        g_realtek_dev[0].mode = RTW_MODE_STA;
        g_realtek_dev[1].mode = RTW_MODE_AP;
        realtek_readMacAddr_From_File(ifname, g_realtek_dev[0].macaddr);
        realtek_readWL_PHY_REG_PG();
        wext_set_traffic_busy_thres(500);
        ret= wifi_on(RTW_MODE_STA_AP);
    }
    else if(mode == RTW_MODE_STA)
    {
      g_realtek_dev[0].mode = RTW_MODE_STA;
      
      realtek_readMacAddr_From_File(ifname, g_realtek_dev[0].macaddr);
      realtek_readWL_PHY_REG_PG();
      wext_set_traffic_busy_thres(500);
      ret = wifi_on(RTW_MODE_STA);
    }
   
    if(ret<0)
    {
      g_realtek_dev[0].mode = RTW_MODE_NONE;
      g_realtek_dev[1].mode = RTW_MODE_NONE;
    }
    DBG_INFO("realtek_wl_initialize successful");
    return ret;
}

int realtek_wl_deinitialize(void)
{

    realtek_netdev_deinit();
    return wifi_off(); 
}

#ifdef CONFIG_WIFI_STATISTICS
#define LINK_STATISTICS_ITEM_NUM 7
static void calc_statistics(unsigned int *cur, unsigned int *last, unsigned int *diff)
{
    int i = 0;

    for (; i < LINK_STATISTICS_ITEM_NUM - 1; i++) {// index 6 is not used now
        if (cur[i] < last[i]) {
            diff[i] = 0xffffffff - last[i] + cur[i];
        } else {
            diff[i] = cur[i] - last[i];
        }
        last[i] = cur[i];
    }
}

void realtek_dump_wifi_statistics(void *priv)
{
    struct ntimer_wrapper *wrap = (struct ntimer_wrapper *)priv;
    struct realtek_dev_s *dev = wifi_stats.dev;
    static unsigned int last_stats[LINK_STATISTICS_ITEM_NUM] = {0};
    unsigned int cur_stats[LINK_STATISTICS_ITEM_NUM] = {0}; // total statistics after wifi on
    unsigned int stats[LINK_STATISTICS_ITEM_NUM] = {0}; // statistics in CONFIG_STATISTICS_PERIOD(s)
    /* 0 tx pkt successfully
     * 1 tx drop
     * 2 tx bytes successfully
     * 3 rx pkt successfully
     * 4 rx drop
     * 5 rx bytes successfully
     * 6 rx overflow, not used now
     */

    unsigned char channel = 0;
    int rssi = 0;
    char ifname[IFNAMSIZ] = {0};

    if (!dev) {
        goto nexttime;
    }

    snprintf(ifname, IFNAMSIZ, "wlan%d", dev->devnum);
    wext_get_channel(ifname, &channel);
    wext_get_rssi(ifname, &rssi);
    wext_get_netinfo(ifname, cur_stats);
    calc_statistics(cur_stats, last_stats, stats);

    rtw_printf("[%s] \"channel: %d, rssi: %d, in %ds: txSuc %d, txBytes %d, txDrop %d, rxSuc %d, rxBytes %d, rxDrop %d\"\n",
        ifname, channel, rssi, wifi_stats.period,
        stats[0], stats[2], stats[1], stats[3], stats[5], stats[4]);
nexttime:
    work_queue(LPWORK, &wrap->work, wrap->callback, wrap, wifi_stats.period * 10000);
}
#endif
