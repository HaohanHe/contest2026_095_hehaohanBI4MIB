#include "ui_ai_radio.h"
#include "radio_config.h"
#include <lvgl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define LCD_W               320
#define LCD_H               240

#define BANNER_H            28
#define FREQ_BAR_H          34
#define TRANSCRIPT_H        90
#define SUMMARY_H           88

#define BANNER_Y            0
#define FREQ_BAR_Y          (BANNER_Y + BANNER_H)
#define TRANSCRIPT_Y        (FREQ_BAR_Y + FREQ_BAR_H)
#define SUMMARY_Y           (TRANSCRIPT_Y + TRANSCRIPT_H)

#define COLOR_BG            lv_color_hex(0x0a0e1a)
#define COLOR_BG_ALERT      lv_color_hex(0x330000)
#define COLOR_BANNER_ALERT  lv_color_hex(0xff0000)
#define COLOR_BANNER_NORMAL lv_color_hex(0x1a1a2e)
#define COLOR_TEXT          lv_color_hex(0xe0e8f0)
#define COLOR_TEXT_DIM      lv_color_hex(0x8899aa)
#define COLOR_ACCENT        lv_color_hex(0x00d4ff)
#define COLOR_GREEN         lv_color_hex(0x33dd66)
#define COLOR_YELLOW        lv_color_hex(0xffdd33)
#define COLOR_RED           lv_color_hex(0xff3344)
#define COLOR_CARD_BORDER   lv_color_hex(0x253048)
#define COLOR_PANEL_BG      lv_color_hex(0x141c30)

#define TRANSCRIPT_BUF_LEN  1024
#define LABEL_TEXT_LEN      255

static lv_obj_t *g_screen = NULL;
static lv_obj_t *g_banner = NULL;
static lv_obj_t *g_alert_label = NULL;
static lv_obj_t *g_freq_label = NULL;
static lv_obj_t *g_mode_label = NULL;
static lv_obj_t *g_transcript_title = NULL;
static lv_obj_t *g_transcript_label = NULL;
static lv_obj_t *g_summary_title = NULL;
static lv_obj_t *g_summary_label = NULL;
static lv_obj_t *g_alert_level_label = NULL;

static char g_transcript_history[TRANSCRIPT_BUF_LEN];
static char g_partial_text[LABEL_TEXT_LEN];
static float g_current_freq_hz = 14250000.0f;
static char g_current_mode[16] = "USB";
static bool g_alert_show = false;
static int g_alert_level = ALERT_LEVEL_NONE;
static bool g_alert_flash_on = false;
static uint32_t g_last_blink_ms = 0;
static lv_obj_t *g_main_screen = NULL;
static bool g_visible = false;

static const char *alert_level_str(int level)
{
    switch (level) {
    case ALERT_LEVEL_NONE:        return "NONE";
    case ALERT_LEVEL_INFO:        return "INFO";
    case ALERT_LEVEL_WARNING:     return "WARNING";
    case ALERT_LEVEL_MAYDAY:      return "MAYDAY";
    case ALERT_LEVEL_INTERFERENCE:return "INTERFERENCE";
    case ALERT_LEVEL_MALICIOUS:   return "MALICIOUS";
    default:                      return "UNKNOWN";
    }
}

