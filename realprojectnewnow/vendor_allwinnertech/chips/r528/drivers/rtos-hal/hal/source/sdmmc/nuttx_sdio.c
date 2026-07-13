/**
  * @file  nuttx_sdio.c
  * @author  CGD
  */

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
 *  LIMITED TO, )|hhst->sdio_irq_maskPROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <debug.h>
#include "nuttx_sdio.h"

#ifdef CONFIG_SDIO_MUXBUS
static hal_mutex_t sdio_mutex = NULL;
#endif

static struct mmc_host *sunxi_mmc_host = NULL;

/*----only support 1 sdc host*/
static unsigned int csd_get = 0;
static unsigned int buffer_set = 0;
static unsigned int sunxi_mmc_csd[4];
static unsigned int send_cmd_error = 0;
static unsigned int host_init = 0;

static struct mmc_request sunxi_mmc_mrq;
static struct mmc_command sunxi_mmc_cmd;
static struct mmc_data sunxi_mmc_data;
static struct scatterlist sunxi_mmc_sg = {0};

/* See descriptions of each method in the access macros provided
* above.
*/

#ifdef CONFIG_SDIO_IRQ_SUPPORT
int sunxi_sdio_set_sdio_card_isr(struct sdio_dev_s *dev,
                            int (*func)(void *), void *arg)
{
	if ((sunxi_mmc_host == NULL) || (func == NULL)) {
		_err("%s,%d: set sdio card isr error\n", __func__, __LINE__);
		return-1;
	}

	sunxi_mmc_host->do_sdio_card = func;
	sunxi_mmc_host->do_sdio_arg = arg;
	return 0;
}

int sunxi_sdio_enable_irq(struct sdio_dev_s *dev)
{
	if (sunxi_mmc_host == NULL) {
		_err("%s,%d: set sdio card isr error\n", __func__, __LINE__);
		return-1;
	}

	HAL_SDC_Enable_Sdio_Irq(sunxi_mmc_host, 1);
	return 0;
}
#endif

/* Mutual exclusion */
#ifdef CONFIG_SDIO_MUXBUS
int   sunxi_mmc_lock(FAR struct sdio_dev_s *dev, bool lock)
{
	if (sdio_mutex == NULL) {
		SM_ERR("mutx is NULL\n");
		return -1;
	}

	if (lock) {
		SUNXI_MutexLock(sdio_mutex);
	} else {
		SUNXI_MutexUnlock(sdio_mutex);
	}

	return 0;
}
#endif

/* Initialization/setup */
void  sunxi_mmc_reset(FAR struct sdio_dev_s *dev)
{
	//__mci_reset(sunxi_mmc_host);
	return;
}

/**/
sdio_capset_t sunxi_mmc_capabilities(FAR struct sdio_dev_s *dev)
{
	return SDIO_CAPS_DMASUPPORTED | SDIO_CAPS_DMABEFOREWRITE | SDIO_CAPS_4BIT;
}

/**/
sdio_statset_t sunxi_mmc_status(FAR struct sdio_dev_s *dev)
{
	sdio_statset_t ret = 0;
	int wp_grp_enable = (sunxi_mmc_csd[0] >> 31) & 0x01;
	int write_protect = (sunxi_mmc_csd[0] >> 13) & 0x01;

	if (sunxi_mmc_host->present) {
		ret |= SDIO_STATUS_PRESENT;
	}

	if (csd_get == 0) {
		_err("%s,%d: get csd error\n", __func__, __LINE__);
		return ret;
	}

	if (wp_grp_enable && write_protect)
	{
	    ret |= SDIO_STATUS_WRPROTECTED;
	}

	return ret;
}

/**/
void  sunxi_mmc_widebus(FAR struct sdio_dev_s *dev, bool enable)
{
	if (enable) {
		HAL_SDC_Set_BusWidth(sunxi_mmc_host, MMC_BUS_WIDTH_4);
	} else {
		HAL_SDC_Set_BusWidth(sunxi_mmc_host, MMC_BUS_WIDTH_1);
	}
}

