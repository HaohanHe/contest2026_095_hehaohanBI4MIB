#include "asr_engine.h"
#include "wav_encoder.h"
#include "siliconflow_client.h"
#include "radio_config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>

#undef AUDIO_SAMPLE_RATE
#define ASR_TARGET_SAMPLE_RATE 16000
#define AUDIO_SAMPLE_RATE ASR_TARGET_SAMPLE_RATE

#define ASR_AUDIO_BUF_SAMPLES  (AUDIO_SAMPLE_RATE * 30)
#define ASR_WAV_BUF_SIZE       (ASR_AUDIO_BUF_SAMPLES * 2 + 4096)
#define INPUT_SAMPLE_RATE      8000
#define UPSAMPLE_FACTOR        (AUDIO_SAMPLE_RATE / INPUT_SAMPLE_RATE)

static ai_config_t g_config;
static asr_state_t g_state = ASR_STATE_IDLE;
static bool g_running = false;
static bool g_enabled = false;
static asr_callbacks_t g_cbs;

static int16_t g_audio_buf[ASR_AUDIO_BUF_SAMPLES];
static size_t g_audio_count = 0;
static uint8_t g_wav_buf[ASR_WAV_BUF_SIZE];
static char g_last_text[MAX_TRANSCRIPT_LEN];
static int16_t g_last_sample = 0;

static void upsample_and_feed(const int16_t *samples, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        for (int j = 0; j < UPSAMPLE_FACTOR; j++) {
            int16_t s;
            if (j == 0) {
                s = g_last_sample;
            } else {
                float frac = (float)j / (float)UPSAMPLE_FACTOR;
                s = (int16_t)(g_last_sample * (1.0f - frac) + samples[i] * frac);
            }
            if (g_audio_count < ASR_AUDIO_BUF_SAMPLES) {
                g_audio_buf[g_audio_count++] = s;
            }
        }
        g_last_sample = samples[i];
    }
}

static pthread_t g_asr_thread;
static pthread_mutex_t g_mutex;
static volatile bool g_flush_requested = false;
static volatile bool g_thread_should_exit = false;

static void set_state(asr_state_t s)
{
    g_state = s;
    if (g_cbs.on_state_change) {
        g_cbs.on_state_change(s, g_cbs.user_data);
    }
}

static int do_asr_request(void)
{
    if (g_audio_count < AUDIO_SAMPLE_RATE / 2) {
        return 0;
    }

    set_state(ASR_STATE_UPLOADING);

    wav_encoder_t wav;
    wav_encoder_init(&wav, AUDIO_SAMPLE_RATE, AUDIO_CHANNELS, AUDIO_BITS_PER_SAMPLE,
                     g_wav_buf, sizeof(g_wav_buf));
    wav_encoder_write_samples(&wav, g_audio_buf, g_audio_count);
    wav_encoder_finalize(&wav);
    size_t wav_len = wav_encoder_get_total_size(&wav);

    char text[MAX_TRANSCRIPT_LEN];
    const char *model = g_config.asr_model[0] ? g_config.asr_model : DEFAULT_ASR_MODEL;

    int ret = sf_client_transcribe_audio(g_wav_buf, wav_len, model, text, sizeof(text));
    if (ret != 0) {
        set_state(ASR_STATE_ERROR);
        if (g_cbs.on_error) {
            g_cbs.on_error(-1, sf_client_get_last_error(), g_cbs.user_data);
        }
        g_audio_count = 0;
        return -1;
    }

    pthread_mutex_lock(&g_mutex);
    strncpy(g_last_text, text, sizeof(g_last_text) - 1);
    g_last_text[sizeof(g_last_text) - 1] = '\0';
    pthread_mutex_unlock(&g_mutex);

    set_state(ASR_STATE_READY);
    if (g_cbs.on_result) {
        g_cbs.on_result(text, false, g_cbs.user_data);
    }

    g_audio_count = 0;
    return 0;
}

static void *asr_worker_thread(void *arg)
{
    (void)arg;
    while (!g_thread_should_exit) {
        bool should_flush = false;
        size_t count = 0;

        pthread_mutex_lock(&g_mutex);
        should_flush = g_flush_requested;
        count = g_audio_count;
        pthread_mutex_unlock(&g_mutex);

        if (should_flush || count >= ASR_CHUNK_SAMPLES) {
            pthread_mutex_lock(&g_mutex);
            g_flush_requested = false;
            pthread_mutex_unlock(&g_mutex);
            do_asr_request();
        }

        usleep(100 * 1000);
    }
    return NULL;
}

int asr_engine_init(const ai_config_t *config)
{
    if (!config) return -1;
    memcpy(&g_config, config, sizeof(g_config));
    g_enabled = config->asr_enabled && config->api_key[0] != '\0';
    g_state = ASR_STATE_IDLE;
    g_audio_count = 0;
    g_last_text[0] = '\0';
    g_last_sample = 0;
    pthread_mutex_init(&g_mutex, NULL);
    sf_client_init(config);
    return 0;
}

int asr_engine_deinit(void)
{
    asr_engine_stop();
    sf_client_deinit();
    pthread_mutex_destroy(&g_mutex);
    return 0;
}

int asr_engine_start(void)
{
    if (!g_enabled) return -1;
    if (g_running) return 0;
    g_running = true;
    g_thread_should_exit = false;
    pthread_create(&g_asr_thread, NULL, asr_worker_thread, NULL);
    set_state(ASR_STATE_RECORDING);
    return 0;
}

int asr_engine_stop(void)
{
    if (!g_running) return 0;
    g_thread_should_exit = true;
    pthread_join(g_asr_thread, NULL);
    g_running = false;
    set_state(ASR_STATE_IDLE);
    return 0;
}

int asr_engine_feed_audio(const int16_t *samples, size_t count)
{
    if (!g_running || !g_enabled) return -1;
    pthread_mutex_lock(&g_mutex);
    size_t before = g_audio_count;
    upsample_and_feed(samples, count);
    size_t added = g_audio_count - before;
    pthread_mutex_unlock(&g_mutex);
    (void)added;
    return 0;
}

int asr_engine_flush(void)
{
    pthread_mutex_lock(&g_mutex);
    g_flush_requested = true;
    pthread_mutex_unlock(&g_mutex);
    return 0;
}

asr_state_t asr_engine_get_state(void)
{
    return g_state;
}

const char *asr_engine_get_last_text(void)
{
    return g_last_text;
}

void asr_engine_set_callbacks(const asr_callbacks_t *callbacks)
{
    if (callbacks) {
        g_cbs = *callbacks;
    } else {
        memset(&g_cbs, 0, sizeof(g_cbs));
    }
}

bool asr_engine_is_enabled(void)
{
    return g_enabled;
}

int asr_engine_transcribe_file(const char *wav_path, char *out_text, size_t max_len)
{
    FILE *f = fopen(wav_path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = (uint8_t *)malloc(fsize);
    if (!data) { fclose(f); return -1; }
    fread(data, 1, fsize, f);
    fclose(f);

    const char *model = g_config.asr_model[0] ? g_config.asr_model : DEFAULT_ASR_MODEL;
    int ret = sf_client_transcribe_audio(data, fsize, model, out_text, max_len);
    free(data);
    return ret;
}
