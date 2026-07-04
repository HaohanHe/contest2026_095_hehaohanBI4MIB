#ifndef __AUDIO_CAPTURE_H
#define __AUDIO_CAPTURE_H

#include <stdint.h>
#include <stdbool.h>
#include "radio_config.h"

typedef void (*audio_frame_callback_t)(const int16_t *samples, size_t frame_count, void *user_data);

int audio_capture_init(void);
int audio_capture_start(audio_frame_callback_t cb, void *user_data);
int audio_capture_stop(void);
int audio_capture_deinit(void);

int audio_capture_get_level_db(float *db_out);
bool audio_capture_is_active(void);

int audio_capture_save_wav(const char *path, const int16_t *data, size_t samples);

#endif
