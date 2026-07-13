#ifndef __CW_DECODER_H
#define __CW_DECODER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define CW_DECODER_MAX_TEXT 256

typedef void (*cw_char_callback_t)(char c, void *user_data);
typedef void (*cw_text_callback_t)(const char *text, void *user_data);

typedef struct {
    int wpm;
    float dot_threshold_ms;
    float dash_threshold_ms;
    float element_gap_ms;
    float char_gap_ms;
    float word_gap_ms;
} cw_config_t;

int cw_decoder_init(const cw_config_t *config);
int cw_decoder_deinit(void);
int cw_decoder_feed(const int16_t *samples, size_t count);
int cw_decoder_reset(void);

const char *cw_decoder_get_text(void);
int cw_decoder_get_wpm(void);
void cw_decoder_set_char_callback(cw_char_callback_t cb, void *user_data);
void cw_decoder_set_text_callback(cw_text_callback_t cb, void *user_data);

#endif
