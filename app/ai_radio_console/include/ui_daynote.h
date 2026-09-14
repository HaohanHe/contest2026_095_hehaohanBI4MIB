#ifndef __UI_DAYNOTE_H
#define __UI_DAYNOTE_H

#include <stdbool.h>
#include <stdint.h>
#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

int  ui_daynote_init(void);
void ui_daynote_show(void);
void ui_daynote_hide(void);
bool ui_daynote_is_visible(void);
void ui_daynote_update_recording_state(bool recording);
void ui_daynote_set_result(const char *transcript, const char *summary);
void ui_daynote_refresh(void);
lv_obj_t *ui_daynote_get_count_label(void);
lv_obj_t *ui_daynote_get_screen(void);

#ifdef __cplusplus
}
#endif

#endif
