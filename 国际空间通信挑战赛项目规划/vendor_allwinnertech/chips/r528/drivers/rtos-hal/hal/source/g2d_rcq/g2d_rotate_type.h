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

#ifndef _G2D_ROTATE_TYPE_H
#define _G2D_ROTATE_TYPE_H

union g2d_rot_ctrl {
	unsigned int dwval;
	struct {
		unsigned int mode_sel:2;
		unsigned int res0:2;
		unsigned int degreee:2;
		unsigned int vflip_en:1;
		unsigned int hflip_en:1;
		unsigned int res1:22;
		unsigned int bist_en:1;
		unsigned int start:1;
	} bits;
};

union g2d_rot_interrupt {
	unsigned int dwval;
	struct {
		unsigned int rot_irq:1;
		unsigned int res0:15;
		unsigned int finish_irq:1;
		unsigned int res1:15;
	} bits;
};

union g2d_rot_time_ctrl {
	unsigned int dwval;
	struct {
		unsigned int timeout_st:1;
		unsigned int res0:29;
		unsigned int timeout_rst_en:1;
		unsigned int timeout_rst:1;
	} bits;
};

union g2d_rot_in_fmt {
	unsigned int dwval;
	struct {
		unsigned int fmt:6;
		unsigned int res0:26;
	} bits;
};

union g2d_rot_size {
	unsigned int dwval;
	struct {
		unsigned int width:13;
		unsigned int res0:3;
		unsigned int height:13;
		unsigned int res1:3;
	} bits;
};

union g2d_rot_rand_ctrl {
	unsigned int dwval;
	struct {
		unsigned int rand_en:1;
		unsigned int res0:3;
		unsigned int mode:2;
		unsigned int res1:2;
		unsigned int seed:24;
	} bits;
};

union g2d_rot_rand_clk {
	unsigned int dwval;
	struct {
		unsigned int neg_num:16;
		unsigned int pos_num:16;
	};
};


struct g2d_rot_reg {
	/*0x00*/
	union g2d_rot_ctrl rot_ctrl;
	union g2d_rot_interrupt rot_int;
	union g2d_rot_time_ctrl time_ctrl;
	unsigned int res0[5];
	/*0x20*/
	union g2d_rot_in_fmt infmt;
	union g2d_rot_size insize;
	unsigned int res1[2];
	/*0x30*/
	unsigned int pitch0;
	unsigned int pitch1;
	unsigned int pitch2;
	unsigned int res2;
	/*0x40*/
	unsigned int laddr0;
	unsigned int haddr0;
	unsigned int laddr1;
	unsigned int haddr1;
	/*0x50*/
	unsigned int laddr2;
	unsigned int haddr2;
	unsigned int res3[11];
	/*0x84*/
	union g2d_rot_size outsize;
	unsigned int res4[2];
	/*0x90*/
	unsigned int out_pitch0;
	unsigned int out_pitch1;
	unsigned int out_pitch2;
	unsigned int res5;
	/*0xa0*/
	unsigned int out_laddr0;
	unsigned int out_haddr0;
	unsigned int out_laddr1;
	unsigned int out_haddr1;
	/*0xb0*/
	unsigned int out_laddr2;
	unsigned int out_haddr2;
	union g2d_rot_rand_ctrl rand_in_ctrl;
	union g2d_rot_rand_clk rand_in_clk;
	/*0xc0*/
	union g2d_rot_rand_ctrl rand_out_ctrl;
	union g2d_rot_rand_clk rand_out_clk;
};

#endif /*End of file*/
