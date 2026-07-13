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

#ifndef __RTMX_EE_H__
#define __RTMX_EE_H__

#if 0
enum de_color_space {
	DE_BT601  = 0,
	DE_BT709  = 1,
	DE_YCC    = 2,
	DE_ENHANCE = 3,
	/* DE_VXYCC  = 3, */
};

enum de_pixel_format {
	DE_FORMAT_ARGB_8888                    = 0x00,/* MSB  A-R-G-B  LSB */
	DE_FORMAT_ABGR_8888                    = 0x01,
	DE_FORMAT_RGBA_8888                    = 0x02,
	DE_FORMAT_BGRA_8888                    = 0x03,
	DE_FORMAT_XRGB_8888                    = 0x04,
	DE_FORMAT_XBGR_8888                    = 0x05,
	DE_FORMAT_RGBX_8888                    = 0x06,
	DE_FORMAT_BGRX_8888                    = 0x07,
	DE_FORMAT_RGB_888                      = 0x08,
	DE_FORMAT_BGR_888                      = 0x09,
	DE_FORMAT_RGB_565                      = 0x0a,
	DE_FORMAT_BGR_565                      = 0x0b,
	DE_FORMAT_ARGB_4444                    = 0x0c,
	DE_FORMAT_ABGR_4444                    = 0x0d,
	DE_FORMAT_RGBA_4444                    = 0x0e,
	DE_FORMAT_BGRA_4444                    = 0x0f,
	DE_FORMAT_ARGB_1555                    = 0x10,
	DE_FORMAT_ABGR_1555                    = 0x11,
	DE_FORMAT_RGBA_5551                    = 0x12,
	DE_FORMAT_BGRA_5551                    = 0x13,

	/* SP: semi-planar, P:planar, I:interleaved
	 * UVUV: U in the LSBs;     VUVU: V in the LSBs
	 */
	DE_FORMAT_YUV444_I_AYUV         = 0x40,/* MSB  A-Y-U-V  LSB, reserved */
	DE_FORMAT_YUV444_I_VUYA                = 0x41,/* MSB  V-U-Y-A  LSB */
	DE_FORMAT_YUV422_I_YVYU                = 0x42,/* MSB  Y-V-Y-U  LSB */
	DE_FORMAT_YUV422_I_YUYV                = 0x43,/* MSB  Y-U-Y-V  LSB */
	DE_FORMAT_YUV422_I_UYVY                = 0x44,/* MSB  U-Y-V-Y  LSB */
	DE_FORMAT_YUV422_I_VYUY                = 0x45,/* MSB  V-Y-U-Y  LSB */
	DE_FORMAT_YUV444_P                     = 0x46,
			/* MSB  P3-2-1-0 LSB,  YYYY UUUU VVVV, reserved */
	DE_FORMAT_YUV422_P                     = 0x47,
			/* MSB  P3-2-1-0 LSB   YYYY UU   VV */
	DE_FORMAT_YUV420_P                     = 0x48,
			/* MSB  P3-2-1-0 LSB   YYYY U    V */
	DE_FORMAT_YUV411_P                     = 0x49,
			/* MSB  P3-2-1-0 LSB   YYYY U    V */
	DE_FORMAT_YUV422_SP_UVUV               = 0x4a,/* MSB  V-U-V-U  LSB */
	DE_FORMAT_YUV422_SP_VUVU               = 0x4b,/* MSB  U-V-U-V  LSB */
	DE_FORMAT_YUV420_SP_UVUV               = 0x4c,
	DE_FORMAT_YUV420_SP_VUVU               = 0x4d,
	DE_FORMAT_YUV411_SP_UVUV               = 0x4e,
	DE_FORMAT_YUV411_SP_VUVU               = 0x4f,
};

struct de_fb {
	unsigned int w;
	unsigned int h;
};

struct de_rect {
	int x;
	int y;
	unsigned int w;
	unsigned int h;
};

struct de_rect64 {
	long long x;
	long long y;
	unsigned long long w;
	unsigned long long h;
};
#endif

int rtmx_set_base(unsigned int reg_base);
void rt_mixer_init(int sel, unsigned int addr0, unsigned int addr1,
		unsigned int addr2, unsigned int w, unsigned int h,
		unsigned int outw, unsigned int outh, unsigned int fmt);
void rt_mixer_set_addr(int sel, unsigned int addr0);

#endif
