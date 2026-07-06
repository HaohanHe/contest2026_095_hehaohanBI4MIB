#ifndef __LLM_ANALYZER_H
#define __LLM_ANALYZER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "radio_config.h"

typedef enum {
    ANALYSIS_NONE = 0,
    ANALYSIS_MAYDAY_DETECTED,
    ANALYSIS_VIOLATION_DETECTED,
    ANALYSIS_NORMAL_QSO,
    ANALYSIS_NO_CONTENT
} analysis_result_type_t;

typedef enum {
    VIOLATION_NONE = 0,
    VIOLATION_PROFANITY,
    VIOLATION_ILLEGAL_CONTENT,
    VIOLATION_UNLICENSED,
    VIOLATION_INTERFERENCE,
    VIOLATION_OTHER
} violation_type_t;

typedef struct {
    analysis_result_type_t type;
    alert_level_t alert_level;
    violation_type_t violation;
    float confidence;
    char summary[512];
    char keywords[128];
    char callsigns[128];
    char translation[512];
    bool needs_alert;
} analysis_result_t;

typedef void (*llm_analysis_cb_t)(const analysis_result_t *result, void *user_data);

int llm_analyzer_init(const ai_config_t *config);
int llm_analyzer_deinit(void);
int llm_analyzer_analyze_transcript(const char *transcript, float frequency,
                                     const char *mode, llm_analysis_cb_t cb, void *user_data);
int llm_analyzer_summarize_log(const char *context, char *summary, size_t max_len);
int llm_analyzer_translate(const char *text, translate_lang_t from, translate_lang_t to,
                            char *translated, size_t max_len);
bool llm_analyzer_is_enabled(void);
const char *llm_violation_type_str(violation_type_t type);

#endif