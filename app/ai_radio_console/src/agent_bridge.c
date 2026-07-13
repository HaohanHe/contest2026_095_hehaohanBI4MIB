#include "agent_bridge.h"
#include "asr_engine.h"
#include "llm_analyzer.h"
#include "siliconflow_client.h"
#include "radio_log.h"
#include "radio_config.h"
#include "config_store.h"
#include "ui_ai_radio.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>

static ai_config_t g_config;
static bool g_initialized = false;
static bool g_connected = false;
static agent_msg_callback_t g_msg_cb = NULL;
static agent_alert_callback_t g_alert_cb = NULL;
static agent_transcript_cb_t g_transcript_cb = NULL;
static agent_translate_cb_t g_translate_cb = NULL;
static agent_cw_text_cb_t g_cw_cb = NULL;
static void *g_cb_data = NULL;
static float g_current_freq = 14250000.0f;
static char g_current_mode[16] = "USB";

static void on_asr_result(const char *text, bool is_partial, void *user_data);
static void on_asr_state(asr_state_t state, void *user_data);
static void on_asr_error(int error_code, const char *message, void *user_data);
static void on_llm_analysis(const analysis_result_t *result, void *ud);
static void on_llm_stream_chunk(const char *chunk_text, bool is_done, void *user_data);

static char g_current_utt_text[MAX_TRANSCRIPT_LEN];

static void on_llm_stream_chunk(const char *chunk_text, bool is_done, void *user_data)
{
    (void)user_data;
    if (is_done) return;
    if (!chunk_text || chunk_text[0] == '\0') return;
    printf("[agent_bridge] LLM analysis stream: %s", chunk_text);
}

static void on_llm_analysis(const analysis_result_t *result, void *ud)
{
    (void)ud;

    ui_ai_radio_set_analysis(result->summary, result->alert_level);
    ui_ai_radio_show_alert(result->alert_level > ALERT_LEVEL_NONE, result->alert_level);

    if (result->needs_alert && g_alert_cb) {
        alert_event_t alert;
        memset(&alert, 0, sizeof(alert));
        alert.level = result->alert_level;
        alert.timestamp = (uint32_t)time(NULL);
        alert.frequency = g_current_freq;
        if (result->type == ANALYSIS_MAYDAY_DETECTED) {
            snprintf(alert.message, sizeof(alert.message),
                     "MAYDAY/SOS求救信号检测!");
            snprintf(alert.details, sizeof(alert.details),
                     "置信度: %.0f%%, 摘要: %s, 关键词: %s",
                     result->confidence * 100, result->summary, result->keywords);
        } else if (result->type == ANALYSIS_VIOLATION_DETECTED) {
            snprintf(alert.message, sizeof(alert.message),
                     "违规内容检测: %s",
                     llm_violation_type_str(result->violation));
            snprintf(alert.details, sizeof(alert.details),
                     "置信度: %.0f%%, 摘要: %s",
                     result->confidence * 100, result->summary);
        }
        g_alert_cb(&alert, g_cb_data);
        radio_log_add_event(&alert);
    }
    if (g_msg_cb && result->summary[0]) {
        char msg[1024];
        snprintf(msg, sizeof(msg), "{\"type\":\"analysis\",\"summary\":\"%s\",\"mayday\":%s,\"violation\":\"%s\",\"confidence\":%.2f}",
                 result->summary,
                 result->type == ANALYSIS_MAYDAY_DETECTED ? "true" : "false",
                 llm_violation_type_str(result->violation),
                 result->confidence);
        g_msg_cb(msg, g_cb_data);
    }
}

static void on_asr_result(const char *text, bool is_partial, void *user_data)
{
    (void)user_data;
    if (!text || text[0] == '\0') return;

    printf("[agent_bridge] ASR %s: %s\n", is_partial ? "partial" : "final", text);

    ui_ai_radio_append_transcript(text, is_partial);

    /* Accumulate utterance text; partial results are deltas from ASR engine */
    size_t curr_len = strlen(g_current_utt_text);
    size_t text_len = strlen(text);
    if (curr_len + text_len < sizeof(g_current_utt_text) - 1) {
        memcpy(g_current_utt_text + curr_len, text, text_len);
        g_current_utt_text[curr_len + text_len] = '\0';
    }

    if (g_transcript_cb) {
        g_transcript_cb(text, is_partial ? 1 : 0, g_cb_data);
    }

    if (is_partial) {
        return;
    }

    /* Final result: log complete utterance and run LLM analysis */
    if (g_config.auto_logging) {
        radio_log_qso_text(g_current_freq, g_current_mode, g_current_utt_text);
    }

    if (g_config.llm_enabled && (g_config.mayday_detection_via_llm || g_config.violation_detection)) {
        llm_analyzer_analyze_transcript_stream(g_current_utt_text, g_current_freq,
                                                g_current_mode,
                                                on_llm_stream_chunk,
                                                on_llm_analysis, NULL);
    }

    g_current_utt_text[0] = '\0';
}

static void on_asr_state(asr_state_t state, void *user_data)
{
    (void)user_data;
    const char *state_names[] = {"IDLE", "RECORDING", "UPLOADING", "PROCESSING", "READY", "ERROR"};
    if (state >= 0 && state <= ASR_STATE_ERROR) {
        printf("[agent_bridge] ASR state: %s\n", state_names[state]);
    }
}

static void on_asr_error(int error_code, const char *message, void *user_data)
{
    (void)error_code;
    (void)user_data;
    printf("[agent_bridge] ASR error: %s\n", message);
    if (g_msg_cb) {
        char msg[512];
        snprintf(msg, sizeof(msg), "{\"type\":\"asr_error\",\"error\":\"%s\"}", message);
        g_msg_cb(msg, g_cb_data);
    }
}

