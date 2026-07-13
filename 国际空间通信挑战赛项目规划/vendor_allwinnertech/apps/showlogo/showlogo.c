/*
 * Copyright (C) 2024 Xiaomi Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * apps/showlogo/main.c
 * Provide the ability to show a logo during system startup.
 *
 */

#include <nuttx/config.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <nuttx/video/fb.h>
#include <nuttx/video/rgbcolors.h>

/*Image color format*/
#define IMG_CF_TRUE_COLOR 4
#define IMG_CF_TRUE_COLOR_ALPHA 5
#define IMG_CF_TRUE_COLOR_CHROMA_KEYED 6
#define IMG_CF_RGB888 15
#define IMG_CF_RGBA8888 16
#define IMG_CF_RGBX8888 17

struct fb_handle_t {
    int fb_device;
    struct fb_planeinfo_s plane_info;
    struct fb_videoinfo_s video_info;
    void* fb_mem;
};

/* same to lv_image_header_t from LVGL V9 */
struct img_head_s {
    uint32_t magic: 8;          /*Magic number. Must be LV_IMAGE_HEADER_MAGIC*/
    uint32_t cf : 8;            /*Color format: See `lv_color_format_t`*/
    uint32_t flags: 16;         /*Image flags, see `lv_image_flags_t`*/

    uint32_t w: 16;
    uint32_t h: 16;
    uint32_t stride: 16;        /*Number of bytes in a row*/
    uint32_t reserved_2: 16;    /*Reserved to be used later*/
};
typedef struct img_head_s img_head_t;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: show_usage
 ****************************************************************************/

static void show_usage(void)
{
    printf("Usage: -n img_path -f /dev/fb0\n");
    exit(EXIT_FAILURE);
}

static uint8_t* read_all_from_file(const char* path)
{
    int fd = 0;
    int flen = 0;
    uint8_t* content = NULL;

    fd = open(path, O_RDONLY);

    if (fd < 0) {
        fprintf(stderr, "ERROR(showlogo): file :%s open failed! errno is %d\n", path, errno);
        return NULL;
    }

    flen = lseek(fd, 0L, SEEK_END);

    if (flen == -1) {
      fprintf(stderr, "ERROR(showlogo): lseek failed: %d\n", errno);
      close(fd);
      return NULL;
    }
    lseek(fd, 0L, SEEK_SET);

    content = malloc(flen + 1);

    DEBUGASSERT(content != NULL);

    if (content) {
        if (read(fd, content, flen) == -1) {
            fprintf(stderr, "ERROR(showlogo): read failed: %d\n", errno);
            free(content);
            close(fd);
            return NULL;
        }
        content[flen] = 0;
    }
    close(fd);
    return content;
}

