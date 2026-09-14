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


#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

struct sdmmc_controller
{
	struct sdio_dev_s controller_sunxi_mmc_ops;
#ifdef CONFIG_SDIO_MUXBUS
	hal_mutex_t controller_sdio_mutex;
#endif
	struct mmc_host *controller_sunxi_mmc_host;
	int sdc_id;
	unsigned int controller_csd_get;
	unsigned int controller_buffer_set;
 	unsigned int controller_sunxi_mmc_csd[4];
 	unsigned int controller_send_cmd_error;
 	unsigned int controller_host_init;
	struct mmc_request controller_sunxi_mmc_mrq;
	struct mmc_command controller_sunxi_mmc_cmd;
	struct mmc_data controller_sunxi_mmc_data;
	struct scatterlist controller_sunxi_mmc_sg;
	SDC_InitTypeDef controller_sdc_param;
};

#ifdef CONFIG_SDIO_IRQ_SUPPORT
struct sd_media_change
{
	void (*media_changed)(FAR void *arg);
	void *media_arg;
};
struct sd_media_change  media_change;
#endif

static struct sdmmc_controller  sdc_cont[SDC_NUM];

/* See descriptions of each method in the access macros provided
* above.
*/

#ifdef CONFIG_SDIO_IRQ_SUPPORT
int sunxi_sdio_set_sdio_card_isr(struct sdio_dev_s *dev,
                            int (*func)(void *), void *arg)
{
	struct sdmmc_controller *temp_sdc_cont = (struct sdmmc_controller*)dev;
	if ((temp_sdc_cont->controller_sunxi_mmc_host == NULL) || (func == NULL)) {
		SM_ERR("%s,%d: set sdio card isr error\n", __func__, __LINE__);
		return-1;
	}

	temp_sdc_cont->controller_sunxi_mmc_host->do_sdio_card = func;
	temp_sdc_cont->controller_sunxi_mmc_host->do_sdio_arg = arg;
	return 0;
}

int sunxi_sdio_enable_irq(struct sdio_dev_s *dev)
{
	struct sdmmc_controller *temp_sdc_cont = (struct sdmmc_controller*)dev;
	if (temp_sdc_cont->controller_sunxi_mmc_host == NULL) {
		SM_ERR("%s,%d: set sdio card isr error\n", __func__, __LINE__);
		return-1;
	}

	HAL_SDC_Enable_Sdio_Irq(temp_sdc_cont->controller_sunxi_mmc_host, 1);
	return 0;
}
#endif

/* Mutual exclusion */
#ifdef CONFIG_SDIO_MUXBUS
int   sunxi_mmc_mult_lock(FAR struct sdio_dev_s *dev, bool lock)
{
	struct sdmmc_controller *temp_sdc_cont = (struct sdmmc_controller*)dev;
	if (temp_sdc_cont->controller_sdio_mutex == NULL) {
		SM_ERR("mutx is NULL\n");
		return -1;
	}

	if (lock) {
		SUNXI_MutexLock(temp_sdc_cont->controller_sdio_mutex);
	} else {
		SUNXI_MutexUnlock(temp_sdc_cont->controller_sdio_mutex);
	}

	return 0;
}
#endif

/* Initialization/setup */
void  sunxi_mmc_mult_reset(FAR struct sdio_dev_s *dev)
{
	//__mci_reset(sunxi_mmc_host);
	return;
}

/**/
sdio_capset_t sunxi_mmc_mult_capabilities(FAR struct sdio_dev_s *dev)
{
	return SDIO_CAPS_DMASUPPORTED | SDIO_CAPS_DMABEFOREWRITE | SDIO_CAPS_4BIT;
}

