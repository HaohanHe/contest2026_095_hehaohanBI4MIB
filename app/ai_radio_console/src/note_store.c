/****************************************************************************
 * note_store.c - Simple note storage for DayNote
 *
 * Each note: WAV file + JSON metadata on /data/daynote/
 ****************************************************************************/

#include "note_store.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

#define NOTE_DIR       "/data/daynote"
#define NOTE_MAX       128
#define META_PATH      NOTE_DIR "/index.json"

static note_t g_notes[NOTE_MAX];
static int g_note_count = 0;

static int  note_store_save(void);
static int  note_store_load(void);

static void ensure_dir(void)
{
    struct stat st;
    if (stat(NOTE_DIR, &st) != 0) {
        mkdir(NOTE_DIR, 0755);
    }
}

int note_store_init(void)
{
    ensure_dir();
    return note_store_load();
}

void note_store_deinit(void)
{
    note_store_save();
}

int note_store_add(const char *wav_path, const char *transcript,
                   const char *summary, int duration_sec)
{
    if (g_note_count >= NOTE_MAX) return -1;

    note_t *n = &g_notes[g_note_count];
    time_t now = time(NULL);
    n->id = (int)now;
    n->timestamp = now;
    n->duration_sec = duration_sec;
    n->transcript = transcript ? strdup(transcript) : NULL;
    n->summary = summary ? strdup(summary) : NULL;
    snprintf(n->wav_path, sizeof(n->wav_path), "%s", wav_path);
    n->highlight = false;

    g_note_count++;
    note_store_save();
    return n->id;
}

int note_store_count(void)
{
    return g_note_count;
}

const note_t *note_store_get(int id)
{
    for (int i = 0; i < g_note_count; i++) {
        if (g_notes[i].id == id) {
            return &g_notes[i];
        }
    }
    return NULL;
}

const note_t *note_store_get_last(void)
{
    if (g_note_count <= 0) return NULL;
    return &g_notes[g_note_count - 1];
}

void note_store_set_highlight(int note_id, bool highlight)
{
    for (int i = 0; i < g_note_count; i++) {
        if (g_notes[i].id == note_id) {
            g_notes[i].highlight = highlight;
            break;
        }
    }
    note_store_save();
}

int note_store_get_highlights_count(void)
{
    int c = 0;
    for (int i = 0; i < g_note_count; i++) {
        if (g_notes[i].highlight) c++;
    }
    return c;
}

const note_t *note_store_get_highlight(int index)
{
    int found = 0;
    for (int i = 0; i < g_note_count; i++) {
        if (g_notes[i].highlight) {
            if (found == index) return &g_notes[i];
            found++;
        }
    }
    return NULL;
}

void note_store_clear(void)
{
    g_note_count = 0;
    memset(g_notes, 0, sizeof(g_notes));
    note_store_save();
}

int note_store_save(void)
{
    ensure_dir();
    FILE *f = fopen(META_PATH, "w");
    if (!f) return -1;

    fprintf(f, "[\n");
    for (int i = 0; i < g_note_count; i++) {
        note_t *n = &g_notes[i];
        struct tm *tm = localtime(&n->timestamp);
        char ts[32];
        strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", tm);

        fprintf(f, "  {\"id\":%d,\"ts\":\"%s\",\"dur\":%d,\"highlight\":%s",
                n->id, ts, n->duration_sec, n->highlight ? "true" : "false");
        if (n->transcript) {
            fprintf(f, ",\"txt\":\"");
            for (int j = 0; n->transcript[j]; j++) {
                char c = n->transcript[j];
                if (c == '"' || c == '\\') fputc('\\', f);
                fputc(c, f);
            }
            fprintf(f, "\"");
        }
        if (n->summary) {
            fprintf(f, ",\"sum\":\"");
            for (int j = 0; n->summary[j]; j++) {
                char c = n->summary[j];
                if (c == '"' || c == '\\') fputc('\\', f);
                fputc(c, f);
            }
            fprintf(f, "\"");
        }
        fprintf(f, ",\"wav\":\"%s\"", n->wav_path);
        fprintf(f, "}");
        if (i < g_note_count - 1) fprintf(f, ",");
        fprintf(f, "\n");
    }
    fprintf(f, "]\n");
    fclose(f);
    return 0;
}

int note_store_load(void)
{
    ensure_dir();
    FILE *f = fopen(META_PATH, "r");
    if (!f) return 0;

    g_note_count = 0;
    memset(g_notes, 0, sizeof(g_notes));

    char line[1024];
    while (fgets(line, sizeof(line), f) && g_note_count < NOTE_MAX) {
        note_t *n = &g_notes[g_note_count];

        char *p = strstr(line, "\"id\":");
        if (p) n->id = atoi(p + 5);

        p = strstr(line, "\"ts\":\"");
        if (p) {
            memcpy(n->ts_str, p + 6, 19);
            n->ts_str[19] = '\0';
            struct tm tm = {0};
            strptime(n->ts_str, "%Y-%m-%dT%H:%M:%S", &tm);
            n->timestamp = mktime(&tm);
        }

        p = strstr(line, "\"dur\":");
        if (p) n->duration_sec = atoi(p + 6);

        p = strstr(line, "\"highlight\":");
        if (p) n->highlight = (strstr(p, "true") != NULL);

        p = strstr(line, "\"wav\":\"");
        if (p) {
            char *end = strchr(p + 7, '"');
            if (end) {
                size_t len = (size_t)(end - (p + 7));
                if (len >= sizeof(n->wav_path)) len = sizeof(n->wav_path) - 1;
                memcpy(n->wav_path, p + 7, len);
                n->wav_path[len] = '\0';
            }
        }

        p = strstr(line, "\"txt\":\"");
        if (p) {
            char *end = strchr(p + 7, '"');
            if (end) {
                size_t len = (size_t)(end - (p + 7));
                n->transcript = malloc(len + 1);
                memcpy(n->transcript, p + 7, len);
                n->transcript[len] = '\0';
            }
        }

        p = strstr(line, "\"sum\":\"");
        if (p) {
            char *end = strchr(p + 7, '"');
            if (end) {
                size_t len = (size_t)(end - (p + 7));
                n->summary = malloc(len + 1);
                memcpy(n->summary, p + 7, len);
                n->summary[len] = '\0';
            }
        }

        g_note_count++;
    }

    fclose(f);
    return g_note_count;
}