int agent_bridge_init(void)
{
    radio_config_store_load(&g_config);
    g_current_utt_text[0] = '\0';

    if (g_config.api_key[0] != '\0') {
        sf_client_init(&g_config);
        llm_analyzer_init(&g_config);
        asr_engine_init(&g_config);

        asr_callbacks_t cbs = {0};
        cbs.on_result = on_asr_result;
        cbs.on_state_change = on_asr_state;
        cbs.on_error = on_asr_error;
        asr_engine_set_callbacks(&cbs);

        if (g_config.asr_enabled) {
            asr_engine_start();
        }

        g_connected = sf_client_check_connection();
        printf("[agent_bridge] Initialized with SiliconFlow (ASR model: %s, LLM model: %s)\n",
               g_config.asr_model, g_config.llm_model);
    } else {
        printf("[agent_bridge] Initialized (no API key configured - using stub mode)\n");
        printf("[agent_bridge] To enable voice AI, configure SiliconFlow API key in Settings\n");
    }

    g_initialized = true;
    return 0;
}

int agent_bridge_deinit(void)
{
    asr_engine_stop();
    asr_engine_deinit();
    llm_analyzer_deinit();
    sf_client_deinit();
    g_initialized = false;
    g_connected = false;
    return 0;
}

int agent_bridge_send_audio(const int16_t *samples, size_t count)
{
    if (!g_initialized) return -1;
    if (g_config.asr_enabled && asr_engine_is_enabled()) {
        asr_engine_feed_audio(samples, count);
    }
    return 0;
}

int agent_bridge_start_asr(void)
{
    if (!g_initialized || !g_config.asr_enabled) return -1;
    return asr_engine_start();
}

int agent_bridge_stop_asr(void)
{
    return asr_engine_stop();
}

int agent_bridge_flush_asr(void)
{
    return asr_engine_flush();
}

int agent_bridge_send_text(const char *text)
{
    if (!text) return -1;
    printf("[agent_bridge] Text: %s\n", text);
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
    if (!g_initialized || !llm_analyzer_is_enabled() || !text) return -1;
    char translated[512];
    if (llm_analyzer_translate(text, from, to, translated, sizeof(translated)) == 0) {
        if (g_translate_cb) {
            g_translate_cb(text, translated, g_cb_data);
        }
    }
    return 0;
}

int agent_bridge_request_summary(const char *context)
{
    if (!g_initialized || !llm_analyzer_is_enabled() || !context) return -1;
    char summary[1024];
    if (llm_analyzer_summarize_log(context, summary, sizeof(summary)) == 0) {
        if (g_msg_cb) {
            char msg[2048];
            snprintf(msg, sizeof(msg), "{\"type\":\"summary\",\"text\":\"%s\"}", summary);
            g_msg_cb(msg, g_cb_data);
        }
    }
    return 0;
}

int agent_bridge_send_cw_text(const char *text)
{
    if (!text) return -1;
    if (g_cw_cb) {
        g_cw_cb(text, g_cb_data);
    }
    return 0;
}

int agent_bridge_send_signal_report(float freq, float rssi, float snr, const char *interference_json)
{
    g_current_freq = freq;
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

int agent_bridge_set_mode(const char *mode)
{
    if (mode) {
        strncpy(g_current_mode, mode, sizeof(g_current_mode) - 1);
        g_current_mode[sizeof(g_current_mode) - 1] = '\0';
    }
    ui_ai_radio_set_frequency(g_current_freq, g_current_mode);
    return 0;
}

int agent_bridge_set_frequency(float freq_hz)
{
    g_current_freq = freq_hz;
    ui_ai_radio_set_frequency(g_current_freq, g_current_mode);
    return 0;
}

int agent_bridge_update_config(const ai_config_t *config)
{
    if (!config) return -1;
    bool was_enabled = g_config.asr_enabled;
    memcpy(&g_config, config, sizeof(g_config));
    g_current_utt_text[0] = '\0';
    radio_config_store_save(&g_config);

    asr_engine_stop();
    asr_engine_deinit();
    llm_analyzer_deinit();
    sf_client_deinit();

    sf_client_init(&g_config);
    llm_analyzer_init(&g_config);
    asr_engine_init(&g_config);

    asr_callbacks_t cbs = {0};
    cbs.on_result = on_asr_result;
    cbs.on_state_change = on_asr_state;
    cbs.on_error = on_asr_error;
    asr_engine_set_callbacks(&cbs);

    g_connected = sf_client_check_connection();

    if (g_config.asr_enabled && !was_enabled) {
        asr_engine_start();
    }
    return 0;
}

const ai_config_t *agent_bridge_get_config(void)
{
    return &g_config;
}

int agent_bridge_install_skills(void)
{
    return 0;
}

bool agent_bridge_is_connected(void)
{
    return g_connected;
}

asr_state_t agent_bridge_get_asr_state(void)
{
    return asr_engine_get_state();
}

int agent_bridge_get_status(char *status, size_t max_len)
{
    if (!status || max_len == 0) return -1;
    const char *asr_state_names[] = {"idle", "recording", "uploading", "processing", "ready", "error"};
    asr_state_t s = asr_engine_get_state();
    snprintf(status, max_len,
             "{\"connected\":%s,\"mode\":\"%s\",\"asr_enabled\":%s,\"llm_enabled\":%s,\"asr_state\":\"%s\",\"api_key_set\":%s}",
             g_connected ? "true" : "false",
             g_config.asr_enabled ? "siliconflow" : "stub",
             g_config.asr_enabled ? "true" : "false",
             g_config.llm_enabled ? "true" : "false",
             asr_state_names[s],
             g_config.api_key[0] ? "true" : "false");
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
