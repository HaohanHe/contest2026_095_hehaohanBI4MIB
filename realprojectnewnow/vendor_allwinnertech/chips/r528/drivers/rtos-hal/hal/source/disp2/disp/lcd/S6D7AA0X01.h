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


#ifndef __S6D7AA0X01_PANEL_H__
#define  __S6D7AA0X01_PANEL_H__

#include "panels.h"

extern struct __lcd_panel S6D7AA0X01_panel;

extern __s32 dsi_dcs_wr(__u32 sel, __u8 cmd, __u8 *para_p, __u32 para_num);
extern __s32 dsi_dcs_wr_0para(__u32 sel, __u8 cmd);
extern __s32 dsi_dcs_wr_1para(__u32 sel, __u8 cmd, __u8 para);
extern __s32 dsi_dcs_wr_2para(__u32 sel, __u8 cmd, __u8 para1, __u8 para2);
extern __s32 dsi_dcs_wr_3para(__u32 sel, __u8 cmd, __u8 para1, __u8 para2,
			      __u8 para3);
extern __s32 dsi_dcs_wr_4para(__u32 sel, __u8 cmd, __u8 para1, __u8 para2,
			      __u8 para3, __u8 para4);
extern __s32 dsi_dcs_wr_5para(__u32 sel, __u8 cmd, __u8 para1, __u8 para2,
			      __u8 para3, __u8 para4, __u8 para5);
extern __s32 dsi_gen_wr_0para(__u32 sel, __u8 cmd);
extern __s32 dsi_gen_wr_1para(__u32 sel, __u8 cmd, __u8 para);
extern __s32 dsi_gen_wr_2para(__u32 sel, __u8 cmd, __u8 para1, __u8 para2);
extern __s32 dsi_gen_wr_3para(__u32 sel, __u8 cmd, __u8 para1, __u8 para2,
			      __u8 para3);
extern __s32 dsi_gen_wr_4para(__u32 sel, __u8 cmd, __u8 para1, __u8 para2,
			      __u8 para3, __u8 para4);
extern __s32 dsi_gen_wr_5para(__u32 sel, __u8 cmd, __u8 para1, __u8 para2,
			      __u8 para3, __u8 para4, __u8 para5);

#endif
