/****************************************************************************
 * agent_bridge.c - DayNote Agent Bridge
 *
 * Bridges ASR, LLM, SiliconFlow client, config, and translation services
 * into a unified API for the DayNote voice memo application.
 *
 * All radio-specific code (radio_log, mayday, CW, frequency/mode tracking,
 * alert levels, violation detection) has been removed.
 ****************************************************************************/

#include "agent_bridge.h"
#include "asr_engine.h"
#include "llm_analyzer.h"
#include "siliconflow_client.h"
#include "radio_config.h"
#include "config_store.h"
#include "ui_daynote.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

/****************************************************************************
 * Private Data
 ****************************************************************************/

static ai_config_t g_config;
static bool g_initialized = false;
static bool g_connected = false;

/* User-registered callbacks */
static agent_msg_callback_t      g_msg_cb       = NULL;
static agent_transcript_cb_t    g_transcript_cb = NULL;
static agent_translate_cb_t     g_translate_cb  = NULL;
static void                     *g_cb_data      = NULL;

/* Accumulator for the current utterance */
static char g_current_utt_text[MAX_TRANSCRIPT_LEN];

/****************************************************************************
 * Private ASR Callbacks
 ****************************************************************************/

static void on_asr_result(const char *text, bool is_partial, void *user_data)
{
    (void)user_data;

    if (!text || text[0] == '\0') {
        return;
    }

    printf("[agent_bridge] ASR %s: %s\n", is_partial ? "partial" : "final", text);

    /* Accumulate partial results into the utterance buffer */
    size_t curr_len = strlen(g_current_utt_text);
    size_t text_len = strlen(text);
    if (curr_len + text_len < sizeof(g_current_utt_text) - 1) {
        memcpy(g_current_utt_text + curr_len, text, text_len);
        g_current_utt_text[curr_len + text_len] = '\0';
    }

    /* Forward directly to transcript callback */
    if (g_transcript_cb) {
        g_transcript_cb(text, is_partial ? 1 : 0, g_cb_data);
    }

    /* On final result, reset the accumulator */
    if (!is_partial) {
        g_current_utt_text[0] = '\0';
    }
}

static void on_asr_state(asr_state_t state, void *user_data)
{
    (void)user_data;

    static const char *state_names[] = {
        "IDLE", "RECORDING", "UPLOADING", "PROCESSING", "READY", "ERROR"
    };

    if (state >= 0 && state <= ASR_STATE_ERROR) {
        printf("[agent_bridge] ASR state -> %s\n", state_names[state]);
    }
}

static void on_asr_error(int error_code, const char *message, void *user_data)
{
    (void)user_data;

    fprintf(stderr, "[agent_bridge] ASR error %d: %s\n",
            error_code, message ? message : "(null)");

    if (g_msg_cb) {
        g_msg_cb("{\"type\":\"error\",\"source\":\"asr\"}", g_cb_data);
    }
}

/****************************************************************************
 * Private LLM / Translate Callbacks
 ****************************************************************************/

static void on_llm_analysis(const analysis_result_t *result, void *user_data)
{
    (void)user_data;

    if (!result) {
        return;
    }

    printf("[agent_bridge] LLM analysis done, summary=%zu chars\n",
           strlen(result->summary));

    if (g_msg_cb) {
        g_msg_cb(result->summary, g_cb_data);
    }
}

static void on_translate_done(const char *original, const char *translated,
                              void *user_data)
{
    (void)user_data;

    if (g_translate_cb) {
        g_translate_cb(original, translated, g_cb_data);
    }
}

/****************************************************************************
 * Public API - Lifecycle
 ****************************************************************************/

