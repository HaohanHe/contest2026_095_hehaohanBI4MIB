#include "mayday_detector.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <math.h>

static bool g_initialized = false;
static bool g_triggered = false;
static mayday_alert_callback_t g_alert_cb = NULL;
static void *g_cb_data = NULL;
static alert_event_t g_last_alert;

static const char *MAYDAY_KEYWORDS[] = {
    "mayday", "sos", "求救", "紧急", "遇险", "事故",
    "help", "救命", "emergency", "distress", NULL
};

static int keyword_match_score(const char *text) {
    int score = 0;
    if (!text) return 0;
    for (int i = 0; MAYDAY_KEYWORDS[i] != NULL; i++) {
        if (strstr(text, MAYDAY_KEYWORDS[i])) {
            score += 25;
        }
    }
    char lower[512];
    strncpy(lower, text, sizeof(lower) - 1);
    lower[sizeof(lower) - 1] = '\0';
    for (char *p = lower; *p; p++) {
        if (*p >= 'A' && *p <= 'Z') *p += 32;
    }
    if (strstr(lower, "i am") || strstr(lower, "i'm") || strstr(lower, "we are")) {
        score += 5;
    }
    if (strstr(lower, "locate") || strstr(lower, "position") || strstr(lower, "坐标") || strstr(lower, "位置")) {
        score += 10;
    }
    if (strstr(lower, "injur") || strstr(lower, "hurt") || strstr(lower, "伤") || strstr(lower, "伤亡")) {
        score += 15;
    }
    if (strstr(lower, "sink") || strstr(lower, "crash") || strstr(lower, "沉没") || strstr(lower, "坠毁")) {
        score += 20;
    }
    if (strstr(lower, "fire") || strstr(lower, "火")) {
        score += 15;
    }
    return score;
}

int mayday_detector_init(void) {
    memset(&g_last_alert, 0, sizeof(g_last_alert));
    g_triggered = false;
    g_initialized = true;
    return 0;
}

int mayday_detector_deinit(void) {
    g_initialized = false;
    return 0;
}

int mayday_detector_feed_audio(const int16_t *samples, size_t count) {
    (void)samples;
    (void)count;
    return 0;
}

int mayday_detector_notify_transcript(const char *text, float confidence) {
    if (!text) return -1;
    int score = keyword_match_score(text);
    float total_conf = confidence * (float)score / 100.0f;
    if (total_conf >= MAYDAY_TRIGGER_CONF && score >= 40) {
        g_triggered = true;
        g_last_alert.level = ALERT_LEVEL_MAYDAY;
        g_last_alert.timestamp = (uint32_t)time(NULL);
        g_last_alert.frequency = 0;
        snprintf(g_last_alert.message, sizeof(g_last_alert.message), "MAYDAY/求救信号 detected!");
        snprintf(g_last_alert.details, sizeof(g_last_alert.details),
                 "Transcript: %s (score=%d, conf=%.2f)", text, score, total_conf);
        if (g_alert_cb) {
            g_alert_cb(&g_last_alert, g_cb_data);
        }
    }
    return 0;
}

bool mayday_detector_is_triggered(void) {
    return g_triggered;
}

const alert_event_t *mayday_detector_get_last_alert(void) {
    return &g_last_alert;
}

void mayday_detector_clear_alert(void) {
    g_triggered = false;
    memset(&g_last_alert, 0, sizeof(g_last_alert));
}

void mayday_detector_set_alert_cb(mayday_alert_callback_t cb, void *user_data) {
    g_alert_cb = cb;
    g_cb_data = user_data;
}
