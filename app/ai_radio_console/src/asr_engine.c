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
#include <math.h>

#define ASR_TARGET_SAMPLE_RATE  ASR_STREAM_TARGET_RATE
#define ASR_INPUT_SAMPLE_RATE   ASR_STREAM_INPUT_RATE
#define ASR_UPSAMPLE_FACTOR     (ASR_TARGET_SAMPLE_RATE / ASR_INPUT_SAMPLE_RATE)
#define ASR_VAD_FRAME_SAMPLES   (ASR_TARGET_SAMPLE_RATE * ASR_VAD_FRAME_MS / 1000)
#define ASR_PARTIAL_INTERVAL_SAMPLES (ASR_TARGET_SAMPLE_RATE * ASR_PARTIAL_INTERVAL_MS / 1000)
#define ASR_UTTERANCE_MAX_SAMPLES    (ASR_TARGET_SAMPLE_RATE * ASR_UTTERANCE_MAX_S)
#define ASR_WAV_BUF_SIZE             (ASR_UTTERANCE_MAX_SAMPLES * 2 + 4096)

typedef enum {
    VAD_SILENCE = 0,
    VAD_SPEAKING
} vad_state_t;

static ai_config_t g_config;
static asr_state_t g_state = ASR_STATE_IDLE;
static bool g_running = false;
static bool g_enabled = false;
static asr_callbacks_t g_cbs;

static pthread_t g_asr_thread;
static pthread_mutex_t g_mutex;
static volatile bool g_thread_should_exit = false;
static volatile bool g_flush_requested = false;

/* Audio streaming buffer (16 kHz, current utterance) */
static int16_t g_utt_buf[ASR_UTTERANCE_MAX_SAMPLES];
static size_t g_utt_count = 0;

static uint8_t g_wav_buf[ASR_WAV_BUF_SIZE];

/* Upsampling state */
static int16_t g_last_input_sample = 0;

/* VAD state */
static int16_t g_vad_frame[ASR_VAD_FRAME_SAMPLES];
static size_t g_vad_frame_count = 0;
static vad_state_t g_vad_state = VAD_SILENCE;
static int g_vad_speech_frames = 0;
static int g_vad_silent_frames = 0;

/* Streaming timing */
static uint32_t g_stream_elapsed_ms = 0;
static uint32_t g_last_partial_ms = 0;

/* Last recognized text for incremental diff */
static char g_last_text[MAX_TRANSCRIPT_LEN];
static char g_last_partial_text[MAX_TRANSCRIPT_LEN];

static bool g_partial_pending = false;
static bool g_final_pending = false;

static void set_state(asr_state_t s)
{
    g_state = s;
    if (g_cbs.on_state_change) {
        g_cbs.on_state_change(s, g_cbs.user_data);
    }
}

static float compute_frame_db(const int16_t *samples, size_t count)
{
    if (!samples || count == 0) return -100.0f;
    double sum_sq = 0.0;
    for (size_t i = 0; i < count; i++) {
        float s = (float)samples[i] / 32768.0f;
        sum_sq += (double)(s * s);
    }
    float rms = sqrtf((float)(sum_sq / count));
    return 20.0f * log10f(rms + 1e-10f);
}

static void compute_text_delta(const char *prev, const char *curr,
                               char *out, size_t max_len)
{
    if (!prev || !curr || !out || max_len == 0) {
        if (out && max_len > 0) out[0] = '\0';
        return;
    }
    size_t prev_len = strlen(prev);
    size_t curr_len = strlen(curr);
    if (curr_len >= prev_len && strncmp(curr, prev, prev_len) == 0) {
        size_t delta_len = curr_len - prev_len;
        if (delta_len >= max_len) delta_len = max_len - 1;
        memcpy(out, curr + prev_len, delta_len);
        out[delta_len] = '\0';
    } else {
        size_t copy_len = curr_len;
        if (copy_len >= max_len) copy_len = max_len - 1;
        memcpy(out, curr, copy_len);
        out[copy_len] = '\0';
    }
}