/**/
sdio_statset_t sunxi_mmc_mult_status(FAR struct sdio_dev_s *dev)
{
	sdio_statset_t ret = 0;
	struct sdmmc_controller *temp_sdc_cont = (struct sdmmc_controller*)dev;
	int wp_grp_enable = (temp_sdc_cont->controller_sunxi_mmc_csd[0] >> 31) & 0x01;
	int write_protect = (temp_sdc_cont->controller_sunxi_mmc_csd[0] >> 13) & 0x01;

	if (temp_sdc_cont->controller_sunxi_mmc_host->present) {
		ret |= SDIO_STATUS_PRESENT;
	}

	if (temp_sdc_cont->controller_csd_get == 0) {
		SM_ERR("%s,%d: get csd error\n", __func__, __LINE__);
		return ret;
	}

	if (wp_grp_enable && write_protect)
	{
	    ret |= SDIO_STATUS_WRPROTECTED;
	}

	return ret;
}

/**/
void  sunxi_mmc_mult_widebus(FAR struct sdio_dev_s *dev, bool enable)
{
	struct sdmmc_controller *temp_sdc_cont = (struct sdmmc_controller*)dev;
	if (enable) {
		HAL_SDC_Set_BusWidth(temp_sdc_cont->controller_sunxi_mmc_host, MMC_BUS_WIDTH_4);
	} else {
		HAL_SDC_Set_BusWidth(temp_sdc_cont->controller_sunxi_mmc_host, MMC_BUS_WIDTH_1);
	}
}

void  sunxi_mmc_mult_clock(FAR struct sdio_dev_s *dev, enum sdio_clock_e rate)
{
	struct sdmmc_controller *temp_sdc_cont = (struct sdmmc_controller*)dev;
	if (rate == CLOCK_SDIO_DISABLED) {
		//hal_sdc_clock_disabled(sunxi_mmc_host->sdc_id);
		HAL_SDC_Update_Clk(temp_sdc_cont->controller_sunxi_mmc_host, 400000);
	} else if (rate == CLOCK_IDMODE) {
		HAL_SDC_Update_Clk(temp_sdc_cont->controller_sunxi_mmc_host, 400000);
	} else {
		HAL_SDC_Update_Clk(temp_sdc_cont->controller_sunxi_mmc_host, 50000000);
	}
}

int sunxi_mmc_mult_attach(FAR struct sdio_dev_s *dev)
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
		SM_ERR("unkwon response flag\n");
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
		SM_ERR("unkwon data flag\n");
		break;
	}

	return ret_flag;
}

