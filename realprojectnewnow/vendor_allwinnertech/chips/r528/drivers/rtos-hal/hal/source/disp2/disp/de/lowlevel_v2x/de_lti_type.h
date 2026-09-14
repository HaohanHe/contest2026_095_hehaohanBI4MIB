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

#ifndef __DE_LTI_TYPE_H__
#define __DE_LTI_TYPE_H__

#include "de_rtmx.h"

#define LTI_PARA_NUM 1
#define LTI_MODE_NUM 2

union LTI_EN {
	unsigned int dwval;
	struct {
		unsigned int en:1;
		unsigned int res0:7;
		unsigned int sel:1;
		unsigned int res1:7;
		unsigned int nonl_en:1;
		unsigned int res2:7;
		unsigned int win_en:1;
		unsigned int res3:7;

	} bits;
};

union LTI_SIZE {
	unsigned int dwval;
	struct {
		unsigned int width:12;
		unsigned int res0:4;
		unsigned int height:12;
		unsigned int res1:4;
	} bits;
};

union LTI_FIR_COFF0 {
	unsigned int dwval;
	struct {
		unsigned int c0:8;
		unsigned int res0:8;
		unsigned int c1:8;
		unsigned int res1:8;
	} bits;
};
union LTI_FIR_COFF1 {
	unsigned int dwval;
	struct {
		unsigned int c2:8;
		unsigned int res0:8;
		unsigned int c3:8;
		unsigned int res1:8;
	} bits;
};
union LTI_FIR_COFF2 {
	unsigned int dwval;
	struct {
		unsigned int c4:8;
		unsigned int res0:24;
	} bits;
};

union LTI_FIR_GAIN {
	unsigned int dwval;
	struct {
		unsigned int lti_fil_gain:4;
		unsigned int res0:28;

	} bits;
};

union LTI_COR_TH {
	unsigned int dwval;
	struct {
		unsigned int lti_cor_th:10;
		unsigned int res0:22;
	} bits;
};

union LTI_DIFF_CTL {
	unsigned int dwval;
	struct {
		unsigned int offset:8;
		unsigned int res0:8;
		unsigned int slope:5;
		unsigned int res1:11;
	} bits;
};

union LTI_EDGE_GAIN {
	unsigned int dwval;
	struct {
		unsigned int edge_gain:5;
		unsigned int res0:27;
	} bits;
};

union LTI_OS_CON {
	unsigned int dwval;
	struct {
		unsigned int core_x:8;
		unsigned int res0:8;
		unsigned int clip:8;
		unsigned int res1:4;
		unsigned int peak_limit:3;
		unsigned int res2:1;
	} bits;
};

union LTI_WIN_EXPANSION {
	unsigned int dwval;
	struct {
		unsigned int win_range:8;
		unsigned int res0:24;
	} bits;
};

union LTI_EDGE_ELVEL_TH {
	unsigned int dwval;
	struct {
		unsigned int elvel_th:8;
		unsigned int res0:24;
	} bits;
};

union LTI_WIN0_REG {
	unsigned int dwval;
	struct {
		unsigned int win_left:12;
		unsigned int res0:4;
		unsigned int win_top:12;
		unsigned int res1:4;
	} bits;
};

union LTI_WIN1_REG {
	unsigned int dwval;
	struct {
		unsigned int win_right:12;
		unsigned int res0:4;
		unsigned int win_bot:12;
		unsigned int res1:4;
	} bits;
};

struct __lti_reg_t {
	union LTI_EN ctrl;		               /* 0x0000 */
	unsigned int res0[2];	         /* 0x0004-0x0008 */
	union LTI_SIZE size;		             /* 0x000c */
	union LTI_FIR_COFF0 coef0;	         /* 0x0010 */
	union LTI_FIR_COFF1 coef1;	         /* 0x0014 */
	union LTI_FIR_COFF2 coef2;	         /* 0x0018 */
	union LTI_FIR_GAIN gain;	           /* 0x001c */
	union LTI_COR_TH corth;	             /* 0x0020 */
	union LTI_DIFF_CTL diff;	           /* 0x0024 */
	union LTI_EDGE_GAIN edge_gain;	     /* 0x0028 */
	union LTI_OS_CON os_con;	           /* 0x002c */
	union LTI_WIN_EXPANSION win_range;	 /* 0x0030 */
	union LTI_EDGE_ELVEL_TH elvel_th;	   /* 0x0034 */
	union LTI_WIN0_REG win0;	           /* 0x0038 */
	union LTI_WIN1_REG win1;	           /* 0x003c */
};

struct __lti_config_data {
	/* lti */
	unsigned int lti_en;
	unsigned int gain;

	/* window */
	unsigned int win_en;
	struct de_rect win;

};

#endif
