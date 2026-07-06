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

static const char *SYSTEM_PROMPT_ANALYSIS =
    "你是一名专业的无线电通联内容分析安全员，负责分析业余无线电通联的语音转写文本。\n"
    "请严格按照指定JSON格式返回分析结果，不要输出其他内容。\n"
    "分析维度：\n"
    "1. 是否包含求救信号（MAYDAY、SOS、求救、遇险、紧急医疗等）\n"
    "2. 是否包含违规内容（脏话、非法交易、未持证通联、恶意干扰、政治敏感、违法信息）\n"
    "3. 提取呼号、关键词\n"
    "4. 给出简短中文摘要\n"
    "返回格式：{\"mayday\":true/false,\"violation\":\"none/profanity/illegal/unlicensed/interference/other\",\"confidence\":0-1,\"summary\":\"摘要\",\"keywords\":\"关键词\",\"callsigns\":\"呼号\"}";

static const char *SYSTEM_PROMPT_TRANSLATE =
    "你是一名无线电通联实时翻译员。将用户输入的通联文本翻译成目标语言，只输出翻译结果，不要解释。";

static const char *SYSTEM_PROMPT_SUMMARY =
    "你是一名无线电通联日志记录员。请将以下通联内容整理为简洁的中文QSO日志摘要，包含：时间、频率、呼号、信号报告、通联内容要点。";

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

static int parse_analysis_result(const char *response, analysis_result_t *result)
{
    memset(result, 0, sizeof(*result));
    result->type = ANALYSIS_NORMAL_QSO;

    char mayday_str[16] = {0};
    char viol_str[64] = {0};
    float conf = 0.5f;

    const char *p;
    p = strstr(response, "\"mayday\"");
    if (p) {
        p = strchr(p, ':');
        if (p) {
            p++;
            while (*p == ' ' || *p == '\t' || *p == '\n') p++;
            if (strncmp(p, "true", 4) == 0) {
                result->type = ANALYSIS_MAYDAY_DETECTED;
                result->alert_level = ALERT_LEVEL_MAYDAY;
                result->needs_alert = true;
            }
        }
    }

    p = strstr(response, "\"violation\"");
    if (p) {
        p = strchr(p, ':');
        if (p) {
            p++;
            while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '"') p++;
            const char *end = strchr(p, '"');
            if (end) {
                size_t len = end - p;
                if (len >= sizeof(viol_str)) len = sizeof(viol_str) - 1;
                strncpy(viol_str, p, len);
                viol_str[len] = '\0';

                if (strcmp(viol_str, "profanity") == 0) result->violation = VIOLATION_PROFANITY;
                else if (strcmp(viol_str, "illegal") == 0) result->violation = VIOLATION_ILLEGAL_CONTENT;
                else if (strcmp(viol_str, "unlicensed") == 0) result->violation = VIOLATION_UNLICENSED;
                else if (strcmp(viol_str, "interference") == 0) result->violation = VIOLATION_INTERFERENCE;
                else if (strcmp(viol_str, "other") == 0) result->violation = VIOLATION_OTHER;

                if (result->violation != VIOLATION_NONE) {
                    result->type = ANALYSIS_VIOLATION_DETECTED;
                    result->alert_level = ALERT_LEVEL_WARNING;
                    result->needs_alert = true;
                }
            }
        }
    }

    p = strstr(response, "\"confidence\"");
    if (p) {
        p = strchr(p, ':');
        if (p) conf = strtof(p + 1, NULL);
    }
    result->confidence = conf;

    json_extract_string(response, "summary", result->summary, sizeof(result->summary));
    json_extract_string(response, "keywords", result->keywords, sizeof(result->keywords));
    json_extract_string(response, "callsigns", result->callsigns, sizeof(result->callsigns));

    if (result->type == ANALYSIS_NORMAL_QSO && result->summary[0] == '\0') {
        result->type = ANALYSIS_NO_CONTENT;
    }

    return 0;
}

typedef struct {
    char transcript[MAX_TRANSCRIPT_LEN];
    float frequency;
    char mode[16];
    llm_analysis_cb_t cb;
    void *user_data;
} analysis_task_t;

typedef struct {
    char transcript[MAX_TRANSCRIPT_LEN];
    float frequency;
    char mode[16];
    llm_analysis_stream_cb_t stream_cb;
    llm_analysis_cb_t final_cb;
    void *user_data;
    char response[MAX_LLM_RESPONSE_LEN];
    size_t response_len;
} stream_analysis_task_t;

