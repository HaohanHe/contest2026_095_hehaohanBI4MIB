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

#include "../de/disp_private.h"
#include "../dev_disp.h"

static struct color_enhanc pq_enhance= {
	0,0,50,50,50,50
};

void de_set_pq_reg(unsigned long addr, u32 off, u32 value)
{
	pr_info("addr = 0x%lx, off = 0x%x, value = 0x%x\n", addr, off, value);
	*((unsigned int *)((void *)addr + off)) = value;
	return;
}

void de_get_pq_reg(unsigned long addr, u32 off, u32 *value)
{
	*value = *((unsigned int *)((void *)addr + off));
	return;
}

void pq_set_reg(u32 sel, u32 off, u32 value)
{
	unsigned long reg_base = 0;
	//shadow_protect
	reg_base = disp_al_get_reg_base(sel, &off, 1);
	pr_info("[%s]+++reg_base = 0x%lx, off = 0x%x\n", __func__, reg_base, off);
	if (reg_base)
		de_set_pq_reg(reg_base, off, value);
	else
		pr_err("Something err,set reg failed\n");

	return;
}

void pq_get_reg(u32 sel, u32 offset, u32 *value)
{
	unsigned long reg_base = 0;
	//shadow_protect
	reg_base = disp_al_get_reg_base(sel, &offset, 0);
	if (reg_base)
		de_get_pq_reg(reg_base, offset, value);
	else
		pr_err("Something err,Get reg failed\n");
}

void pq_get_enhance(struct disp_csc_config *conig)
{
	conig->brightness = pq_enhance.brightness;
	conig->contrast = pq_enhance.contrast;
	conig->saturation = pq_enhance.saturation;
	conig->hue = pq_enhance.hue;
}

void pq_set_enhance(struct color_enhanc *pq_enh, int read)
{
	if (read) {
		*pq_enh = pq_enhance;
	} else {
		pq_enhance = *pq_enh;
		disp_enhance_bright_store(0,pq_enh->brightness);
		disp_enhance_contrast_store(0,pq_enh->contrast);
		disp_enhance_saturation_store(0,pq_enh->saturation);
		disp_enhance_hue_store(0,pq_enh->hue);
	}
}