static void process_vad_frame(const int16_t *frame)
{
    float db = compute_frame_db(frame, ASR_VAD_FRAME_SAMPLES);
    bool is_speech = db > ASR_VAD_THRESHOLD_DB;

    if (is_speech) {
        g_vad_speech_frames++;
        g_vad_silent_frames = 0;
    } else {
        g_vad_silent_frames++;
        g_vad_speech_frames = 0;
    }

    g_stream_elapsed_ms += ASR_VAD_FRAME_MS;

    if (g_vad_state == VAD_SILENCE) {
        if (g_vad_speech_frames >= ASR_VAD_SPEECH_FRAMES) {
            g_vad_state = VAD_SPEAKING;
            g_last_partial_ms = g_stream_elapsed_ms;
            g_last_partial_text[0] = '\0';
        }
    } else {
        if (g_vad_silent_frames >= ASR_VAD_SILENT_FRAMES) {
            g_vad_state = VAD_SILENCE;
            g_final_pending = true;
        } else if (g_stream_elapsed_ms - g_last_partial_ms >= (uint32_t)ASR_PARTIAL_INTERVAL_MS) {
            g_partial_pending = true;
            g_last_partial_ms = g_stream_elapsed_ms;
        }
    }
}

/* Returns 0 on success, -1 on error. On error out_delta contains the error message. */
static int do_asr_request_locked(bool is_partial, char *out_delta, size_t delta_max_len)
{
    out_delta[0] = '\0';

    if (g_utt_count < ASR_TARGET_SAMPLE_RATE / 2) {
        return 0;
    }

    wav_encoder_t wav;
    wav_encoder_init(&wav, ASR_TARGET_SAMPLE_RATE, AUDIO_CHANNELS, AUDIO_BITS_PER_SAMPLE,
                     g_wav_buf, sizeof(g_wav_buf));
    wav_encoder_write_samples(&wav, g_utt_buf, g_utt_count);
    wav_encoder_finalize(&wav);
    size_t wav_len = wav_encoder_get_total_size(&wav);

    char text[MAX_TRANSCRIPT_LEN];
    const char *model = g_config.asr_model[0] ? g_config.asr_model : DEFAULT_ASR_MODEL;

    /* Release mutex during network call so feed_audio can keep collecting */
    pthread_mutex_unlock(&g_mutex);
    int ret = sf_client_transcribe_audio(g_wav_buf, wav_len, model, text, sizeof(text));
    pthread_mutex_lock(&g_mutex);

    if (ret != 0) {
        strncpy(out_delta, sf_client_get_last_error(), delta_max_len - 1);
        out_delta[delta_max_len - 1] = '\0';
        return -1;
    }

    compute_text_delta(g_last_partial_text, text, out_delta, delta_max_len);

    strncpy(g_last_text, text, sizeof(g_last_text) - 1);
    g_last_text[sizeof(g_last_text) - 1] = '\0';

    if (is_partial) {
        strncpy(g_last_partial_text, text, sizeof(g_last_partial_text) - 1);
        g_last_partial_text[sizeof(g_last_partial_text) - 1] = '\0';
    }

    return 0;
}

static void reset_utterance_locked(void)
{
    g_utt_count = 0;
    g_vad_frame_count = 0;
    g_vad_state = VAD_SILENCE;
    g_vad_speech_frames = 0;
    g_vad_silent_frames = 0;
    g_last_partial_text[0] = '\0';
    g_last_partial_ms = 0;
    g_stream_elapsed_ms = 0;
}