void  sunxi_mmc_clock(FAR struct sdio_dev_s *dev, enum sdio_clock_e rate)
{
	if (rate == CLOCK_SDIO_DISABLED) {
		//hal_sdc_clock_disabled(sunxi_mmc_host->sdc_id);
		HAL_SDC_Update_Clk(sunxi_mmc_host, 400000);
	} else if (rate == CLOCK_IDMODE) {
		HAL_SDC_Update_Clk(sunxi_mmc_host, 400000);
	} else {
		HAL_SDC_Update_Clk(sunxi_mmc_host, 50000000);
	}
}

int sunxi_mmc_attach(FAR struct sdio_dev_s *dev)
{
	//return hal_sdc_intterupt_enabled();
	return 0;
}

static uint32_t parse_cmd(uint32_t cmd)
{
	uint32_t resp_flag = cmd & 0x3c0;
	uint32_t DATA_flag = cmd & 0x1c00;
	uint32_t ret_flag = 0;

	switch (resp_flag) {
	case MMCSD_NO_RESPONSE:
		break;
	case MMCSD_R1_RESPONSE:
		ret_flag |= MMC_RSP_R1;
		break;
	case MMCSD_R1B_RESPONSE:
		ret_flag |= MMC_RSP_R1B;
		break;
	case MMCSD_R2_RESPONSE:
		ret_flag |= MMC_RSP_R2;
		break;
	case MMCSD_R3_RESPONSE:
		ret_flag |= MMC_RSP_R3;
		break;
	case MMCSD_R4_RESPONSE:
		ret_flag |= MMC_RSP_R4;
		break;
	case MMCSD_R5_RESPONSE:
		ret_flag |= MMC_RSP_R5;
		break;
	case MMCSD_R6_RESPONSE:
		ret_flag |= MMC_RSP_R6;
		break;
	case MMCSD_R7_RESPONSE:
		ret_flag |= MMC_RSP_R7;
		break;

	default:
		_err("unkwon response flag\n");
		break;
	}

	switch (DATA_flag) {
	case MMCSD_NODATAXFR:
		ret_flag |= MMC_CMD_BCR;
		break;
	case MMCSD_RDSTREAM:
		break;
	case MMCSD_WRSTREAM:
		break;
	case MMCSD_RDDATAXFR:
		ret_flag |= MMC_CMD_ADTC;
		break;
	case MMCSD_WRDATAXFR:
		ret_flag |= MMC_CMD_ADTC;
		break;

	default:
		_err("unkwon data flag\n");
		break;
	}

	return ret_flag;
}

/* Command/Status/Data Transfer */
int sunxi_mmc_sendcmd(FAR struct sdio_dev_s *dev, uint32_t cmd, uint32_t arg)
{
	int32_t ret = 0;
	/* the SD_ACMD52ABRT with arg =0 only use on AP6212A*/
	if (cmd == SD_ACMD52ABRT && arg == 0) {
		return 0;
	}
	send_cmd_error = 0;
	if (buffer_set == 1) {
		sunxi_mmc_mrq.data = &sunxi_mmc_data;
		sunxi_mmc_cmd.data = &sunxi_mmc_data;
	} else {
		sunxi_mmc_mrq.data = NULL;
		sunxi_mmc_cmd.data = NULL;
	}

	sunxi_mmc_cmd.opcode = cmd & 0x3f;
	sunxi_mmc_cmd.arg = arg;
	sunxi_mmc_cmd.flags = parse_cmd(cmd);
	sunxi_mmc_mrq.cmd = &sunxi_mmc_cmd;

	/* ACMD CMD55 */
	if (sunxi_mmc_cmd.opcode == 55) {
		sunxi_mmc_mrq.data = NULL;
		sunxi_mmc_cmd.data = NULL;
	}

	/* CMD52 don't have data */
	if (sunxi_mmc_cmd.opcode == 52) {
		sunxi_mmc_mrq.data = NULL;
		sunxi_mmc_cmd.data = NULL;
	}

	ret = rom_HAL_SDC_Request(sunxi_mmc_host, &sunxi_mmc_mrq);

	if (ret) {
		_err("%s,%d: cmd %ld send error\n", __func__, __LINE__, sunxi_mmc_cmd.opcode);
		send_cmd_error = 1;
		buffer_set = 0;
		return -1;
	}

	if (cmd == MMCSD_CMD9) {
		sunxi_mmc_csd[0] = sunxi_mmc_mrq.cmd->resp[0];
		sunxi_mmc_csd[1] = sunxi_mmc_mrq.cmd->resp[1];
		sunxi_mmc_csd[2] = sunxi_mmc_mrq.cmd->resp[2];
		sunxi_mmc_csd[3] = sunxi_mmc_mrq.cmd->resp[3];
		csd_get = 1;
	}

	if (sunxi_mmc_cmd.opcode != 55)
		buffer_set = 0;

	return 0;

}