/* Command/Status/Data Transfer */
int sunxi_mmc_mult_sendcmd(FAR struct sdio_dev_s *dev, uint32_t cmd, uint32_t arg)
{
	int32_t ret = 0;
	struct sdmmc_controller *temp_sdc_cont = (struct sdmmc_controller*)dev;
	/* the SD_ACMD52ABRT with arg =0 only use on AP6212A*/
	if (cmd == SD_ACMD52ABRT && arg == 0) {
		return 0;
	}
	temp_sdc_cont->controller_send_cmd_error = 0;
	if (temp_sdc_cont->controller_buffer_set == 1) {
		temp_sdc_cont->controller_sunxi_mmc_mrq.data = &temp_sdc_cont->controller_sunxi_mmc_data;
		temp_sdc_cont->controller_sunxi_mmc_cmd.data = &temp_sdc_cont->controller_sunxi_mmc_data;
	} else {
		temp_sdc_cont->controller_sunxi_mmc_mrq.data = NULL;
		temp_sdc_cont->controller_sunxi_mmc_cmd.data = NULL;
	}

	temp_sdc_cont->controller_sunxi_mmc_cmd.opcode = cmd & 0x3f;
	temp_sdc_cont->controller_sunxi_mmc_cmd.arg = arg;
	temp_sdc_cont->controller_sunxi_mmc_cmd.flags = parse_cmd(cmd);
	temp_sdc_cont->controller_sunxi_mmc_mrq.cmd = &temp_sdc_cont->controller_sunxi_mmc_cmd;

	/* ACMD CMD55 */
	if (temp_sdc_cont->controller_sunxi_mmc_cmd.opcode == 55) {
		temp_sdc_cont->controller_sunxi_mmc_mrq.data = NULL;
		temp_sdc_cont->controller_sunxi_mmc_cmd.data = NULL;
	}

	/* CMD52 don't have data */
	if (temp_sdc_cont->controller_sunxi_mmc_cmd.opcode == 52) {
		temp_sdc_cont->controller_sunxi_mmc_mrq.data = NULL;
		temp_sdc_cont->controller_sunxi_mmc_cmd.data = NULL;
	}

	ret = rom_HAL_SDC_Request(temp_sdc_cont->controller_sunxi_mmc_host, &temp_sdc_cont->controller_sunxi_mmc_mrq);

	if (ret) {
		// SM_ERR("%s,%d: cmd %lu send error\n", __func__, __LINE__, temp_sdc_cont->controller_sunxi_mmc_cmd.opcode);
		temp_sdc_cont->controller_send_cmd_error = 1;
		temp_sdc_cont->controller_buffer_set = 0;
		return -1;
	}

	if (cmd == MMCSD_CMD9) {
		temp_sdc_cont->controller_sunxi_mmc_csd[0] = temp_sdc_cont->controller_sunxi_mmc_mrq.cmd->resp[0];
		temp_sdc_cont->controller_sunxi_mmc_csd[1] = temp_sdc_cont->controller_sunxi_mmc_mrq.cmd->resp[1];
		temp_sdc_cont->controller_sunxi_mmc_csd[2] = temp_sdc_cont->controller_sunxi_mmc_mrq.cmd->resp[2];
		temp_sdc_cont->controller_sunxi_mmc_csd[3] = temp_sdc_cont->controller_sunxi_mmc_mrq.cmd->resp[3];
		temp_sdc_cont->controller_csd_get = 1;
	}

	if (temp_sdc_cont->controller_sunxi_mmc_cmd.opcode != 55)
		temp_sdc_cont->controller_buffer_set = 0;

	return 0;

}

#ifdef CONFIG_SDIO_BLOCKSETUP
void  sunxi_mmc_mult_blocksetup(FAR struct sdio_dev_s *dev, unsigned int blocklen,
      unsigned int nblocks)
{
	struct sdmmc_controller *temp_sdc_cont = (struct sdmmc_controller*)dev;
	temp_sdc_cont->controller_sunxi_mmc_data.blksz = blocklen;
	temp_sdc_cont->controller_sunxi_mmc_data.blocks = nblocks;
}
#endif

int   sunxi_mmc_mult_recvsetup(FAR struct sdio_dev_s *dev, FAR uint8_t *buffer,
      size_t nbytes)
{
	struct sdmmc_controller *temp_sdc_cont = (struct sdmmc_controller*)dev;
	if (temp_sdc_cont->controller_buffer_set == 1) {
		SM_INFO("%s,%d: data set agin\n", __func__, __LINE__);
		return -1;
	}

	temp_sdc_cont->controller_sunxi_mmc_sg.len = nbytes;
	temp_sdc_cont->controller_sunxi_mmc_sg.buffer = (void *)buffer;

	temp_sdc_cont->controller_sunxi_mmc_data.sg_len = 1;
	temp_sdc_cont->controller_sunxi_mmc_data.sg = &temp_sdc_cont->controller_sunxi_mmc_sg;
	temp_sdc_cont->controller_sunxi_mmc_data.flags = MMC_DATA_READ;
	temp_sdc_cont->controller_buffer_set = 1;
	return 0;
}

int   sunxi_mmc_mult_sendsetup(FAR struct sdio_dev_s *dev, FAR const uint8_t *buffer,
      size_t nbytes)
{
	struct sdmmc_controller *temp_sdc_cont = (struct sdmmc_controller*)dev;
	if (temp_sdc_cont->controller_buffer_set == 1) {
		SM_INFO("%s,%d: data set agin\n", __func__, __LINE__);
		return -1;
	}

	temp_sdc_cont->controller_sunxi_mmc_sg.len = nbytes;
	temp_sdc_cont->controller_sunxi_mmc_sg.buffer = (void *)buffer;

	temp_sdc_cont->controller_sunxi_mmc_data.sg_len = 1;
	temp_sdc_cont->controller_sunxi_mmc_data.sg = &temp_sdc_cont->controller_sunxi_mmc_sg;
	temp_sdc_cont->controller_sunxi_mmc_data.flags = MMC_DATA_WRITE;
	temp_sdc_cont->controller_buffer_set = 1;
	return 0;
}

