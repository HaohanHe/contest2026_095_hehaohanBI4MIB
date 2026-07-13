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

#ifndef _G2D_MIXER_H
#define _G2D_MIXER_H

#include <aw_list.h>

#include "g2d_driver_i.h"
#include "g2d_ovl_v.h"
#include "g2d_ovl_u.h"
#include "g2d_wb.h"
#include "g2d_bld.h"
#include "g2d_scal.h"
#include "g2d_top.h"

struct g2d_mixer_task;

/**
 * mixer frame
 */
struct g2d_mixer_frame {
	struct ovl_v_submodule *ovl_v;
	struct ovl_u_submodule *ovl_u;
	struct blender_submodule *bld;
	struct scaler_submodule *scal;
	struct wb_submodule *wb;
	__u32 frame_id;
	u8 *g2d_base;
	struct dmabuf_item *src_item;
	struct dmabuf_item *dst_item;
	struct dmabuf_item *ptn_item;
	struct dmabuf_item *mask_item;
	__s32 (*destory)(struct g2d_mixer_frame *p_frame);
	__s32 (*apply)(struct g2d_mixer_frame *p_frame,
		     struct mixer_para *p_para);
	__s32 (*frame_mem_setup)(struct g2d_mixer_frame *p_frame,
				 struct mixer_para *p_para,
				 struct g2d_mixer_task *p_task);
	__u32 (*frame_get_reg_block_num)(struct g2d_mixer_frame *p_frame);
	__u32 (*frame_get_rcq_mem_size)(struct g2d_mixer_frame *p_frame);
};

/**
 * mixer task
 */
struct g2d_mixer_task {
	int task_id;
	__u32 frame_cnt;
	bool en_split_mem;
	struct g2d_mixer_frame *frame;
	struct g2d_rcq_mem_info *p_rcq_info;
	struct mixer_para *p_para;
	__g2d_info_t *p_g2d_info;
	struct list_head list;
	__s32 (*mixer_mem_setup)(struct g2d_mixer_task *p_task,
				 struct mixer_para *p_para);
	__s32 (*apply)(struct g2d_mixer_task *p_task,
		     struct mixer_para *p_para);
	__s32 (*destory)(struct g2d_mixer_task *p_task);
};


/**
 * @name       :mixer_task_process
 * @brief      :mixer task process
 * @param[IN]  :p_g2d_info:pointer of hardware resource
 * @param[IN]  :p_para:mixer task parameter
 * @param[IN]  :frame_len:number of frame
 * @return     :0 if success, -1 else
 */
__s32 mixer_task_process(__g2d_info_t *p_g2d_info, struct mixer_para *p_para,
			 unsigned int frame_len);

/**
 * @name       :create_mixer_task
 * @brief      :create mixer task instance include memory allocate
 * @param[IN]  :p_g2d_info:pointer of hardware resource
 * @param[IN]  :p_para:mixer task parameter
 * @param[IN]  :frame_len:number of frame
 * @return     :task_id >= 1, else fail
 */
int create_mixer_task(__g2d_info_t *p_g2d_info, struct mixer_para *p_para,
			 unsigned int frame_len);

/**
 * @name       :g2d_mixer_get_inst
 * @brief      :get task instance of specified task id
 * @param[IN]  :id: task id
 * @return     :pointer of mixer task or NULL if fail
 */
struct g2d_mixer_task *g2d_mixer_get_inst(int id);
int g2d_mixer_idr_init(void);
int g2d_mixer_idr_remove(void);



#endif /*End of file*/
