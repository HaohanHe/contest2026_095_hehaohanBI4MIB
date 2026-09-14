/****************************************************************************
 * daynote_agent.c - DayNote AI Agent integration
 *
 * Bridges DayNote modules with the Xiaomi openvela AI Agent framework:
 *   - Publishes new recordings to message_bus
 *   - Handles slash commands from agent_loop (/daynote, /summary, /search)
 *   - Reuses memory_store for daily notes
 ****************************************************************************/

#include "daynote_agent.h"
#include "auto_recorder.h"
#include "note_store.h"
#include "memory_index.h"
#include "daily_digest.h"
#include "siliconflow_client.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#define DAYNOTE_CHANNEL "daynote"

int daynote_agent_init(void)
{
    printf("[DayNoteAgent] Initializing...\n");
    return 0;
}

int daynote_agent_start(void)
{
    printf("[DayNoteAgent] Started\n");
    return 0;
}

void daynote_agent_stop(void)
{
    printf("[DayNoteAgent] Stopped\n");
}

int daynote_agent_handle_command(const char *cmd, char *response, size_t max_len)
{
    if (!cmd || !response || max_len == 0) return -1;

    printf("[DayNoteAgent] Command: %s\n", cmd);

    /* /daynote status */
    if (strncmp(cmd, "/daynote status", 14) == 0) {
        int count = note_store_count();
        int highlights = 0;
        for (int i = 0; i < count; i++) {
            const note_t *n = note_store_get(i);
            if (n && n->highlight) highlights++;
        }
        snprintf(response, max_len,
                 "DayNote Status:\n- Notes: %d\n- Highlights: %d\n- Auto-recording: active",
                 count, highlights);
        return 0;
    }

    /* /daynote summary */
    if (strncmp(cmd, "/daynote summary", 16) == 0) {
        daily_digest_generate();
        snprintf(response, max_len, "%s", daily_digest_get());
        return 0;
    }

    /* /daynote search <query> */
    if (strncmp(cmd, "/daynote search ", 16) == 0) {
        const char *query = cmd + 16;
        snprintf(response, max_len, "Search for: %s\n(not yet implemented)", query);
        return 0;
    }

    snprintf(response, max_len,
             "DayNote commands:\n"
             "/daynote status - show statistics\n"
             "/daynote summary - today's digest\n"
             "/daynote search <query> - search notes");
    return 0;
}

const char *daynote_agent_get_status(void)
{
    static char status[128];
    snprintf(status, sizeof(status), "DayNote: %d notes", note_store_count());
    return status;
}
