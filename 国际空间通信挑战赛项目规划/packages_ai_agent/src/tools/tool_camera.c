/*
 * Copyright (C) 2026 Xiaomi Corporation
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
 */

/**
 * tool_camera.c — Camera capture tool for AI Agent
 *
 * Captures a photo from the device camera via NuttX V4L2 interface,
 * then sends the JPEG data to a Vision LLM for analysis.
 *
 * Designed for BES1700/BES2800 AON camera which outputs hardware-
 * compressed JPEG via V4L2_PIX_FMT_ENTROPY.
 */

#include "tools/tool_camera.h"
#include "llm/llm_proxy.h"
#include "agent_compat.h"
#include "agent_config.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <poll.h>
#include <unistd.h>

#include "cJSON.h"

#ifdef CONFIG_AI_AGENT_CAMERA
#include <nuttx/video/video.h>

static const char *TAG = "camera";

/* ── Camera device and resolution defaults ───────────────── */

#ifndef AGENT_CAMERA_DEV
#define AGENT_CAMERA_DEV "/dev/video0"
#endif

#define CAM_WIDTH_HIGH   1280
#define CAM_HEIGHT_HIGH  720
#define CAM_WIDTH_LOW    320
#define CAM_HEIGHT_LOW   180

/* Buffer size for compressed JPEG from hardware encoder */
#define CAM_BUF_SIZE     (160 * 1024)
#define CAM_NUM_BUFFERS  2
#define CAM_DQBUF_TIMEOUT_MS  5000  /* 5s timeout for frame capture */

/* BES hardware JPEG pixel format (V4L2_PIX_FMT_ENTROPY).
 * Verify this value against your BSP's aoncam_v4l2.h if capture fails. */
#ifndef V4L2_PIX_FMT_ENTROPY
#define V4L2_PIX_FMT_ENTROPY  v4l2_fourcc('G', 'R', 'E', 'P')
#endif

#define CAM_DEFAULT_PROMPT \
    "Describe what you see in this image in detail. " \
    "If there is text, read it. If there are objects, identify them."

/* ── V4L2 capture helper ─────────────────────────────────── */

/**
 * Capture one JPEG frame from the camera via V4L2.
 *
 * @param width    Desired width (320 or 1280)
 * @param height   Desired height (180 or 720)
 * @param out_data Pointer to receive malloc'd JPEG data (caller frees)
 * @param out_size Receives the JPEG data size in bytes
 * @return OK on success, ERROR on failure
 */
