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

#ifndef _G2D_TOP_TYPE_H
#define _G2D_TOP_TYPE_H


union g2d_sclk_gate {
	unsigned int dwval;
	struct {
		unsigned int mixer_sclk_gate:1;
		unsigned int rot_sclk_gate:1;
		unsigned int res0:30;
	} bits;
};

union g2d_hclk_gate {
	unsigned int dwval;
	struct {
		unsigned int mixer_hclk_gate:1;
		unsigned int rot_hclk_gate:1;
		unsigned int res0:30;
	} bits;
};

union g2d_ahb_reset {
	unsigned int dwval;
	struct {
		unsigned int mixer_ahb_rst:1;
		unsigned int rot_ahb_rst:1;
		unsigned int res0:30;
	} bits;
};

union g2d_sclk_div {
	unsigned int dwval;
	struct {
		unsigned int mixer_sclk_div:4;
		unsigned int rot_sclk_div:4;
		unsigned int res0:24;
	} bits;
};

union g2d_version {
	unsigned int dwval;
	struct {
		unsigned int gsu_no:2;
		unsigned int vsu_no:2;
		unsigned int rtmx_no:1;
		unsigned int res0:3;
		unsigned int rot_no:1;
		unsigned int res1:7;
		unsigned int ip_version:16;
	} bits;
};

union g2d_rcq_irq_ctl {
	unsigned int dwval;
	struct {
		unsigned int rcq_sel:1;
		unsigned int res0:3;
		unsigned int task_end_irq_en:1;
		unsigned int res1:1;
		unsigned int rcq_cfg_finish_irq_en:1;
		unsigned int res2:25;
	} bits;
};

union g2d_rcq_status {
	unsigned int dwval;
	struct {
		unsigned int task_end_irq:1;
		unsigned int res0:1;
		unsigned int cfg_finish_irq:1;
		unsigned int res1:5;
		unsigned int frame_cnt:8;
		unsigned int res2:16;
	} bits;
};

union g2d_rcq_ctrl {
	unsigned int dwval;
	struct {
		unsigned int update:1;
		unsigned int res0:31;
	} bits;
};

union g2d_rcq_header_len {
	unsigned int dwval;
	struct {
		unsigned int rcq_header_len:16;
		unsigned int res0:16;
	} bits;
};


struct g2d_top_reg {
	/*0x00*/
	union g2d_sclk_gate sclk_gate;
	union g2d_hclk_gate hclk_gate;
	union g2d_ahb_reset ahb_rst;
	union g2d_sclk_div sclk_div;
	/*0x10*/
	union g2d_version version;
	unsigned int res0[3];
	/*0x20*/
	union g2d_rcq_irq_ctl rcq_irq_ctl;
	union g2d_rcq_status rcq_status;
	union g2d_rcq_ctrl rcq_ctrl;
	unsigned int rcq_header_low_addr;
	/*0x30*/
	unsigned int rcq_header_high_addr;
	union g2d_rcq_header_len rcq_header_len;
};

/*mixer global register define start*/
union g2d_mxier_ctrl {
	unsigned int dwval;
	struct {
		unsigned int res0:4;
		unsigned int scan_order:2;
		unsigned int res1:2;
		unsigned int bist_en:1;
		unsigned int res2:22;
		unsigned int start:1;
	} bits;
};

union g2d_mixer_interrupt {
	unsigned int dwval;
	struct {
		unsigned int mixer_irq:1;
		unsigned int res0:3;
		unsigned int finish_irq_en:1;
		unsigned int res1:27;
	} bits;
};

struct g2d_mixer_glb_reg {
	union g2d_mxier_ctrl mixer_ctrl;
	union g2d_mixer_interrupt mixer_interrupt;
};
/*mixer global register define end*/

#endif /*End of file*/
