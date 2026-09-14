/*
* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
*
* Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
* the the people's Republic of China and other countries.
* All Allwinner Technology Co.,Ltd. trademarks are used with permission.
*
* DISCLAIMER
* THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
* IF YOU NEED TO INTEGRATE THIRD PARTY'S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
* IN ALLWINNERS'SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
* ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
* ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
* COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
* YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY'S TECHNOLOGY.
*
*
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <hal_mem.h>
#include <hal_cache.h>
#include "dev_disp.h"
#include <nuttx/video/fb.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include "../source/g2d_rcq/g2d_driver.h"
#define OVERLAY_NUM 2
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 800
extern int sunxi_g2d_control(int cmd, void *arg);
extern int sunxi_g2d_close(void);
extern int sunxi_g2d_open(void);

static void printf_from_to(unsigned long from, unsigned long to)
{
	unsigned int num = (to - from)/16;
	while(num--){
		printf("0x%08lx: ",from);
		printf("0x%08x 0x%08x 0x%08x 0x%08x\n",
			(*((volatile unsigned int  *)(from))),
			(*((volatile unsigned int  *)(from+4))),
			(*((volatile unsigned int  *)(from+8))),
			(*((volatile unsigned int  *)(from+12)))
		);
		from+=16;
	}
}
static void print_reg(void)
{
	printf("=========dump register================\n");
	printf("DE top register\n\n");
	printf_from_to(0x05000000,0x050000f0);
	printf("DE global register\n\n");
	printf_from_to(0x05100000,0x05100020);
	printf("DE blender register\n");
	printf_from_to(0x05101000,0x05101100);

	printf("DE VI overlay (channel 0) register\n");
	printf_from_to(0x05102000,0x051020ff);
	printf("DE UI overlay (channel 1) register\n");
	printf_from_to(0x05103000,0x051030ff);

	printf("DE VI scaler register\n");
	printf_from_to(0x05120000,0x05121000);

	printf("DE UI scaler (channel 1) register\n");
	printf_from_to(0x05140000,0x05141000);

	printf("tcon lcd0\n");
	printf_from_to(0x05461000,0x05461240);
	printf("disp if top\n");
	printf_from_to(0x05460000,0x05460100);
	printf("csc\n");
	printf_from_to(0x51b0000,0x51b00ff);
}

#ifdef CONFIG_FB_OVERLAY
static int overlay_blank(int fb, FAR struct fb_overlayinfo_s *oinfo)
{
  int ret;

  printf("Overlay: %d, set blank: %08x\n", oinfo->overlay, oinfo->blank);

  ret = ioctl(fb, FBIOSET_BLANK, (unsigned long)(uintptr_t)oinfo);
  if (ret != OK)
    {
      fprintf(stderr, "Unable to blank overlay\n");
    }

  return ret;
}
#endif

int main(int argc, const char **argv)
{
	int i = 0, err = 0;
	struct disp_manager *mgr = NULL;
	u32 num_screens;
	char tmp[10] = {0};

	num_screens = bsp_disp_feat_get_num_screens();


	if (argc == 1) {
		disp_sys_show();
	} else {

		while(i < argc) {
			/*colorbar*/
			if ( ! strcmp(argv[i], "-c")) {
				if (argc > i+2) {
					i+=1;
					disp_colorbar_store(atoi(argv[i]), atoi(argv[i + 1]));
					i+=1;
				} else {
					DE_WRN("-c para error!\n");
					err++;
				}
			}
			if ( ! strcmp(argv[i], "-r")) {
				print_reg();
			}
			/*switch display*/
			if ( ! strcmp(argv[i], "-s")) {
				if (argc > i+3) {
					i+=1;
					bsp_disp_device_switch(atoi(argv[i]), atoi(argv[i + 1]), atoi(argv[i + 2]));
					i+=2;
				} else {
					DE_WRN("-s para error!\n");
					err++;
				}
			}
			/*dump de data*/
			if ( ! strcmp(argv[i], "-d")) {
				if (argc > i+2) {
					i+=1;
					disp_capture_dump(atoi(argv[i]), argv[i + 1]);
					i+=1;
				} else {
					DE_WRN("-d para error!\n");
					err++;
				}
			}

			/*enhance */
			if ( ! strcmp(argv[i], "-e")) {
				if (argc > i+2) {
					i+=1;

					switch(argv[i][0]) {
					case 'm'://mode
						disp_enhance_mode_store(atoi(argv[i + 1]), atoi(argv[i + 2]));
						break;
					case 's'://saturation
						disp_enhance_saturation_store(atoi(argv[i + 1]), atoi(argv[i + 2]));
						break;
					case 'b'://bright
						disp_enhance_bright_store(atoi(argv[i + 1]), atoi(argv[i + 2]));
						break;
					case 'c'://contrast
						disp_enhance_contrast_store(atoi(argv[i + 1]), atoi(argv[i + 2]));
						break;
					case 'g'://gamma color_temperature
						printf("gamma %s %s %d %d\n",argv[i + 1],argv[i + 2],atoi(argv[i + 1]), atoi(argv[i + 2]));
						disp_color_temperature_store(atoi(argv[i + 1]), atoi(argv[i + 2]));
						break;
					case 'n'://denoise
						disp_enhance_denoise_store(atoi(argv[i + 1]), atoi(argv[i + 2]));
						break;
					case 'd'://detail
						disp_enhance_detail_store(atoi(argv[i + 1]), atoi(argv[i + 2]));
						break;
					case 'p'://print

						if (atoi(argv[i + 1]) < 0 || atoi(argv[i + 1]) > 1) {
							i-=1;
							DE_WRN("para error!\n");
							break;
						}
						DISP_PRINT("screen %d:\n", atoi(argv[i + 1]));
						disp_enhance_mode_show(atoi(argv[i + 1]), tmp);
						DISP_PRINT("mode %s\n", tmp);
						disp_enhance_saturation_show(atoi(argv[i + 1]), tmp);
						DISP_PRINT("saturation %s\n", tmp);
						disp_enhance_bright_show(atoi(argv[i + 1]), tmp);
						DISP_PRINT("bright %s\n", tmp);
						disp_enhance_contrast_show(atoi(argv[i + 1]), tmp);
						DISP_PRINT("contrast %s\n", tmp);
						disp_color_temperature_show(atoi(argv[i + 1]), tmp);
						DISP_PRINT("color_temperature %s\n", tmp);
						disp_enhance_denoise_show(atoi(argv[i + 1]), tmp);
						DISP_PRINT("denoise %s\n", tmp);
						disp_enhance_detail_show(atoi(argv[i + 1]), tmp);
						DISP_PRINT("detail %s\n", tmp);
						i-=1;
						break;
					default:
						DE_WRN("para error!\n");
						break;
					}
					i+=2;
				} else {
					DE_WRN("para error!\n");
					err++;
				}
			}

			/*backlight*/
			if ( ! strcmp(argv[i], "-b")) {
				if (argc > i+2) {
					i+=1;
					if (atoi(argv[i]) < num_screens) {
						DE_WRN("set backligt:lcd%d %d\n", atoi(argv[i]), atoi(argv[i + 1]));
						mgr = g_disp_drv.mgr[atoi(argv[i])];
						mgr->device->set_bright(mgr->device, atoi(argv[i + 1]));
					}
					i+=1;
				} else {
					DE_WRN("-b para error!\n");
					err++;
				}
			}
