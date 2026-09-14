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

#include "include.h"
#include "disp_features.h"
#include <sunxi_hal_common.h>
s32 bsp_disp_feat_get_num_screens(void)
{
	return de_feat_get_num_screens();
}

s32 bsp_disp_feat_get_num_devices(void)
{
	return de_feat_get_num_devices();
}

s32 bsp_disp_feat_get_num_channels(u32 disp)
{
	return de_feat_get_num_chns(disp);
}

s32 bsp_disp_feat_get_num_layers(u32 disp)
{
	return de_feat_get_num_layers(disp);
}

s32 bsp_disp_feat_get_num_layers_by_chn(u32 disp, u32 chn)
{
	return de_feat_get_num_layers_by_chn(disp, chn);
}

/**
 * Query whether specified timing controller support the output_type spcified
 * @disp: the index of timing controller
 * @output_type: the display output type
 * On support, returns 1. Otherwise, returns 0.
 */
s32 bsp_disp_feat_is_supported_output_types(u32 disp,
					    u32 output_type)
{
	return de_feat_is_supported_output_types(disp, output_type);
}

s32 bsp_disp_feat_is_support_smbl(u32 disp)
{
	return de_feat_is_support_smbl(disp);
}

s32 bsp_disp_feat_is_support_capture(u32 disp)
{
	return de_feat_is_support_wb(disp);
}

s32 bsp_disp_feat_is_support_enhance(u32 disp)
{
	return de_feat_is_support_vep(disp);
}

s32 disp_init_feat(void)
{
	return de_feat_init();
}

s32 disp_exit_feat(void)
{
	return de_feat_exit();
}

u32 bsp_disp_feat_get_num_vdpo(void)
{
	return de_feat_get_number_of_vdpo();
}
