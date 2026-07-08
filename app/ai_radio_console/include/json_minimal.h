#ifndef __JSON_MINIMAL_H
#define __JSON_MINIMAL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

int json_extract_string(const char *json, const char *key, char *out, size_t max_len);
int json_extract_float(const char *json, const char *key, float *out);
int json_extract_int(const char *json, const char *key, int *out);
int json_extract_bool(const char *json, const char *key, bool *out);
const char *json_find_array_start(const char *json, const char *key);
int json_array_count(const char *arr_start);
int json_build_chat_completion(char *buf, size_t max_len, const char *api_key,
                                const char *model, const char *system_prompt,
                                const char *user_message, bool stream);
int json_build_asr_multipart(char *buf, size_t *buf_len, const char *boundary,
                              const char *api_key, const char *model,
                              const uint8_t *wav_data, size_t wav_size);

#endif