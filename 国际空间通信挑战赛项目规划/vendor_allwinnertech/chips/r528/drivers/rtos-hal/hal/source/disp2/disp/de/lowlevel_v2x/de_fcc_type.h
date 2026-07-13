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

#ifndef __DE_FCC_TYPE__
#define __DE_FCC_TYPE__

#include "de_rtmx.h"

#define FCC_PARA_NUM  6
#define FCC_MODE_NUM  3

union FCC_CTRL_REG {
	unsigned int dwval;
	struct {
		unsigned int en:1;
		unsigned int res0:7;
		unsigned int win_en:1;
		unsigned int res1:23;
	} bits;
};

union FCC_SIZE_REG {
	unsigned int dwval;
	struct {
		unsigned int width:13;
		unsigned int res0:3;
		unsigned int height:13;
		unsigned int res1:3;
	} bits;
};

union FCC_WIN0_REG {
	unsigned int dwval;
	struct {
		unsigned int left:13;
		unsigned int res0:3;
		unsigned int top:13;
		unsigned int res1:3;
	} bits;
};

union FCC_WIN1_REG {
	unsigned int dwval;
	struct {
		unsigned int right:13;
		unsigned int res0:3;
		unsigned int bot:13;
		unsigned int res1:3;
	} bits;
};

union FCC_HUE_RANGE_REG {
	unsigned int dwval;
	struct {
		unsigned int hmin:12;
		unsigned int res0:4;
		unsigned int hmax:12;
		unsigned int res1:4;
	} bits;
};

union FCC_HS_GAIN_REG {
	unsigned int dwval;
	struct {
		unsigned int sgain:9;
		unsigned int res0:7;
		unsigned int hgain:9;
		unsigned int res1:7;
	} bits;
};

union FCC_CSC_CTL_REG {
	unsigned int dwval;
	struct {
		unsigned int bypass:1;
		unsigned int res0:31;
	} bits;
};

union FCC_CSC_COEFF_REG {
	unsigned int dwval;
	struct {
		unsigned int coff:13;
		unsigned int res0:19;
	} bits;
};

union FCC_CSC_CONST_REG {
	unsigned int dwval;
	struct {
		unsigned int cont:20;
		unsigned int res0:12;
	} bits;
};

union FCC_GLB_APH_REG {
	unsigned int dwval;
	struct {
		unsigned int res0:24;
		unsigned int alpha:8;
	} bits;
};

struct __fcc_reg_t {
	union FCC_CTRL_REG fcc_ctl;	                /* 0x00      */
	union FCC_SIZE_REG fcc_size;	              /* 0x04      */
	union FCC_WIN0_REG fcc_win0;	              /* 0x08      */
	union FCC_WIN1_REG fcc_win1;	              /* 0x0c      */
	union FCC_HUE_RANGE_REG fcc_range[6];       /* 0x10-0x24 */
	unsigned int res0[2];	                /* 0x28-0x2c */
	union FCC_HS_GAIN_REG fcc_gain[6];	        /* 0x30-0x44 */
	unsigned int res1[2];	                /* 0x48-0x4c */
	union FCC_CSC_CTL_REG fcc_csc_ctl;	        /* 0x50      */
	unsigned int res2[3];	                /* 0x54-0x5c */
	union FCC_CSC_COEFF_REG fcc_csc_coff0[3];	  /* 0x60-0x68 */
	union FCC_CSC_CONST_REG fcc_csc_const0;	    /* 0x6c      */
	union FCC_CSC_COEFF_REG fcc_csc_coff1[3];	  /* 0x70-0x78 */
	union FCC_CSC_CONST_REG fcc_csc_const1;	    /* 0x7c      */
	union FCC_CSC_COEFF_REG fcc_csc_coff2[3];	  /* 0x80-0x88 */
	union FCC_CSC_CONST_REG fcc_csc_const2;	    /* 0x8c      */
	union FCC_GLB_APH_REG fcc_glb_alpha;	      /* 0x90      */
};

struct __fcc_config_data {
	/* ase */
	unsigned int fcc_en;
	unsigned int sgain[6];

	/* window */
	unsigned int win_en;
	struct de_rect win;
};

#endif