static int camera_v4l2_capture(int width, int height,
                               uint8_t **out_data, size_t *out_size)
{
    int fd = -1;
    int ret = ERROR;
    void *buffers[CAM_NUM_BUFFERS] = { NULL };
    size_t buf_sizes[CAM_NUM_BUFFERS] = { 0 };
    uint32_t nbuffers = 0;

    *out_data = NULL;
    *out_size = 0;

    /* Open camera device */
    fd = open(AGENT_CAMERA_DEV, O_RDWR);
    if (fd < 0) {
        syslog(LOG_ERR, "[%s] Cannot open %s: %d\n",
               TAG, AGENT_CAMERA_DEV, errno);
        return ERROR;
    }

    /* Query capabilities */
    struct v4l2_capability cap;
    memset(&cap, 0, sizeof(cap));
    if (ioctl(fd, VIDIOC_QUERYCAP, (uintptr_t)&cap) < 0) {
        syslog(LOG_ERR, "[%s] VIDIOC_QUERYCAP failed: %d\n", TAG, errno);
        goto cleanup;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        syslog(LOG_ERR, "[%s] Device does not support video capture\n", TAG);
        goto cleanup;
    }

    /* Set format: hardware JPEG encoder */
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = width;
    fmt.fmt.pix.height      = height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;
    fmt.fmt.pix.sizeimage   = CAM_BUF_SIZE;
    fmt.fmt.pix.field       = V4L2_FIELD_NONE;

    if (ioctl(fd, VIDIOC_S_FMT, (uintptr_t)&fmt) < 0) {
        /* Fallback: try V4L2_PIX_FMT_ENTROPY (BES hardware JPEG) */
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_ENTROPY;
        if (ioctl(fd, VIDIOC_S_FMT, (uintptr_t)&fmt) < 0) {
            syslog(LOG_ERR, "[%s] VIDIOC_S_FMT failed: %d\n", TAG, errno);
            goto cleanup;
        }
    }

    syslog(LOG_INFO, "[%s] Format set: %dx%d\n", TAG, width, height);

    /* Request buffers */
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count  = CAM_NUM_BUFFERS;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_REQBUFS, (uintptr_t)&req) < 0) {
        syslog(LOG_ERR, "[%s] VIDIOC_REQBUFS failed: %d\n", TAG, errno);
        goto cleanup;
    }

    nbuffers = req.count;

    /* Query and mmap buffers */
    for (uint32_t i = 0; i < nbuffers; i++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;

        if (ioctl(fd, VIDIOC_QUERYBUF, (uintptr_t)&buf) < 0) {
            syslog(LOG_ERR, "[%s] VIDIOC_QUERYBUF %u failed\n", TAG, i);
            goto cleanup;
        }

        buf_sizes[i] = buf.length;
        buffers[i] = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                          MAP_SHARED, fd, buf.m.offset);
        if (buffers[i] == MAP_FAILED) {
            buffers[i] = NULL;
            syslog(LOG_ERR, "[%s] mmap buffer %u failed\n", TAG, i);
            goto cleanup;
        }
    }

    /* Enqueue all buffers */
    for (uint32_t i = 0; i < nbuffers; i++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;

        if (ioctl(fd, VIDIOC_QBUF, (uintptr_t)&buf) < 0) {
            syslog(LOG_ERR, "[%s] VIDIOC_QBUF %u failed\n", TAG, i);
            goto cleanup;
        }
    }

    /* Start streaming */
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, (uintptr_t)&type) < 0) {
        syslog(LOG_ERR, "[%s] VIDIOC_STREAMON failed: %d\n", TAG, errno);
        goto cleanup;
    }

    /* Dequeue one frame with timeout to avoid blocking the agent loop */
    struct pollfd pfd = { .fd = fd, .events = POLLIN };
    int poll_ret = poll(&pfd, 1, CAM_DQBUF_TIMEOUT_MS);
    if (poll_ret <= 0) {
        syslog(LOG_ERR, "[%s] Frame capture timeout (%d ms)\n",
               TAG, CAM_DQBUF_TIMEOUT_MS);
        ioctl(fd, VIDIOC_STREAMOFF, (uintptr_t)&type);
        goto cleanup;
    }

    struct v4l2_buffer dqbuf;
    memset(&dqbuf, 0, sizeof(dqbuf));
    dqbuf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    dqbuf.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_DQBUF, (uintptr_t)&dqbuf) < 0) {
        syslog(LOG_ERR, "[%s] VIDIOC_DQBUF failed: %d\n", TAG, errno);
        ioctl(fd, VIDIOC_STREAMOFF, (uintptr_t)&type);
        goto cleanup;
    }

    syslog(LOG_INFO, "[%s] Captured frame: %u bytes from buffer %u\n",
           TAG, dqbuf.bytesused, dqbuf.index);

    /* Copy JPEG data out (mmap buffer will be released) */
    if (dqbuf.bytesused > 0 && dqbuf.index < nbuffers) {
        *out_data = malloc(dqbuf.bytesused);
        if (*out_data) {
            memcpy(*out_data, buffers[dqbuf.index], dqbuf.bytesused);
            *out_size = dqbuf.bytesused;
            ret = OK;
        } else {
            syslog(LOG_ERR, "[%s] OOM copying frame (%u bytes)\n",
                   TAG, dqbuf.bytesused);
        }
    }

    /* Stop streaming */
    ioctl(fd, VIDIOC_STREAMOFF, (uintptr_t)&type);