static uint32_t get_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static void trim_for_label(char *dst, size_t dst_len, const char *src)
{
    if (!src || !dst || dst_len == 0) return;
    size_t n = strlen(src);
    if (n > dst_len - 1) n = dst_len - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static void update_transcript_label(void)
{
    if (!g_transcript_label) return;

    char combined[LABEL_TEXT_LEN];
    combined[0] = '\0';

    /* Show the most recent portion of history that fits */
    if (g_transcript_history[0]) {
        size_t hist_len = strlen(g_transcript_history);
        size_t take = hist_len;
        if (take > LABEL_TEXT_LEN - 1) take = LABEL_TEXT_LEN - 1;
        memcpy(combined, g_transcript_history + hist_len - take, take);
        combined[take] = '\0';
    }

    if (g_partial_text[0]) {
        size_t used = strlen(combined);
        size_t space = LABEL_TEXT_LEN - used - 1;
        if (space > 0 && used > 0) {
            strncat(combined, "\n", space);
            used = strlen(combined);
            space = LABEL_TEXT_LEN - used - 1;
        }
        if (space > 0) {
            strncat(combined, "[...] ", space);
            used = strlen(combined);
            space = LABEL_TEXT_LEN - used - 1;
        }
        if (space > 0) {
            size_t partial_len = strlen(g_partial_text);
            if (partial_len > space) partial_len = space;
            memcpy(combined + used, g_partial_text, partial_len);
            combined[used + partial_len] = '\0';
        }
        lv_obj_set_style_text_color(g_transcript_label, COLOR_TEXT_DIM, 0);
    } else if (combined[0]) {
        lv_obj_set_style_text_color(g_transcript_label, COLOR_TEXT, 0);
    } else {
        strncpy(combined, "Waiting for audio...", sizeof(combined) - 1);
        combined[sizeof(combined) - 1] = '\0';
        lv_obj_set_style_text_color(g_transcript_label, COLOR_TEXT_DIM, 0);
    }

    lv_label_set_text(g_transcript_label, combined);
}

/* 显式引用中文字体，防止链接器丢弃 */
extern const lv_font_t ai_radio_font;

int ui_ai_radio_init(void)
{
    g_screen = lv_obj_create(NULL);
    if (!g_screen) return -1;

    lv_obj_set_size(g_screen, LCD_W, LCD_H);
    lv_obj_set_style_bg_color(g_screen, COLOR_BG, 0);
    lv_obj_set_style_pad_all(g_screen, 0, 0);
    lv_obj_set_style_radius(g_screen, 0, 0);
    lv_obj_set_style_border_width(g_screen, 0, 0);

    /* AI 页面使用中文字体，确保 ASR/LLM 中文结果显示正常 */
    lv_obj_set_style_text_font(g_screen, &ai_radio_font, 0);

    /* Alert banner */
    g_banner = lv_obj_create(g_screen);
    lv_obj_set_size(g_banner, LCD_W, BANNER_H);
    lv_obj_set_pos(g_banner, 0, BANNER_Y);
    lv_obj_set_style_bg_color(g_banner, COLOR_BANNER_NORMAL, 0);
    lv_obj_set_style_border_width(g_banner, 0, 0);
    lv_obj_set_style_pad_all(g_banner, 4, 0);
    lv_obj_set_style_radius(g_banner, 0, 0);

    g_alert_label = lv_label_create(g_banner);
    lv_label_set_text(g_alert_label, "AI RADIO READY");
    lv_obj_set_style_text_color(g_alert_label, COLOR_TEXT, 0);
    /* font inherited from screen */
    lv_obj_set_pos(g_alert_label, 8, 6);

    /* Frequency / mode bar */
    lv_obj_t *freq_bar = lv_obj_create(g_screen);
    lv_obj_set_size(freq_bar, LCD_W, FREQ_BAR_H);
    lv_obj_set_pos(freq_bar, 0, FREQ_BAR_Y);
    lv_obj_set_style_bg_color(freq_bar, COLOR_PANEL_BG, 0);
    lv_obj_set_style_border_width(freq_bar, 0, 0);
    lv_obj_set_style_pad_all(freq_bar, 4, 0);
    lv_obj_set_style_radius(freq_bar, 0, 0);

    g_freq_label = lv_label_create(freq_bar);
    lv_label_set_text(g_freq_label, "14.250.000 MHz");
    lv_obj_set_style_text_color(g_freq_label, COLOR_ACCENT, 0);
    /* font inherited from screen */
    lv_obj_set_pos(g_freq_label, 8, 6);

    g_mode_label = lv_label_create(freq_bar);
    lv_label_set_text(g_mode_label, "USB");
    lv_obj_set_style_text_color(g_mode_label, COLOR_GREEN, 0);
    /* font inherited from screen */
    lv_obj_set_pos(g_mode_label, LCD_W - 56, 8);

    /* Transcript panel */
    lv_obj_t *transcript_panel = lv_obj_create(g_screen);
    lv_obj_set_size(transcript_panel, LCD_W, TRANSCRIPT_H);
    lv_obj_set_pos(transcript_panel, 0, TRANSCRIPT_Y);
    lv_obj_set_style_bg_color(transcript_panel, COLOR_BG, 0);
    lv_obj_set_style_border_color(transcript_panel, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(transcript_panel, 1, 0);
    lv_obj_set_style_pad_all(transcript_panel, 4, 0);
    lv_obj_set_style_radius(transcript_panel, 0, 0);

    g_transcript_title = lv_label_create(transcript_panel);
    lv_label_set_text(g_transcript_title, "ASR TRANSCRIPT");
    lv_obj_set_style_text_color(g_transcript_title, COLOR_TEXT_DIM, 0);
    /* font inherited from screen */
    lv_obj_set_pos(g_transcript_title, 4, 2);

    g_transcript_label = lv_label_create(transcript_panel);
    lv_label_set_text(g_transcript_label, "Waiting for audio...");
    lv_obj_set_style_text_color(g_transcript_label, COLOR_TEXT_DIM, 0);
    /* font inherited from screen */
    lv_label_set_long_mode(g_transcript_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_size(g_transcript_label, LCD_W - 12, TRANSCRIPT_H - 20);
    lv_obj_set_pos(g_transcript_label, 4, 18);

    /* Analysis panel */
    lv_obj_t *summary_panel = lv_obj_create(g_screen);
    lv_obj_set_size(summary_panel, LCD_W, SUMMARY_H);
    lv_obj_set_pos(summary_panel, 0, SUMMARY_Y);
    lv_obj_set_style_bg_color(summary_panel, COLOR_PANEL_BG, 0);
    lv_obj_set_style_border_color(summary_panel, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(summary_panel, 1, 0);
    lv_obj_set_style_pad_all(summary_panel, 4, 0);
    lv_obj_set_style_radius(summary_panel, 0, 0);

    g_summary_title = lv_label_create(summary_panel);
    lv_label_set_text(g_summary_title, "AI ANALYSIS");
    lv_obj_set_style_text_color(g_summary_title, COLOR_TEXT_DIM, 0);
    /* font inherited from screen */
    lv_obj_set_pos(g_summary_title, 4, 2);

    g_summary_label = lv_label_create(summary_panel);
    lv_label_set_text(g_summary_label, "No analysis yet.");
    lv_obj_set_style_text_color(g_summary_label, COLOR_TEXT, 0);
    /* font inherited from screen */
    lv_label_set_long_mode(g_summary_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_size(g_summary_label, LCD_W - 12, SUMMARY_H - 44);
    lv_obj_set_pos(g_summary_label, 4, 18);

    g_alert_level_label = lv_label_create(summary_panel);
    lv_label_set_text(g_alert_level_label, "ALERT: NONE");
    lv_obj_set_style_text_color(g_alert_level_label, COLOR_GREEN, 0);
    /* font inherited from screen */
    lv_obj_set_pos(g_alert_level_label, 4, SUMMARY_H - 22);

    g_transcript_history[0] = '\0';
    g_partial_text[0] = '\0';

    ui_ai_radio_set_frequency(g_current_freq_hz, g_current_mode);
    /* NOTE: Do NOT call lv_scr_load() here.
     * The main screen (create_main_screen) is the default active screen.
     * This AI radio screen is used as an overlay/secondary view and will be
     * loaded on demand via ui_ai_radio_show(). */
    g_main_screen = lv_scr_act();
    return 0;
}

void ui_ai_radio_show(void)
{
    if (!g_screen) return;
    if (g_main_screen == NULL) {
        g_main_screen = lv_scr_act();
    }
    lv_scr_load(g_screen);
    g_visible = true;
}

void ui_ai_radio_hide(void)
{
    if (!g_main_screen || !g_visible) return;
    lv_scr_load(g_main_screen);
    g_visible = false;
}

bool ui_ai_radio_is_visible(void)
{
    return g_visible;
}

void ui_ai_radio_set_frequency(float freq_hz, const char *mode)
{
    g_current_freq_hz = freq_hz;
    if (mode) {
        strncpy(g_current_mode, mode, sizeof(g_current_mode) - 1);
        g_current_mode[sizeof(g_current_mode) - 1] = '\0';
    }

    if (g_freq_label) {
        uint32_t mhz = (uint32_t)(freq_hz / 1000000.0f);
        uint32_t khz = ((uint32_t)(freq_hz / 1000.0f)) % 1000;
        uint32_t hz  = ((uint32_t)freq_hz) % 1000;
        lv_label_set_text_fmt(g_freq_label, "%lu.%03lu.%03lu MHz",
                              (unsigned long)mhz, (unsigned long)khz, (unsigned long)hz);
    }
    if (g_mode_label && mode) {
        lv_label_set_text(g_mode_label, mode);
    }
}

void ui_ai_radio_append_transcript(const char *text, bool is_partial)
{
    if (!text || text[0] == '\0') return;

    if (is_partial) {
        trim_for_label(g_partial_text, sizeof(g_partial_text), text);
    } else {
        size_t hist_len = strlen(g_transcript_history);
        size_t text_len = strlen(text);
        if (hist_len + text_len + 3 < sizeof(g_transcript_history)) {
            if (hist_len > 0) {
                strcat(g_transcript_history, "\n");
            }
            strcat(g_transcript_history, text);
        } else {
            /* Keep recent text by shifting */
            size_t keep = sizeof(g_transcript_history) / 2;
            memmove(g_transcript_history, g_transcript_history + keep, hist_len - keep + 1);
            if (strlen(g_transcript_history) + text_len + 3 < sizeof(g_transcript_history)) {
                strcat(g_transcript_history, "\n");
                strcat(g_transcript_history, text);
            }
        }
        g_partial_text[0] = '\0';
    }

    update_transcript_label();
}

void ui_ai_radio_set_analysis(const char *summary, int alert_level)
{
    g_alert_level = alert_level;

    if (g_summary_label) {
        if (summary && summary[0]) {
            char trimmed[LABEL_TEXT_LEN];
            trim_for_label(trimmed, sizeof(trimmed), summary);
            lv_label_set_text(g_summary_label, trimmed);
            lv_obj_set_style_text_color(g_summary_label, COLOR_TEXT, 0);
        } else {
            lv_label_set_text(g_summary_label, "No analysis yet.");
            lv_obj_set_style_text_color(g_summary_label, COLOR_TEXT_DIM, 0);
        }
    }

    if (g_alert_level_label) {
        lv_label_set_text_fmt(g_alert_level_label, "ALERT: %s",
                              alert_level_str(alert_level));
        switch (alert_level) {
        case ALERT_LEVEL_NONE:
            lv_obj_set_style_text_color(g_alert_level_label, COLOR_GREEN, 0);
            break;
        case ALERT_LEVEL_INFO:
            lv_obj_set_style_text_color(g_alert_level_label, COLOR_ACCENT, 0);
            break;
        case ALERT_LEVEL_WARNING:
            lv_obj_set_style_text_color(g_alert_level_label, COLOR_YELLOW, 0);
            break;
        default:
            lv_obj_set_style_text_color(g_alert_level_label, COLOR_RED, 0);
            break;
        }
    }
}

void ui_ai_radio_show_alert(bool show, int alert_level)
{
    g_alert_show = show;
    if (show) {
        g_alert_level = alert_level;
    }

    if (!g_banner) return;

    if (show) {
        lv_obj_set_style_bg_color(g_banner, COLOR_BANNER_ALERT, 0);
        if (g_alert_label) {
            lv_label_set_text_fmt(g_alert_label, "ALERT: %s",
                                  alert_level_str(alert_level));
            lv_obj_set_style_text_color(g_alert_label, COLOR_TEXT, 0);
        }
    } else {
        lv_obj_set_style_bg_color(g_banner, COLOR_BANNER_NORMAL, 0);
        if (g_alert_label) {
            lv_label_set_text(g_alert_label, "AI RADIO READY");
            lv_obj_set_style_text_color(g_alert_label, COLOR_TEXT, 0);
        }
    }
}

void ui_ai_radio_refresh(void)
{
    if (!g_alert_show || !g_screen) return;

    uint32_t now = get_ms();
    if (now - g_last_blink_ms < 500) return;
    g_last_blink_ms = now;

    g_alert_flash_on = !g_alert_flash_on;
    if (g_alert_flash_on) {
        lv_obj_set_style_bg_color(g_screen, COLOR_BG_ALERT, 0);
        if (g_banner) {
            lv_obj_set_style_bg_color(g_banner, COLOR_BANNER_ALERT, 0);
        }
    } else {
        lv_obj_set_style_bg_color(g_screen, COLOR_BG, 0);
        if (g_banner) {
            lv_obj_set_style_bg_color(g_banner, COLOR_BANNER_NORMAL, 0);
        }
    }
}