static void *asr_worker_thread(void *arg)
{
    (void)arg;
    while (!g_thread_should_exit) {
        bool do_partial = false;
        bool do_final = false;
        bool do_flush = false;

        pthread_mutex_lock(&g_mutex);
        do_partial = g_partial_pending;
        do_final = g_final_pending;
        do_flush = g_flush_requested;
        g_partial_pending = false;
        g_flush_requested = false;
        pthread_mutex_unlock(&g_mutex);

        if (do_flush) {
            char delta[MAX_TRANSCRIPT_LEN];
            bool had_data = false;
            pthread_mutex_lock(&g_mutex);
            had_data = g_utt_count > 0;
            int ret = had_data ? do_asr_request_locked(false, delta, sizeof(delta)) : 0;
            reset_utterance_locked();
            pthread_mutex_unlock(&g_mutex);

            if (had_data) {
                if (ret == 0) {
                    set_state(ASR_STATE_READY);
                    if (g_cbs.on_result && delta[0] != '\0') {
                        g_cbs.on_result(delta, false, g_cbs.user_data);
                    }
                } else {
                    set_state(ASR_STATE_ERROR);
                    if (g_cbs.on_error) {
                        g_cbs.on_error(-1, delta, g_cbs.user_data);
                    }
                }
            }
        } else if (do_final) {
            char delta[MAX_TRANSCRIPT_LEN];
            pthread_mutex_lock(&g_mutex);
            int ret = do_asr_request_locked(false, delta, sizeof(delta));
            reset_utterance_locked();
            g_final_pending = false;
            pthread_mutex_unlock(&g_mutex);

            if (ret == 0) {
                set_state(ASR_STATE_READY);
                if (g_cbs.on_result && delta[0] != '\0') {
                    g_cbs.on_result(delta, false, g_cbs.user_data);
                }
            } else {
                set_state(ASR_STATE_ERROR);
                if (g_cbs.on_error) {
                    g_cbs.on_error(-1, delta, g_cbs.user_data);
                }
            }
        } else if (do_partial) {
            char delta[MAX_TRANSCRIPT_LEN];
            pthread_mutex_lock(&g_mutex);
            set_state(ASR_STATE_UPLOADING);
            int ret = do_asr_request_locked(true, delta, sizeof(delta));
            pthread_mutex_unlock(&g_mutex);

            if (ret == 0) {
                set_state(ASR_STATE_READY);
                if (g_cbs.on_result && delta[0] != '\0') {
                    g_cbs.on_result(delta, true, g_cbs.user_data);
                }
            } else {
                set_state(ASR_STATE_ERROR);
                if (g_cbs.on_error) {
                    g_cbs.on_error(-1, delta, g_cbs.user_data);
                }
            }
        }

        usleep(50 * 1000);
    }
    return NULL;
}

int asr_engine_init(const ai_config_t *config)
{
    if (!config) return -1;
    memcpy(&g_config, config, sizeof(g_config));
    g_enabled = config->asr_enabled && config->api_key[0] != '\0';
    g_state = ASR_STATE_IDLE;
    g_running = false;
    g_thread_should_exit = false;
    g_flush_requested = false;
    g_partial_pending = false;
    g_final_pending = false;
    g_utt_count = 0;
    g_vad_frame_count = 0;
    g_vad_state = VAD_SILENCE;
    g_last_input_sample = 0;
    g_last_text[0] = '\0';
    g_last_partial_text[0] = '\0';
    g_stream_elapsed_ms = 0;
    g_last_partial_ms = 0;
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

    pthread_mutex_lock(&g_mutex);
    reset_utterance_locked();
    g_thread_should_exit = false;
    pthread_mutex_unlock(&g_mutex);

    g_running = true;
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

    pthread_mutex_lock(&g_mutex);
    reset_utterance_locked();
    pthread_mutex_unlock(&g_mutex);

    set_state(ASR_STATE_IDLE);
    return 0;
}

int asr_engine_feed_audio(const int16_t *samples, size_t count)
{
    if (!g_running || !g_enabled || !samples || count == 0) return -1;

    pthread_mutex_lock(&g_mutex);

    /* Safety cap: if utterance too long, force final and reset */
    if (g_utt_count >= ASR_UTTERANCE_MAX_SAMPLES - (count * ASR_UPSAMPLE_FACTOR) - ASR_VAD_FRAME_SAMPLES) {
        if (g_utt_count > ASR_TARGET_SAMPLE_RATE / 2) {
            g_final_pending = true;
        }
        reset_utterance_locked();
    }

    for (size_t i = 0; i < count; i++) {
        int16_t in = samples[i];
        for (int j = 0; j < ASR_UPSAMPLE_FACTOR; j++) {
            float frac = (float)j / (float)ASR_UPSAMPLE_FACTOR;
            int16_t s = (int16_t)(g_last_input_sample * (1.0f - frac) + in * frac);

            if (g_utt_count < ASR_UTTERANCE_MAX_SAMPLES) {
                g_utt_buf[g_utt_count++] = s;
            }

            g_vad_frame[g_vad_frame_count++] = s;
            if (g_vad_frame_count >= ASR_VAD_FRAME_SAMPLES) {
                process_vad_frame(g_vad_frame);
                g_vad_frame_count = 0;
            }
        }
        g_last_input_sample = in;
    }

    pthread_mutex_unlock(&g_mutex);
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
