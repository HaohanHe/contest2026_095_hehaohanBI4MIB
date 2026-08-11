/****************************************************************************
 * auto_recorder.c - Automatic voice recorder with VAD
 *
 * Background thread that continuously records audio and segments it
 * based on VAD (Voice Activity Detection).
 ****************************************************************************/

#include "auto_recorder.h"
#include "vad_detector.h"
#include "wav_encoder.h"
#include "note_store.h"
#include "siliconflow_client.h"
#include "daily_summary.h"
#include "config_store.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>

#define AUTO_RECORDER_STACK    (32 * 1024)
#define MAX_SEGMENT_SAMPLES    (16000 * 300)
#define MAX_BUFFER_SAMPLES     (MAX_SEGMENT_SAMPLES + 16000)
#define SEGMENT_DIR            "/data/daynote/audio"
#define MAX_PENDING            4

typedef enum {
    REC_MODE_IDLE,
    REC_MODE_RECORDING,
    REC_MODE_PROCESSING
} rec_mode_t;

struct auto_recorder_s {
    pthread_t        thread;
    pthread_mutex_t  mutex;
    int              running;
    int              audio_fd;
    int              sample_rate;
    int16_t         *buffer;
    int              buffer_len;
    int              buffer_max;
    vad_detector_t  *vad;
    rec_mode_t       mode;
    int              pending_count;
    char             pending_paths[MAX_PENDING][256];
    int              segment_count;
};

typedef struct {
    const char *wav_path;
    int16_t    *samples;
    int         n_samples;
    int         duration_sec;
} proc_thread_args_t;

static auto_recorder_t g_recorder;
static int g_recorder_initialized = 0;

static void ensure_segment_dir(void)
{
    struct stat st;
    if (stat(SEGMENT_DIR, &st) != 0) {
        mkdir(SEGMENT_DIR, 0755);
    }
}

static int save_wav(const char *path, const int16_t *samples, int n)
{
    FILE *f = fopen(path, "wb");
    if (!f) return -1;

    int data_size = n * sizeof(int16_t);
    int file_size = 36 + data_size;

    fwrite("RIFF", 1, 4, f);
    fwrite(&file_size, 4, 1, f);
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);
    short fmt_size = 16;
    fwrite(&fmt_size, 4, 1, f);
    short fmt = 1;
    fwrite(&fmt, 2, 1, f);
    short channels = 1;
    fwrite(&channels, 2, 1, f);
    int sample_rate = 16000;
    fwrite(&sample_rate, 4, 1, f);
    int byte_rate = sample_rate * channels * sizeof(int16_t);
    fwrite(&byte_rate, 4, 1, f);
    short block_align = channels * sizeof(int16_t);
    fwrite(&block_align, 2, 1, f);
    short bits = 16;
    fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f);
    fwrite(&data_size, 4, 1, f);
    fwrite(samples, 1, data_size, f);
    fclose(f);
    return 0;
}

static void *process_segment_thread(void *arg)
{
    proc_thread_args_t *args = (proc_thread_args_t *)arg;
    if (!args) return NULL;

    const char *wav_path = args->wav_path;
    const int16_t *samples = args->samples;
    int n = args->n_samples;
    int duration_sec = args->duration_sec;

    printf("[AutoRec] Processing segment: %s (%d sec)\n", wav_path, duration_sec);

    /* ASR */
    char transcript[2048] = {0};
    int ret = sf_client_transcribe_audio((const uint8_t *)samples, n * sizeof(int16_t),
                                          "FunAudioLLM/SenseVoiceSmall",
                                          transcript, sizeof(transcript));

    if (ret != 0 || transcript[0] == '\0') {
        strcpy(transcript, "(ASR failed - no speech detected)");
    }

    /* LLM summary */
    char response[2048] = {0};
    char prompt[4096];
    snprintf(prompt, sizeof(prompt),
             "你是一个个人助理。请将以下语音笔记转写为简洁的中文摘要（1-2句话）：\n\n%s",
             transcript);

    int ai_ret = sf_client_chat_completion(
        "Qwen/Qwen2.5-7B-Instruct",
        "你是个人助理，帮助总结语音笔记。",
        prompt,
        response, sizeof(response), NULL);

    const char *summary = response;
    if (ai_ret != 0 || response[0] == '\0') {
        summary = transcript;
    }

    /* Store note */
    note_store_add(wav_path, transcript, summary, duration_sec);

    /* Update memory index */
    memory_index_add(note_store_count() - 1, time(NULL), summary, false, 0.5f);

    /* Refresh daily digest */
    daily_digest_refresh();

    free(args->samples);
    free(args->wav_path);
    free(args);

    printf("[AutoRec] Segment processed\n");
    return NULL;
}

