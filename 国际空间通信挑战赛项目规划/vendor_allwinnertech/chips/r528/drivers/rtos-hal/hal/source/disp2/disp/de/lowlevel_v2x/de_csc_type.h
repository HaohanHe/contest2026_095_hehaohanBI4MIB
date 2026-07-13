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

#ifndef __DE_CSC_TYPE_H__
#define __DE_CSC_TYPE_H__

union CSC_BYPASS_REG {
	unsigned int dwval;
	struct {
		unsigned int enable:1;
		unsigned int res0:31;
	} bits;
};

union CSC_BYPASS_REG2 {
	unsigned int dwval;
	struct {
		unsigned int res0:1;
		unsigned int enable:1;
		unsigned int res1:30;
	} bits;
};

union CSC_COEFF_REG {
	unsigned int dwval;
	struct {
		unsigned int coeff:13;
		unsigned int res0:19;
	} bits;
};

union CSC_CONST_REG {
	unsigned int dwval;
	struct {
		unsigned int cnst:20;
		unsigned int res0:12;
	} bits;
};

union CSC_CONST_REG2 {
	unsigned int dwval;
	struct {
		unsigned int cnst:14;
		unsigned int res0:18;
	} bits;
};

union GLB_ALPHA_REG {
	unsigned int dwval;
	struct {
		unsigned int cnst:24;
		unsigned int alpha:8;
	} bits;
};

union GAMMA_CTL_REG {
	unsigned int dwval;
	struct {
		unsigned int gamma_en:1;
		unsigned int color_mode:3;
		unsigned int blue_en:1;
		unsigned int res0:27;
	} bits;
};

union COLOR_REG {
	unsigned int dwval;
	struct {
		unsigned int B:10;
		unsigned int G:10;
		unsigned int R:10;
		unsigned int res0:2;
	} bits;
};

/* Channel CSC and Device CSC */
struct __csc_reg_t {
	union CSC_BYPASS_REG bypass;
	unsigned int res[3];
	union CSC_COEFF_REG c00;
	union CSC_COEFF_REG c01;
	union CSC_COEFF_REG c02;
	union CSC_CONST_REG c03;
	union CSC_COEFF_REG c10;
	union CSC_COEFF_REG c11;
	union CSC_COEFF_REG c12;
	union CSC_CONST_REG c13;
	union CSC_COEFF_REG c20;
	union CSC_COEFF_REG c21;
	union CSC_COEFF_REG c22;
	union CSC_CONST_REG c23;
	union GLB_ALPHA_REG alpha;
};

struct __gamma_reg_t {
	union GAMMA_CTL_REG gamma_ctl;
	union COLOR_REG blue_screen;
	union COLOR_REG gamma_tbl[1024];
};

/* CSC IN SMBL */
struct __csc2_reg_t {
	union CSC_BYPASS_REG2 bypass;
	unsigned int res[31];
	union CSC_COEFF_REG c00;
	union CSC_COEFF_REG c01;
	union CSC_COEFF_REG c02;
	union CSC_CONST_REG2 c03;
	union CSC_COEFF_REG c10;
	union CSC_COEFF_REG c11;
	union CSC_COEFF_REG c12;
	union CSC_CONST_REG2 c13;
	union CSC_COEFF_REG c20;
	union CSC_COEFF_REG c21;
	union CSC_COEFF_REG c22;
	union CSC_CONST_REG2 c23;
};

/* Input CSC in FCE */
struct __icsc_reg_t {
	union CSC_BYPASS_REG bypass;
};
#endif