#ifdef CONFIG_SDIO_BLOCKSETUP
void  sunxi_mmc_blocksetup(FAR struct sdio_dev_s *dev, unsigned int blocklen,
      unsigned int nblocks)
{
	sunxi_mmc_data.blksz = blocklen;
	sunxi_mmc_data.blocks = nblocks;
}
#endif

int   sunxi_mmc_recvsetup(FAR struct sdio_dev_s *dev, FAR uint8_t *buffer,
      size_t nbytes)
{
	if (buffer_set == 1) {
		_info("%s,%d: data set agin\n", __func__, __LINE__);
		return -1;
	}

	sunxi_mmc_sg.len = nbytes;
	sunxi_mmc_sg.buffer = (void *)buffer;

	sunxi_mmc_data.sg_len = 1;
	sunxi_mmc_data.sg = &sunxi_mmc_sg;
	sunxi_mmc_data.flags = MMC_DATA_READ;
	buffer_set = 1;
	return 0;
}

int   sunxi_mmc_sendsetup(FAR struct sdio_dev_s *dev, FAR const uint8_t *buffer,
      size_t nbytes)
{
	if (buffer_set == 1) {
		_info("%s,%d: data set agin\n", __func__, __LINE__);
		return -1;
	}

	sunxi_mmc_sg.len = nbytes;
	sunxi_mmc_sg.buffer = (void *)buffer;

	sunxi_mmc_data.sg_len = 1;
	sunxi_mmc_data.sg = &sunxi_mmc_sg;
	sunxi_mmc_data.flags = MMC_DATA_WRITE;
	buffer_set = 1;
	return 0;
}

int   sunxi_mmc_cancel(FAR struct sdio_dev_s *dev)
{
	hal_sdc_reset_resume(sunxi_mmc_host);
	return 0;
}

int   sunxi_mmc_waitresponse(FAR struct sdio_dev_s *dev, uint32_t cmd)
{
	//send_cmd have been waitresponsed
	return 0;
}

int   sunxi_mmc_recv_r1(FAR struct sdio_dev_s *dev, uint32_t cmd, uint32_t *R1)
{
	if (send_cmd_error) {
		_err("%s,%d: cmd %ld response error\n", __func__, __LINE__, sunxi_mmc_mrq.cmd->opcode);
		return -1;
	}

	*R1 = sunxi_mmc_mrq.cmd->resp[0];
	return 0;
}

int   sunxi_mmc_recv_r2(FAR struct sdio_dev_s *dev, uint32_t cmd, uint32_t R2[4])
{
	if (send_cmd_error) {
		_err("%s,%d: cmd %ld response error\n", __func__, __LINE__, sunxi_mmc_mrq.cmd->opcode);
		return -1;
	}

	R2[0] = sunxi_mmc_mrq.cmd->resp[0];
	R2[1] = sunxi_mmc_mrq.cmd->resp[1];
	R2[2] = sunxi_mmc_mrq.cmd->resp[2];
	R2[3] = sunxi_mmc_mrq.cmd->resp[3];

	return 0;
}

int   sunxi_mmc_recv_r3(FAR struct sdio_dev_s *dev, uint32_t cmd, uint32_t *R3)
{
	if (send_cmd_error) {
		_err("%s,%d: cmd %ld response error\n", __func__, __LINE__, sunxi_mmc_mrq.cmd->opcode);
		return -1;
	}

	*R3 = sunxi_mmc_mrq.cmd->resp[0];
	return 0;
}

int   sunxi_mmc_recv_r4(FAR struct sdio_dev_s *dev, uint32_t cmd, uint32_t *R4)
{
	if (send_cmd_error) {
		_err("%s,%d: cmd %ld response error\n", __func__, __LINE__, sunxi_mmc_mrq.cmd->opcode);
		return -1;
	}

	*R4 = sunxi_mmc_mrq.cmd->resp[0];
	return 0;
}