#ifdef CONFIG_FB_OVERLAY
			if ( ! strcmp(argv[i], "-blank")) {
				struct fb_overlayinfo_s oinfo;
				oinfo.overlay  = atoi(argv[2]);
				oinfo.blank    = strtoul(argv[3], NULL, 10);
				int fb = open("/dev/fb0", O_RDWR);
				if (fb >= 0)
				{
					overlay_blank(fb, &oinfo);
					close(fb);
				}
			}
			if ( ! strcmp(argv[i], "-transp")) {
				struct fb_overlayinfo_s oinfo;
				oinfo.overlay            = atoi(argv[2]);
				oinfo.transp.transp      = strtoul(argv[3], NULL, 10);
				oinfo.transp.transp_mode = strtoul(argv[4], NULL, 10);
				if (oinfo.transp.transp_mode != FB_CONST_ALPHA &&
					oinfo.transp.transp_mode != FB_PIXEL_ALPHA)
				{
					fprintf(stderr, "Invalid transparency mode: %d\n",
						oinfo.transp.transp_mode);
				}
				else
				{
					int fb = open("/dev/fb0", O_RDWR);
					if (fb >= 0)
					{
						ioctl(fb, FBIOSET_TRANSP, (unsigned long)(uintptr_t)&oinfo);
						close(fb);
					}
				}
			}
			if ( ! strcmp(argv[i], "-area")) {
				struct fb_overlayinfo_s oinfo;
				oinfo.overlay = atoi(argv[2]);
				oinfo.sarea.x = strtoul(argv[3], NULL, 10);
				oinfo.sarea.y = strtoul(argv[4], NULL, 10);
				oinfo.sarea.w = strtoul(argv[5], NULL, 10);
				oinfo.sarea.h = strtoul(argv[6], NULL, 10);
				int fb = open("/dev/fb0", O_RDWR);
				if (fb >= 0)
				{
					ioctl(fb, FBIOSET_AREA, (unsigned long)(uintptr_t)&oinfo);
					close(fb);
				}
			}
			if ( ! strcmp(argv[i], "-darea")) {
				struct fb_overlayinfo_s oinfo;
				oinfo.overlay = atoi(argv[2]);
				oinfo.darea.x = strtoul(argv[3], NULL, 10);
				oinfo.darea.y = strtoul(argv[4], NULL, 10);
				oinfo.darea.w = strtoul(argv[5], NULL, 10);
				oinfo.darea.h = strtoul(argv[6], NULL, 10);
				int fb = open("/dev/fb0", O_RDWR);
				if (fb >= 0)
				{
					ioctl(fb, FBIOSET_DESTAREA, (unsigned long)(uintptr_t)&oinfo);
					close(fb);
				}
			}

			if ( ! strcmp(argv[i], "-load")) {
				struct fb_overlayinfo_s oinfo;
				int overlayno = atoi(argv[2]);
				int yoffset = atoi(argv[3]);
				const char *resource = argv[4];
				int width = atoi(argv[5]);
				int height = atoi(argv[6]);
				int fb = open("/dev/fb0", O_RDWR);
				if (fb >= 0)
				{
					ioctl(fb, FBIO_SELECT_OVERLAY, 0);
					memset(&oinfo, 0, sizeof(oinfo));	
					ioctl(fb, FBIOGET_OVERLAYINFO, (unsigned long)(uintptr_t)&oinfo);
					void *fbmem = mmap(NULL, oinfo.fblen, PROT_READ | PROT_WRITE,
						 MAP_SHARED | MAP_FILE, fb, 0);
					int fd = open(resource, O_RDWR);
					unsigned char *dst = (unsigned char *)fbmem + yoffset * oinfo.stride  * 3 / 2;
					unsigned char *src = malloc(width * height * 3 / 2);
					unsigned char *src_y = src;
					unsigned char *src_uv = src_y + (width * height);
					read(fd, src_y, width * height *3 /2);
					unsigned char *temp = dst;
					for (i = 0; i < height; i++) {
						memcpy(temp, src_y, width);
						src_y += width;
						temp += oinfo.stride;
					}
					temp = dst + oinfo.stride * oinfo.yres_virtual / OVERLAY_NUM;
					for (i = 0; i < height / 2; i++) {
						memcpy(temp, src_uv, width);
						src_uv += width;
						temp += oinfo.stride;
					}
					hal_dcache_clean_invalidate((unsigned long)oinfo.fbmem, oinfo.fblen);
					oinfo.sarea.x = 0;
					oinfo.sarea.y = 0;
					oinfo.sarea.w = width;
					oinfo.sarea.h = height;

					int x_ratio = (width << 16) / SCREEN_WIDTH;
					int y_ratio = (height << 16) / SCREEN_HEIGHT;

					if (x_ratio > y_ratio) {
						oinfo.darea.w = SCREEN_HEIGHT;
						oinfo.darea.h = SCREEN_HEIGHT *  height / width;
						oinfo.darea.y = (SCREEN_WIDTH - oinfo.darea.h) / 2;
						oinfo.darea.x = 0;
					} else {
						oinfo.darea.h = SCREEN_WIDTH;
						oinfo.darea.w = SCREEN_WIDTH * width / height;
						oinfo.darea.x = (SCREEN_HEIGHT - oinfo.darea.w) / 2;
						oinfo.darea.y = 0;
					}
					ioctl(fb, FBIOSET_AREA, (unsigned long)(uintptr_t)&oinfo);
					ioctl(fb, FBIOSET_DESTAREA, (unsigned long)(uintptr_t)&oinfo);

					oinfo.overlay = overlayno;
					oinfo.xoffset = 0;
					oinfo.yoffset = yoffset;
					if (fb >= 0)
					{
						ioctl(fb, FBIOPAN_OVERLAY, (unsigned long)(uintptr_t)&oinfo);
					}
					free(src);
					close(fd);
					close(fb);
					return 0;
						
					/*sunxi_g2d_open();
					g2d_blt_h blit;
					memset(&blit, 0, sizeof(g2d_blt_h));

					blit.flag_h = G2D_ROT_270;
					blit.src_image_h.format = G2D_FORMAT_YUV420UVC_V1U1V0U0;
					blit.src_image_h.mode = G2D_GLOBAL_ALPHA;
					blit.src_image_h.alpha = 0xff;
					blit.src_image_h.clip_rect.x = 0;
					blit.src_image_h.clip_rect.y = 0;
					blit.src_image_h.clip_rect.w = width / 2;
					blit.src_image_h.clip_rect.h = height / 2;
					blit.src_image_h.width = width;
					blit.src_image_h.height = height;
					blit.src_image_h.laddr[0] = (__u32)src_y;
					blit.src_image_h.laddr[1] = (__u32)src_uv;
					blit.src_image_h.laddr[2] = blit.src_image_h.laddr[1];

					blit.dst_image_h.format = G2D_FORMAT_YUV420UVC_V1U1V0U0;
					blit.dst_image_h.mode = G2D_GLOBAL_ALPHA;
					blit.dst_image_h.alpha = 0xff;
					blit.dst_image_h.width = 1920;
					blit.dst_image_h.height = 1920;
					blit.dst_image_h.laddr[0] = (__u32)dst;
					blit.dst_image_h.laddr[1] = (__u32)(dst + oinfo.stride * oinfo.yres_virtual / OVERLAY_NUM);
					blit.dst_image_h.laddr[2] = blit.dst_image_h.laddr[1];
					syslog(LOG_INFO,"dst=%p\n", dst);
					hal_dcache_clean_invalidate((unsigned long)oinfo.fbmem, oinfo.fblen);
					hal_dcache_clean_invalidate((unsigned long)src_y, width * height * 3 /2);

					if(sunxi_g2d_control(G2D_CMD_BITBLT_H ,&blit) < 0)
					{
						printf("fail\n");
					}*/
					/*oinfo.sarea.x = 0;
					oinfo.sarea.y = 0;
					oinfo.sarea.w = height;
					oinfo.sarea.h = width;

					int x_ratio = (height << 16) / SCREEN_WIDTH;
					int y_ratio = (width << 16) / SCREEN_HEIGHT;

					if (x_ratio > y_ratio) {
						oinfo.darea.w = SCREEN_WIDTH;
						oinfo.darea.h = SCREEN_WIDTH * width / height;
						oinfo.darea.y = (SCREEN_HEIGHT -oinfo.darea.h) / 2;
						oinfo.darea.x = 0;
					} else {
						oinfo.darea.h = SCREEN_HEIGHT;
						oinfo.darea.w = SCREEN_HEIGHT * height / width;
						oinfo.darea.x = (SCREEN_WIDTH -oinfo.darea.w) / 2;
						oinfo.darea.y = 0;
					}
					ioctl(fb, FBIOSET_AREA, (unsigned long)(uintptr_t)&oinfo);
					ioctl(fb, FBIOSET_DESTAREA, (unsigned long)(uintptr_t)&oinfo);
					close(fd);
					close(fb);*/
				}
			}
			if ( ! strcmp(argv[i], "-pan")) {
				struct fb_overlayinfo_s oinfo;
				oinfo.overlay = atoi(argv[2]);
				oinfo.xoffset = atoi(argv[3]);
				oinfo.yoffset = atoi(argv[4]);
				int fb = open("/dev/fb0", O_RDWR);
				if (fb >= 0)
				{
					ioctl(fb, FBIOPAN_OVERLAY, (unsigned long)(uintptr_t)&oinfo);
					close(fb);
				}
			}
#endif
			++i;
		}
	}
	return 0;
}


