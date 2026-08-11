/****************************************************************************
 * ui_daynote.c - DayNote UI for 320x240 screen
 *
 * Main screen layout:
 *   [Time | Note count]     <- top bar 26px
 *   [    RECORD BUTTON    ] <- 40px high, centered
 *   [  Last Note Title     ] <- 20px
 *   [  Transcript text...  ] <- 70px
 *   [  AI Summary...       ] <- 50px
 *   [ENTER:Rec HOME(L):Set] <- bottom hint
 ****************************************************************************/

#include "ui_daynote.h"
#include "note_store.h"
#include "daily_summary.h"
#include "memory_index.h"
#include <lvgl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

extern const lv_font_t ai_radio_font;

#define LCD_W   320
#define LCD_H  240

#define COLOR_BG          lv_color_hex(0x0a0e1a)
#define COLOR_BAR         lv_color_hex(0x0d1220)
#define COLOR_TEXT        lv_color_hex(0xe0e8f0)
#define COLOR_TEXT_DIM    lv_color_hex(0x8899aa)
#define COLOR_ACCENT      lv_color_hex(0x00d4ff)
#define COLOR_GREEN       lv_color_hex(0x33dd66)
#define COLOR_RED         lv_color_hex(0xff3344)
#define COLOR_AMBER       lv_color_hex(0xffaa00)
#define COLOR_CARD        lv_color_hex(0x141c30)
#define COLOR_CARD_BORDER lv_color_hex(0x253048)

static lv_obj_t *g_screen = NULL;
static lv_obj_t *g_time_label = NULL;
static lv_obj_t *g_note_count_label = NULL;
static lv_obj_t *g_record_btn = NULL;
static lv_obj_t *g_record_label = NULL;
static lv_obj_t *g_last_title = NULL;
static lv_obj_t *g_transcript_label = NULL;
static lv_obj_t *g_summary_label = NULL;
static lv_obj_t *g_hint_label = NULL;
static lv_obj_t *g_main_scr = NULL;
static bool g_visible = false;