int   sunxi_mmc_mult_cancel(FAR struct sdio_dev_s *dev)
{
	struct sdmmc_controller *temp_sdc_cont = (struct sdmmc_controller*)dev;
	hal_sdc_reset_resume(temp_sdc_cont->controller_sunxi_mmc_host);
	return 0;
}

int   sunxi_mmc_mult_waitresponse(FAR struct sdio_dev_s *dev, uint32_t cmd)
{
	//send_cmd have been waitresponsed
	return 0;
}

int   sunxi_mmc_mult_recv_r1(FAR struct sdio_dev_s *dev, uint32_t cmd, uint32_t *R1)
{
	struct sdmmc_controller *temp_sdc_cont = (struct sdmmc_controller*)dev;
	if (temp_sdc_cont->controller_send_cmd_error) {
		SM_ERR("%s,%d: cmd %ld response error\n", __func__, __LINE__, temp_sdc_cont->controller_sunxi_mmc_mrq.cmd->opcode);
		return -1;
	}

	*R1 = temp_sdc_cont->controller_sunxi_mmc_mrq.cmd->resp[0];
	return 0;
}

int   sunxi_mmc_mult_recv_r2(FAR struct sdio_dev_s *dev, uint32_t cmd, uint32_t R2[4])
{
	struct sdmmc_controller *temp_sdc_cont = (struct sdmmc_controller*)dev;
	if (temp_sdc_cont->controller_send_cmd_error) {
		SM_ERR("%s,%d: cmd %ld response error\n", __func__, __LINE__, temp_sdc_cont->controller_sunxi_mmc_mrq.cmd->opcode);
		return -1;
	}

	R2[0] = temp_sdc_cont->controller_sunxi_mmc_mrq.cmd->resp[0];
	R2[1] = temp_sdc_cont->controller_sunxi_mmc_mrq.cmd->resp[1];
	R2[2] = temp_sdc_cont->controller_sunxi_mmc_mrq.cmd->resp[2];
	R2[3] = temp_sdc_cont->controller_sunxi_mmc_mrq.cmd->resp[3];

	return 0;
}

int   sunxi_mmc_mult_recv_r3(FAR struct sdio_dev_s *dev, uint32_t cmd, uint32_t *R3)
{
	struct sdmmc_controller *temp_sdc_cont = (struct sdmmc_controller*)dev;
	if (temp_sdc_cont->controller_send_cmd_error) {
		// SM_ERR("%s,%d: cmd %ld response error\n", __func__, __LINE__, temp_sdc_cont->controller_sunxi_mmc_mrq.cmd->opcode);
		return -1;
	}

	*R3 = temp_sdc_cont->controller_sunxi_mmc_mrq.cmd->resp[0];
	return 0;
}

int   sunxi_mmc_mult_recv_r4(FAR struct sdio_dev_s *dev, uint32_t cmd, uint32_t *R4)
{
	struct sdmmc_controller *temp_sdc_cont = (struct sdmmc_controller*)dev;
	if (temp_sdc_cont->controller_send_cmd_error) {
		SM_ERR("%s,%d: cmd %ld response error\n", __func__, __LINE__, temp_sdc_cont->controller_sunxi_mmc_mrq.cmd->opcode);
		return -1;
	}

	*R4 = temp_sdc_cont->controller_sunxi_mmc_mrq.cmd->resp[0];
	return 0;
}

