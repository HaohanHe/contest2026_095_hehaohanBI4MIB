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


#include "g2d_driver_i.h"
#include "g2d_top.h"
#include "g2d_top_type.h"

static volatile struct g2d_top_reg *g2d_top;
static volatile struct g2d_mixer_glb_reg *mixer_glb;

void g2d_top_set_base(unsigned long base)
{
	g2d_top = (struct g2d_top_reg *)(base);
	mixer_glb = (struct g2d_mixer_glb_reg *)(base + 0x0100);
}

void g2d_mixer_scan_order_fun(__u32 scan_order)
{
	mixer_glb->mixer_ctrl.bits.scan_order = scan_order;
}

void g2d_mixer_start(__u32 start)
{
	mixer_glb->mixer_ctrl.bits.start = start;
}

void g2d_mixer_irq_en(__u32 en)
{
	mixer_glb->mixer_interrupt.bits.finish_irq_en = en;
}

__s32 g2d_mixer_irq_query(void)
{
	if (mixer_glb->mixer_interrupt.bits.mixer_irq & 0x1) {
		mixer_glb->mixer_interrupt.bits.mixer_irq = 1;
		return 1;
	}
	return 0;
}

__s32 g2d_bsp_open(void)
{
	g2d_top->sclk_gate.bits.mixer_sclk_gate = 1;
	g2d_top->hclk_gate.bits.mixer_hclk_gate = 1;
	g2d_top->ahb_rst.bits.mixer_ahb_rst = 1;
	g2d_top->sclk_gate.bits.rot_sclk_gate = 1;
	g2d_top->hclk_gate.bits.rot_hclk_gate = 1;
	g2d_top->ahb_rst.bits.rot_ahb_rst = 1;
	return 0;
}

__s32 g2d_bsp_close(void)
{
	g2d_top->sclk_gate.bits.mixer_sclk_gate = 0;
	g2d_top->hclk_gate.bits.mixer_hclk_gate = 0;
	g2d_top->ahb_rst.bits.mixer_ahb_rst = 0;
	g2d_top->sclk_gate.bits.rot_sclk_gate = 0;
	g2d_top->hclk_gate.bits.rot_hclk_gate = 0;
	g2d_top->ahb_rst.bits.rot_ahb_rst = 0;
	return 0;
}

__s32 g2d_bsp_reset(void)
{
	g2d_top->ahb_rst.bits.mixer_ahb_rst = 0;
	g2d_top->ahb_rst.bits.mixer_ahb_rst = 1;
	g2d_top->ahb_rst.bits.rot_ahb_rst = 0;
	g2d_top->ahb_rst.bits.rot_ahb_rst = 1;
	return 0;
}

__s32 g2d_top_mixer_reset(void)
{
	g2d_top->ahb_rst.bits.mixer_ahb_rst = 0;
	g2d_top->ahb_rst.bits.mixer_ahb_rst = 1;
	return 0;
}

__s32 g2d_top_rot_reset(void)
{
	g2d_top->ahb_rst.bits.rot_ahb_rst = 0;
	g2d_top->ahb_rst.bits.rot_ahb_rst = 1;
	return 0;
}


__s32 g2d_top_mixer_sclk_div(__u32 div)
{
	g2d_top->sclk_div.bits.mixer_sclk_div = div;
	return 0;
}

__s32 g2d_top_rot_sclk_div(__u32 div)
{
	g2d_top->sclk_div.bits.rot_sclk_div = div;
	return 0;
}

void g2d_top_rcq_irq_en(__u32 en)
{
	g2d_top->rcq_irq_ctl.bits.task_end_irq_en = en;
	/*g2d_top->rcq_irq_ctl.bits.rcq_cfg_finish_irq_en = en;*/
}

void g2d_top_rcq_update_en(__u32 en)
{
	g2d_top->rcq_ctrl.bits.update = en;
}

__s32 g2d_top_rcq_task_irq_query(void)
{
	if (g2d_top->rcq_status.bits.task_end_irq & 0x1) {
		g2d_top->rcq_status.bits.task_end_irq = 1;
		return 1;
	}
	return 0;
}

__s32 g2d_top_rcq_cfg_irq_query(void)
{
	if (g2d_top->rcq_status.bits.cfg_finish_irq & 0x1) {
		g2d_top->rcq_status.bits.cfg_finish_irq = 1;
		return 1;
	}
	return 0;
}

__u32 g2d_top_get_rcq_frame_cnt(void)
{
	return g2d_top->rcq_status.bits.frame_cnt;
}


void g2d_top_set_rcq_head(u64 addr, __u32 len)
{
	__u32 haddr = (__u32)(addr >> 32);

	g2d_top->rcq_header_low_addr = addr;
	g2d_top->rcq_header_high_addr = haddr;
	g2d_top->rcq_header_len.bits.rcq_header_len = len;
}

