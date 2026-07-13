#include "signal_analyzer.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#define FFT_SIZE 512
#define FFT_BINS (FFT_SIZE / 2)

static uint32_t g_sample_rate = 16000;
static interference_callback_t g_interf_cb = NULL;
static malicious_callback_t g_malicious_cb = NULL;
static void *g_cb_data = NULL;

static float g_window[FFT_SIZE];
static float g_fft_real[FFT_SIZE];
static float g_fft_imag[FFT_SIZE];
static int16_t g_sample_buffer[FFT_SIZE];
static size_t g_buffer_pos = 0;

static float g_signal_db = -100.0f;
static float g_noise_floor_db = -100.0f;
static interference_info_t g_current_interf;
static malicious_detection_t g_malicious;

static uint32_t g_occupancy_start = 0;
static bool g_band_occupied = false;
static uint64_t g_total_samples = 0;

static void init_window(void) {
    for (int i = 0; i < FFT_SIZE; i++) {
        g_window[i] = 0.5f - 0.5f * cosf(2.0f * 3.14159265f * i / (FFT_SIZE - 1));
    }
}

static void fft_compute(float *real, float *imag, int n) {
    int i, j, k, l;
    float tr, ti, ur, ui, wr, wi;
    for (i = 1, j = 0; i < n; i++) {
        for (k = n >> 1; k > (j ^= k); k >>= 1);
        if (i < j) {
            tr = real[j]; real[j] = real[i]; real[i] = tr;
            ti = imag[j]; imag[j] = imag[i]; imag[i] = ti;
        }
    }
    for (l = 1; l < n; l <<= 1) {
        for (i = 0; i < n; i += (l << 1)) {
            for (k = 0; k < l; k++) {
                float angle = -3.14159265f * k / l;
                wr = cosf(angle);
                wi = sinf(angle);
                ur = real[i + k];
                ui = imag[i + k];
                tr = real[i + k + l] * wr - imag[i + k + l] * wi;
                ti = real[i + k + l] * wi + imag[i + k + l] * wr;
                real[i + k] = ur + tr;
                imag[i + k] = ui + ti;
                real[i + k + l] = ur - tr;
                imag[i + k + l] = ui - ti;
            }
        }
    }
}

static void analyze_spectrum(void) {
    for (int i = 0; i < FFT_SIZE; i++) {
        g_fft_real[i] = (float)g_sample_buffer[i] * g_window[i];
        g_fft_imag[i] = 0.0f;
    }
    fft_compute(g_fft_real, g_fft_imag, FFT_SIZE);
    float max_power = -1000.0f;
    int max_bin = 0;
    float bin_width = (float)g_sample_rate / FFT_SIZE;
    for (int i = 1; i < FFT_BINS; i++) {
        float power = g_fft_real[i] * g_fft_real[i] + g_fft_imag[i] * g_fft_imag[i];
        float db = 10.0f * log10f(power + 1e-10f);
        if (i < 10) {
            if (db > g_noise_floor_db) g_noise_floor_db = g_noise_floor_db * 0.99f + db * 0.01f;
        }
        if (db > max_power) {
            max_power = db;
            max_bin = i;
        }
    }
    g_signal_db = g_signal_db * 0.8f + max_power * 0.2f;
    float center_freq = max_bin * bin_width;
    g_current_interf.present = false;
    if (g_signal_db > g_noise_floor_db + 6.0f) {
        float offset = center_freq - (CW_BPF_LOW_FREQ + CW_BPF_HIGH_FREQ) / 2;
        float bw_ratio = 0;
        int active_bins = 0;
        for (int i = 1; i < FFT_BINS; i++) {
            float p = g_fft_real[i] * g_fft_real[i] + g_fft_imag[i] * g_fft_imag[i];
            float db = 10.0f * log10f(p + 1e-10f);
            if (db > g_noise_floor_db + 10.0f) active_bins++;
        }
        bw_ratio = (float)active_bins / FFT_BINS;
        if (bw_ratio > 0.6f) {
            g_current_interf.present = true;
            g_current_interf.type = INTERFERENCE_BLOCKING;
            g_current_interf.strength_db = g_signal_db;
        } else if (fabsf(offset) < bin_width * 2) {
            g_current_interf.present = true;
            g_current_interf.type = INTERFERENCE_COCHANNEL;
            g_current_interf.frequency_offset_hz = offset;
            g_current_interf.strength_db = g_signal_db;
        } else if (fabsf(offset) < bin_width * 10) {
            g_current_interf.present = true;
            g_current_interf.type = INTERFERENCE_ADJCHANNEL;
            g_current_interf.frequency_offset_hz = offset;
            g_current_interf.strength_db = g_signal_db;
        } else if (active_bins <= 3) {
            g_current_interf.present = true;
            g_current_interf.type = INTERFERENCE_CARRIER;
            g_current_interf.frequency_offset_hz = offset;
            g_current_interf.strength_db = g_signal_db;
        }
        if (g_current_interf.present) {
            g_current_interf.duration_ms += FFT_SIZE * 1000 / g_sample_rate;
            if (g_interf_cb && g_current_interf.duration_ms % 500 == 0) {
                g_interf_cb(&g_current_interf, g_cb_data);
            }
        } else {
            g_current_interf.duration_ms = 0;
        }
        if (!g_band_occupied) {
            g_band_occupied = true;
            g_occupancy_start = g_total_samples / g_sample_rate * 1000;
        }
    } else {
        if (g_band_occupied) {
            g_band_occupied = false;
            uint32_t occupied = (uint32_t)(g_total_samples / g_sample_rate * 1000 - g_occupancy_start);
            if (occupied > MALICIOUS_MIN_SECONDS * 1000) {
                g_malicious.malicious = true;
                g_malicious.occupied_ms = occupied;
                g_malicious.avg_power_db = g_signal_db;
                snprintf(g_malicious.signature, sizeof(g_malicious.signature), "FREQ_%d", (int)(center_freq));
                if (g_malicious_cb) {
                    g_malicious_cb(&g_malicious, g_cb_data);
                }
            }
        }
        g_current_interf.duration_ms = 0;
    }
}

