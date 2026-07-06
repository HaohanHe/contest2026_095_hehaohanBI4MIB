#include "audio_capture.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <math.h>

static bool g_initialized = false;
static bool g_recording = false;
static pthread_t g_capture_thread;
static audio_frame_callback_t g_callback = NULL;
static void *g_cb_data = NULL;
static int g_audio_fd = -1;

static int16_t g_frame_buffer[AUDIO_FRAME_SIZE / sizeof(int16_t)];
static float g_current_db = -60.0f;

static void *capture_thread_func(void *arg) {
    (void)arg;
    while (g_recording) {
        ssize_t bytes_read = read(g_audio_fd, g_frame_buffer, AUDIO_FRAME_SIZE);
        if (bytes_read <= 0) {
            usleep(10000);
            continue;
        }
        size_t frames = bytes_read / sizeof(int16_t);
        double sum_sq = 0;
        for (size_t i = 0; i < frames; i++) {
            float s = (float)g_frame_buffer[i] / 32768.0f;
            sum_sq += s * s;
        }
        float rms = sqrtf(sum_sq / frames);
        g_current_db = 20.0f * log10f(rms + 1e-10f);
        if (g_callback) {
            g_callback(g_frame_buffer, frames, g_cb_data);
        }
    }
    return NULL;
}

int audio_capture_init(void) {
    g_audio_fd = open("/dev/pcmC0D0c", O_RDONLY | O_NONBLOCK);
    if (g_audio_fd < 0) {
        g_audio_fd = open("/dev/audio_in", O_RDONLY | O_NONBLOCK);
    }
    if (g_audio_fd < 0) {
        fprintf(stderr, "audio_capture: cannot open audio device, using simulated input\n");
    }
    g_initialized = true;
    return 0;
}

int audio_capture_start(audio_frame_callback_t cb, void *user_data) {
    if (!g_initialized) return -1;
    if (g_recording) return 0;
    g_callback = cb;
    g_cb_data = user_data;
    g_recording = true;
    if (g_audio_fd >= 0) {
        pthread_create(&g_capture_thread, NULL, capture_thread_func, NULL);
    }
    return 0;
}

int audio_capture_stop(void) {
    g_recording = false;
    if (g_audio_fd >= 0) {
        pthread_join(g_capture_thread, NULL);
    }
    return 0;
}

int audio_capture_deinit(void) {
    audio_capture_stop();
    if (g_audio_fd >= 0) {
        close(g_audio_fd);
        g_audio_fd = -1;
    }
    g_initialized = false;
    return 0;
}

int audio_capture_get_level_db(float *db_out) {
    if (!db_out) return -1;
    *db_out = g_current_db;
    return 0;
}

bool audio_capture_is_active(void) {
    return g_recording;
}

int audio_capture_save_wav(const char *path, const int16_t *data, size_t samples) {
    if (!path || !data || samples == 0) return -1;
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    uint32_t data_size = samples * sizeof(int16_t);
    uint32_t file_size = 36 + data_size;
    uint16_t channels = AUDIO_CHANNELS;
    uint32_t sample_rate = AUDIO_SAMPLE_RATE;
    uint16_t bits_per_sample = AUDIO_BITS_PER_SAMPLE;
    uint16_t block_align = channels * bits_per_sample / 8;
    uint32_t byte_rate = sample_rate * block_align;
    fwrite("RIFF", 1, 4, f);
    fwrite(&file_size, 4, 1, f);
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);
    uint32_t fmt_size = 16;
    uint16_t audio_format = 1;
    fwrite(&fmt_size, 4, 1, f);
    fwrite(&audio_format, 2, 1, f);
    fwrite(&channels, 2, 1, f);
    fwrite(&sample_rate, 4, 1, f);
    fwrite(&byte_rate, 4, 1, f);
    fwrite(&block_align, 2, 1, f);
    fwrite(&bits_per_sample, 2, 1, f);
    fwrite("data", 1, 4, f);
    fwrite(&data_size, 4, 1, f);
    fwrite(data, data_size, 1, f);
    fclose(f);
    return 0;
}
