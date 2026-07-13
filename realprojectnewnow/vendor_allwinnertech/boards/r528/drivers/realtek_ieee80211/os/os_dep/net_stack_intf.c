/* mbed Microcontroller Library
 * Copyright (c) 2013-2016 Realtek Semiconductor Corp.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *                                        
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
 
//#define _NET_STACK_INTF_C_

#include <autoconf.h>
#include <net_stack_intf.h>
#include <osdep_service.h>
#include <wifi_util.h>
#include <realtek_driver.h>
//----- ------------------------------------------------------------------
// External Reference
//----- ------------------------------------------------------------------

/**
 *      rltk_wlan_set_netif_info - set netif hw address and register dev pointer to netif device
 *      @idx_wlan: netif index
 *			    0 for STA only or SoftAP only or STA in STA+SoftAP concurrent mode, 
 *			    1 for SoftAP in STA+SoftAP concurrent mode
 *      @dev: register netdev pointer to LWIP. Reserved.
 *      @dev_addr: set netif hw address
 *
 *      Return Value: None
 */     
void rltk_wlan_set_netif_info(int idx_wlan, void * dev, unsigned char * dev_addr)
{	
	realtek_wl_netif_info_handler(idx_wlan, dev, dev_addr);
}

int netif_is_valid_IP(int idx, unsigned char *ip_dest)
{
	return 1;
}

int netif_get_idx(struct netif* pnetif)
{
	return -1;
}

unsigned char *netif_get_hwaddr(int idx_wlan)
{
	return NULL;
}

void netif_rx(int idx, unsigned int len)
{
	realtek_wl_notify_rx_handler(idx, len);
}

void netif_post_sleep_processing(void)
{

}

void netif_pre_sleep_processing(void)
{

}

