/*
 * Copyright (C) 2023 ALLWINNERTECH TECHNOLOGY CO., LTD. All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the
 *       distribution.
 *    3. Neither the name of ALLWINNERTECH TECHNOLOGY CO., LTD. nor the names of
 *       its contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _ROM_DRIVER_CHIP_SDMMC_NUTTX_SDIO_H_
#define _ROM_DRIVER_CHIP_SDMMC_NUTTX_SDIO_H_

/* mutx */
#include <hal_mutex.h>
#include <hal_log.h>
#include "_sdhost.h"
#include <string.h>
#include <stdio.h>
#include "sys/xr_sys_debug.h"
#include "os_time.h"
#include "os_semaphore.h"
#include "hal_def.h"
#include "hal_ccm.h"
#include "hal_sdhost.h"
#include "sdmmc.h"
#include "sdio.h"
#include "_core.h"
#include "_sd_define.h"
#include <nuttx/sdio.h>


#define SM_ERR   hal_log_err
#define SM_WARN  hal_log_warn
#define SM_INFO  hal_log_info
#define SM_DBG	 hal_log_debug

#define SUNXI_MutexCreate()			hal_mutex_create()
#define SUNXI_MutexDelete(m)		hal_mutex_delete(m)
#define SUNXI_MutexLock(m)			hal_mutex_lock(m)
#define SUNXI_MutexUnlock(m)		hal_mutex_unlock(m)

extern int32_t __mci_reset(struct mmc_host *host);
extern void HAL_SDC_Set_BusWidth(struct mmc_host *host, uint32_t width);
extern int32_t hal_sdc_clock_disabled(uint32_t sdc_id);
extern int32_t hal_sdc_intterupt_enabled(uint32_t sdc_id);
extern int32_t hal_sdc_reset_resume(struct mmc_host *host);

#ifdef CONFIG_SDIO_IRQ_SUPPORT
int sunxi_sdio_set_sdio_card_isr(struct sdio_dev_s *dev, int (*func)(void *), void *arg);
int sunxi_sdio_enable_irq(struct sdio_dev_s *dev);
#endif
struct sdio_dev_s *sdio_initialize(int sdcno);
#ifdef CONFIG_USE_SDMMC_MULT
void set_sdio_param(int sdcno,int cd_mode,void (*card_detect_cb)(uint32_t present, uint16_t sdc_id));
#else
#define set_sdio_param(x,y,z)
#endif

#endif 
