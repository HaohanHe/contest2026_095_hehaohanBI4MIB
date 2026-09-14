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

#ifndef _DRV_TV_H_
#define  _DRV_TV_H_

#include <string.h>
#include <stdlib.h>
#include <interrupt.h>
#include <unistd.h>
#include <sunxi_display2.h>
#include <sunxi_hal_regulator.h>
#include <hal_cmd.h>
#include "de_tvec.h"
#include "../disp/de/disp_display.h"
#include <hal_clk.h>
#include <hal_reset.h>
#include <hal_sem.h>
#include <hal_cfg.h>
#include <hal_thread.h>
#include <log.h>
#include <hal_atomic.h>

enum hpd_status {
	STATUE_CLOSE = 0,
	STATUE_OPEN  = 1,
};

#define    SCREEN_COUNT    TVE_DEVICE_NUM
#define    DAC_COUNT       TVE_DAC_NUM
#define    DAC_INVALID_SOURCE 0xff

#define DEBUG_PRINT printf("[DEBUG] %s, %s, %d \n", __FILE__, __func__, __LINE__);

int tv_open(void);
int tv_probe(void);
int tv_release(void);
int tv_ioctl(int cmd, void *arg);

/* extern interface exported by display driver */
extern s32 bsp_disp_tv_register(struct disp_tv_func *func);
extern unsigned int disp_boot_para_parse(const char *name);

struct tv_screen_t {
	enum  disp_tv_mode      tv_mode;
	void *base_addr;
	u32                     sid;
	u32                     cali_offset;
	u32                     enable;
	u32			dac_no[DAC_COUNT];
	u32			dac_type[DAC_COUNT];
	u32			dac_num;
	enum disp_tv_dac_source dac_source;
	hal_clk_t clk;
	hal_clk_t clk_parent;
	hal_clk_t bus_clk;
	struct reset_control *rst_bus;
	bool			suspend;
	bool			used;
	hal_sem_t mlock;
};

struct tv_info_t {
	struct tv_screen_t screen[SCREEN_COUNT];
	hal_clk_t bus_clk;
	struct reset_control *rst_bus;

	void *base_addr;
	struct regulator_dev *regulator;
	u32 tv_number;
};

extern struct tv_info_t g_tv_info;

static inline bool is_vga_mode(enum disp_tv_mode mode)
{
	if ((mode >= DISP_VGA_MOD_640_480P_60)
		&& (mode < DISP_VGA_MOD_MAX_NUM))
		return true;

	return false;
}

s32 tv_init(void);
s32 tv_exit(void);
s32 tv_get_mode(u32 sel);
s32 tv_set_mode(u32 sel, enum disp_tv_mode tv_mod);
s32 tv_get_input_csc(u32 sel);
s32 tv_get_video_timing_info(u32 sel, struct disp_video_timings **video_info);
s32 tv_enable(u32 sel);
s32 tv_disable(u32 sel);
s32 tv_suspend(u32 sel);
s32 tv_resume(u32 sel);
s32 tv_mode_support(u32 sel, enum disp_tv_mode mode);
void tv_report_hpd_work(u32 sel, u32 hpd);
void tv_detect_thread(void *parg);
s32 tv_detect_enable(u32 sel);
s32 tv_detect_disable(u32 sel);
s32 tv_get_dac_hpd(u32 sel);
s32 tv_hot_plugging_detect(u32 state);
int get_tve_hpg_status(void);

#endif
