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

#ifndef __SUNXI_METADATA_H__
#define __SUNXI_METADATA_H__

enum {
	/* hdr static metadata is available */
	SUNXI_METADATA_FLAG_HDR_SATIC_METADATA   = 0x00000001,
	/* hdr dynamic metadata is available */
	SUNXI_METADATA_FLAG_HDR_DYNAMIC_METADATA = 0x00000002,

	/* afbc header data is available */
	SUNXI_METADATA_FLAG_AFBC_HEADER          = 0x00000010,
};

struct afbc_header {
	u32 signature;
	u16 filehdr_size;
	u16 version;
	u32 body_size;
	u8 ncomponents;
	u8 header_layout;
	u8 yuv_transform;
	u8 block_split;
	u8 inputbits[4];
	u16 block_width;
	u16 block_height;
	u16 width;
	u16 height;
	u8  left_crop;
	u8  top_crop;
	u16 block_layout;
};

struct display_master_data {
	/* display primaries */
	u16 display_primaries_x[3];
	u16 display_primaries_y[3];

	/* white_point */
	u16 white_point_x;
	u16 white_point_y;

	/* max/min display mastering luminance */
	u32 max_display_mastering_luminance;
	u32 min_display_mastering_luminance;
};

/* static metadata type 1 */
struct hdr_static_metadata {
	struct display_master_data disp_master;

	u16 maximum_content_light_level;
	u16 maximum_frame_average_light_level;
};

/* sunxi video metadata for ve and de */
struct sunxi_metadata {
	struct hdr_static_metadata hdr_smetada;
	struct afbc_header afbc_head;
};

#endif /* #ifndef __SUNXI_METADATA_H__ */
