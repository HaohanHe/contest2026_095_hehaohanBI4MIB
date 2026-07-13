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

#ifndef __DE_PEAK_TYPE_H__
#define __DE_PEAK_TYPE_H__

#include "de_rtmx.h"

#define PEAK_PARA_NUM 3
#define PEAK_MODE_NUM 3

union LP_CTRL_REG {
	unsigned int dwval;
	struct {
		unsigned int en:1;
		unsigned int res0:7;
		unsigned int win_en:1;
		unsigned int res1:23;
	} bits;
};

union LP_SIZE_REG {
	unsigned int dwval;
	struct {
		unsigned int width:12;
		unsigned int res0:4;
		unsigned int height:12;
		unsigned int res1:4;
	} bits;
};

union LP_WIN0_REG {
	unsigned int dwval;
	struct {
		unsigned int win_left:12;
		unsigned int res0:4;
		unsigned int win_top:12;
		unsigned int res1:4;
	} bits;
};

union LP_WIN1_REG {
	unsigned int dwval;
	struct {
		unsigned int win_right:12;
		unsigned int res0:4;
		unsigned int win_bot:12;
		unsigned int res1:4;
	} bits;
};

union LP_FILTER_REG {
	unsigned int dwval;
	struct {
		unsigned int bp1_ratio:6;
		unsigned int res0:2;
		unsigned int bp0_ratio:6;
		unsigned int res1:2;
		unsigned int hp_ratio:6;
		unsigned int res2:9;
		unsigned int filter_sel:1;
	} bits;
};

union LP_CSTM_FILTER0_REG {
	unsigned int dwval;
	struct {
		unsigned int c0:9;
		unsigned int res0:7;
		unsigned int c1:9;
		unsigned int res1:7;
	} bits;
};

union LP_CSTM_FILTER1_REG {
	unsigned int dwval;
	struct {
		unsigned int c2:9;
		unsigned int res0:7;
		unsigned int c3:9;
		unsigned int res1:7;
	} bits;
};

union LP_CSTM_FILTER2_REG {
	unsigned int dwval;
	struct {
		unsigned int c4:9;
		unsigned int res0:23;
	} bits;
};

union LP_GAIN_REG {
	unsigned int dwval;
	struct {
		unsigned int gain:8;
		unsigned int res0:24;
	} bits;
};

union LP_GAINCTRL_REG {
	unsigned int dwval;
	struct {
		unsigned int beta:5;
		unsigned int res0:11;
		unsigned int dif_up:8;
		unsigned int res1:8;
	} bits;
};

union LP_SHOOTCTRL_REG {
	unsigned int dwval;
	struct {
		unsigned int neg_gain:6;
		unsigned int res0:26;
	} bits;
};

union LP_CORING_REG {
	unsigned int dwval;
	struct {
		unsigned int corthr:8;
		unsigned int res0:24;
	} bits;
};

struct __peak_reg_t {
	union LP_CTRL_REG ctrl;	                  /* 0x0000 */
	union LP_SIZE_REG size;	                  /* 0x0004 */
	union LP_WIN0_REG win0;	                  /* 0x0008 */
	union LP_WIN1_REG win1;	                  /* 0x000c */
	union LP_FILTER_REG filter;	              /* 0x0010 */
	union LP_CSTM_FILTER0_REG cfilter0;	      /* 0x0014 */
	union LP_CSTM_FILTER1_REG cfilter1;	      /* 0x0018 */
	union LP_CSTM_FILTER2_REG cfilter2;	      /* 0x001c */
	union LP_GAIN_REG gain;	                  /* 0x0020 */
	union LP_GAINCTRL_REG gainctrl;	          /* 0x0024 */
	union LP_SHOOTCTRL_REG shootctrl;	        /* 0x0028 */
	union LP_CORING_REG coring;	              /* 0x002c */
};

struct __peak_config_data {
	/* peak */
	unsigned int peak_en;
	unsigned int gain;
	unsigned int hp_ratio;
	unsigned int bp0_ratio;

	/* window */
	unsigned int win_en;
	struct de_rect win;

};

struct __pq_peak_config {
	/* peak */
	unsigned int peak_en;
	unsigned int gain;
	unsigned int hp_ratio;
	unsigned int bp0_ratio;
	unsigned int bp1_ratio;
	unsigned int corth;
	unsigned int neg_gain;
	unsigned int dif_up;
	unsigned int beta;
};

#endif
