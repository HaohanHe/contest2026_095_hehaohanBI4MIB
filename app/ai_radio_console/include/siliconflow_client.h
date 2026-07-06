#ifndef __SILICONFLOW_CLIENT_H
#define __SILICONFLOW_CLIENT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "radio_config.h"

#define SF_HTTP_BUF_SIZE        16384
#define SF_MULTIPART_BOUNDARY   "----ai-radio-boundary-7d3b"

typedef struct {
    char response[MAX_LLM_RESPONSE_LEN];
    size_t response_len;
    int http_status;
    bool completed;
} sf_http_response_t;

int sf_client_init(const ai_config_t *config);
void sf_client_deinit(void);

int sf_client_transcribe_audio(const uint8_t *wav_data, size_t wav_size,
                                const char *model, char *out_text, size_t max_len);

int sf_client_chat_completion(const char *model, const char *system_prompt,
                               const char *user_message, char *out_response,
                               size_t max_len, float *out_confidence);

bool sf_client_check_connection(void);
const char *sf_client_get_last_error(void);

#endif