int agent_bridge_init(void)
{
    if (g_initialized) {
        printf("[agent_bridge] already initialized\n");
        return 0;
    }

    printf("[agent_bridge] initializing...\n");

    /* Load configuration from persistent storage */
    radio_config_set_defaults(&g_config);
    radio_config_store_load(&g_config);

    /* Initialize SiliconFlow HTTP client */
    if (sf_client_init(&g_config) != 0) {
        fprintf(stderr, "[agent_bridge] siliconflow client init failed\n");
        /* Continue anyway - offline mode is acceptable */
    }

    /* Initialize ASR engine with callbacks */
    asr_callbacks_t asr_cbs = {
        .on_result       = on_asr_result,
        .on_state_change = on_asr_state,
        .on_error        = on_asr_error,
        .user_data       = NULL
    };
    asr_engine_set_callbacks(&asr_cbs);

    if (asr_engine_init(&g_config) != 0) {
        fprintf(stderr, "[agent_bridge] ASR engine init failed\n");
        /* Continue anyway - ASR may be disabled */
    }

    /* Initialize LLM analyzer */
    if (llm_analyzer_init(&g_config) != 0) {
        fprintf(stderr, "[agent_bridge] LLM analyzer init failed\n");
        /* Continue anyway */
    }

    /* Check backend connectivity */
    g_connected = sf_client_check_connection();
    printf("[agent_bridge] backend connected: %s\n",
           g_connected ? "yes" : "no");

    g_initialized = true;
    printf("[agent_bridge] initialization complete\n");

    return 0;
}

int agent_bridge_deinit(void)
{
    if (!g_initialized) {
        return 0;
    }

    printf("[agent_bridge] deinitializing...\n");

    llm_analyzer_deinit();
    asr_engine_deinit();
    sf_client_deinit();

    g_initialized = false;
    g_connected   = false;

    return 0;
}

/****************************************************************************
 * Public API - Audio / ASR
 ****************************************************************************/

int agent_bridge_send_audio(const int16_t *samples, size_t count)
{
    if (!g_initialized || !samples || count == 0) {
        return -EINVAL;
    }

    return asr_engine_feed_audio(samples, count);
}

int agent_bridge_start_asr(void)
{
    if (!g_initialized) {
        return -ENODEV;
    }

    g_current_utt_text[0] = '\0';
    return asr_engine_start();
}

int agent_bridge_stop_asr(void)
{
    if (!g_initialized) {
        return -ENODEV;
    }

    return asr_engine_stop();
}

int agent_bridge_flush_asr(void)
{
    if (!g_initialized) {
        return -ENODEV;
    }

    return asr_engine_flush();
}

/****************************************************************************
 * Public API - Text / LLM
 ****************************************************************************/

int agent_bridge_send_text(const char *text)
{
    if (!g_initialized || !text) {
        return -EINVAL;
    }

    if (!g_config.llm_enabled || !llm_analyzer_is_enabled()) {
        fprintf(stderr, "[agent_bridge] LLM is disabled\n");
        return -ENODEV;
    }

    /* Analyze the user-supplied text via LLM */
    return llm_analyzer_analyze_transcript(text, on_llm_analysis, NULL);
}

int agent_bridge_send_event(const char *event_type, const char *json_payload)
{
    if (!g_initialized || !event_type) {
        return -EINVAL;
    }

    printf("[agent_bridge] event: %s\n", event_type);

    if (g_msg_cb) {
        /* Construct a minimal JSON envelope and forward to the message cb */
        char buf[1024];
        if (json_payload) {
            snprintf(buf, sizeof(buf),
                     "{\"type\":\"event\",\"event\":\"%s\",\"data\":%s}",
                     event_type, json_payload);
        } else {
            snprintf(buf, sizeof(buf),
                     "{\"type\":\"event\",\"event\":\"%s\"}",
                     event_type);
        }
        g_msg_cb(buf, g_cb_data);
    }

    return 0;
}

/****************************************************************************
 * Public API - Translation
 ****************************************************************************/

