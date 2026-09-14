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

#include "fb_g2d_rot.h"
#include "r528_memorymap.h"
extern int g2d_blit_h(g2d_blt_h *para);
extern int sunxi_g2d_open(void);
extern int sunxi_g2d_close(void);
extern void g2d_ioctl_mutex_lock(void);
extern void g2d_ioctl_mutex_unlock(void);

int fb_g2d_rot_free(struct fb_g2d_rot_t *inst)
{
	if (inst) {
		hal_free(inst->dst_vir_addr);
		hal_free(inst);
		sunxi_g2d_close();
	}
	return 0;
}

#ifdef CONFIG_FB_OVERLAY
int fb_g2d_realloc(struct fb_g2d_rot_t *inst, const struct fb_overlayinfo_s *oinfo)
{
	if (inst->dst_overlay_size != oinfo->sarea.w * oinfo->sarea.h * 3 * FB_NUM_OVERLAY / 2){
		hal_free(inst->rot_dst_overlay);
		inst->rot_dst_overlay = hal_malloc(oinfo->sarea.w * oinfo->sarea.h * 3 * FB_NUM_OVERLAY / 2);
		inst->overlay_index = 0;
	}
	return 0;
}
#endif

int fb_g2d_rot_apply(struct fb_g2d_rot_t *inst)
{
	int ret = -1;

	if (!inst || !inst->vinfo || !inst->pinfo) {
		DE_WRN("%s:Null pointer\n", __func__);
		return ret;
	}


	inst->info.src_image_h.laddr[0] =
	    (unsigned long)inst->pinfo->fbmem;
	inst->info.dst_image_h.laddr[0] =
	    (unsigned long)inst->dst_phy_addr +
	    inst->pinfo->stride * inst->pinfo->yoffset;
	hal_dcache_clean_invalidate((unsigned long)inst->pinfo->fbmem, inst->pinfo->fblen);
	hal_dcache_clean_invalidate((unsigned long)inst->dst_phy_addr, inst->pinfo->fblen);
	bsp_disp_shadow_protect(0, 1);
	if(sunxi_g2d_control(G2D_CMD_BITBLT_H ,&inst->info) < 0)
		DE_WRN("g2d_blit_h fail!ret:%d\n", ret);
	bsp_disp_shadow_protect(0, 0);
	return ret;
}
#ifdef CONFIG_FB_OVERLAY
int fboverlay_g2d_rot_apply(struct fb_g2d_rot_t *inst, const struct fb_overlayinfo_s *oinfo)
{
	g2d_blt_h blit;
	unsigned char *dst;

	dst = inst->rot_dst_overlay + (oinfo->yoffset / oinfo->yres) * oinfo->sarea.w * oinfo->sarea.h * 3 / 2;

	memset(&blit, 0, sizeof(g2d_blt_h));

	// 关键修复：使用实例中的旋转标志，而不是硬编码270度
	blit.flag_h = inst->info.flag_h;

	blit.src_image_h.format = G2D_FORMAT_YUV420UVC_V1U1V0U0;
	blit.src_image_h.mode = G2D_GLOBAL_ALPHA;
	blit.src_image_h.alpha = 0xff;
	blit.src_image_h.clip_rect.x = oinfo->sarea.x;
	blit.src_image_h.clip_rect.y = oinfo->sarea.y;
	blit.src_image_h.clip_rect.w = oinfo->sarea.w;
	blit.src_image_h.clip_rect.h = oinfo->sarea.h;
	blit.src_image_h.width = VIDEO_WIDTH;
	blit.src_image_h.height = VIDEO_HEIGHT;
	blit.src_image_h.laddr[0] = (__u32)(oinfo->fbmem + oinfo->yoffset * oinfo->stride * 3 / 2);
	blit.src_image_h.laddr[1] = (__u32)(oinfo->fbmem + oinfo->yoffset * oinfo->stride * 3 / 2 + oinfo->stride * oinfo->yres_virtual / FB_NUM_OVERLAY);
	blit.src_image_h.laddr[2] = blit.src_image_h.laddr[1];
	blit.dst_image_h.format = G2D_FORMAT_YUV420UVC_V1U1V0U0;
	blit.dst_image_h.mode = G2D_GLOBAL_ALPHA;
	blit.dst_image_h.alpha = 0xff;

	// 根据旋转角度调整目标尺寸
	switch (inst->info.flag_h) {
	case G2D_ROT_90:
	case G2D_ROT_270:
		// 90度和270度旋转时，宽高互换
	blit.dst_image_h.width = oinfo->sarea.h;
	blit.dst_image_h.height = oinfo->sarea.w;
		break;
	case G2D_ROT_0:
	case G2D_ROT_180:
	default:
		// 0度和180度旋转时，宽高不变
		blit.dst_image_h.width = oinfo->sarea.w;
		blit.dst_image_h.height = oinfo->sarea.h;
		break;
	}

	blit.dst_image_h.laddr[0] = (__u32)dst;
	blit.dst_image_h.laddr[1] = (__u32)(dst + oinfo->sarea.w * oinfo->sarea.h);
	blit.dst_image_h.laddr[2] = blit.dst_image_h.laddr[1];
	hal_dcache_clean_invalidate((unsigned long)oinfo->fbmem, oinfo->fblen);
	hal_dcache_clean_invalidate((unsigned long)dst, oinfo->sarea.w * oinfo->sarea.h * 3 / 2);
	if(sunxi_g2d_control(G2D_CMD_BITBLT_H ,&blit) < 0)
	{
		printf("fail\n");
	}

	return 0;
}
#endif

