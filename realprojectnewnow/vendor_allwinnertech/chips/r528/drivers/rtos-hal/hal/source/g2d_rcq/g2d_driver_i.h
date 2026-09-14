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

#ifndef __G2D_DRIVER_I_H
#define __G2D_DRIVER_I_H

#ifdef CONFIG_OS_NUTTX
#include "aw_types.h"
#endif
#include <hal_sem.h>
#include <hal_clk.h>
#include <hal_reset.h>
#include <hal_interrupt.h>
#include "g2d_bsp.h"

/* #include "g2d_bsp_v2.h" */

#define INFO(format, args...) printf("%s: " format, "G2D", ## args)

#if defined(CONFIG_FPGA_V4_PLATFORM) || defined(CONFIG_FPGA_V7_PLATFORM)
#define WAIT_CMD_TIME_MS 500
#else
#define WAIT_CMD_TIME_MS 100
#endif

#define G2D_CLK_NUM 3

#define G2DALIGN(value, align) ((align == 0) ? \
				value : \
				(((value) + ((align) - 1)) & ~((align) - 1)))



struct test
{
int i;
 hal_sem_t mutex;


};

struct info_mem {
	unsigned long phy_addr;
	void *virt_addr;
	__u32 b_used;
	__u32 mem_len;
};

typedef struct {
	__u32 io;
	__u32 irq;
	 hal_sem_t mutex;
	bool opened;
	__u32 user_cnt;
	hal_clk_t clk[G2D_CLK_NUM];
	hal_clk_t clk_parent;
	struct reset_control *reset;
	u32 clk_rate;
} __g2d_info_t;

typedef struct {
	__u32 mid;
	__u32 used;
	__u32 status;
	hal_sem_t queue_sem;
	__u32 finish_flag;
} __g2d_drv_t;

struct g2d_alloc_struct {
	__u32 address;
	__u32 size;
	__u32 u_size;
	struct g2d_alloc_struct *next;
};

/* g2d_format_attr - g2d format attribute
 *
 * @format: pixel format
 * @bits: bits of each component
 * @hor_rsample_u: reciprocal of horizontal sample rate
 * @hor_rsample_v: reciprocal of horizontal sample rate
 * @ver_rsample_u: reciprocal of vertical sample rate
 * @hor_rsample_v: reciprocal of vertical sample rate
 * @uvc: 1: u & v component combined
 * @interleave: 0: progressive, 1: interleave
 * @factor & div: bytes of pixel = factor / div (bytes)
 * @addr[out]: address for each plane
 * @trd_addr[out]: address for each plane of right eye buffer
 */
struct g2d_format_attr {
	g2d_fmt_enh format;
	unsigned int bits;
	unsigned int hor_rsample_u;
	unsigned int hor_rsample_v;
	unsigned int ver_rsample_u;
	unsigned int ver_rsample_v;
	unsigned int uvc;
	unsigned int interleave;
	unsigned int factor;
	unsigned int div;
};

void clear_g2d_waitqueue_sem(void);
int g2d_wait_cmd_finish(unsigned int timeout);
void *g2d_malloc(__u32 bytes_num, __u32 *phy_addr);
void g2d_free(void *virt_addr, void *phy_addr, unsigned int size);
__s32 g2d_image_check(g2d_image_enh *p_image);
__s32 g2d_byte_cal(__u32 format, __u32 *ycnt, __u32 *ucnt, __u32 *vcnt);
__u32 cal_align(__u32 width, __u32 align);

#endif /* __G2D_DRIVER_I_H */
