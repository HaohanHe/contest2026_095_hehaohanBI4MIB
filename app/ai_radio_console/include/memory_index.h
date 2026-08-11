#ifndef MEMORY_INDEX_H
#define MEMORY_INDEX_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int      note_id;
    time_t   timestamp;
    char     ts_str[32];
    char     keywords[128];
    bool     highlight;
    float    importance;
} index_entry_t;

int  memory_index_add(int note_id, time_t ts, const char *keywords, bool highlight, float importance);
int  memory_index_search(const char *query, index_entry_t **results, int max_results);
int  memory_index_get_highlights(index_entry_t **results, int max_results);
int  memory_index_get_today(index_entry_t **results, int max_results);
int  memory_index_save(void);
int  memory_index_load(void);
int  memory_index_count(void);

#endif
