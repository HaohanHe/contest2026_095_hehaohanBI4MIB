#ifndef __FREQ_RECOMMENDER_H
#define __FREQ_RECOMMENDER_H

#include <stdint.h>
#include <stdbool.h>
#include "radio_config.h"

typedef struct {
    float frequency_khz;
    float snr_db;
    float activity_score;
    float propagation_score;
    char reason[128];
} freq_recommendation_t;

int freq_recommender_init(void);
int freq_recommender_deinit(void);

int freq_recommender_update_location(float lat, float lon, const char *maidenhead);
int freq_recommender_update_band(radio_band_t band);

int freq_recommender_get_recommendations(freq_recommendation_t *recs, size_t max_count, size_t *out_count);
float freq_recommender_get_best_frequency(radio_band_t band);

int freq_recommender_fetch_propagation(void);
int freq_recommender_scan_band(radio_band_t band, float *best_freq, float *best_snr);

#endif