static void process_segment(const int16_t *samples, int n)
{
    ensure_segment_dir();

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char wav_path[512];
    strftime(wav_path, sizeof(wav_path), SEGMENT_DIR "/note_%Y%m%d_%H%M%S.wav", tm_info);

    if (save_wav(wav_path, samples, n) != 0) {
        printf("[AutoRec] Failed to save WAV\n");
        return;
    }

    printf("[AutoRec] Segment saved: %s (%d samples, %d sec)\n",
           wav_path, n, n / 16000);

    /* Trigger ASR + LLM in background */
    pthread_t proc_thread;
    proc_thread_args_t *args = malloc(sizeof(*args));
    if (!args) return;
    args->wav_path = strdup(wav_path);
    args->samples = malloc(n * sizeof(int16_t));
    if (!args->samples) { free(args->wav_path); free(args); return; }
    memcpy(args->samples, samples, n * sizeof(int16_t));
    args->n_samples = n;
    args->duration_sec = n / 16000;

    if (pthread_create(&proc_thread, NULL, process_segment_thread, args) != 0) {
        free(args->wav_path);
        free(args->samples);
        free(args);
    } else {
        pthread_detach(proc_thread);
    }
}

static void *auto_recorder_thread(void *arg)
{
    (void)arg;
    int16_t frame[320];
    int frame_samples = vad_get_frame_samples(g_recorder.vad);

    printf("[AutoRec] Thread started, frame=%d samples\n", frame_samples);

    while (g_recorder.running) {
        /* Read audio */
        int n = read(g_recorder.audio_fd, frame, frame_samples * sizeof(int16_t));
        if (n <= 0) {
            usleep(10000);
            continue;
        }
        n = n / sizeof(int16_t);

        /* VAD processing */
        vad_state_t state = vad_process(g_recorder.vad, frame, n);

        pthread_mutex_lock(&g_recorder.mutex);
        if (state == VAD_STATE_SPEECH && g_recorder.mode == REC_MODE_IDLE) {
            g_recorder.mode = REC_MODE_RECORDING;
            g_recorder.buffer_len = 0;
            printf("[AutoRec] Speech detected, recording...\n");
        }

        if (g_recorder.mode == REC_MODE_RECORDING) {
            if (g_recorder.buffer_len + n > g_recorder.buffer_max) {
                int new_max = g_recorder.buffer_max * 2;
                if (new_max > MAX_BUFFER_SAMPLES) new_max = MAX_BUFFER_SAMPLES;
                int16_t *new_buf = realloc(g_recorder.buffer, new_max * sizeof(int16_t));
                if (new_buf) {
                    g_recorder.buffer = new_buf;
                    g_recorder.buffer_max = new_max;
                }
            }
            if (g_recorder.buffer_len + n <= g_recorder.buffer_max) {
                memcpy(g_recorder.buffer + g_recorder.buffer_len, frame, n * sizeof(int16_t));
                g_recorder.buffer_len += n;
            }

            /* Check max duration */
            if (g_recorder.buffer_len >= MAX_SEGMENT_SAMPLES) {
                printf("[AutoRec] Max duration reached, segmenting...\n");
                process_segment(g_recorder.buffer, g_recorder.buffer_len);
                g_recorder.buffer_len = 0;
                g_recorder.mode = REC_MODE_IDLE;
                vad_reset(g_recorder.vad);
            }
        }

        if (state == VAD_STATE_END && g_recorder.mode == REC_MODE_RECORDING) {
            printf("[AutoRec] Speech ended, segmenting...\n");
            if (g_recorder.buffer_len > 16000) {
                process_segment(g_recorder.buffer, g_recorder.buffer_len);
            }
            g_recorder.buffer_len = 0;
            g_recorder.mode = REC_MODE_IDLE;
            vad_reset(g_recorder.vad);
        }
        pthread_mutex_unlock(&g_recorder.mutex);
    }

    return NULL;
}

int auto_recorder_init(int audio_fd, int sample_rate)
{
    if (g_recorder_initialized) return 0;

    memset(&g_recorder, 0, sizeof(g_recorder));
    pthread_mutex_init(&g_recorder.mutex, NULL);
    g_recorder.audio_fd = audio_fd;
    g_recorder.sample_rate = sample_rate;
    g_recorder.buffer_max = 16000 * 10;
    g_recorder.buffer = malloc(g_recorder.buffer_max * sizeof(int16_t));
    if (!g_recorder.buffer) return -1;

    g_recorder.vad = vad_create(sample_rate);
    if (!g_recorder.vad) {
        free(g_recorder.buffer);
        return -1;
    }

    g_recorder.running = 1;
    if (pthread_create(&g_recorder.thread, NULL, auto_recorder_thread, NULL) != 0) {
        vad_destroy(g_recorder.vad);
        free(g_recorder.buffer);
        return -1;
    }

    g_recorder_initialized = 1;
    printf("[AutoRec] Initialized, sample_rate=%d\n", sample_rate);
    return 0;
}

void auto_recorder_deinit(void)
{
    g_recorder.running = 0;
    if (g_recorder.thread) {
        pthread_join(g_recorder.thread, NULL);
    }
    if (g_recorder.vad) {
        vad_destroy(g_recorder.vad);
    }
    if (g_recorder.buffer) {
        free(g_recorder.buffer);
    }
    pthread_mutex_destroy(&g_recorder.mutex);
    g_recorder_initialized = 0;
}

int auto_recorder_get_segment_count(void)
{
    pthread_mutex_lock(&g_recorder.mutex);
    int count = g_recorder.segment_count;
    pthread_mutex_unlock(&g_recorder.mutex);
    return count;
}
