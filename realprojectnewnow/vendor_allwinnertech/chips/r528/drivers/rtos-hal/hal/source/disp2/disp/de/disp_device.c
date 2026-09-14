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

#include "include.h"
#include "disp_device.h"

static LIST_HEAD(device_list);

s32 disp_device_set_manager(struct disp_device *dispdev,
			    struct disp_manager *mgr)
{
	if ((dispdev == NULL) || (mgr == NULL)) {
		DE_WRN("NULL hdl!\n");
		return DIS_FAIL;
	}
	DE_INF("device %d, mgr %d\n", dispdev->disp, mgr->disp);

	dispdev->manager = mgr;
	mgr->device = dispdev;

	return DIS_SUCCESS;
}

s32 disp_device_swap_manager(struct disp_device *dispdev1, struct disp_device *dispdev2)
{
	struct disp_manager *mgr;
	if ((NULL == dispdev1) || (NULL == dispdev2)) {
		DE_WRN("NULL hdl!\n");
		return DIS_FAIL;
	}
	DE_INF("device %p, %p swap manager\n", dispdev1, dispdev2);
	if ((NULL == dispdev1) && (NULL == dispdev2)) {
		return DIS_SUCCESS;
	} else if (dispdev1->manager == NULL) {
		dispdev1->manager = dispdev2->manager;
		dispdev2->manager->device = dispdev1;
		dispdev2->manager = NULL;
	} else if (dispdev2->manager == NULL) {
		dispdev2->manager = dispdev1->manager;
		dispdev1->manager->device = dispdev2;
		dispdev1->manager = NULL;
	} else {
		mgr = dispdev1->manager;
		dispdev1->manager = dispdev2->manager;
		dispdev2->manager = mgr;
		dispdev1->manager->device = dispdev1;
		dispdev2->manager->device = dispdev2;

	}
	return DIS_SUCCESS;
}

s32 disp_device_unset_manager(struct disp_device *dispdev)
{
	if (dispdev == NULL) {
		DE_WRN("NULL hdl!\n");
		return DIS_FAIL;
	}

	if (dispdev->manager)
		dispdev->manager->device = NULL;
	dispdev->manager = NULL;

	return DIS_SUCCESS;
}

s32 disp_device_get_resolution(struct disp_device *dispdev, u32 *xres,
			       u32 *yres)
{
	if (dispdev == NULL) {
		DE_WRN("NULL hdl!\n");
		return DIS_FAIL;
	}

	*xres = dispdev->timings.x_res;
	*yres = dispdev->timings.y_res;

	return 0;
}

s32 disp_device_get_timings(struct disp_device *dispdev,
			    struct disp_video_timings *timings)
{
	if (dispdev == NULL) {
		DE_WRN("NULL hdl!\n");
		return DIS_FAIL;
	}

	if (timings)
		memcpy(timings, &dispdev->timings,
		       sizeof(struct disp_video_timings));

	return 0;
}

s32 disp_device_is_interlace(struct disp_device *dispdev)
{
	if (dispdev == NULL) {
		DE_WRN("NULL hdl!\n");
		return DIS_FAIL;
	}

	return dispdev->timings.b_interlace;
}

s32 disp_device_get_status(struct disp_device *dispdev)
{
	if (dispdev == NULL) {
		DE_WRN("NULL hdl!\n");
		return 0;
	}

	return disp_al_device_get_status(dispdev->hwdev_index);
}

bool disp_device_is_in_safe_period(struct disp_device *dispdev)
{
	int cur_line;
	int start_delay;
	bool ret = true;

	if (dispdev == NULL) {
		DE_WRN("NULL hdl!\n");
		goto exit;
	}

	start_delay = disp_al_device_get_start_delay(dispdev->hwdev_index);
	cur_line = disp_al_device_get_cur_line(dispdev->hwdev_index);
	if (cur_line >= start_delay)
		ret = false;

exit:
	return ret;
}

void disp_device_show_builtin_patten(struct disp_device *dispdev, u32 patten)
{
	if (dispdev)
		disp_al_show_builtin_patten(dispdev->hwdev_index, patten);
}

u32 disp_device_usec_before_vblank(struct disp_device *dispdev)
{
	int cur_line;
	int start_delay;
	u32 usec = 0;
	struct disp_video_timings *timings;
	u32 usec_per_line;
	unsigned long long n_temp, base_temp;
	//u32 mod;

	if (dispdev == NULL) {
		DE_WRN("NULL hdl!\n");
		goto exit;
	}

	start_delay = disp_al_device_get_start_delay(dispdev->hwdev_index);
	cur_line = disp_al_device_get_cur_line(dispdev->hwdev_index);
	if (cur_line > (start_delay - 4)) {
		timings = &dispdev->timings;
		n_temp = (unsigned long long)timings->hor_total_time *
			 (unsigned long long)(1000000);
		base_temp = (unsigned long long)timings->pixel_clk;
		/*mod = (u32)do_div(n_temp, base_temp);*/
		n_temp = (u32)n_temp/base_temp;
		//mod = (u32)n_temp%base_temp;
		usec_per_line = (u32)n_temp;
		usec = (timings->ver_total_time - cur_line + 1) * usec_per_line;
	}

exit:
	return usec;
}

/* get free device */
struct disp_device *disp_device_get(int disp, enum disp_output_type output_type)
{
	struct disp_device *dispdev = NULL;

	list_for_each_entry(dispdev, &device_list, list) {
		if ((dispdev->type == output_type) && (dispdev->disp == disp)
		    && (dispdev->manager == NULL)) {
			return dispdev;
		}
	}

	return NULL;
}

/**
 * @name       :disp_device_get_from_priv
 * @brief      :get disp_device by comparing pointer of priv_data
 * @param[IN]  :priv_data:pointer of private date of disp_device
 * @return     :pointer of disp_device; NULL if not found
 */
struct disp_device *disp_device_get_from_priv(void *priv_data)
{
	struct disp_device *dispdev = NULL;

	list_for_each_entry(dispdev, &device_list, list) {
		if (dispdev->priv_data == priv_data)
			return dispdev;
	}

	return NULL;
}

struct disp_device *disp_device_find(int disp,
				     enum disp_output_type output_type)
{
	struct disp_device *dispdev = NULL;

	list_for_each_entry(dispdev, &device_list, list) {
		if ((dispdev->type == output_type) && (dispdev->disp == disp))
			return dispdev;
	}

	return NULL;
}

struct list_head *disp_device_get_list_head(void)
{
	return &device_list;
}

s32 disp_device_register(struct disp_device *dispdev)
{
	list_add_tail(&dispdev->list, &device_list);
	return 0;
}

s32 disp_device_unregister(struct disp_device *dispdev)
{
	list_del(&dispdev->list);
	return 0;
}
