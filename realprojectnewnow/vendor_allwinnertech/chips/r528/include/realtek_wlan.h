/****************************************************************************
 * include/nuttx/wireless/ieee80211/realtek_wlan.h
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
#ifndef __INCLUDE_NUTTX_WIRELESS_IEEE80211_REALTEK_WLAN_H
#define __INCLUDE_NUTTX_WIRELESS_IEEE80211_REALTEK_WLAN_H

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <nuttx/sdio.h>

/****************************************************************************
 * Public Function Prototypes
 * Implementation needs to be provided based on different platform
 ****************************************************************************/

/****************************************************************************
 * Function: realtek_setup_oob_irq
 *
 * Description:
 *   Board specific function called from realtek wifi driver
 *   that must be implemented to use WLAN chip interrupt signal
 *
 * Input Parameters:
 *   dev   - SDIO device used to communicate with the wlan chip
 *   func  - WLAN chip callback function that must be called
 *   arg   - WLAN chip internal structure that must be passed to callback
 *
 ****************************************************************************/
void realtek_setup_oob_irq(FAR struct sdio_dev_s *dev,int (*func)(void *), FAR void *arg);

/****************************************************************************
 * Function: realtek_sdio_irq_clear
 *
 * Description:
 *   clear sdio interrput bit (enable sdio interrupt)
 *
 * Input Parameters:
 *   dev   - SDIO device used to communicate with the wlan chip
 *
 ****************************************************************************/
void realtek_sdio_irq_clear(FAR struct sdio_dev_s *dev);

/****************************************************************************
 * Public Function Prototypes
 * Function declaration
 ****************************************************************************/

/****************************************************************************
 * Name: realtek_wl_sdio_init
 *
 * Description:
 *   Initialize wlan use sdio driver.
 *
 * Input Parameters:
 *   dev   - SDIO device used to communicate with the wlan chip
 *
 * Returned Value:
 *   Zero on success; a negated errno value on failure.
 *
 ****************************************************************************/
int realtek_wl_sdio_init(FAR struct sdio_dev_s *dev);

/****************************************************************************
 * Name: realtek_wl_initialize
 *
 * Description:
 *   Initialize wlan hardware and driver.
 *
 * Input Parameters:
 *   mode   - wlan inital mode; 0  = none mode; 2- sta mode; 3- sta + ap mode;
 *
 * Returned Value:
 *   Zero on success; a negated errno value on failure.
 *
 ****************************************************************************/
int realtek_wl_initialize(FAR unsigned char mode);

#endif
