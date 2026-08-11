/****************************************************************************
 * llm_analyzer.c - DayNote LLM Analysis Module
 *
 * Provides voice-transcript analysis, translation, and log summarization
 * using the SiliconFlow LLM API.
 ****************************************************************************/

#include "llm_analyzer.h"
#include "siliconflow_client.h"
#include "json_minimal.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>

static ai_config_t g_config;
static bool g_enabled = false;
static pthread_mutex_t g_mutex;

/* DayNote-friendly system prompts */
static const char *SYSTEM_PROMPT_ANALYSIS =
    "你是一个语音笔记分析助手。分析以下语音转写文本，提取关键词并给出简短中文摘要。"
    "返回JSON格式：{\"confidence\":0-1,\"summary\":\"摘要\",\"keywords\":\"关键词\"}";

static const char *SYSTEM_PROMPT_TRANSLATE =
    "你是一个翻译助手。将用户输入的文本翻译成目标语言，只输出翻译结果。";

static const char *SYSTEM_PROMPT_SUMMARY =
    "你是一个每日笔记摘要助手。请将以下笔记内容整理为简洁的中文摘要，突出重点和今日高光。";

/****************************************************************************
 * Public API
 ****************************************************************************/

int llm_analyzer_init(const ai_config_t *config)
{
    if (!config) return -1;
    memcpy(&g_config, config, sizeof(g_config));
    g_enabled = config->llm_enabled && config->api_key[0] != '\0';
    pthread_mutex_init(&g_mutex, NULL);
    return 0;
}

int llm_analyzer_deinit(void)
{
    pthread_mutex_destroy(&g_mutex);
    return 0;
}

/****************************************************************************
 * Internal: parse LLM JSON response into analysis_result_t
 ****************************************************************************/

static int parse_analysis_result(const char *response, analysis_result_t *result)
{
    memset(result, 0, sizeof(*result));
    result->needs_summary = false;

    json_extract_float(response, "confidence", &result->confidence);
    json_extract_string(response, "summary", result->summary, sizeof(result->summary));
    json_extract_string(response, "keywords", result->keywords, sizeof(result->keywords));

    return 0;
}

/****************************************************************************
 * Internal: build the user-prompt from a transcript
 ****************************************************************************/

static void build_analysis_prompt(const char *transcript,
                                  char *out_buf, size_t out_buf_len)
{
    snprintf(out_buf, out_buf_len,
             "请分析以下语音转写文本：\n%s", transcript);
}

/****************************************************************************
 * Internal: analysis thread (non-stream)
 ****************************************************************************/

typedef struct {
    char transcript[MAX_TRANSCRIPT_LEN];
    llm_analysis_cb_t cb;
    void *user_data;
} analysis_task_t;

static void *analysis_thread(void *arg)
{
    analysis_task_t *task = (analysis_task_t *)arg;
    if (!task) return NULL;

    const char *model = g_config.llm_model[0] ? g_config.llm_model : DEFAULT_LLM_MODEL;

    /* Heap-allocate large buffers to avoid stack overflow on small thread stacks */
    char *user_msg = (char *)malloc(MAX_TRANSCRIPT_LEN + 128);
    char *response = (char *)malloc(MAX_LLM_RESPONSE_LEN);
    if (!user_msg || !response) {
        free(user_msg);
        free(response);
        free(task);
        return NULL;
    }

    build_analysis_prompt(task->transcript, user_msg, MAX_TRANSCRIPT_LEN + 128);

    float conf = 0;
    int ret = sf_client_chat_completion(model, SYSTEM_PROMPT_ANALYSIS, user_msg,
                                        response, MAX_LLM_RESPONSE_LEN, &conf);
    free(user_msg);

    analysis_result_t result;
    if (ret == 0) {
        parse_analysis_result(response, &result);
    } else {
        memset(&result, 0, sizeof(result));
        result.needs_summary = false;
        snprintf(result.summary, sizeof(result.summary), "[LLM调用失败: %s]",
                 sf_client_get_last_error());
    }
    free(response);

    if (task->cb) {
        task->cb(&result, task->user_data);
    }

    free(task);
    return NULL;
}

/****************************************************************************
 * Internal: streaming analysis thread
 ****************************************************************************/

typedef struct {
    char transcript[MAX_TRANSCRIPT_LEN];
    llm_analysis_stream_cb_t stream_cb;
    llm_analysis_cb_t final_cb;
    void *user_data;
    char response[MAX_LLM_RESPONSE_LEN];
    size_t response_len;
    volatile bool task_freed;
} stream_analysis_task_t;

static void stream_analysis_chunk_cb(const char *chunk_text, bool is_done, void *user_data)
{
    stream_analysis_task_t *task = (stream_analysis_task_t *)user_data;
    if (!task) return;

    if (is_done) {
        analysis_result_t result;
        parse_analysis_result(task->response, &result);
        if (task->final_cb) {
            task->final_cb(&result, task->user_data);
        }
        task->task_freed = true;
        free(task);
        return;
    }

    if (task->stream_cb) {
        task->stream_cb(chunk_text, false, task->user_data);
    }

    size_t chunk_len = strlen(chunk_text);
    if (task->response_len + chunk_len < sizeof(task->response) - 1) {
        memcpy(task->response + task->response_len, chunk_text, chunk_len);
        task->response_len += chunk_len;
        task->response[task->response_len] = '\0';
    }
}

