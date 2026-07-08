#ifndef __UI_AI_RADIO_H
#define __UI_AI_RADIO_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int ui_ai_radio_init(void);
void ui_ai_radio_set_frequency(float freq_hz, const char *mode);
void ui_ai_radio_append_transcript(const char *text, bool is_partial);
void ui_ai_radio_set_analysis(const char *summary, int alert_level);
void ui_ai_radio_show_alert(bool show, int alert_level);
void ui_ai_radio_refresh(void);

#ifdef __cplusplus
}
#endif

#endif