int main(int argc, char* argv[])
{
    const char* file_path = NULL;
    const char* fbdev = "/dev/fb0";
    struct fb_handle_t handle;
    uint8_t* img_buffer = NULL;
    img_head_t* img_head = NULL;
    uint32_t* img_data = NULL;
    uint32_t* fb_bpp32 = NULL;
    uint32_t y = 0;
    uint32_t fb_offset = 0;
    uint32_t img_offset = 0;
    fb_coord_t plane_stride_bpp32 = 0;
    int ch;

    while ((ch = getopt(argc, argv, "hn:f::")) != -1) {
        switch (ch) {
        case 'h':
            show_usage();
            break;
        case 'n':
            file_path = optarg;
            break;
        case 'f':
            fbdev = optarg;
            break;
        default:
            break;
        }
    }

    if (file_path == NULL) {
        show_usage();
        return EXIT_FAILURE;
    }

    /* Open the framebuffer device */
    handle.fb_device = open(fbdev, O_RDWR);
    if (handle.fb_device < 0) {
        fprintf(stderr, "ERROR(showlogo): Failed to open %s.\n", fbdev);
        return EXIT_FAILURE;
    }

    /* Get the characteristics of the framebuffer */
    if (ioctl(handle.fb_device, FBIOGET_VIDEOINFO,
            (unsigned long)((uintptr_t)&handle.video_info))
        < 0) {
        fprintf(stderr, "ERROR(showlogo): ioctl(FBIOGET_VIDEOINFO) failed.\n");
        goto fail;
    }

    if (ioctl(handle.fb_device, FBIOGET_PLANEINFO,
            (unsigned long)((uintptr_t)&handle.plane_info))
        < 0) {
        fprintf(stderr, "ERROR(showlogo): ioctl(FBIOGET_PLANEINFO) failed.\n");
        goto fail;
    }

    if (handle.plane_info.bpp != 32) {
        fprintf(stderr, "ERROR(showlogo): bpp=%u only supported 32.\n", handle.plane_info.bpp);
        goto fail;
    }

    handle.fb_mem = mmap(NULL, handle.plane_info.fblen, PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_FILE, handle.fb_device, 0);
    if (handle.fb_mem == MAP_FAILED) {
        fprintf(stderr, "ERROR(showlogo): mmap failed. errno is %d\n", errno);
        goto fail;
    }

    img_buffer = read_all_from_file(file_path);
    img_head = (img_head_t*)img_buffer;
    if (!img_head) {
        fprintf(stderr, "ERROR(showlogo): no image buffer found\n");
        goto fail_munmap;
    }

    if (img_head->cf != IMG_CF_TRUE_COLOR && img_head->cf != IMG_CF_TRUE_COLOR_ALPHA
        && img_head->cf != IMG_CF_TRUE_COLOR_CHROMA_KEYED && img_head->cf != IMG_CF_RGB888
        && img_head->cf != IMG_CF_RGBA8888 && img_head->cf != IMG_CF_RGBX8888) {
        fprintf(stderr, "ERROR(showlogo): color format:%u image only supported 32.\n", img_head->cf);
        goto fail_munmap;
    }

    if (img_head->w > handle.video_info.xres || img_head->h > handle.video_info.yres) {
        fprintf(stderr, "ERROR(showlogo): image size exceeds Horizontal or Vertical resolution\n");
        goto fail_munmap;
    }

    if (img_head->stride == 0 || img_head->stride > handle.plane_info.stride) {
        fprintf(stderr, "ERROR(showlogo): image stride invalid\n");
        goto fail_munmap;
    }

    img_data = (uint32_t*)(img_buffer + sizeof(img_head_t));

    fb_bpp32 = handle.fb_mem;
    plane_stride_bpp32 = handle.plane_info.stride / (handle.plane_info.bpp / 8.);
    fb_offset = (handle.video_info.xres - img_head->w) / 2
        + (handle.video_info.yres - img_head->h) / 2 * plane_stride_bpp32;

    if (handle.video_info.xres > img_head->w || handle.video_info.yres > img_head->h) {
        memset(fb_bpp32, 0, handle.plane_info.fblen);
    }

    for (y = 0; y < img_head->h; y++) {
        memcpy(fb_bpp32 + fb_offset, img_data + img_offset, img_head->stride);
        fb_offset += plane_stride_bpp32;
        img_offset += img_head->w;
    }

#if defined(CONFIG_FB_UPDATE)
    struct fb_area_s fb_area;
    fb_area.x = 0;
    fb_area.y = 0;
    fb_area.w = handle.video_info.xres;
    fb_area.h = handle.video_info.yres;

    if (ioctl(handle.fb_device, FBIO_UPDATE, (unsigned long)((uintptr_t)&fb_area))
        < 0) {
        fprintf(stderr, "ERROR(showlogo): ioctl(FBIO_UPDATE) failed. errno code:%d.\n", errno);
        goto fail_munmap;
    }
#endif

    /* Commit buffer to fb if double buffering is enabled */
    if(handle.plane_info.yres_virtual == (handle.video_info.yres * 2)) {
        if (ioctl(handle.fb_device, FBIOPAN_DISPLAY,
                (unsigned long)((uintptr_t)&handle.plane_info))
            < 0) {
            fprintf(stderr, "ERROR(showlogo): ioctl(FBIOPAN_DISPLAY) failed. errno code:%d.\n", errno);
        }
    }

fail_munmap:
    free(img_buffer);
    munmap(handle.fb_mem, handle.plane_info.fblen);

fail:
    close(handle.fb_device);
    return EXIT_SUCCESS;
}
