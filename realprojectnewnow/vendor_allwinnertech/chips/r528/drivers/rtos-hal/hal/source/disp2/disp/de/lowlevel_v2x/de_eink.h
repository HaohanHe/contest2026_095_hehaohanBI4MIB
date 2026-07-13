/* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.

 * Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
 * the the People's Republic of China and other countries.
 * All Allwinner Technology Co.,Ltd. trademarks are used with permission.

 * DISCLAIMER
 * THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
 * IF YOU NEED TO INTEGRATE THIRD PARTY’S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
 * IN ALLWINNERS’SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
 * ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
 * ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
 * COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
 * YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY’S TECHNOLOGY.


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

#ifndef __DE_EINK_H__
#define __DE_EINK_H__

#include "../include.h"

int eink_config(unsigned char in_mode, unsigned int out_mode);
int eink_start_idx(struct ee_img *last_img, struct ee_img *curr_img, unsigned char flash_mode, unsigned char win_en,
					unsigned long last_idx_addr, unsigned long curr_idx_addr, struct area_info *info);
int eink_set_base(unsigned long reg_base);
int eink_irq_enable(void);
int eink_irq_disable(void);
int eink_irq_query(void);
int eink_set_mode(unsigned char in_mode, unsigned char out_mode);
int eink_get_updata_area(struct area_info *info);
int eink_pipe_enable(unsigned int pipe_no);
int eink_pipe_disable(unsigned int pipe_no);
int eink_pipe_config(struct area_info *info, unsigned int pipe_no);
int eink_pipe_config_wavefile(unsigned long wav_file_addr, unsigned int pipe_no);

int eink_edma_init(unsigned char mode);
int eink_edma_en(unsigned char en);
int eink_dbuf_rdy(void);
int eink_set_wb(unsigned char wb_en, unsigned long wb_addr);
int eink_decoder_start(unsigned long new_idx_addr, unsigned long wav_data_addr,
						struct eink_init_param *para);
int eink_edma_cfg(unsigned long wav_addr, struct eink_init_param *para);
int eink_edma_cfg_addr(unsigned long wav_addr);
int eink_index_finish(void);


#endif