struct fb_g2d_rot_t *fb_g2d_rot_create(struct fb_videoinfo_s *vinfo,
						struct fb_planeinfo_s *pinfo,
						unsigned int fb_id,
						struct disp_layer_config *config)
{
	int ret = -1;
	s32 value = 0;
	char sub_name[32] = {0};
	struct fb_g2d_rot_t *fb_rot = NULL;

	if (!pinfo || !vinfo || !config) {
		DE_WRN("%s:Null pointer\n", __func__);
		return NULL;
	}

	ret = disp_sys_script_get_item("disp", "disp_rotation_used", &value, 1);
	if (ret != 1)
		value = 0;

	if (!value) {
		DE_WRN("rotation hw function is configed as no used\n");
		return NULL;
	}
	sprintf(sub_name, "degree%d", fb_id);
	ret = disp_sys_script_get_item("disp", sub_name, &value, 1);
	if (value == FB_ROTATION_HW_0 || ret != 1) {
		DE_WRN("rotation hw function is configed to zero degree\n");
		return NULL;
	}

	fb_rot = hal_malloc(sizeof(struct fb_g2d_rot_t));
	if (!fb_rot) {
		DE_WRN("kmalloc fail!!\n");
		return NULL;
	}
	ret = sunxi_g2d_open();
	if (ret)
		goto ERROR;

	fb_rot->info.src_image_h.width = vinfo->xres;
	fb_rot->info.src_image_h.height = vinfo->yres;

	switch (value) {
	case FB_ROTATION_HW_90:
		fb_rot->info.flag_h = G2D_ROT_90;
		fb_rot->info.dst_image_h.width = vinfo->yres;
		fb_rot->info.dst_image_h.height = vinfo->xres;
		fb_rot->info.dst_image_h.clip_rect.w = fb_rot->info.dst_image_h.height;
		fb_rot->info.dst_image_h.clip_rect.h = fb_rot->info.dst_image_h.width;
		config->info.fb.crop.width = ((long long)vinfo->yres << 32);
		config->info.fb.crop.height = ((long long)vinfo->xres << 32);
		config->info.fb.size[0].width = vinfo->yres;
		config->info.fb.size[0].height = vinfo->xres;
		config->info.fb.size[1].width = vinfo->yres;
		config->info.fb.size[1].height = vinfo->xres;
		config->info.fb.size[2].width = vinfo->yres;
		config->info.fb.size[2].height = vinfo->xres;
		break;
	case FB_ROTATION_HW_180:
		fb_rot->info.flag_h = G2D_ROT_180;
		fb_rot->info.dst_image_h.width = vinfo->xres;
		fb_rot->info.dst_image_h.height = vinfo->yres;
		fb_rot->info.dst_image_h.clip_rect.w = fb_rot->info.dst_image_h.width;
		fb_rot->info.dst_image_h.clip_rect.h = fb_rot->info.dst_image_h.height;

		config->info.fb.crop.width = ((long long)vinfo->xres << 32);
		config->info.fb.crop.height = ((long long)vinfo->yres << 32);
		config->info.fb.size[0].width = vinfo->xres;
		config->info.fb.size[0].height = vinfo->yres;
		config->info.fb.size[1].width = vinfo->xres;
		config->info.fb.size[1].height = vinfo->yres;
		config->info.fb.size[2].width = vinfo->xres;
		config->info.fb.size[2].height = vinfo->yres;
		break;
	case FB_ROTATION_HW_270:
		fb_rot->info.flag_h = G2D_ROT_270;
		fb_rot->info.dst_image_h.width = vinfo->yres;
		fb_rot->info.dst_image_h.height = vinfo->xres;
		fb_rot->info.dst_image_h.clip_rect.w = fb_rot->info.dst_image_h.height;
		fb_rot->info.dst_image_h.clip_rect.h = fb_rot->info.dst_image_h.width;
		config->info.fb.crop.width = ((long long)vinfo->yres << 32);
		config->info.fb.crop.height = ((long long)vinfo->xres << 32);
		config->info.fb.size[0].width = vinfo->yres;
		config->info.fb.size[0].height = (pinfo->yres_virtual / vinfo->yres) * vinfo->xres;
		config->info.fb.size[1].width = vinfo->yres;
		config->info.fb.size[1].height = (pinfo->yres_virtual / vinfo->yres) * vinfo->xres;
		config->info.fb.size[2].width = vinfo->yres;
		config->info.fb.size[2].height = (pinfo->yres_virtual / vinfo->yres) * vinfo->xres;
		break;
	default:
		DE_WRN("Not support degree:%d\n", value);
		goto G2D_RELEASE;
		break;
	}

