#ifndef __SIGNAL_ANALYZER_H
#define __SIGNAL_ANALYZER_H

#include <stdint.h>
#include <stdbool.h>
#include "radio_config.h"

typedef enum {
    INTERFERENCE_NONE = 0,
    INTERFERENCE_COCHANNEL,
    INTERFERENCE_ADJCHANNEL,
    INTERFERENCE_BLOCKING,
    INTERFERENCE_CARRIER,
    INTERFERENCE_NOISE
} interference_type_t;

typedef struct {
    bool present;
    interference_type_t type;
    float frequency_offset_hz;
    float strength_db;
    float duty_cycle;
    uint32_t duration_ms;
} interference_info_t;

typedef struct {
    bool malicious;
    uint32_t occupied_ms;
    float avg_power_db;
    char signature[64];
} malicious_detection_t;

typedef void (*interference_callback_t)(const interference_info_t *info, void *user_data);
typedef void (*malicious_callback_t)(const malicious_detection_t *info, void *user_data);

int signal_analyzer_init(uint32_t sample_rate);
int signal_analyzer_deinit(void);
int signal_analyzer_feed(const int16_t *samples, size_t count);
int signal_analyzer_reset(void);

float signal_analyzer_get_signal_db(void);
float signal_analyzer_get_noise_floor_db(void);
float signal_analyzer_get_snr_db(void);
int signal_analyzer_get_band_power(float low_hz, float high_hz, float *power_db);

const interference_info_t *signal_analyzer_get_interference(void);
const malicious_detection_t *signal_analyzer_check_malicious(void);

void signal_analyzer_set_interference_cb(interference_callback_t cb, void *user_data);
void signal_analyzer_set_malicious_cb(malicious_callback_t cb, void *user_data);

#endif
