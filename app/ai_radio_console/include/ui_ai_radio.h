#ifndef __UI_AI_RADIO_H
#define __UI_AI_RADIO_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int  ui_ai_radio_init(void);
void ui_ai_radio_show(void);
void ui_ai_radio_hide(void);
bool ui_ai_radio_is_visible(void);
void ui_ai_radio_update_recording_state(bool recording);
void ui_ai_radio_set_result(const char *transcript, const char *summary);
void ui_ai_radio_set_status(const char *status, int color);
void ui_ai_radio_refresh(void);

#ifdef __cplusplus
}
#endif

#endif
