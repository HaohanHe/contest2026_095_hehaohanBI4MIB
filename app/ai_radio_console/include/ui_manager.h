#ifndef __UI_MANAGER_H
#define __UI_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "radio_config.h"

typedef enum {
    UI_SCREEN_MAIN = 0,
    UI_SCREEN_CW,
    UI_SCREEN_LOG,
    UI_SCREEN_ALERTS,
    UI_SCREEN_SETTINGS,
    UI_SCREEN_FREQ,
    UI_SCREEN_TRANSLATE,
    UI_SCREEN_MAX
} ui_screen_t;

int ui_manager_init(void);
int ui_manager_deinit(void);

int ui_manager_update(void);
int ui_manager_switch_screen(ui_screen_t screen);
ui_screen_t ui_manager_get_current_screen(void);

int ui_manager_set_frequency(float freq_mhz);
int ui_manager_set_mode(radio_mode_t mode);
int ui_manager_set_signal(float rssi, float snr);
int ui_manager_set_cw_text(const char *text);
int ui_manager_set_alert(const alert_event_t *alert);
int ui_manager_add_qso_entry(const qso_entry_t *qso);
int ui_manager_set_translate(const char *original, const char *translated);
int ui_manager_set_freq_recommend(const freq_recommendation_t *recs, size_t count);
int ui_manager_set_status_text(const char *text);
int ui_manager_set_net_summary(const char *summary);

int ui_manager_play_alert_tone(alert_level_t level);

#endif
