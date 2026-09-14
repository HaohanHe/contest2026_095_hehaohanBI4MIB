#ifndef NOTE_STORE_H
#define NOTE_STORE_H

#include <stdbool.h>
#include <time.h>

#define NOTE_MAX    128
#define NOTE_DIR    "/data/daynote"

typedef struct {
    int      id;
    time_t   timestamp;
    char     ts_str[32];
    int      duration_sec;
    char    *transcript;
    char    *summary;
    char     wav_path[256];
    bool     highlight;
} note_t;

int  note_store_init(void);
void note_store_deinit(void);
int  note_store_add(const char *wav_path, const char *transcript,
                    const char *summary, int duration_sec);
int  note_store_count(void);
const note_t *note_store_get(int id);
const note_t *note_store_get_last(void);
void note_store_set_highlight(int note_id, bool highlight);
int  note_store_get_highlights_count(void);
const note_t *note_store_get_highlight(int index);
void note_store_clear(void);

#endif