cleanup:
    /* Unmap buffers */
    for (uint32_t i = 0; i < nbuffers; i++) {
        if (buffers[i] && buffers[i] != MAP_FAILED) {
            munmap(buffers[i], buf_sizes[i]);
        }
    }

    /* Release buffers */
    if (fd >= 0) {
        struct v4l2_requestbuffers rel;
        memset(&rel, 0, sizeof(rel));
        rel.count  = 0;
        rel.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        rel.memory = V4L2_MEMORY_MMAP;
        ioctl(fd, VIDIOC_REQBUFS, (uintptr_t)&rel);
        close(fd);
    }

    return ret;
}

/* ── Tool execute entry point ────────────────────────────── */

int tool_camera_capture_execute(const char *input_json,
                                char *output, size_t output_size)
{
    const char *prompt = CAM_DEFAULT_PROMPT;
    char *prompt_copy = NULL;
    int width = CAM_WIDTH_LOW;
    int height = CAM_HEIGHT_LOW;

    /* Parse optional parameters */
    cJSON *root = cJSON_Parse(input_json);
    if (root) {
        cJSON *p = cJSON_GetObjectItem(root, "prompt");
        if (p && cJSON_IsString(p) && p->valuestring[0]) {
            prompt_copy = strdup(p->valuestring);
            if (prompt_copy) {
                prompt = prompt_copy;
            }
        }

        cJSON *r = cJSON_GetObjectItem(root, "resolution");
        if (r && cJSON_IsString(r)) {
            if (strcmp(r->valuestring, "high") == 0) {
                width = CAM_WIDTH_HIGH;
                height = CAM_HEIGHT_HIGH;
            }
        }
    }
    cJSON_Delete(root);

    syslog(LOG_INFO, "[%s] Capturing %dx%d\n", TAG, width, height);

    /* Capture JPEG from camera */
    uint8_t *jpeg_data = NULL;
    size_t jpeg_size = 0;

    int ret = camera_v4l2_capture(width, height, &jpeg_data, &jpeg_size);
    if (ret != OK || !jpeg_data || jpeg_size == 0) {
        snprintf(output, output_size,
                 "{\"error\":\"Camera capture failed. "
                 "Is %s available?\"}", AGENT_CAMERA_DEV);
        free(prompt_copy);
        return ERROR;
    }

    syslog(LOG_INFO, "[%s] Captured %zu bytes JPEG, sending to Vision LLM\n",
           TAG, jpeg_size);

    /* Detect MIME type from magic bytes */
    const char *mime = "image/jpeg";
    if (jpeg_size >= 4 && jpeg_data[0] == 0x89 && jpeg_data[1] == 'P') {
        mime = "image/png";
    }

    /* Send to Vision LLM */
    char *resp_buf = calloc(1, output_size);
    if (!resp_buf) {
        free(jpeg_data);
        snprintf(output, output_size, "{\"error\":\"OOM for LLM response\"}");
        free(prompt_copy);
        return ERROR;
    }

    ret = llm_chat_vision_raw(prompt, jpeg_data, jpeg_size, mime,
                              resp_buf, output_size);
    free(jpeg_data);
    free(prompt_copy);

    if (ret != OK) {
        snprintf(output, output_size, "{\"error\":\"%s\"}", resp_buf);
        free(resp_buf);
        return ERROR;
    }

    /* Build JSON result */
    cJSON *result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "analysis", resp_buf);
    free(resp_buf);

    char *json_str = cJSON_PrintUnformatted(result);
    cJSON_Delete(result);

    if (json_str) {
        strncpy(output, json_str, output_size - 1);
        output[output_size - 1] = '\0';
        free(json_str);
    }

    return OK;
}

#else /* !CONFIG_AI_AGENT_CAMERA */

int tool_camera_capture_execute(const char *input_json,
                                char *output, size_t output_size)
{
    (void)input_json;
    snprintf(output, output_size,
             "{\"error\":\"Camera not supported (CONFIG_AI_AGENT_CAMERA disabled)\"}");
    return ERROR;
}

#endif /* CONFIG_AI_AGENT_CAMERA */