static void *analysis_thread(void *arg)
{
    analysis_task_t *task = (analysis_task_t *)arg;
    if (!task) return NULL;

    const char *model = g_config.llm_model[0] ? g_config.llm_model : DEFAULT_LLM_MODEL;

    char user_msg[MAX_TRANSCRIPT_LEN + 128];
    snprintf(user_msg, sizeof(user_msg),
        "当前频率：%.3f MHz，模式：%s\n"
        "通联转写文本：%s\n"
        "请分析此内容。",
        task->frequency / 1000000.0f, task->mode, task->transcript);

    char response[MAX_LLM_RESPONSE_LEN];
    float conf = 0;
    int ret = sf_client_chat_completion(model, SYSTEM_PROMPT_ANALYSIS, user_msg,
                                         response, sizeof(response), &conf);

    analysis_result_t result;
    if (ret == 0) {
        parse_analysis_result(response, &result);
    } else {
        memset(&result, 0, sizeof(result));
        result.type = ANALYSIS_NO_CONTENT;
        result.alert_level = ALERT_LEVEL_NONE;
        snprintf(result.summary, sizeof(result.summary), "[LLM调用失败: %s]",
                 sf_client_get_last_error());
    }

    if (task->cb) {
        task->cb(&result, task->user_data);
    }

    free(task);
    return NULL;
}

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

    char user_msg[MAX_TRANSCRIPT_LEN + 128];
    snprintf(user_msg, sizeof(user_msg),
        "当前频率：%.3f MHz，模式：%s\n"
        "通联转写文本：%s\n"
        "请分析此内容。",
        task->frequency / 1000000.0f, task->mode, task->transcript);

    int ret = sf_client_chat_completion_stream(model, SYSTEM_PROMPT_ANALYSIS, user_msg,
                                                stream_analysis_chunk_cb, task);
    if (ret != 0) {
        analysis_result_t result;
        memset(&result, 0, sizeof(result));
        result.type = ANALYSIS_NO_CONTENT;
        result.alert_level = ALERT_LEVEL_NONE;
        snprintf(result.summary, sizeof(result.summary), "[LLM流式调用失败: %s]",
                 sf_client_get_last_error());
        if (task->final_cb) {
            task->final_cb(&result, task->user_data);
        }
        free(task);
    }
    return NULL;
}

int llm_analyzer_analyze_transcript(const char *transcript, float frequency,
                                     const char *mode, llm_analysis_cb_t cb, void *user_data)
{
    if (!g_enabled || !transcript) return -1;

    analysis_task_t *task = (analysis_task_t *)malloc(sizeof(analysis_task_t));
    if (!task) return -1;

    strncpy(task->transcript, transcript, sizeof(task->transcript) - 1);
    task->transcript[sizeof(task->transcript) - 1] = '\0';
    task->frequency = frequency;
    strncpy(task->mode, mode ? mode : "USB", sizeof(task->mode) - 1);
    task->cb = cb;
    task->user_data = user_data;

    pthread_t tid;
    pthread_create(&tid, NULL, analysis_thread, task);
    pthread_detach(tid);
    return 0;
}

int llm_analyzer_analyze_transcript_stream(const char *transcript, float frequency,
                                            const char *mode,
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
    task->frequency = frequency;
    strncpy(task->mode, mode ? mode : "USB", sizeof(task->mode) - 1);
    task->mode[sizeof(task->mode) - 1] = '\0';
    task->stream_cb = stream_cb;
    task->final_cb = final_cb;
    task->user_data = user_data;

    pthread_t tid;
    pthread_create(&tid, NULL, stream_analysis_thread, task);
    pthread_detach(tid);
    return 0;
}

int llm_analyzer_summarize_log(const char *context, char *summary, size_t max_len)
{
    if (!g_enabled || !context || !summary) return -1;
    const char *model = g_config.llm_model[0] ? g_config.llm_model : DEFAULT_LLM_MODEL;
    return sf_client_chat_completion(model, SYSTEM_PROMPT_SUMMARY, context,
                                      summary, max_len, NULL);
}

int llm_analyzer_translate(const char *text, translate_lang_t from, translate_lang_t to,
                            char *translated, size_t max_len)
{
    if (!g_enabled || !text || !translated) return -1;
    const char *model = g_config.llm_model[0] ? g_config.llm_model : DEFAULT_LLM_MODEL;

    char user_msg[MAX_TRANSCRIPT_LEN + 64];
    const char *to_lang = "中文";
    const char *from_lang = "原文";
    switch (to) {
        case TRANSLATE_ZH: to_lang = "中文"; break;
        case TRANSLATE_EN: to_lang = "English"; break;
        case TRANSLATE_JA: to_lang = "日本語"; break;
        case TRANSLATE_RU: to_lang = "Русский"; break;
        default: break;
    }
    snprintf(user_msg, sizeof(user_msg), "将以下文本翻译成%s：%s", to_lang, text);
    return sf_client_chat_completion(model, SYSTEM_PROMPT_TRANSLATE, user_msg,
                                      translated, max_len, NULL);
}

bool llm_analyzer_is_enabled(void)
{
    return g_enabled;
}

const char *llm_violation_type_str(violation_type_t type)
{
    switch (type) {
        case VIOLATION_NONE: return "无违规";
        case VIOLATION_PROFANITY: return "粗俗语言";
        case VIOLATION_ILLEGAL_CONTENT: return "非法内容";
        case VIOLATION_UNLICENSED: return "未持证通联";
        case VIOLATION_INTERFERENCE: return "恶意干扰";
        case VIOLATION_OTHER: return "其他违规";
        default: return "未知";
    }
}
