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

#ifndef __DRV_PQ_H__
#define __DRV_PQ_H__

#define PQ_REG_MASK 0xff000

struct matrix4x4 {
	__s64 x00;
	__s64 x01;
	__s64 x02;
	__s64 x03;
	__s64 x10;
	__s64 x11;
	__s64 x12;
	__s64 x13;
	__s64 x20;
	__s64 x21;
	__s64 x22;
	__s64 x23;
	__s64 x30;
	__s64 x31;
	__s64 x32;
	__s64 x33;
};

enum choice{
	BT601_F2F,
	BT709_F2F,
	YCC,
	ENHANCE,
	NUM_SUM,
};

struct matrix_user {
	int cmd;
	int read;
	int choice;
	struct matrix4x4 matrix;
};

struct color_enhanc {
	int cmd;
	int read;
	int contrast;
	int brightness;
	int saturation;
	int hue;
};

struct color_matrix {
	int cmd;
	int read;
};

enum {
	PQ_SET_REG = 0x1,
	PQ_GET_REG = 0x2,
	PQ_ENABLE = 0x3,
	PQ_COLOR_MATRIX = 0x4,
};

struct pq_private_data {
	bool enabled;
	s32 (*shadow_protect)(u32 sel, bool protect);
};

struct pq_reg {
	u32 offset;
	u32 value;
};

enum pq_block_flag {
	OP_NONE = 0,
	OP_PEAK = 0x1,
	OP_FTC = 0x2,
	OP_CE = 0x4,
	OP_BWS = 0x8,
	OP_LTI = 0x10,
	OP_FCC = 0x20,
	OP_GAMMA = 0x40,
};

void pq_set_reg(u32 sel, u32 off, u32 value);
void pq_get_reg(u32 sel, u32 offset, u32 *value);
void pq_set_matrix(struct matrix4x4 *conig, int choice, int out, int write);
void pq_set_enhance(struct color_enhanc *pq_enh, int read);
void disp_pq_force_flush(int disp);
int pq_ioctl(unsigned int cmd, unsigned long arg);
#endif
