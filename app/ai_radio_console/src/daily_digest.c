/****************************************************************************
 * daily_digest.c - Daily digest and highlight extraction
 *
 * Generates daily summary by analyzing today's recordings.
 ****************************************************************************/

#include "daily_digest.h"
#include "memory_index.h"
#include "note_store.h"
#include "siliconflow_client.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#define DIGEST_BUF  2048

static char g_digest[DIGEST_BUF];
static bool g_digest_ready = false;

int daily_digest_generate(void)
{
    index_entry_t *results[32];
    int count = memory_index_get_today(results, 32);
    if (count == 0) {
        snprintf(g_digest, sizeof(g_digest),
                 "今天还没有录音笔记。\n\n点击中央按钮或等待自动录音。");
        g_digest_ready = true;
        return 0;
    }

    /* Build prompt from today's notes */
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
        index_entry_t *e = results[i];
        const note_t *note = note_store_get(e->note_id);
        if (!note || !note->transcript) continue;

        char time_str[16];
        struct tm *tm = localtime(&note->timestamp);
        strftime(time_str, sizeof(time_str), "%H:%M", tm);

        size_t plen = strlen(prompt);
        snprintf(prompt + plen, sizeof(prompt) - plen,
                 "[%s] %s\n", time_str, note->transcript);
        added++;
    }

    /* Call LLM */
    char response[DIGEST_BUF];
    int ret = sf_client_chat_completion(
        "Qwen/Qwen2.5-7B-Instruct",
        "你是个人助理，帮助总结一天的语音笔记。",
        prompt,
        response, sizeof(response), NULL);

    if (ret == 0 && response[0] != '\0') {
        snprintf(g_digest, sizeof(g_digest), "%s", response);
    } else {
        snprintf(g_digest, sizeof(g_digest),
                 "今天有 %d 条录音笔记。\n(LLM unavailable)", count);
    }

    g_digest_ready = true;
    return 0;
}

const char *daily_digest_get(void)
{
    return g_digest;
}

bool daily_digest_is_ready(void)
{
    return g_digest_ready;
}

void daily_digest_refresh(void)
{
    g_digest_ready = false;
    daily_digest_generate();
}