int agent_bridge_request_translate(const char *text, translate_lang_t from,
                                   translate_lang_t to)
{
    if (!g_initialized || !text) {
        return -EINVAL;
    }

    if (!g_config.llm_enabled || !llm_analyzer_is_enabled()) {
        fprintf(stderr, "[agent_bridge] translation unavailable (LLM disabled)\n");
        return -ENODEV;
    }

    /* Use LLM analyzer for translation, which calls on_translate_done */
    char translated[MAX_LLM_RESPONSE_LEN];
    int ret = llm_analyzer_translate(text, from, to,
                                     translated, sizeof(translated));
    if (ret == 0) {
        on_translate_done(text, translated, NULL);
    }

    return ret;
}

/****************************************************************************
 * Public API - Summary
 ****************************************************************************/

int agent_bridge_request_summary(const char *context)
{
    if (!g_initialized || !context) {
        return -EINVAL;
    }

    if (!g_config.llm_enabled || !llm_analyzer_is_enabled()) {
        fprintf(stderr, "[agent_bridge] summary unavailable (LLM disabled)\n");
        return -ENODEV;
    }

    char summary[MAX_LLM_RESPONSE_LEN];
    int ret = llm_analyzer_summarize_log(context, summary, sizeof(summary));
    if (ret == 0 && g_msg_cb) {
        g_msg_cb(summary, g_cb_data);
    }

    return ret;
}

/****************************************************************************
 * Public API - Configuration
 ****************************************************************************/

int agent_bridge_update_config(const ai_config_t *config)
{
    if (!config) {
        return -EINVAL;
    }

    memcpy(&g_config, config, sizeof(ai_config_t));

    /* Persist to storage */
    radio_config_store_save(&g_config);

    /* Re-initialize subsystems that depend on config */
    asr_engine_deinit();
    asr_engine_init(&g_config);

    llm_analyzer_deinit();
    llm_analyzer_init(&g_config);

    return 0;
}

const ai_config_t *agent_bridge_get_config(void)
{
    return &g_config;
}

/****************************************************************************
 * Public API - Skills (placeholder)
 ****************************************************************************/

int agent_bridge_install_skills(void)
{
    printf("[agent_bridge] install_skills: not implemented (placeholder)\n");
    return 0;
}

/****************************************************************************
 * Public API - Status Queries
 ****************************************************************************/

bool agent_bridge_is_connected(void)
{
    if (!g_initialized) {
        return false;
    }

    /* Refresh connectivity status */
    g_connected = sf_client_check_connection();
    return g_connected;
}

asr_state_t agent_bridge_get_asr_state(void)
{
    if (!g_initialized) {
        return ASR_STATE_IDLE;
    }

    return asr_engine_get_state();
}

int agent_bridge_get_status(char *status, size_t max_len)
{
    if (!status || max_len == 0) {
        return -EINVAL;
    }

    const char *asr_state_str = "idle";
    asr_state_t asr_st = g_initialized ? asr_engine_get_state() : ASR_STATE_IDLE;
    static const char *state_names[] = {
        "idle", "recording", "uploading", "processing", "ready", "error"
    };
    if (asr_st <= ASR_STATE_ERROR) {
        asr_state_str = state_names[asr_st];
    }

    snprintf(status, max_len,
             "{\"initialized\":%s,\"connected\":%s,"
             "\"asr_state\":\"%s\",\"asr_enabled\":%s,\"llm_enabled\":%s}",
             g_initialized ? "true" : "false",
             g_connected   ? "true" : "false",
             asr_state_str,
             g_config.asr_enabled ? "true" : "false",
             g_config.llm_enabled ? "true" : "false");

    return 0;
}

/****************************************************************************
 * Public API - Callback Registration
 ****************************************************************************/

void agent_bridge_set_msg_cb(agent_msg_callback_t cb, void *user_data)
{
    g_msg_cb    = cb;
    g_cb_data   = user_data;
}

void agent_bridge_set_transcript_cb(agent_transcript_cb_t cb, void *user_data)
{
    g_transcript_cb = cb;
    g_cb_data       = user_data;
}

void agent_bridge_set_translate_cb(agent_translate_cb_t cb, void *user_data)
{
    g_translate_cb = cb;
    g_cb_data      = user_data;
}
