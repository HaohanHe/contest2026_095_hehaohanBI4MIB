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

#ifdef CONFIG_DISP2_SUNXI_SUPPORT_ENAHNCE
#include "de_ase_type.h"
#include "de_rtmx.h"
#include "de_enhance.h"

static volatile struct __ase_reg_t *ase_dev[DE_NUM][CHN_NUM];
static struct de_reg_blocks ase_block[DE_NUM][CHN_NUM];

/*****************************************************************************
 * function       : de_ase_set_reg_base(unsigned int sel, unsigned int chno,
 *                       unsigned int base)
 * description    : set ase reg base
 * parameters     :
 *                  sel         <rtmx select>
 *                  chno        <overlay select>
 *                  base        <reg base>
 * return         :
 *                  success
 *****************************************************************************/

int de_ase_set_reg_base(unsigned int sel, unsigned int chno, void *base)
{
	ase_dev[sel][chno] = (struct __ase_reg_t *) base;

	return 0;
}

int de_ase_update_regs(unsigned int sel, unsigned int chno)
{
	if (ase_block[sel][chno].dirty == 0x1) {
		regwrite((void *)ase_block[sel][chno].off,
			ase_block[sel][chno].val, ase_block[sel][chno].size);
		ase_block[sel][chno].dirty = 0x0;
	}

	return 0;
}

int de_ase_init(unsigned int sel, unsigned int chno, uintptr_t reg_base)
{
	uintptr_t base;
	void *memory;

	base = reg_base + (sel + 1) * 0x00100000 + ASE_OFST;
#if defined(CONFIG_ARCH_SUN50IW10)
	if (sel)
		base = base - 0x00100000;
#endif
	/*FIXME  display path offset should be defined*/
	DE_INF("sel %d, ase_base[%d]=0x%p\n", sel, chno, (void *)base);

	memory = disp_sys_malloc(sizeof(struct __ase_reg_t));
	if (memory == NULL) {
		DE_WRN("disp_sys_malloc ase[%d][%d] memory fail! size=0x%x\n", sel, chno,
			(unsigned int)sizeof(struct __ase_reg_t));
		return -1;
	}

	ase_block[sel][chno].off = base;
	ase_block[sel][chno].val = memory;
	ase_block[sel][chno].size = 0x14;
	ase_block[sel][chno].dirty = 0;

	de_ase_set_reg_base(sel, chno, memory);

	return 0;
}

int de_ase_exit(unsigned int sel, unsigned int chno)
{
	disp_sys_free(ase_block[sel][chno].val);

	return 0;
}

/*****************************************************************************
 * function       : de_ase_enable(unsigned int sel, unsigned int chno,
 *                           unsigned int en)
 * description    : enable/disable ase
 * parameters     :
 *                  sel         <rtmx select>
 *                  chno        <overlay select>
 *                  en          <enable: 0-disable; 1-enable>
 * return         :
 *                   success
 ******************************************************************************/
int de_ase_enable(unsigned int sel, unsigned int chno, unsigned int en)
{
	ase_dev[sel][chno]->ctrl.bits.en = en;
	ase_block[sel][chno].dirty = 1;
	return 0;
}

/*****************************************************************************
 * function       : de_ase_set_size(unsigned int sel, unsigned int chno,
 *                         unsigned int width, unsigned int height)
 * description    : set ase size
 * parameters     :
 *                  sel         <rtmx select>
 *                  chno        <overlay select>
 *                  width       <input width>
 *                  height  <input height>
 * return         :
 *                     success
 ******************************************************************************/
int de_ase_set_size(unsigned int sel, unsigned int chno, unsigned int width,
				unsigned int height)
{
	ase_dev[sel][chno]->size.bits.width = width - 1;
	ase_dev[sel][chno]->size.bits.height = height - 1;
	ase_block[sel][chno].dirty = 1;
	return 0;
}

/*****************************************************************************
 * function       : de_ase_set_window(unsigned int sel, unsigned int chno,
 *                   unsigned int win_enable, struct de_rect window)
 * description    : set ase window
 * parameters     :
 *                  sel         <rtmx select>
 *                  chno        <overlay select>
 *                  win_enable  <enable: 0-window mode disable;
 *                  1-window mode enable>
 *                  window  <window rectangle>
 * return         :
 *                  success
 ******************************************************************************/
int de_ase_set_window(unsigned int sel, unsigned int chno,
					unsigned int win_enable,
					struct de_rect window)
{
	ase_dev[sel][chno]->ctrl.bits.win_en = win_enable;

	if (win_enable) {
		ase_dev[sel][chno]->win0.bits.left = window.x;
		ase_dev[sel][chno]->win0.bits.top = window.y;
		ase_dev[sel][chno]->win1.bits.right = window.x + window.w - 1;
		ase_dev[sel][chno]->win1.bits.bot = window.y + window.h - 1;
	}
	ase_block[sel][chno].dirty = 1;
	return 0;
}

/*****************************************************************************
 * function       : de_ase_set_para(unsigned int sel, unsigned int chno,
 *                  unsigned int gain)
 * description    : set ase para
 * parameters     :
 *                  sel         <rtmx select>
 *                  chno        <overlay select>
 *                  gain        <ase gain: normal setting 16-24>
 * return         :
 *                  success
 ******************************************************************************/
int de_ase_set_para(unsigned int sel, unsigned int chno, unsigned int gain)
{
	ase_dev[sel][chno]->gain.bits.gain = gain;
	ase_block[sel][chno].dirty = 1;
	return 0;
}

/**
 * function       : de_ase_info2para
 * description    : info->para conversion
 * parameters     :
 *                  auto_color  <gain info from user>
 *                  window              <window info>
 *                  para                <bsp para>
 * return         :
 *                  success
 */
int de_ase_info2para(unsigned int auto_color, struct de_rect window,
				 struct __ase_config_data *para)
{
	int ase_mode;
	int ase_para[ASE_PARA_NUM][ASE_MODE_NUM] = {
		{0, 12, 16, 20}
	};

	ase_mode = ((auto_color >> 4) & 0xf) ? 1 : 0;
	para->ase_en = ase_mode ? 1 : 0;
	para->gain = ase_para[0][ase_mode];

	return 0;
}
#endif
