#include "cw_decoder.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

static cw_config_t g_config;
static cw_char_callback_t g_char_cb = NULL;
static cw_text_callback_t g_text_cb = NULL;
static void *g_cb_data = NULL;

static float g_dot_len_ms = 120.0f;
static uint32_t g_sample_rate = 8000;
static uint32_t g_samples_per_dot = 960;

static float g_bpf_z1 = 0, g_bpf_z2 = 0;
static float g_lpf_z = 0;

static bool g_signal_active = false;
static uint32_t g_signal_start = 0;
static uint32_t g_signal_duration = 0;
static uint32_t g_silence_duration = 0;
static uint32_t g_sample_counter = 0;

static char g_text_buffer[CW_DECODER_MAX_TEXT + 1];
static int g_text_pos = 0;

static char g_symbol_buffer[32];
static int g_symbol_pos = 0;

static const char *MORSE_TABLE[] = {
    ".-",    "-...",  "-.-.",  "-..",   ".",     "..-.",  "--.",   "....",
    "..",    ".---",  "-.-",   ".-..",  "--",    "-.",    "---",   ".--.",
    "--.-",  ".-.",   "...",   "-",     "..-",   "...-",  ".--",   "-..-",
    "-.--",  "--..",
    "-----", ".----", "..---", "...--", "....-", ".....", "-....", "--...",
    "---..", "----.",
    ".-.-.-", "--..--", "..--..", ".----.", "-.-.--", "-..-.", "-.--.", "-...-",
    ".-.-.", "-....-", ".-..-.", ".--.-."
};

static const char MORSE_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,?'!/()&:;=+_\"$@";

static void bpf_init(void) {
    g_bpf_z1 = 0;
    g_bpf_z2 = 0;
}

static float bpf_process(float x) {
    float fc = 750.0f;
    float bw = 500.0f;
    float fs = (float)g_sample_rate;
    float f = fc / fs;
    float q = fc / bw;
    float w0 = 2.0f * 3.14159265f * f;
    float alpha = sinf(w0) / (2.0f * q);
    float a0 = 1.0f + alpha;
    float b0 = alpha;
    float b2 = -alpha;
    float a1 = -2.0f * cosf(w0);
    float a2 = 1.0f - alpha;
    float y = (b0 / a0) * x + (b2 / a0) * g_bpf_z2 - (a1 / a0) * g_bpf_z1 - (a2 / a0) * g_bpf_z2;
    g_bpf_z2 = g_bpf_z1;
    g_bpf_z1 = y;
    return y;
}

static float envelope_detect(float x) {
    float y = fabsf(x);
    g_lpf_z = 0.1f * y + 0.9f * g_lpf_z;
    return g_lpf_z;
}

static void process_symbol(char *symbols) {
    int len = strlen(symbols);
    if (len == 0) return;
    for (int i = 0; MORSE_CHARS[i]; i++) {
        if (strcmp(symbols, MORSE_TABLE[i]) == 0) {
            char c = MORSE_CHARS[i];
            if (g_text_pos < CW_DECODER_MAX_TEXT) {
                g_text_buffer[g_text_pos++] = c;
                g_text_buffer[g_text_pos] = '\0';
            }
            if (g_char_cb) g_char_cb(c, g_cb_data);
            if (g_text_cb) g_text_cb(g_text_buffer, g_cb_data);
            return;
        }
    }
    if (g_text_pos < CW_DECODER_MAX_TEXT) {
        g_text_buffer[g_text_pos++] = '?';
        g_text_buffer[g_text_pos] = '\0';
    }
}

static void add_symbol(char sym) {
    if (g_symbol_pos < 31) {
        g_symbol_buffer[g_symbol_pos++] = sym;
        g_symbol_buffer[g_symbol_pos] = '\0';
    }
}

static void end_character(void) {
    if (g_symbol_pos > 0) {
        process_symbol(g_symbol_buffer);
        g_symbol_pos = 0;
        g_symbol_buffer[0] = '\0';
    }
}

static void end_word(void) {
    end_character();
    if (g_text_pos < CW_DECODER_MAX_TEXT) {
        g_text_buffer[g_text_pos++] = ' ';
        g_text_buffer[g_text_pos] = '\0';
    }
}

int cw_decoder_init(const cw_config_t *config) {
    if (config) {
        memcpy(&g_config, config, sizeof(cw_config_t));
    } else {
        g_config.wpm = 15;
        g_config.dot_threshold_ms = 120.0f;
    }
    g_sample_rate = AUDIO_SAMPLE_RATE;
    g_dot_len_ms = 1200.0f / g_config.wpm;
    g_samples_per_dot = (uint32_t)(g_sample_rate * g_dot_len_ms / 1000.0f);
    bpf_init();
    g_lpf_z = 0;
    g_signal_active = false;
    g_sample_counter = 0;
    g_text_pos = 0;
    g_text_buffer[0] = '\0';
    g_symbol_pos = 0;
    g_symbol_buffer[0] = '\0';
    return 0;
}

int cw_decoder_deinit(void) {
    return 0;
}

int cw_decoder_feed(const int16_t *samples, size_t count) {
    float threshold = 500.0f;
    for (size_t i = 0; i < count; i++) {
        float x = (float)samples[i];
        float filtered = bpf_process(x);
        float env = envelope_detect(filtered);
        g_sample_counter++;
        bool above = env > threshold;
        if (above && !g_signal_active) {
            if (g_silence_duration > g_samples_per_dot * 7) {
                end_word();
            } else if (g_silence_duration > g_samples_per_dot * 3) {
                end_character();
            } else if (g_silence_duration > g_samples_per_dot) {
            }
            g_signal_active = true;
            g_signal_start = g_sample_counter;
            g_silence_duration = 0;
        } else if (!above && g_signal_active) {
            g_signal_active = false;
            g_signal_duration = g_sample_counter - g_signal_start;
            if (g_signal_duration < g_samples_per_dot * 2) {
                add_symbol('.');
            } else {
                add_symbol('-');
            }
        } else if (above && g_signal_active) {
            g_signal_duration = g_sample_counter - g_signal_start;
        } else {
            g_silence_duration++;
        }
    }
    return 0;
}

int cw_decoder_reset(void) {
    g_text_pos = 0;
    g_text_buffer[0] = '\0';
    g_symbol_pos = 0;
    g_symbol_buffer[0] = '\0';
    g_signal_active = false;
    return 0;
}

const char *cw_decoder_get_text(void) {
    return g_text_buffer;
}

int cw_decoder_get_wpm(void) {
    return g_config.wpm;
}

void cw_decoder_set_char_callback(cw_char_callback_t cb, void *user_data) {
    g_char_cb = cb;
    g_cb_data = user_data;
}

void cw_decoder_set_text_callback(cw_text_callback_t cb, void *user_data) {
    g_text_cb = cb;
    g_cb_data = user_data;
}