int   sunxi_mmc_recv_r5(FAR struct sdio_dev_s *dev, uint32_t cmd, uint32_t *R5)
{
	if (send_cmd_error) {
		_err("%s,%d: cmd %ld response error\n", __func__, __LINE__, sunxi_mmc_mrq.cmd->opcode);
		return -1;
	}

	*R5 = sunxi_mmc_mrq.cmd->resp[0];
	return 0;
}

int   sunxi_mmc_recv_r6(FAR struct sdio_dev_s *dev, uint32_t cmd, uint32_t *R6)
{
	if (send_cmd_error) {
		_err("%s,%d: cmd %ld response error\n", __func__, __LINE__, sunxi_mmc_mrq.cmd->opcode);
		return -1;
	}

	*R6 = sunxi_mmc_mrq.cmd->resp[0];
	return 0;
}

int   sunxi_mmc_recv_r7(FAR struct sdio_dev_s *dev, uint32_t cmd, uint32_t *R7)
{
	if (send_cmd_error) {
		_err("%s,%d: cmd %ld response error\n", __func__, __LINE__, sunxi_mmc_mrq.cmd->opcode);
		return -1;
	}

	*R7 = sunxi_mmc_mrq.cmd->resp[0];
	return 0;
}

/* Event/Callback support */
void  sunxi_mmc_waitenable(FAR struct sdio_dev_s *dev, sdio_eventset_t eventset,
      uint32_t timeout)
{
	//send_cmd have been waitresponsed
	return;
}

sdio_eventset_t sunxi_mmc_eventwait(FAR struct sdio_dev_s *dev)
{
	//send_cmd have been waitresponsed
	return 0;
}

void  sunxi_mmc_callbackenable(FAR struct sdio_dev_s *dev,
      sdio_eventset_t eventset)
{
	if (host_init && (eventset & SDIOMEDIA_EJECTED)) {
		hal_sdc_deinit(sunxi_mmc_host->sdc_id);
		hal_sdc_destroy(sunxi_mmc_host);
		host_init = 0;
		return;
	} else {
		_info("%s,%d: unsupport\n", __func__, __LINE__);
		return;
	}
}

#if defined(CONFIG_SCHED_WORKQUEUE) && defined(CONFIG_SCHED_HPWORK)
int   sunxi_mmc_registercallback(FAR struct sdio_dev_s *dev,
      worker_t callback, void *arg)
{
	_info("%s,%d: unsupport\n", __func__, __LINE__);

	return 0;
}
#endif

/* DMA.  CONFIG_SDIO_DMA should be set if the driver supports BOTH DMA
* and non-DMA transfer modes.  If the driver supports only one mode
* CONFIG_SDIO_DMA is not required.
*/

#ifdef CONFIG_SDIO_DMA
#ifdef CONFIG_ARCH_HAVE_SDIO_PREFLIGHT
int   sunxi_mmc_dmapreflight(FAR struct sdio_dev_s *dev,
      FAR const uint8_t *buffer, size_t buflen)
{
	_info("%s,%d: unsupport\n", __func__, __LINE__);
}
#endif
int   sunxi_mmc_dmarecvsetup(FAR struct sdio_dev_s *dev, FAR uint8_t *buffer,
      size_t buflen)
{
	if (buffer_set == 1) {
		_info("%s,%d: data set agin\n", __func__, __LINE__);
		return -1;
	}

	sunxi_mmc_sg.len = buflen;
	sunxi_mmc_sg.buffer = (void *)buffer;

	sunxi_mmc_data.sg_len = 1;
	sunxi_mmc_data.sg = &sunxi_mmc_sg;
	sunxi_mmc_data.flags = MMC_DATA_READ;
	buffer_set = 1;
	return 0;
}

int   sunxi_mmc_dmasendsetup(FAR struct sdio_dev_s *dev,
      FAR const uint8_t *buffer, size_t buflen)
{
	if (buffer_set == 1) {
		_info("%s,%d: data set agin\n", __func__, __LINE__);
		return -1;
	}
	sunxi_mmc_sg.len = buflen;
	sunxi_mmc_sg.buffer = (void *)buffer;

	sunxi_mmc_data.sg_len = 1;
	sunxi_mmc_data.sg = &sunxi_mmc_sg;
	sunxi_mmc_data.flags = MMC_DATA_WRITE;
	buffer_set = 1;
	return 0;
}
#endif /* CONFIG_SDIO_DMA */

