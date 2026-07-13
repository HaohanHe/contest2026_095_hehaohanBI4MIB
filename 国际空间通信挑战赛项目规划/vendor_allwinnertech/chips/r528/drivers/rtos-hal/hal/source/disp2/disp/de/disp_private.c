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

#include "../dev_disp.h"
#include "disp_private.h"

extern struct disp_drv_info g_disp_drv;


u32 dump_layer_config(struct disp_layer_config_data *data)
{
	u32 count = 0;
	char buf[512];

	count +=
	    sprintf(buf + count, " %6s ",
		    (data->config.info.mode == LAYER_MODE_BUFFER) ?
		    "buffer" : "color");
	count +=
	    sprintf(buf + count, " %8s ",
		    (data->config.enable == 1) ? "enable" : "disable");
	count += sprintf(buf + count, "ch[%1d] ", data->config.channel);
	count += sprintf(buf + count, "lyr[%1d] ", data->config.layer_id);
	count += sprintf(buf + count, "z[%1d] ", data->config.info.zorder);
	count +=
	    sprintf(buf + count, "pre_m[%1s] ",
		    (data->config.info.fb.pre_multiply) ? "Y" : "N");
	count +=
	    sprintf(buf + count, "alpha[%5s %3d] ",
		    (data->config.info.alpha_mode) ? "globl" : "pixel",
		    data->config.info.alpha_value);
	count += sprintf(buf + count, "fmt[%3d] ", data->config.info.fb.format);
	count +=
	    sprintf(buf + count, "size[%4d,%4d;%4d,%4d;%4d,%4d] ",
		    data->config.info.fb.size[0].width,
		    data->config.info.fb.size[0].height,
		    data->config.info.fb.size[0].width,
		    data->config.info.fb.size[0].height,
		    data->config.info.fb.size[0].width,
		    data->config.info.fb.size[0].height);
	count +=
	    sprintf(buf + count, "crop[%4d,%4d,%4d,%4d] ",
		    (int) (data->config.info.fb.crop.x >> 32),
		    (int) (data->config.info.fb.crop.y >> 32),
		    (int) (data->config.info.fb.crop.width >> 32),
		    (int) (data->config.info.fb.crop.height >> 32));
	count +=
	    sprintf(buf + count, "frame[%4d,%4d,%4d,%4d] ",
		    data->config.info.screen_win.x,
		    data->config.info.screen_win.y,
		    data->config.info.screen_win.width,
		    data->config.info.screen_win.height);
	count +=
	    sprintf(buf + count, "addr[%8llx,%8llx,%8llx] ",
		    data->config.info.fb.addr[0], data->config.info.fb.addr[1],
		    data->config.info.fb.addr[2]);
	count += sprintf(buf + count, "flag[0x%8x] ", data->flag);
	count += sprintf(buf + count, "\n");

	DE_WRN("%s", buf);
	return count;
}