	switch (config->info.fb.format) {
	case DISP_FORMAT_ARGB_8888:
		fb_rot->info.src_image_h.format = G2D_FORMAT_ARGB8888;
		fb_rot->info.dst_image_h.format = G2D_FORMAT_ARGB8888;
		break;
	case DISP_FORMAT_ABGR_8888:
		fb_rot->info.src_image_h.format = G2D_FORMAT_ABGR8888;
		fb_rot->info.dst_image_h.format = G2D_FORMAT_ABGR8888;
		break;
	case DISP_FORMAT_RGBA_8888:
		fb_rot->info.src_image_h.format = G2D_FORMAT_RGBA8888;
		fb_rot->info.dst_image_h.format = G2D_FORMAT_RGBA8888;
		break;
	case DISP_FORMAT_BGRA_8888:
		fb_rot->info.src_image_h.format = G2D_FORMAT_BGRA8888;
		fb_rot->info.dst_image_h.format = G2D_FORMAT_BGRA8888;
		break;
	case DISP_FORMAT_RGB_888:
		fb_rot->info.src_image_h.format = G2D_FORMAT_RGB888;
		fb_rot->info.dst_image_h.format = G2D_FORMAT_RGB888;
		break;
	case DISP_FORMAT_BGR_888:
		fb_rot->info.src_image_h.format = G2D_FORMAT_BGR888;
		fb_rot->info.dst_image_h.format = G2D_FORMAT_BGR888;
		break;
	case DISP_FORMAT_RGB_565:
		fb_rot->info.src_image_h.format = G2D_FORMAT_RGB565;
		fb_rot->info.dst_image_h.format = G2D_FORMAT_RGB565;
		break;
	case DISP_FORMAT_BGR_565:
		fb_rot->info.src_image_h.format = G2D_FORMAT_BGR565;
		fb_rot->info.dst_image_h.format = G2D_FORMAT_BGR565;
		break;
	default:
		DE_WRN("Not support pixel format %d\n", config->info.fb.format);
		goto ERROR;
		break;
	}

	fb_rot->info.src_image_h.laddr[0] = (unsigned long)pinfo->fbmem;
	fb_rot->info.src_image_h.align[0] = 0;
	fb_rot->info.src_image_h.align[1] = 0;
	fb_rot->info.src_image_h.align[2] = 0;
	fb_rot->info.src_image_h.clip_rect.x = 0;
	fb_rot->info.src_image_h.clip_rect.y = 0;
	fb_rot->info.src_image_h.clip_rect.w = fb_rot->info.src_image_h.width;
	fb_rot->info.src_image_h.clip_rect.h = fb_rot->info.src_image_h.height;
	fb_rot->info.src_image_h.alpha = 1;
	fb_rot->info.src_image_h.mode = 255;
	fb_rot->info.src_image_h.use_phy_addr = 1;


	/*double buffer*/
	fb_rot->dst_mem_len = pinfo->stride * vinfo->yres * 2;
	fb_rot->dst_vir_addr = (void *)(R528_DISP_DDR_MAPVADDR  + pinfo->fblen);
	fb_rot->dst_phy_addr = fb_rot->dst_vir_addr;
	if (!fb_rot->dst_vir_addr || !fb_rot->dst_phy_addr)
		goto G2D_RELEASE;
	config->info.fb.addr[0] =
	    (unsigned long long)(unsigned int)fb_rot->dst_phy_addr;

	fb_rot->info.dst_image_h.laddr[0] = (u32)fb_rot->dst_phy_addr;
	fb_rot->info.dst_image_h.align[0] = 0;
	fb_rot->info.dst_image_h.align[1] = 0;
	fb_rot->info.dst_image_h.align[2] = 0;
	fb_rot->info.dst_image_h.clip_rect.x = 0;
	fb_rot->info.dst_image_h.clip_rect.y = 0;
	fb_rot->info.dst_image_h.alpha = 1;
	fb_rot->info.dst_image_h.mode = 255;
	fb_rot->info.dst_image_h.use_phy_addr = 1;

	fb_rot->pinfo = pinfo;
	fb_rot->vinfo = vinfo;
#ifdef CONFIG_FB_OVERLAY
	fb_rot->rot_dst_overlay = NULL;
	fb_rot->dst_overlay_size = 0;
	fb_rot->overlay_index = 0;
	fb_rot->realloc = fb_g2d_realloc;
	fb_rot->apply_overlay = fboverlay_g2d_rot_apply;
#endif
	fb_rot->apply = fb_g2d_rot_apply;
	fb_rot->free = fb_g2d_rot_free;
		return fb_rot;

G2D_RELEASE:
	sunxi_g2d_close();
ERROR:
	hal_free(fb_rot);
	return NULL;
}