void  sunxi_mmc_gotextcsd(FAR struct sdio_dev_s *dev, FAR const uint8_t *buffer)
{
	_info("%s,%d: unsupport\n", __func__, __LINE__);
}

static struct sdio_dev_s sunxi_mmc_ops = {
#ifdef CONFIG_SDIO_MUXBUS
	.lock = sunxi_mmc_lock,
#endif
	.reset = sunxi_mmc_reset,
	.capabilities = sunxi_mmc_capabilities,
	.status = sunxi_mmc_status,
	.widebus = sunxi_mmc_widebus,
	.clock = sunxi_mmc_clock,
	.attach = sunxi_mmc_attach,
	.sendcmd = sunxi_mmc_sendcmd,
#ifdef CONFIG_SDIO_BLOCKSETUP
	.blocksetup = sunxi_mmc_blocksetup,
#endif
	.recvsetup = sunxi_mmc_recvsetup,
	.sendsetup = sunxi_mmc_sendsetup,
	.cancel = sunxi_mmc_cancel,
	.waitresponse = sunxi_mmc_waitresponse,
	.recv_r1 = sunxi_mmc_recv_r1,
	.recv_r2 = sunxi_mmc_recv_r2,
	.recv_r3 = sunxi_mmc_recv_r3,
	.recv_r4 = sunxi_mmc_recv_r4,
	.recv_r5 = sunxi_mmc_recv_r5,
	.recv_r6 = sunxi_mmc_recv_r6,
	.recv_r7 = sunxi_mmc_recv_r7,
	.waitenable = sunxi_mmc_waitenable,
	.eventwait = sunxi_mmc_eventwait,
	.callbackenable = sunxi_mmc_callbackenable,
#if defined(CONFIG_SCHED_WORKQUEUE) && defined(CONFIG_SCHED_HPWORK)
	.registercallback = sunxi_mmc_registercallback,
#endif
#ifdef CONFIG_SDIO_DMA
#ifdef CONFIG_ARCH_HAVE_SDIO_PREFLIGHT
	.dmapreflight = sunxi_mmc_dmapreflight,
#endif
	.dmarecvsetup = sunxi_mmc_dmarecvsetup,
	.dmasendsetup = sunxi_mmc_dmasendsetup,
#endif
	.gotextcsd = sunxi_mmc_gotextcsd,
};

struct sdio_dev_s *sdio_initialize(int sdcno)
{
	struct mmc_host *ret_p = NULL;
	int ret;
	SDC_InitTypeDef sdc_param = { 0 };

	if (sdcno != 1) {
		_info("unsupport the host no\n");
		return NULL;
	}

	if (host_init) {
		hal_sdc_reset_resume(sunxi_mmc_host);
		return &sunxi_mmc_ops;
	}

#ifdef CONFIG_DETECT_CARD
	sdc_param.cd_mode = CARD_ALWAYS_PRESENT;
#endif
	sdc_param.debug_mask = ROM_WRN_MASK|ROM_ERR_MASK;
#ifdef CONFIG_SDC_DMA_USED
	sdc_param.dma_use = 1;
#else
	sdc_param.dma_use = 0;
#endif

	sunxi_mmc_host = hal_sdc_create(sdcno, &sdc_param);
	if (sunxi_mmc_host == NULL) {
		_err("sdc create fail\n");
		return NULL;
	}

	ret_p = hal_sdc_init(sunxi_mmc_host);
	if (ret_p == NULL) {
		_err("sdc init fail\n");
		return NULL;
	}
	ret = rom_HAL_SDC_PowerOn(sunxi_mmc_host);
	if (ret) {
		_err("HAL SDC PowerOn fail\n");
		return NULL;
	}
#ifdef CONFIG_SDIO_MUXBUS
	sdio_mutex = SUNXI_MutexCreate();
	if (sdio_mutex == NULL) {
		_err("sdc create fail\n");
		return NULL;
	}
#endif
	csd_get = 0;
	host_init = 1;
	return &sunxi_mmc_ops;
}
