#include "agent_bridge.h"
#include <stdio.h>
#include <string.h>

static bool g_initialized = false;
static agent_msg_callback_t g_msg_cb = NULL;
static agent_alert_callback_t g_alert_cb = NULL;
static agent_transcript_cb_t g_transcript_cb = NULL;
static agent_translate_cb_t g_translate_cb = NULL;
static agent_cw_text_cb_t g_cw_cb = NULL;
static void *g_cb_data = NULL;

int agent_bridge_init(void)
{
    printf("[agent_bridge] Initialized (stub mode - no real ai_agent connection)\n");
    g_initialized = true;
    return 0;
}

int agent_bridge_deinit(void)
{
    g_initialized = false;
    return 0;
}

int agent_bridge_send_audio(const int16_t *samples, size_t count)
{
    (void)samples;
    (void)count;
    return 0;
}

int agent_bridge_send_text(const char *text)
{
    (void)text;
    return 0;
}

int agent_bridge_send_event(const char *event_type, const char *json_payload)
{
    (void)event_type;
    (void)json_payload;
    return 0;
}

int agent_bridge_request_translate(const char *text, translate_lang_t from, translate_lang_t to)
{
    (void)text;
    (void)from;
    (void)to;
    return 0;
}

int agent_bridge_request_summary(const char *context)
{
    (void)context;
    return 0;
}

int agent_bridge_send_cw_text(const char *text)
{
    (void)text;
    return 0;
}

int agent_bridge_send_signal_report(float freq, float rssi, float snr, const char *interference_json)
{
    (void)freq;
    (void)rssi;
    (void)snr;
    (void)interference_json;
    return 0;
}

int agent_bridge_request_freq_recommend(radio_band_t band, float lat, float lon)
{
    (void)band;
    (void)lat;
    (void)lon;
    return 0;
}

int agent_bridge_install_skills(void)
{
    return 0;
}

bool agent_bridge_is_connected(void)
{
    return false;
}

int agent_bridge_get_status(char *status, size_t max_len)
{
    if (!status || max_len == 0) return -1;
    strncpy(status, "{\"connected\":false,\"mode\":\"stub\"}", max_len - 1);
    status[max_len - 1] = '\0';
    return 0;
}

void agent_bridge_set_msg_cb(agent_msg_callback_t cb, void *user_data)
{
    g_msg_cb = cb;
    g_cb_data = user_data;
}

void agent_bridge_set_alert_cb(agent_alert_callback_t cb, void *user_data)
{
    g_alert_cb = cb;
    g_cb_data = user_data;
}

void agent_bridge_set_transcript_cb(agent_transcript_cb_t cb, void *user_data)
{
    g_transcript_cb = cb;
    g_cb_data = user_data;
}

void agent_bridge_set_translate_cb(agent_translate_cb_t cb, void *user_data)
{
    g_translate_cb = cb;
    g_cb_data = user_data;
}

void agent_bridge_set_cw_cb(agent_cw_text_cb_t cb, void *user_data)
{
    g_cw_cb = cb;
    g_cb_data = user_data;
}