int signal_analyzer_init(uint32_t sample_rate) {
    g_sample_rate = sample_rate;
    init_window();
    g_buffer_pos = 0;
    g_signal_db = -100.0f;
    g_noise_floor_db = -80.0f;
    memset(&g_current_interf, 0, sizeof(g_current_interf));
    memset(&g_malicious, 0, sizeof(g_malicious));
    g_band_occupied = false;
    g_total_samples = 0;
    return 0;
}

int signal_analyzer_deinit(void) {
    return 0;
}

int signal_analyzer_feed(const int16_t *samples, size_t count) {
    for (size_t i = 0; i < count; i++) {
        g_sample_buffer[g_buffer_pos++] = samples[i];
        g_total_samples++;
        if (g_buffer_pos >= FFT_SIZE) {
            analyze_spectrum();
            g_buffer_pos = 0;
        }
    }
    return 0;
}

int signal_analyzer_reset(void) {
    g_buffer_pos = 0;
    memset(&g_current_interf, 0, sizeof(g_current_interf));
    return 0;
}

float signal_analyzer_get_signal_db(void) { return g_signal_db; }
float signal_analyzer_get_noise_floor_db(void) { return g_noise_floor_db; }
float signal_analyzer_get_snr_db(void) { return g_signal_db - g_noise_floor_db; }

int signal_analyzer_get_band_power(float low_hz, float high_hz, float *power_db) {
    if (!power_db) return -1;
    int low_bin = (int)(low_hz * FFT_SIZE / g_sample_rate);
    int high_bin = (int)(high_hz * FFT_SIZE / g_sample_rate);
    if (low_bin < 1) low_bin = 1;
    if (high_bin >= FFT_BINS) high_bin = FFT_BINS - 1;
    float total = 0;
    for (int i = low_bin; i <= high_bin; i++) {
        total += g_fft_real[i] * g_fft_real[i] + g_fft_imag[i] * g_fft_imag[i];
    }
    *power_db = 10.0f * log10f(total / (high_bin - low_bin + 1) + 1e-10f);
    return 0;
}

const interference_info_t *signal_analyzer_get_interference(void) {
    return &g_current_interf;
}

const malicious_detection_t *signal_analyzer_check_malicious(void) {
    if (g_malicious.malicious) {
        g_malicious.malicious = false;
        return &g_malicious;
    }
    return NULL;
}

void signal_analyzer_set_interference_cb(interference_callback_t cb, void *user_data) {
    g_interf_cb = cb;
    g_cb_data = user_data;
}

void signal_analyzer_set_malicious_cb(malicious_callback_t cb, void *user_data) {
    g_malicious_cb = cb;
    g_cb_data = user_data;
}
