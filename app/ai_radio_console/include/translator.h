#ifndef __TRANSLATOR_H
#define __TRANSLATOR_H

#include <stdint.h>
#include <stdbool.h>
#include "radio_config.h"

typedef void (*translate_callback_t)(const char *original, const char *translated, translate_lang_t from, translate_lang_t to, void *user_data);

int translator_init(void);
int translator_deinit(void);

int translator_set_languages(translate_lang_t from, translate_lang_t to);
int translator_translate_text(const char *text, char *out, size_t max_out);
int translator_translate_async(const char *text, translate_callback_t cb, void *user_data);

const char *translator_lang_code(translate_lang_t lang);
const char *translator_lang_name(translate_lang_t lang);

#endif
