#include "freq_recommender.h"
#include "agent_bridge.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

static bool g_initialized = false;
static float g_lat = 0.0f, g_lon = 0.0f;
static char g_maidenhead[16] = "OO00";
static radio_band_t g_current_band = BAND_HF_20M;

static freq_recommendation_t g_recommendations[5];
static size_t g_rec_count = 0;

static const float BAND_FREQS[] = {
    1800, 3500, 7000, 10100, 14000, 18100, 21000, 24900, 28000, 145000, 435000
};

static const char *BAND_NAMES[] = {
    "160m", "80m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "2m", "70cm"
};

static void char_to_maidenhead(float lat, float lon, char *mh) {
    lon += 180.0f;
    lat += 90.0f;
    mh[0] = (char)('A' + (int)(lon / 20));
    mh[1] = (char)('A' + (int)(lat / 10));
    mh[2] = (char)('0' + (int)((lon - ((int)(lon / 20)) * 20) / 2));
    mh[3] = (char)('0' + (int)((lat - ((int)(lat / 10)) * 10) / 1));
    float lon_rem = (lon - ((int)(lon / 2)) * 2) * 60 / 5;
    float lat_rem = (lat - ((int)(lat / 1)) * 1) * 60 / 2.5;
    mh[4] = (char)('a' + (int)lon_rem);
    mh[5] = (char)('a' + (int)lat_rem);
    mh[6] = '\0';
}

int freq_recommender_init(void) {
    g_lat = 45.75f;
    g_lon = 126.65f;
    char_to_maidenhead(g_lat, g_lon, g_maidenhead);
    memset(g_recommendations, 0, sizeof(g_recommendations));
    g_rec_count = 0;
    g_initialized = true;
    return 0;
}

int freq_recommender_deinit(void) {
    g_initialized = false;
    return 0;
}

int freq_recommender_update_location(float lat, float lon, const char *maidenhead) {
    g_lat = lat;
    g_lon = lon;
    if (maidenhead) {
        strncpy(g_maidenhead, maidenhead, sizeof(g_maidenhead) - 1);
    } else {
        char_to_maidenhead(lat, lon, g_maidenhead);
    }
    return 0;
}

int freq_recommender_update_band(radio_band_t band) {
    if (band >= BAND_MAX) return -1;
    g_current_band = band;
    return 0;
}

static int get_hour_from_utc(void) {
    time_t now = time(NULL);
    struct tm *utc = gmtime(&now);
    return utc->tm_hour;
}

int freq_recommender_fetch_propagation(void) {
    if (!g_initialized) return -1;
    agent_bridge_request_freq_recommend(g_current_band, g_lat, g_lon);
    return 0;
}

int freq_recommender_get_recommendations(freq_recommendation_t *recs, size_t max_count, size_t *out_count) {
    if (!recs || !out_count) return -1;
    size_t copy = g_rec_count < max_count ? g_rec_count : max_count;
    memcpy(recs, g_recommendations, copy * sizeof(freq_recommendation_t));
    *out_count = copy;
    return 0;
}

float freq_recommender_get_best_frequency(radio_band_t band) {
    if (band >= BAND_MAX) return BAND_FREQS[BAND_HF_20M];
    int hour = get_hour_from_utc();
    float base = BAND_FREQS[band];
    float offset = 0;
    switch (band) {
        case BAND_HF_40M:
        case BAND_HF_80M:
        case BAND_HF_160M:
            if (hour >= 6 && hour < 18) offset = 50;
            else offset = -50;
            break;
        case BAND_HF_20M:
        case BAND_HF_17M:
        case BAND_HF_15M:
            if (hour >= 8 && hour < 20) offset = 0;
            else offset = -100;
            break;
        case BAND_HF_10M:
        case BAND_HF_12M:
            if (hour >= 10 && hour < 16) offset = 50;
            else offset = -150;
            break;
        default:
            break;
    }
    return base + offset;
}

int freq_recommender_scan_band(radio_band_t band, float *best_freq, float *best_snr) {
    if (band >= BAND_MAX) return -1;
    if (best_freq) *best_freq = freq_recommender_get_best_frequency(band);
    if (best_snr) *best_snr = 20.0f;
    return 0;
}