static void *stream_analysis_thread(void *arg)
{
    stream_analysis_task_t *task = (stream_analysis_task_t *)arg;
    if (!task) return NULL;

    const char *model = g_config.llm_model[0] ? g_config.llm_model : DEFAULT_LLM_MODEL;

    /* Heap-allocate large buffer to avoid stack overflow on small thread stacks */
    char *user_msg = (char *)malloc(MAX_TRANSCRIPT_LEN + 128);
    if (!user_msg) {
        free(task);
        return NULL;
    }

    build_analysis_prompt(task->transcript, user_msg, MAX_TRANSCRIPT_LEN + 128);

    int ret = sf_client_chat_completion_stream(model, SYSTEM_PROMPT_ANALYSIS, user_msg,
                                                stream_analysis_chunk_cb, task);
    free(user_msg);
    if (ret != 0) {
        analysis_result_t result;
        memset(&result, 0, sizeof(result));
        result.needs_summary = false;
        snprintf(result.summary, sizeof(result.summary), "[LLM流式调用失败: %s]",
                 sf_client_get_last_error());
        if (task->final_cb) {
            task->final_cb(&result, task->user_data);
        }
        task->task_freed = true;
        free(task);
    }
    /* Defensive free for edge cases where is_done is never received */
    if (!task->task_freed) {
        free(task);
    }
    return NULL;
}

/****************************************************************************
 * Public API: Analyze a single transcript (async)
 ****************************************************************************/

int llm_analyzer_analyze_transcript(const char *transcript,
                                     llm_analysis_cb_t cb, void *user_data)
{
    if (!g_enabled || !transcript) return -1;

    analysis_task_t *task = (analysis_task_t *)malloc(sizeof(analysis_task_t));
    if (!task) return -1;
    memset(task, 0, sizeof(*task));

    strncpy(task->transcript, transcript, sizeof(task->transcript) - 1);
    task->transcript[sizeof(task->transcript) - 1] = '\0';
    task->cb = cb;
    task->user_data = user_data;

    pthread_t tid;
    pthread_create(&tid, NULL, analysis_thread, task);
    pthread_detach(tid);
    return 0;
}

/****************************************************************************
 * Public API: Analyze a single transcript (streaming, async)
 ****************************************************************************/

int llm_analyzer_analyze_transcript_stream(const char *transcript,
                                            llm_analysis_stream_cb_t stream_cb,
                                            llm_analysis_cb_t final_cb,
                                            void *user_data)
{
    if (!g_enabled || !transcript) return -1;

    stream_analysis_task_t *task = (stream_analysis_task_t *)malloc(sizeof(stream_analysis_task_t));
    if (!task) return -1;
    memset(task, 0, sizeof(*task));

    strncpy(task->transcript, transcript, sizeof(task->transcript) - 1);
    task->transcript[sizeof(task->transcript) - 1] = '\0';
    task->stream_cb = stream_cb;
    task->final_cb = final_cb;
    task->user_data = user_data;

    pthread_t tid;
    pthread_create(&tid, NULL, stream_analysis_thread, task);
    pthread_detach(tid);
    return 0;
}

/****************************************************************************
 * Public API: Summarize log / notes content (sync)
 ****************************************************************************/

int llm_analyzer_summarize_log(const char *context, char *summary, size_t max_len)
{
    if (!g_enabled || !context || !summary) return -1;
    const char *model = g_config.llm_model[0] ? g_config.llm_model : DEFAULT_LLM_MODEL;
    return sf_client_chat_completion(model, SYSTEM_PROMPT_SUMMARY, context,
                                      summary, max_len, NULL);
}

/****************************************************************************
 * Public API: Translate text (sync)
 ****************************************************************************/

int llm_analyzer_translate(const char *text, translate_lang_t from, translate_lang_t to,
                            char *translated, size_t max_len)
{
    if (!g_enabled || !text || !translated) return -1;
    const char *model = g_config.llm_model[0] ? g_config.llm_model : DEFAULT_LLM_MODEL;

    char *user_msg = (char *)malloc(MAX_TRANSCRIPT_LEN + 64);
    if (!user_msg) return -1;

    const char *to_lang = "中文";
    switch (to) {
        case TRANSLATE_ZH: to_lang = "中文"; break;
        case TRANSLATE_EN: to_lang = "English"; break;
        case TRANSLATE_JA: to_lang = "日本語"; break;
        case TRANSLATE_RU: to_lang = "Русский"; break;
        default: break;
    }

    snprintf(user_msg, MAX_TRANSCRIPT_LEN + 64, "将以下文本翻译成%s：%s", to_lang, text);
    int ret = sf_client_chat_completion(model, SYSTEM_PROMPT_TRANSLATE, user_msg,
                                        translated, max_len, NULL);
    free(user_msg);
    return ret;
}

/****************************************************************************
 * Public API: Check if LLM analyzer is enabled
 ****************************************************************************/

bool llm_analyzer_is_enabled(void)
{
    return g_enabled;
}