int   sunxi_mmc_mult_recv_r5(FAR struct sdio_dev_s *dev, uint32_t cmd, uint32_t *R5)
{
	struct sdmmc_controller *temp_sdc_cont = (struct sdmmc_controller*)dev;
	if (temp_sdc_cont->controller_send_cmd_error) {
		SM_ERR("%s,%d: cmd %ld response error\n", __func__, __LINE__, temp_sdc_cont->controller_sunxi_mmc_mrq.cmd->opcode);
		return -1;
	}

	*R5 = temp_sdc_cont->controller_sunxi_mmc_mrq.cmd->resp[0];
	return 0;
}

int   sunxi_mmc_mult_recv_r6(FAR struct sdio_dev_s *dev, uint32_t cmd, uint32_t *R6)
{
	struct sdmmc_controller *temp_sdc_cont = (struct sdmmc_controller*)dev;
	if (temp_sdc_cont->controller_send_cmd_error) {
		SM_ERR("%s,%d: cmd %ld response error\n", __func__, __LINE__, temp_sdc_cont->controller_sunxi_mmc_mrq.cmd->opcode);
		return -1;
	}

	*R6 = temp_sdc_cont->controller_sunxi_mmc_mrq.cmd->resp[0];
	return 0;
}

int   sunxi_mmc_mult_recv_r7(FAR struct sdio_dev_s *dev, uint32_t cmd, uint32_t *R7)
{

	struct sdmmc_controller *temp_sdc_cont = (struct sdmmc_controller*)dev;
	if (temp_sdc_cont->controller_send_cmd_error) {
		SM_ERR("%s,%d: cmd %ld response error\n", __func__, __LINE__, temp_sdc_cont->controller_sunxi_mmc_mrq.cmd->opcode);
		return -1;
	}

	*R7 = temp_sdc_cont->controller_sunxi_mmc_mrq.cmd->resp[0];
	return 0;
}

/* Event/Callback support */
void  sunxi_mmc_mult_waitenable(FAR struct sdio_dev_s *dev, sdio_eventset_t eventset,
      uint32_t timeout)
{
	//send_cmd have been waitresponsed
	return;
}

sdio_eventset_t sunxi_mmc_mult_eventwait(FAR struct sdio_dev_s *dev)
{
	//send_cmd have been waitresponsed
	return 0;
}

void  sunxi_mmc_mult_callbackenable(FAR struct sdio_dev_s *dev, sdio_eventset_t eventset)
{
	struct sdmmc_controller *temp_sdc_cont = (struct sdmmc_controller*)dev;
	if (temp_sdc_cont->controller_host_init && (eventset & SDIOMEDIA_EJECTED)) {
#ifdef CONFIG_SDIO_IRQ_SUPPORT
		media_change.media_changed(media_change.media_arg);
#endif
		return;
	} else {
		// SM_INFO("%s,%d: unsupport\n", __func__, __LINE__);
		return;
	}
}

#if defined(CONFIG_SCHED_WORKQUEUE) && defined(CONFIG_SCHED_HPWORK)
int   sunxi_mmc_mult_registercallback(FAR struct sdio_dev_s *dev,
      worker_t callback, void *arg)
{
#ifdef CONFIG_SDIO_IRQ_SUPPORT
	media_change.media_changed = callback;
	media_change.media_arg = arg;
#endif
	return 0;
}
#endif

/* DMA.  CONFIG_SDIO_DMA should be set if the driver supports BOTH DMA
* and non-DMA transfer modes.  If the driver supports only one mode
* CONFIG_SDIO_DMA is not required.
*/

