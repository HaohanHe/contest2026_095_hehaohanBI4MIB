#include "audio_i2s.h"
#include "asr_engine.h"
#include "radio_config.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <nuttx/audio/audio.h>

static int g_i2s_fd = -1;
static pthread_t g_i2s_thread;
static volatile int g_i2s_running = 0;
static volatile int g_i2s_initialized = 0;

#define I2S_READ_SAMPLES    320  /* 20 ms @ 16 kHz mono */

static void *audio_i2s_thread(void *arg)
{
    (void)arg;
    int16_t buf[I2S_READ_SAMPLES * AUDIO_CHANNELS];

    while (g_i2s_running) {
        ssize_t bytes = read(g_i2s_fd, buf, sizeof(buf));
        if (bytes < 0) {
            if (errno == EAGAIN || errno == EINTR) {
                usleep(1000);
                continue;
            }
            printf("[I2S] read error %d\n", errno);
            usleep(10000);
            continue;
        }
        if (bytes == 0) {
            usleep(1000);
            continue;
        }

        int samples = (int)(bytes / sizeof(int16_t));

#if AUDIO_CHANNELS == 2
        /* Drop right channel, keep left */
        int frames = samples / 2;
        for (int i = 0; i < frames; i++) {
            buf[i] = buf[i * 2];
        }
        samples = frames;
#endif

        asr_engine_feed_audio(buf, (size_t)samples);
    }

    return NULL;
}

int audio_i2s_init(void)
{
    if (g_i2s_initialized) {
        return 0;
    }

    g_i2s_fd = open(AUDIO_CAPTURE_DEV, O_RDONLY | O_NONBLOCK);
    if (g_i2s_fd < 0) {
        printf("[I2S] cannot open %s: %d\n", AUDIO_CAPTURE_DEV, errno);
        return -1;
    }

    if (ioctl(g_i2s_fd, AUDIOIOC_RESERVE, 0) < 0) {
        printf("[I2S] reserve failed: %d\n", errno);
        close(g_i2s_fd);
        g_i2s_fd = -1;
        return -1;
    }

    struct audio_caps_desc_s cap_desc;
    memset(&cap_desc, 0, sizeof(cap_desc));
    cap_desc.caps.ac_len = sizeof(struct audio_caps_s);
    cap_desc.caps.ac_type = AUDIO_TYPE_INPUT;
    cap_desc.caps.ac_channels = AUDIO_CHANNELS;
    cap_desc.caps.ac_chmap = 0;
    cap_desc.caps.ac_controls.hw[0] = AUDIO_SAMPLE_RATE;
    cap_desc.caps.ac_controls.b[3] = (AUDIO_SAMPLE_RATE >> 16) & 0xff;
    cap_desc.caps.ac_controls.b[2] = AUDIO_BITS_PER_SAMPLE;
    cap_desc.caps.ac_subtype = AUDIO_FMT_PCM;

    if (ioctl(g_i2s_fd, AUDIOIOC_CONFIGURE, (unsigned long)&cap_desc) < 0) {
        printf("[I2S] configure failed: %d\n", errno);
        ioctl(g_i2s_fd, AUDIOIOC_RELEASE, 0);
        close(g_i2s_fd);
        g_i2s_fd = -1;
        return -1;
    }

    g_i2s_initialized = 1;
    return 0;
}

int audio_i2s_start(void)
{
    if (!g_i2s_initialized || g_i2s_running) {
        return 0;
    }

    if (ioctl(g_i2s_fd, AUDIOIOC_START, 0) < 0) {
        printf("[I2S] start failed: %d\n", errno);
        if (ioctl(g_i2s_fd, AUDIOIOC_STOP, 0) < 0) {
            printf("[I2S] rollback stop failed: %d\n", errno);
        }
        if (ioctl(g_i2s_fd, AUDIOIOC_RELEASE, 0) < 0) {
            printf("[I2S] rollback release failed: %d\n", errno);
        }
        return -1;
    }

    g_i2s_running = 1;

    int ret = pthread_create(&g_i2s_thread, NULL, audio_i2s_thread, NULL);
    if (ret != 0) {
        printf("[I2S] pthread_create failed: %d\n", ret);
        g_i2s_running = 0;
        if (ioctl(g_i2s_fd, AUDIOIOC_STOP, 0) < 0) {
            printf("[I2S] stop failed: %d\n", errno);
        }
        if (ioctl(g_i2s_fd, AUDIOIOC_RELEASE, 0) < 0) {
            printf("[I2S] release failed: %d\n", errno);
        }
        return -1;
    }

    printf("[I2S] capture started, rate=%d, channels=%d, bits=%d\n",
           AUDIO_SAMPLE_RATE, AUDIO_CHANNELS, AUDIO_BITS_PER_SAMPLE);
    return 0;
}

int audio_i2s_stop(void)
{
    if (!g_i2s_running) {
        return 0;
    }

    g_i2s_running = 0;

    if (pthread_join(g_i2s_thread, NULL) != 0) {
        printf("[I2S] pthread_join failed: %d\n", errno);
    }

    if (g_i2s_fd >= 0) {
        if (ioctl(g_i2s_fd, AUDIOIOC_STOP, 0) < 0) {
            printf("[I2S] stop failed: %d\n", errno);
        }
        if (ioctl(g_i2s_fd, AUDIOIOC_RELEASE, 0) < 0) {
            printf("[I2S] release failed: %d\n", errno);
        }
        close(g_i2s_fd);
        g_i2s_fd = -1;
    }

    g_i2s_initialized = 0;
    return 0;
}
