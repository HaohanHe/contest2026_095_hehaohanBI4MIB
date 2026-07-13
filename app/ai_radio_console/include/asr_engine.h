#ifndef __ASR_ENGINE_H
#define __ASR_ENGINE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "radio_config.h"

typedef enum {
    ASR_STATE_IDLE = 0,
    ASR_STATE_RECORDING,
    ASR_STATE_UPLOADING,
    ASR_STATE_PROCESSING,
    ASR_STATE_READY,
    ASR_STATE_ERROR
} asr_state_t;

typedef void (*asr_result_callback_t)(const char *text, bool is_partial, void *user_data);
typedef void (*asr_state_callback_t)(asr_state_t state, void *user_data);
typedef void (*asr_error_callback_t)(int error_code, const char *message, void *user_data);

typedef struct {
    asr_result_callback_t on_result;
    asr_state_callback_t on_state_change;
    asr_error_callback_t on_error;
    void *user_data;
} asr_callbacks_t;

int asr_engine_init(const ai_config_t *config);
int asr_engine_deinit(void);
int asr_engine_start(void);
int asr_engine_stop(void);
int asr_engine_feed_audio(const int16_t *samples, size_t count);
int asr_engine_flush(void);
asr_state_t asr_engine_get_state(void);
const char *asr_engine_get_last_text(void);
void asr_engine_set_callbacks(const asr_callbacks_t *callbacks);
bool asr_engine_is_enabled(void);
int asr_engine_transcribe_file(const char *wav_path, char *out_text, size_t max_len);

#endif