#include "translator.h"
#include "agent_bridge.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static bool g_initialized = false;
static translate_lang_t g_from = TRANSLATE_ZH;
static translate_lang_t g_to = TRANSLATE_EN;

static translate_callback_t g_pending_cb = NULL;
static void *g_pending_cb_data = NULL;

static const char *LANG_CODES[] = {"zh", "en", "ja", "ru"};
static const char *LANG_NAMES[] = {"Chinese", "English", "Japanese", "Russian"};

int translator_init(void) {
    g_initialized = true;
    return 0;
}

int translator_deinit(void) {
    g_initialized = false;
    return 0;
}

int translator_set_languages(translate_lang_t from, translate_lang_t to) {
    if (from >= TRANSLATE_MAX || to >= TRANSLATE_MAX) return -1;
    g_from = from;
    g_to = to;
    return 0;
}

int translator_translate_text(const char *text, char *out, size_t max_out) {
    if (!text || !out || max_out == 0) return -1;
    if (!g_initialized) {
        strncpy(out, text, max_out - 1);
        out[max_out - 1] = '\0';
        return 0;
    }
    agent_bridge_request_translate(text, g_from, g_to);
    strncpy(out, text, max_out - 1);
    out[max_out - 1] = '\0';
    return 0;
}

int translator_translate_async(const char *text, translate_callback_t cb, void *user_data) {
    if (!text) return -1;
    g_pending_cb = cb;
    g_pending_cb_data = user_data;
    return agent_bridge_request_translate(text, g_from, g_to);
}

const char *translator_lang_code(translate_lang_t lang) {
    if (lang >= TRANSLATE_MAX) return "en";
    return LANG_CODES[lang];
}

const char *translator_lang_name(translate_lang_t lang) {
    if (lang >= TRANSLATE_MAX) return "English";
    return LANG_NAMES[lang];
}