int ui_daynote_init(void)
{
    g_screen = lv_obj_create(NULL);
    if (!g_screen) return -1;

    lv_obj_set_size(g_screen, LCD_W, LCD_H);
    lv_obj_set_style_bg_color(g_screen, COLOR_BG, 0);
    lv_obj_set_style_pad_all(g_screen, 0, 0);
    lv_obj_set_style_radius(g_screen, 0, 0);
    lv_obj_set_style_border_width(g_screen, 0, 0);
    lv_obj_set_style_text_font(g_screen, &ai_radio_font, 0);

    /* Top bar */
    lv_obj_t *topbar = lv_obj_create(g_screen);
    lv_obj_set_size(topbar, LCD_W, 26);
    lv_obj_set_pos(topbar, 0, 0);
    lv_obj_set_style_bg_color(topbar, COLOR_BAR, 0);
    lv_obj_set_style_border_width(topbar, 0, 0);
    lv_obj_set_style_pad_all(topbar, 0, 0);
    lv_obj_set_style_radius(topbar, 0, 0);

    g_time_label = lv_label_create(topbar);
    lv_label_set_text(g_time_label, "00:00");
    lv_obj_set_style_text_color(g_time_label, COLOR_TEXT, 0);
    lv_obj_set_pos(g_time_label, 6, 5);

    g_note_count_label = lv_label_create(topbar);
    lv_label_set_text(g_note_count_label, "0 notes");
    lv_obj_set_style_text_color(g_note_count_label, COLOR_TEXT_DIM, 0);
    lv_obj_align(g_note_count_label, LV_ALIGN_RIGHT_MID, -6, 0);

    /* Record button */
    g_record_btn = lv_obj_create(g_screen);
    lv_obj_set_size(g_record_btn, 100, 40);
    lv_obj_set_pos(g_record_btn, (LCD_W - 100) / 2, 34);
    lv_obj_set_style_bg_color(g_record_btn, COLOR_CARD, 0);
    lv_obj_set_style_border_color(g_record_btn, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(g_record_btn, 1, 0);
    lv_obj_set_style_radius(g_record_btn, 8, 0);
    lv_obj_add_flag(g_record_btn, LV_OBJ_FLAG_CLICKABLE);

    g_record_label = lv_label_create(g_record_btn);
    lv_label_set_text(g_record_label, "REC");
    lv_obj_set_style_text_color(g_record_label, COLOR_ACCENT, 0);
    lv_obj_center(g_record_label);

    /* Last note title */
    g_last_title = lv_label_create(g_screen);
    lv_label_set_text(g_last_title, "No notes yet");
    lv_obj_set_style_text_color(g_last_title, COLOR_TEXT_DIM, 0);
    lv_obj_set_pos(g_last_title, 6, 82);

    /* Transcript area */
    lv_obj_t *trans_panel = lv_obj_create(g_screen);
    lv_obj_set_size(trans_panel, LCD_W - 8, 70);
    lv_obj_set_pos(trans_panel, 4, 100);
    lv_obj_set_style_bg_color(trans_panel, COLOR_CARD, 0);
    lv_obj_set_style_border_color(trans_panel, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(trans_panel, 1, 0);
    lv_obj_set_style_pad_all(trans_panel, 4, 0);
    lv_obj_set_style_radius(trans_panel, 4, 0);

    g_transcript_label = lv_label_create(trans_panel);
    lv_label_set_text(g_transcript_label, "Tap REC to start");
    lv_obj_set_style_text_color(g_transcript_label, COLOR_TEXT_DIM, 0);
    lv_label_set_long_mode(g_transcript_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_size(g_transcript_label, LCD_W - 16, 60);

    /* AI Summary area */
    lv_obj_t *sum_panel = lv_obj_create(g_screen);
    lv_obj_set_size(sum_panel, LCD_W - 8, 50);
    lv_obj_set_pos(sum_panel, 4, 174);
    lv_obj_set_style_bg_color(sum_panel, COLOR_BAR, 0);
    lv_obj_set_style_border_color(sum_panel, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(sum_panel, 1, 0);
    lv_obj_set_style_radius(sum_panel, 4, 0);
    lv_obj_set_style_pad_all(sum_panel, 4, 0);

    g_summary_label = lv_label_create(sum_panel);
    lv_label_set_text(g_summary_label, "Daily summary loading...");
    lv_obj_set_style_text_color(g_summary_label, COLOR_TEXT_DIM, 0);
    lv_label_set_long_mode(g_summary_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_size(g_summary_label, LCD_W - 16, 40);

    /* Bottom hint */
    g_hint_label = lv_label_create(g_screen);
    lv_label_set_text(g_hint_label, "ENTER:Rec  HOME(L):Settings");
    lv_obj_set_style_text_color(g_hint_label, COLOR_TEXT_DIM, 0);
    lv_obj_align(g_hint_label, LV_ALIGN_BOTTOM_MID, 0, 0);

    g_main_scr = lv_scr_act();

    /* Update note count */
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d notes", note_store_count());
        lv_label_set_text(g_note_count_label, buf);
    }

    return 0;
}

void ui_daynote_show(void)
{
    if (!g_screen) return;
    if (g_main_scr == NULL) g_main_scr = lv_scr_act();
    lv_scr_load(g_screen);
    g_visible = true;
}

void ui_daynote_hide(void)
{
    if (!g_main_scr || !g_visible) return;
    lv_scr_load(g_main_scr);
    g_visible = false;
}

bool ui_daynote_is_visible(void)
{
    return g_visible;
}

void ui_daynote_update_recording_state(bool recording)
{
    if (!g_record_label) return;
    if (recording) {
        lv_label_set_text(g_record_label, "STOP");
        lv_obj_set_style_text_color(g_record_label, COLOR_RED, 0);
    } else {
        lv_label_set_text(g_record_label, "REC");
        lv_obj_set_style_text_color(g_record_label, COLOR_ACCENT, 0);
    }
}

void ui_daynote_set_result(const char *transcript, const char *summary)
{
    if (g_transcript_label && transcript) {
        lv_label_set_text(g_transcript_label, transcript);
    }
    if (g_summary_label && summary) {
        lv_label_set_text(g_summary_label, summary);
    }
    if (g_note_count_label) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d notes", note_store_count());
        lv_label_set_text(g_note_count_label, buf);
    }
}

lv_obj_t *ui_daynote_get_count_label(void)
{
    return g_note_count_label;
}

lv_obj_t *ui_daynote_get_screen(void)
{
    return g_screen;
}