#ifdef CONFIG_SDIO_DMA
#ifdef CONFIG_ARCH_HAVE_SDIO_PREFLIGHT
int   sunxi_mmc_mult_dmapreflight(FAR struct sdio_dev_s *dev,
      FAR const uint8_t *buffer, size_t buflen)
{
	SM_INFO("%s,%d: unsupport\n", __func__, __LINE__);
	return -1;
}
#endif
int   sunxi_mmc_mult_dmarecvsetup(FAR struct sdio_dev_s *dev, FAR uint8_t *buffer,
      size_t buflen)
{
	struct sdmmc_controller *temp_sdc_cont = (struct sdmmc_controller*)dev;
	if (temp_sdc_cont->controller_buffer_set == 1) {
		SM_INFO("%s,%d: data set agin\n", __func__, __LINE__);
		return -1;
	}

	temp_sdc_cont->controller_sunxi_mmc_sg.len = buflen;
	temp_sdc_cont->controller_sunxi_mmc_sg.buffer = (void *)buffer;

	temp_sdc_cont->controller_sunxi_mmc_data.sg_len = 1;
	temp_sdc_cont->controller_sunxi_mmc_data.sg = &temp_sdc_cont->controller_sunxi_mmc_sg;
	temp_sdc_cont->controller_sunxi_mmc_data.flags = MMC_DATA_READ;
	temp_sdc_cont->controller_buffer_set = 1;
	return 0;
}

int   sunxi_mmc_mult_dmasendsetup(FAR struct sdio_dev_s *dev,
      FAR const uint8_t *buffer, size_t buflen)
{
	struct sdmmc_controller *temp_sdc_cont = (struct sdmmc_controller*)dev;
	if (temp_sdc_cont->controller_buffer_set == 1) {
		SM_INFO("%s,%d: data set agin\n", __func__, __LINE__);
		return -1;
	}
	temp_sdc_cont->controller_sunxi_mmc_sg.len = buflen;
	temp_sdc_cont->controller_sunxi_mmc_sg.buffer = (void *)buffer;

	temp_sdc_cont->controller_sunxi_mmc_data.sg_len = 1;
	temp_sdc_cont->controller_sunxi_mmc_data.sg = &temp_sdc_cont->controller_sunxi_mmc_sg;
	temp_sdc_cont->controller_sunxi_mmc_data.flags = MMC_DATA_WRITE;
	temp_sdc_cont->controller_buffer_set = 1;
	return 0;
}
#endif /* CONFIG_SDIO_DMA */

void  sunxi_mmc_mult_gotextcsd(FAR struct sdio_dev_s *dev, FAR const uint8_t *buffer)
{

	SM_INFO("%s,%d : called,not define\n", __func__, __LINE__);
}





void sdio_dev_initialize(struct sdio_dev_s *mmc_ops)
{
#ifdef CONFIG_SDIO_MUXBUS
	mmc_ops->lock = sunxi_mmc_mult_lock;
#endif
	mmc_ops->reset = sunxi_mmc_mult_reset;
	mmc_ops->capabilities = sunxi_mmc_mult_capabilities;
	mmc_ops->status = sunxi_mmc_mult_status;
	mmc_ops->widebus = sunxi_mmc_mult_widebus;
	mmc_ops->clock = sunxi_mmc_mult_clock;
	mmc_ops->attach = sunxi_mmc_mult_attach;
	mmc_ops->sendcmd = sunxi_mmc_mult_sendcmd;
#ifdef CONFIG_SDIO_BLOCKSETUP
	mmc_ops->blocksetup = sunxi_mmc_mult_blocksetup;
#endif
	mmc_ops->recvsetup = sunxi_mmc_mult_recvsetup;
	mmc_ops->sendsetup = sunxi_mmc_mult_sendsetup;
	mmc_ops->cancel = sunxi_mmc_mult_cancel;
	mmc_ops->waitresponse = sunxi_mmc_mult_waitresponse;
	mmc_ops->recv_r1 = sunxi_mmc_mult_recv_r1;
	mmc_ops->recv_r2 = sunxi_mmc_mult_recv_r2;
	mmc_ops->recv_r3 = sunxi_mmc_mult_recv_r3;
	mmc_ops->recv_r4 = sunxi_mmc_mult_recv_r4;
	mmc_ops->recv_r5 = sunxi_mmc_mult_recv_r5;
	mmc_ops->recv_r6 = sunxi_mmc_mult_recv_r6;
	mmc_ops->recv_r7 = sunxi_mmc_mult_recv_r7;
	mmc_ops->waitenable = sunxi_mmc_mult_waitenable;
	mmc_ops->eventwait = sunxi_mmc_mult_eventwait;
	mmc_ops->callbackenable = sunxi_mmc_mult_callbackenable;
#if defined(CONFIG_SCHED_WORKQUEUE) && defined(CONFIG_SCHED_HPWORK)
	mmc_ops->registercallback = sunxi_mmc_mult_registercallback;
#endif
#ifdef CONFIG_SDIO_DMA
#ifdef CONFIG_ARCH_HAVE_SDIO_PREFLIGHT
	mmc_ops->dmapreflight = sunxi_mmc_mult_dmapreflight;
#endif
	mmc_ops->dmarecvsetup = sunxi_mmc_mult_dmarecvsetup;
	mmc_ops->dmasendsetup = sunxi_mmc_mult_dmasendsetup;
#endif
	mmc_ops->gotextcsd = sunxi_mmc_mult_gotextcsd;
}


