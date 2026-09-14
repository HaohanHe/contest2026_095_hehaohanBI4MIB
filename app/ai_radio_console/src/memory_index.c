/****************************************************************************
 * memory_index.c - HippocampusIndex extension for DayNote
 *
 * Extends ai_agent's memory_store with:
 * - Keyword extraction
 * - Timeline index
 * - Highlight marking
 * - Search by keyword/time/importance
 ****************************************************************************/

#include "memory_index.h"
#include "note_store.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>

#define INDEX_MAX   256
#define INDEX_PATH  "/data/daynote/hippocampus.json"

static index_entry_t g_index[INDEX_MAX];
static int g_index_count = 0;
static pthread_mutex_t g_index_mutex = PTHREAD_MUTEX_INITIALIZER;

static void ensure_dir(void)
{
    struct stat st;
    if (stat("/data/daynote", &st) != 0) {
        mkdir("/data/daynote", 0755);
    }
}

int memory_index_add(int note_id, time_t ts, const char *keywords, bool highlight, float importance)
{
    pthread_mutex_lock(&g_index_mutex);
    if (g_index_count >= INDEX_MAX) {
        pthread_mutex_unlock(&g_index_mutex);
        return -1;
    }

    index_entry_t *e = &g_index[g_index_count++];
    e->note_id = note_id;
    e->timestamp = ts;
    strncpy(e->ts_str, ctime(&ts), sizeof(e->ts_str) - 1);
    e->ts_str[sizeof(e->ts_str) - 1] = '\0';
    strncpy(e->keywords, keywords ? keywords : "", sizeof(e->keywords) - 1);
    e->keywords[sizeof(e->keywords) - 1] = '\0';
    e->highlight = highlight;
    e->importance = importance;

    pthread_mutex_unlock(&g_index_mutex);
    memory_index_save();
    return 0;
}

int memory_index_search(const char *query, index_entry_t **results, int max_results)
{
    if (!query || !results || max_results <= 0) return 0;

    pthread_mutex_lock(&g_index_mutex);
    int count = 0;
    for (int i = 0; i < g_index_count && count < max_results; i++) {
        if (strstr(g_index[i].keywords, query) != NULL ||
            strstr(g_index[i].ts_str, query) != NULL) {
            results[count++] = &g_index[i];
        }
    }
    pthread_mutex_unlock(&g_index_mutex);
    return count;
}

int memory_index_get_highlights(index_entry_t **results, int max_results)
{
    pthread_mutex_lock(&g_index_mutex);
    int count = 0;
    for (int i = 0; i < g_index_count && count < max_results; i++) {
        if (g_index[i].highlight) {
            results[count++] = &g_index[i];
        }
    }
    pthread_mutex_unlock(&g_index_mutex);
    return count;
}

int memory_index_get_today(index_entry_t **results, int max_results)
{
    pthread_mutex_lock(&g_index_mutex);
    int count = 0;
    time_t now = time(NULL);
    struct tm *today = localtime(&now);

    for (int i = 0; i < g_index_count && count < max_results; i++) {
        struct tm *tm = localtime(&g_index[i].timestamp);
        if (tm->tm_year == today->tm_year &&
            tm->tm_mon == today->tm_mon &&
            tm->tm_mday == today->tm_mday) {
            results[count++] = &g_index[i];
        }
    }
    pthread_mutex_unlock(&g_index_mutex);
    return count;
}

int memory_index_save(void)
{
    ensure_dir();
    FILE *f = fopen(INDEX_PATH, "w");
    if (!f) return -1;

    fprintf(f, "[\n");
    for (int i = 0; i < g_index_count; i++) {
        index_entry_t *e = &g_index[i];
        fprintf(f, "  {\"id\":%d,\"ts\":\"%s\",\"kw\":\"%s\",\"hl\":%s,\"imp\":%.2f}",
                e->note_id, e->ts_str, e->keywords,
                e->highlight ? "true" : "false", e->importance);
        if (i < g_index_count - 1) fprintf(f, ",\n");
        else fprintf(f, "\n");
    }
    fprintf(f, "]\n");
    fclose(f);
    return 0;
}

int memory_index_load(void)
{
    ensure_dir();
    FILE *f = fopen(INDEX_PATH, "r");
    if (!f) return 0;

    g_index_count = 0;
    memset(g_index, 0, sizeof(g_index));

    char line[512];
    while (fgets(line, sizeof(line), f) && g_index_count < INDEX_MAX) {
        index_entry_t *e = &g_index[g_index_count];

        char *p = strstr(line, "\"id\":");
        if (p) e->note_id = atoi(p + 5);

        p = strstr(line, "\"kw\":\"");
        if (p) {
            char *end = strchr(p + 6, '"');
            if (end) {
                size_t len = (size_t)(end - (p + 6));
                if (len >= sizeof(e->keywords)) len = sizeof(e->keywords) - 1;
                memcpy(e->keywords, p + 6, len);
                e->keywords[len] = '\0';
            }
        }

        p = strstr(line, "\"hl\":");
        if (p) e->highlight = (strstr(p, "true") != NULL);

        p = strstr(line, "\"imp\":");
        if (p) e->importance = (float)atof(p + 5);

        g_index_count++;
    }

    fclose(f);
    return g_index_count;
}

int memory_index_count(void)
{
    pthread_mutex_lock(&g_index_mutex);
    int count = g_index_count;
    pthread_mutex_unlock(&g_index_mutex);
    return count;
}
