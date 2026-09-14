/****************************************************************************
 * daily_summary.c - Daily digest / highlight extraction
 *
 * 1. Collect all notes from today
 * 2. Send to LLM for summarization and highlight extraction
 * 3. Store results back
 ****************************************************************************/

#include "daily_summary.h"
#include "note_store.h"
#include "siliconflow_client.h"
#include "config_store.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#define SUMMARY_BUF  2048

static char g_today_summary[SUMMARY_BUF];
static bool g_summary_ready = false;

static int is_today(time_t ts)
{
    time_t now = time(NULL);
    struct tm *tnow = localtime(&now);
    struct tm *tts  = localtime(&ts);
    return (tnow->tm_year == tts->tm_year &&
            tnow->tm_mon  == tts->tm_mon  &&
            tnow->tm_mday == tts->tm_mday);
}

int daily_summary_generate(void)
{
    int count = note_store_count();
    if (count == 0) {
        snprintf(g_today_summary, sizeof(g_today_summary),
                 "今天还没有录音笔记。\n\n点击中央按钮开始记录。");
        g_summary_ready = true;
        return 0;
    }

    /* Build prompt from today's transcripts */
    char prompt[4096];
    snprintf(prompt, sizeof(prompt),
             "你是一个个人助理。请根据以下今天的语音笔记，生成一份简短的每日摘要。\n\n"
             "要求：\n"
             "1. 用2-3句话总结今天的主要活动\n"
             "2. 挑出最重要的1-2条作为「今日高光」\n"
             "3. 用中文回复\n\n"
             "今天的笔记：\n");

    int added = 0;
    for (int i = 0; i < count && added < 10; i++) {
        const note_t *n = note_store_get(i);
        if (!n || !n->transcript) continue;
        if (!is_today(n->timestamp) && i > 0) continue;

        char time_str[16];
        struct tm *tm = localtime(&n->timestamp);
        strftime(time_str, sizeof(time_str), "%H:%M", tm);

        size_t plen = strlen(prompt);
        snprintf(prompt + plen, sizeof(prompt) - plen,
                 "[%s] %s\n", time_str, n->transcript);
        added++;
    }

    /* Call LLM */
    char response[SUMMARY_BUF];
    int ret = sf_client_chat_completion(
        "Qwen/Qwen2.5-7B-Instruct",
        "你是个人助理，帮助总结一天的语音笔记。",
        prompt,
        response, sizeof(response), NULL);

    if (ret == 0 && response[0] != '\0') {
        snprintf(g_today_summary, sizeof(g_today_summary), "%s", response);
    } else {
        snprintf(g_today_summary, sizeof(g_today_summary),
                 "%d notes today.\n(LLM unavailable)", added);
    }

    g_summary_ready = true;
    return 0;
}

const char *daily_summary_get(void)
{
    return g_today_summary;
}

bool daily_summary_is_ready(void)
{
    return g_summary_ready;
}