struct sdio_dev_s *sdio_initialize(int sdcno)
{
	struct mmc_host *ret_p = NULL;
	int ret;
	if(sdcno >= SDC_NUM || sdcno < 0) {
		SM_ERR("sdc no is error\n");
		return NULL;
	}
	struct sdmmc_controller *temp_cont = &sdc_cont[sdcno];

	sdio_dev_initialize(&temp_cont->controller_sunxi_mmc_ops);

	if (temp_cont->controller_host_init) {
		hal_sdc_reset_resume(temp_cont->controller_sunxi_mmc_host);
		return &temp_cont->controller_sunxi_mmc_ops;
	}


	temp_cont->controller_sunxi_mmc_host = hal_sdc_create(sdcno, &temp_cont->controller_sdc_param);
	if (temp_cont->controller_sunxi_mmc_host == NULL) {
		SM_ERR("sdc create fail\n");
		return NULL;
	}

	ret_p = hal_sdc_init(temp_cont->controller_sunxi_mmc_host);
	if (ret_p == NULL) {
		SM_ERR("sdc init fail\n");
		hal_sdc_destroy(temp_cont->controller_sunxi_mmc_host);
		return NULL;
	}
	ret = rom_HAL_SDC_PowerOn(temp_cont->controller_sunxi_mmc_host);
	if (ret) {
		SM_ERR("HAL SDC PowerOn fail\n");
		hal_sdc_deinit(sdcno);
		hal_sdc_destroy(temp_cont->controller_sunxi_mmc_host);
		return NULL;
	}
#ifdef CONFIG_SDIO_MUXBUS
	temp_cont->controller_sdio_mutex = SUNXI_MutexCreate();
	if (temp_cont->controller_sdio_mutex == NULL) {
		SM_ERR("sdc create fail\n");
		rom_HAL_SDC_PowerOff(temp_cont->controller_sunxi_mmc_host);
		hal_sdc_destroy(temp_cont->controller_sunxi_mmc_host);
		return NULL;
	}
#endif
	temp_cont->controller_csd_get = 0;
	temp_cont->controller_host_init = 1;
	return (struct sdio_dev_s *)temp_cont;
}

void set_sdio_param(int sdcno,int cd_mode,void (*card_detected_cb)(uint32_t present, uint16_t sdc_id))
{
	struct sdmmc_controller *temp_cont = &sdc_cont[sdcno];
#ifdef CONFIG_DETECT_CARD
	if(cd_mode > CARD_DETECT_BY_D3 || cd_mode < CARD_DETECT_BY_GPIO_IRQ)
		temp_cont->controller_sdc_param.cd_mode = CARD_ALWAYS_PRESENT;
	else
		temp_cont->controller_sdc_param.cd_mode =  cd_mode;

	temp_cont->controller_sdc_param.cd_cb = card_detected_cb;

#endif
	temp_cont->controller_sdc_param.debug_mask = ROM_WRN_MASK|ROM_ERR_MASK;
#ifdef CONFIG_SDC_DMA_USED
	temp_cont->controller_sdc_param.dma_use = 1;
#else
	temp_cont->controller_sdc_param.dma_use = 0;
#endif
}