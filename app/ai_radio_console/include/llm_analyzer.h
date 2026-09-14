#ifndef __LLM_ANALYZER_H
#define __LLM_ANALYZER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "radio_config.h"

typedef struct {
    float confidence;
    char summary[512];
    char keywords[128];
    bool needs_summary;
} analysis_result_t;

typedef void (*llm_analysis_cb_t)(const analysis_result_t *result, void *user_data);
typedef void (*llm_analysis_stream_cb_t)(const char *chunk_text, bool is_done, void *user_data);

int llm_analyzer_init(const ai_config_t *config);
int llm_analyzer_deinit(void);
int llm_analyzer_analyze_transcript(const char *transcript, llm_analysis_cb_t cb, void *user_data);
int llm_analyzer_analyze_transcript_stream(const char *transcript,
                                            llm_analysis_stream_cb_t stream_cb,
                                            llm_analysis_cb_t final_cb,
                                            void *user_data);
int llm_analyzer_summarize_log(const char *context, char *summary, size_t max_len);
int llm_analyzer_translate(const char *text, translate_lang_t from, translate_lang_t to,
                            char *translated, size_t max_len);
bool llm_analyzer_is_enabled(void);

#endif
