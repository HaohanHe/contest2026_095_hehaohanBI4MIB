/****************************************************************************
 * apps/vendor/allwinnertech/apps/ai_radio_console/radio_ui.h
 *
 * LVGL UI definitions for AI Radio Console
 ****************************************************************************/

#ifndef __AI_RADIO_UI_H
#define __AI_RADIO_UI_H

#include <lvgl/lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

/* UI state flags */
typedef enum {
    RADIO_UI_IDLE = 0,
    RADIO_UI_RECORDING,
    RADIO_UI_TRANSCRIBING,
    RADIO_UI_THINKING,
    RADIO_UI_ERROR
} radio_ui_state_t;

/* Initialize LVGL and create the main screen */
int radio_ui_init(void);

/* Main UI handler loop (blocking) */
void radio_ui_run(void);

/* Set the recording/think state shown on the status bar */
void radio_ui_set_state(radio_ui_state_t state);

/* Append a log entry to the on-screen log list */
void radio_ui_add_log(const char *timestamp, const char *text);

/* Update the latest transcription line */
void radio_ui_set_transcript(const char *text);

/* Update status message (network, errors) */
void radio_ui_set_status(const char *text);

/* Return true if the on-screen PTT/Record button is currently pressed */
bool radio_ui_is_ptt_pressed(void);

#ifdef __cplusplus
}
#endif

#endif /* __AI_RADIO_UI_H */
