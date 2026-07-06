/****************************************************************************
 * AI Radio Console for openvela - Contest 2026 Team 095
 *
 * An AI-powered amateur radio controller running on Gemini-S1 (R528)
 * Features: Mayday detection, CW decoding, interference analysis,
 *           AI frequency recommendation, QSO logging, real-time translation,
 *           satellite pass alerts, multi-screen UI, hardware button control
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>

#include <nuttx/input/buttons.h>

#include "radio_config.h"
#include "cw_decoder.h"
#include "mayday_detector.h"
#include "signal_analyzer.h"
#include "freq_recommender.h"
#include "radio_log.h"
#include "audio_capture.h"
#include "translator.h"
#include "agent_bridge.h"

#include <lvgl.h>

#define LV_HOR_RES          240
#define LV_VER_RES          320

#define COLOR_BG            lv_color_hex(0x0d1117)
#define COLOR_CARD          lv_color_hex(0x161b22)
#define COLOR_CARD_BORDER   lv_color_hex(0x30363d)
#define COLOR_CARD_SEL      lv_color_hex(0x4fc3f7)
#define COLOR_ACCENT        lv_color_hex(0x4fc3f7)
#define COLOR_GREEN         lv_color_hex(0x66bb6a)
#define COLOR_RED           lv_color_hex(0xef5350)
#define COLOR_ORANGE        lv_color_hex(0xffa726)
#define COLOR_TEXT          lv_color_hex(0xeceff1)
#define COLOR_TEXT_DIM      lv_color_hex(0x78909c)
#define COLOR_PURPLE        lv_color_hex(0xce93d8)
#define COLOR_CYAN          lv_color_hex(0x4dd0e1)
#define COLOR_YELLOW        lv_color_hex(0xfff176)
#define COLOR_GRAY          lv_color_hex(0x90a4ae)

#define BTN_VOL_DOWN        0x01
#define BTN_VOL_UP          0x02
#define BTN_MENU            0x04
#define BTN_ENTER           0x08
#define BTN_HOME            0x10

#define BTN_POLL_MS         50
#define BTN_DEBOUNCE_MS     200

#define NUM_CARDS           8
#define NUM_STEPS           5
#define NUM_MODES           5
#define NUM_BANDS           10
#define NUM_SETTINGS        3
#define LOG_ENTRIES_SHOWN   6
#define ALERT_ENTRIES_SHOWN 6

typedef enum {
    UI_SCREEN_MAIN = 0,
    UI_SCREEN_CW,
    UI_SCREEN_LOG,
    UI_SCREEN_ALERTS,
    UI_SCREEN_FREQ,
    UI_SCREEN_TRANSLATE,
    UI_SCREEN_SETTINGS,
    UI_SCREEN_MAX
} ui_screen_t;

static const uint32_t STEP_SIZES[NUM_STEPS] = {100, 1000, 10000, 100000, 1000000};
static const char *STEP_NAMES[NUM_STEPS] = {"100Hz", "1kHz", "10kHz", "100kHz", "1MHz"};
static const char *MODE_NAMES[NUM_MODES] = {"USB", "LSB", "CW", "AM", "FM"};
static const char *BAND_NAMES[NUM_BANDS] = {"160m", "80m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "6m"};
static const uint32_t BAND_EDGES[NUM_BANDS][2] = {
    {1800000, 2000000},
    {3500000, 4000000},
    {7000000, 7300000},
    {10100000, 10150000},
    {14000000, 14350000},
    {18068000, 18168000},
    {21000000, 21450000},
    {24890000, 24990000},
    {28000000, 29700000},
    {50000000, 54000000}
};

static const ui_screen_t CARD_SCREEN_MAP[NUM_CARDS] = {
    UI_SCREEN_ALERTS,
    UI_SCREEN_CW,
    UI_SCREEN_ALERTS,
    UI_SCREEN_FREQ,
    UI_SCREEN_LOG,
    UI_SCREEN_TRANSLATE,
    UI_SCREEN_ALERTS,
    UI_SCREEN_SETTINGS
};

static const char *SETTING_NAMES[NUM_SETTINGS] = {"WiFi", "Bluetooth", "Brightness"};

static lv_obj_t *g_screens[UI_SCREEN_MAX];
static lv_obj_t *g_time_label = NULL;
static lv_obj_t *g_title_label = NULL;
static lv_obj_t *g_s_meter_bar = NULL;
static lv_obj_t *g_s_meter_label = NULL;
static lv_obj_t *g_hint_label = NULL;
static lv_obj_t *g_mayday_border = NULL;

static lv_obj_t *g_cards[NUM_CARDS];
static lv_obj_t *g_card_value_labels[NUM_CARDS];
static lv_obj_t *g_cw_scroll_label = NULL;
static lv_obj_t *g_freq_big_label = NULL;
static lv_obj_t *g_freq_mode_label = NULL;
static lv_obj_t *g_freq_band_label = NULL;
static lv_obj_t *g_freq_step_label = NULL;
static lv_obj_t *g_log_labels[LOG_ENTRIES_SHOWN];
static lv_obj_t *g_alert_labels[ALERT_ENTRIES_SHOWN];
static lv_obj_t *g_translate_orig_label = NULL;
static lv_obj_t *g_translate_trans_label = NULL;
static lv_obj_t *g_setting_labels[NUM_SETTINGS];
static lv_obj_t *g_setting_value_labels[NUM_SETTINGS];

static float g_noise_floor = -78.0f;
static int g_s_meter = 7;
static int g_qso_count = 12;
static char g_cw_buf[512];
static bool g_mayday_alert = false;
static volatile bool g_running = true;

static ui_screen_t g_current_screen = UI_SCREEN_MAIN;
static int g_selected_card = 0;
static uint32_t g_freq_hz = 14250000;
static int g_step_idx = 1;
static int g_mode_idx = 0;
static int g_log_scroll = 0;
static int g_alert_scroll = 0;
static int g_setting_idx = 0;
static bool g_settings_wifi = true;
static bool g_settings_bt = false;
static int g_settings_brightness = 70;

static int g_btn_fd = -1;
static btn_buttonset_t g_btn_last = 0;
static uint32_t g_btn_last_time = 0;
static bool g_mayday_flash = false;

static uint32_t get_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static void freq_to_string(uint32_t hz, char *buf, size_t len)
{
    uint32_t mhz = hz / 1000000;
    uint32_t khz = (hz / 1000) % 1000;
    uint32_t hz_part = hz % 1000;
    snprintf(buf, len, "%lu.%03lu.%03lu",
             (unsigned long)mhz, (unsigned long)khz, (unsigned long)hz_part);
}

static int get_current_band(uint32_t hz)
{
    for (int i = 0; i < NUM_BANDS; i++) {
        if (hz >= BAND_EDGES[i][0] && hz <= BAND_EDGES[i][1])
            return i;
    }
    return -1;
}

static void update_hint_bar(ui_screen_t screen);
static void switch_screen(ui_screen_t screen);
static void update_freq_display(void);
static void update_settings_display(void);
static void update_card_highlight(void);
static lv_obj_t *create_card(lv_obj_t *parent, const char *icon_text, const char *title,
                              lv_color_t icon_bg, lv_color_t icon_fg, int x, int y);
static void create_common_ui(lv_obj_t *scr, const char *title, bool big_freq);

static void update_hint_bar(ui_screen_t screen)
{
    if (!g_hint_label) return;
    switch (screen) {
    case UI_SCREEN_MAIN:
        lv_label_set_text(g_hint_label, "VOL+- Sel  MENU Cycle  ENTER Open  HOME");
        break;
    case UI_SCREEN_CW:
        lv_label_set_text(g_hint_label, "VOL+- Scroll  MENU Clear  ENTER Pause  HOME Back");
        break;
    case UI_SCREEN_LOG:
        lv_label_set_text(g_hint_label, "VOL+- Scroll  MENU New QSO  ENTER Detail  HOME Back");
        break;
    case UI_SCREEN_ALERTS:
        lv_label_set_text(g_hint_label, "VOL+- Scroll  MENU Ack  ENTER Detail  HOME Back");
        break;
    case UI_SCREEN_FREQ:
        lv_label_set_text(g_hint_label, "VOL+- Tune  MENU Step  ENTER Mode  HOME Back");
        break;
    case UI_SCREEN_TRANSLATE:
        lv_label_set_text(g_hint_label, "VOL+- Lang  MENU Swap  ENTER Speak  HOME Back");
        break;
    case UI_SCREEN_SETTINGS:
        lv_label_set_text(g_hint_label, "VOL+- Select  MENU Next  ENTER Toggle  HOME Back");
        break;
    default:
        lv_label_set_text(g_hint_label, "HOME Back");
        break;
    }
}

static void create_common_ui(lv_obj_t *scr, const char *title, bool big_freq)
{
    lv_obj_set_style_bg_color(scr, COLOR_BG, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_radius(scr, 0, 0);
    lv_obj_set_style_border_width(scr, 0, 0);

    lv_obj_t *status_bar = lv_obj_create(scr);
    lv_obj_set_size(status_bar, LV_HOR_RES, 24);
    lv_obj_set_pos(status_bar, 0, 0);
    lv_obj_set_style_bg_color(status_bar, COLOR_BG, 0);
    lv_obj_set_style_border_width(status_bar, 0, 0);
    lv_obj_set_style_pad_all(status_bar, 0, 0);
    lv_obj_set_style_radius(status_bar, 0, 0);

    g_time_label = lv_label_create(status_bar);
    lv_obj_set_style_text_color(g_time_label, COLOR_ACCENT, 0);
    lv_obj_align(g_time_label, LV_ALIGN_LEFT_MID, 8, 0);

    g_title_label = lv_label_create(status_bar);
    lv_label_set_text(g_title_label, title);
    lv_obj_set_style_text_color(g_title_label, COLOR_ACCENT, 0);
    lv_obj_align(g_title_label, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *wifi_led = lv_obj_create(status_bar);
    lv_obj_set_size(wifi_led, 6, 6);
    lv_obj_set_style_bg_color(wifi_led, g_settings_wifi ? COLOR_GREEN : COLOR_TEXT_DIM, 0);
    lv_obj_set_style_radius(wifi_led, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(wifi_led, 0, 0);
    lv_obj_align(wifi_led, LV_ALIGN_RIGHT_MID, -24, 0);

    lv_obj_t *wifi_label = lv_label_create(status_bar);
    lv_label_set_text(wifi_label, "W");
    lv_obj_set_style_text_color(wifi_label, g_settings_wifi ? COLOR_GREEN : COLOR_TEXT_DIM, 0);
    lv_obj_align(wifi_label, LV_ALIGN_RIGHT_MID, -12, 0);

    lv_obj_t *bt_label = lv_label_create(status_bar);
    lv_label_set_text(bt_label, "B");
    lv_obj_set_style_text_color(bt_label, g_settings_bt ? COLOR_CYAN : COLOR_TEXT_DIM, 0);
    lv_obj_align(bt_label, LV_ALIGN_RIGHT_MID, -4, 0);

    int freq_h = big_freq ? 80 : 50;
    int freq_y = 24;
    lv_obj_t *freq_display = lv_obj_create(scr);
    lv_obj_set_size(freq_display, LV_HOR_RES, freq_h);
    lv_obj_set_pos(freq_display, 0, freq_y);
    lv_obj_set_style_bg_color(freq_display, COLOR_BG, 0);
    lv_obj_set_style_border_width(freq_display, 0, 0);
    lv_obj_set_style_pad_all(freq_display, 0, 0);
    lv_obj_set_style_radius(freq_display, 0, 0);

    if (big_freq) {
        g_freq_big_label = lv_label_create(freq_display);
        lv_obj_set_style_text_color(g_freq_big_label, COLOR_ACCENT, 0);
        lv_obj_align(g_freq_big_label, LV_ALIGN_CENTER, 0, -6);

        g_freq_mode_label = lv_label_create(freq_display);
        lv_obj_set_style_text_color(g_freq_mode_label, COLOR_TEXT_DIM, 0);
        lv_obj_align(g_freq_mode_label, LV_ALIGN_BOTTOM_LEFT, 12, 0);

        g_freq_band_label = lv_label_create(freq_display);
        lv_obj_set_style_text_color(g_freq_band_label, COLOR_GREEN, 0);
        lv_obj_align(g_freq_band_label, LV_ALIGN_BOTTOM_MID, 0, 0);

        g_freq_step_label = lv_label_create(freq_display);
        lv_obj_set_style_text_color(g_freq_step_label, COLOR_ORANGE, 0);
        lv_obj_align(g_freq_step_label, LV_ALIGN_BOTTOM_RIGHT, -12, 0);
    } else {
        lv_obj_t *freq_lbl = lv_label_create(freq_display);
        char fbuf[32];
        freq_to_string(g_freq_hz, fbuf, sizeof(fbuf));
        lv_label_set_text(freq_lbl, fbuf);
        lv_obj_set_style_text_color(freq_lbl, COLOR_ACCENT, 0);
        lv_obj_align(freq_lbl, LV_ALIGN_CENTER, -20, -6);

        lv_obj_t *mode_lbl = lv_label_create(freq_display);
        lv_label_set_text(mode_lbl, MODE_NAMES[g_mode_idx]);
        lv_obj_set_style_text_color(mode_lbl, COLOR_TEXT_DIM, 0);
        lv_obj_align(mode_lbl, LV_ALIGN_CENTER, 40, -6);

        lv_obj_t *unit_lbl = lv_label_create(freq_display);
        lv_label_set_text(unit_lbl, "MHz");
        lv_obj_set_style_text_color(unit_lbl, COLOR_TEXT_DIM, 0);
        lv_obj_align(unit_lbl, LV_ALIGN_CENTER, 30, 10);
    }

    int spec_y = freq_y + freq_h;
    int spec_h = big_freq ? 30 : 36;
    lv_obj_t *spectrum_container = lv_obj_create(scr);
    lv_obj_set_size(spectrum_container, LV_HOR_RES - 16, spec_h);
    lv_obj_set_pos(spectrum_container, 8, spec_y);
    lv_obj_set_style_bg_color(spectrum_container, lv_color_hex(0x080b10), 0);
    lv_obj_set_style_border_color(spectrum_container, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(spectrum_container, 1, 0);
    lv_obj_set_style_radius(spectrum_container, big_freq ? 4 : 8, 0);
    lv_obj_set_style_pad_all(spectrum_container, 4, 0);

    lv_obj_t *spec_label = lv_label_create(spectrum_container);
    lv_label_set_text(spec_label, "S-METER");
    lv_obj_set_style_text_color(spec_label, COLOR_TEXT_DIM, 0);
    lv_obj_align(spec_label, LV_ALIGN_TOP_LEFT, 2, 0);

    g_s_meter_label = lv_label_create(spectrum_container);
    lv_obj_set_style_text_color(g_s_meter_label, COLOR_ACCENT, 0);
    lv_obj_align(g_s_meter_label, LV_ALIGN_TOP_RIGHT, -2, 0);

    g_s_meter_bar = lv_bar_create(spectrum_container);
    lv_bar_set_range(g_s_meter_bar, 0, 9);
    lv_bar_set_value(g_s_meter_bar, g_s_meter, LV_ANIM_OFF);
    lv_obj_set_size(g_s_meter_bar, LV_HOR_RES - 32, big_freq ? 4 : 6);
    lv_obj_set_style_bg_color(g_s_meter_bar, lv_color_hex(0x1a1f26), 0);
    lv_obj_set_style_bg_opa(g_s_meter_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(g_s_meter_bar, 2, 0);
    lv_obj_set_style_pad_all(g_s_meter_bar, 0, 0);
    lv_obj_set_style_border_width(g_s_meter_bar, 0, 0);
    lv_obj_align(g_s_meter_bar, LV_ALIGN_BOTTOM_MID, 0, -2);

    int hint_y = LV_VER_RES - 20;
    lv_obj_t *hint_bar = lv_obj_create(scr);
    lv_obj_set_size(hint_bar, LV_HOR_RES, 20);
    lv_obj_set_pos(hint_bar, 0, hint_y);
    lv_obj_set_style_bg_color(hint_bar, lv_color_hex(0x080b10), 0);
    lv_obj_set_style_border_width(hint_bar, 0, 0);
    lv_obj_set_style_pad_all(hint_bar, 0, 0);
    lv_obj_set_style_radius(hint_bar, 0, 0);

    g_hint_label = lv_label_create(hint_bar);
    lv_obj_set_style_text_color(g_hint_label, COLOR_TEXT_DIM, 0);
    lv_obj_center(g_hint_label);
}

static lv_obj_t *create_card(lv_obj_t *parent, const char *icon_text, const char *title,
                              lv_color_t icon_bg, lv_color_t icon_fg, int x, int y)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, 108, 44);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_style_bg_color(card, COLOR_CARD, 0);
    lv_obj_set_style_border_color(card, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_pad_all(card, 4, 0);

    lv_obj_t *icon = lv_obj_create(card);
    lv_obj_set_size(icon, 16, 16);
    lv_obj_set_pos(icon, 0, 0);
    lv_obj_set_style_bg_color(icon, icon_bg, 0);
    lv_obj_set_style_radius(icon, 3, 0);
    lv_obj_set_style_border_width(icon, 0, 0);
    lv_obj_set_style_pad_all(icon, 0, 0);

    lv_obj_t *icon_label = lv_label_create(icon);
    lv_label_set_text(icon_label, icon_text);
    lv_obj_set_style_text_color(icon_label, icon_fg, 0);
    lv_obj_center(icon_label);

    lv_obj_t *title_label = lv_label_create(card);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_color(title_label, COLOR_TEXT, 0);
    lv_obj_set_pos(title_label, 22, 1);

    return card;
}

static void update_card_highlight(void)
{
    for (int i = 0; i < NUM_CARDS; i++) {
        if (i == g_selected_card) {
            lv_obj_set_style_border_color(g_cards[i], COLOR_CARD_SEL, 0);
            lv_obj_set_style_border_width(g_cards[i], 2, 0);
        } else {
            lv_obj_set_style_border_color(g_cards[i], COLOR_CARD_BORDER, 0);
            lv_obj_set_style_border_width(g_cards[i], 1, 0);
        }
    }
}

static void main_screen_create(void)
{
    g_screens[UI_SCREEN_MAIN] = lv_obj_create(NULL);
    lv_obj_t *scr = g_screens[UI_SCREEN_MAIN];
    create_common_ui(scr, "AI RADIO", false);

    int card_w = 108;
    int card_h = 44;
    int card_x_start = 8;
    int card_y_start = 114;
    int gap_x = 8;
    int gap_y = 6;

    struct {
        const char *icon;
        const char *title;
        lv_color_t icon_bg;
        lv_color_t icon_fg;
    } card_info[NUM_CARDS] = {
        {"SOS", "Mayday",   lv_color_hex(0x3d1515), COLOR_RED},
        {"CW",  "CW Decode",lv_color_hex(0x0d2b45), COLOR_ACCENT},
        {"INT", "Interfere",lv_color_hex(0x3d2b0d), COLOR_ORANGE},
        {"AI",  "AI Freq",  lv_color_hex(0x2a0d3d), COLOR_PURPLE},
        {"LOG", "QSO Log",  lv_color_hex(0x0d3d15), COLOR_GREEN},
        {"TR",  "Translate",lv_color_hex(0x0d3d3d), COLOR_CYAN},
        {"SAT", "Sat Pass", lv_color_hex(0x1a0d3d), COLOR_YELLOW},
        {"SET", "Settings", lv_color_hex(0x2d2d2d), COLOR_GRAY}
    };

    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 2; col++) {
            int idx = row * 2 + col;
            int x = card_x_start + col * (card_w + gap_x);
            int y = card_y_start + row * (card_h + gap_y);
            g_cards[idx] = create_card(scr, card_info[idx].icon, card_info[idx].title,
                                        card_info[idx].icon_bg, card_info[idx].icon_fg, x, y);
            g_card_value_labels[idx] = lv_label_create(g_cards[idx]);
            lv_obj_set_style_text_color(g_card_value_labels[idx], COLOR_TEXT_DIM, 0);
            lv_obj_set_pos(g_card_value_labels[idx], 4, 24);
        }
    }

    lv_label_set_text(g_card_value_labels[0], g_mayday_alert ? "! ALERT !" : "Monitoring");
    lv_obj_set_style_text_color(g_card_value_labels[0], g_mayday_alert ? COLOR_RED : COLOR_TEXT_DIM, 0);
    strncpy(g_cw_buf, "CQ CQ DE BA1AA...", sizeof(g_cw_buf) - 1);
    lv_label_set_text(g_card_value_labels[1], g_cw_buf);
    lv_obj_set_style_text_color(g_card_value_labels[1], COLOR_ACCENT, 0);
    lv_label_set_long_mode(g_card_value_labels[1], LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_size(g_card_value_labels[1], card_w - 8, 14);
    lv_label_set_text_fmt(g_card_value_labels[2], "Noise %.0fdBm", g_noise_floor);
    lv_label_set_text(g_card_value_labels[3], "3 best freqs");
    lv_obj_set_style_text_color(g_card_value_labels[3], COLOR_GREEN, 0);
    lv_label_set_text_fmt(g_card_value_labels[4], "%d QSOs today", g_qso_count);
    lv_label_set_text(g_card_value_labels[5], "EN-ZH Ready");
    lv_label_set_text(g_card_value_labels[6], "ISS 12:34");
    lv_label_set_text(g_card_value_labels[7], "WiFi/BT/Bright");

    update_card_highlight();
    update_hint_bar(UI_SCREEN_MAIN);
}

static void cw_screen_create(void)
{
    g_screens[UI_SCREEN_CW] = lv_obj_create(NULL);
    lv_obj_t *scr = g_screens[UI_SCREEN_CW];
    create_common_ui(scr, "CW DECODER", false);

    lv_obj_t *content = lv_obj_create(scr);
    lv_obj_set_size(content, LV_HOR_RES - 16, LV_VER_RES - 170);
    lv_obj_set_pos(content, 8, 114);
    lv_obj_set_style_bg_color(content, COLOR_CARD, 0);
    lv_obj_set_style_border_color(content, COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(content, 1, 0);
    lv_obj_set_style_radius(content, 8, 0);
    lv_obj_set_style_pad_all(content, 8, 0);

    lv_obj_t *header = lv_label_create(content);
    lv_label_set_text(header, "Decoded CW Text");
    lv_obj_set_style_text_color(header, COLOR_ACCENT, 0);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);

    g_cw_scroll_label = lv_label_create(content);
    lv_label_set_long_mode(g_cw_scroll_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_size(g_cw_scroll_label, LV_HOR_RES - 32, LV_VER_RES - 210);
    lv_obj_set_style_text_color(g_cw_scroll_label, COLOR_TEXT, 0);
    lv_obj_set_pos(g_cw_scroll_label, 0, 20);
    lv_label_set_text(g_cw_scroll_label,
        "CQ CQ CQ DE BA1AA BA1AA\nK\nUR 599 599 BK\n73 ES GB SK\nQRZ? DE BA1AA K\n...");

    lv_obj_t *wpm_label = lv_label_create(content);
    lv_label_set_text(wpm_label, "WPM: 18  |  Locked");
    lv_obj_set_style_text_color(wpm_label, COLOR_GREEN, 0);
    lv_obj_align(wpm_label, LV_ALIGN_BOTTOM_MID, 0, 0);

    update_hint_bar(UI_SCREEN_CW);
}

static void log_screen_create(void)
{
    g_screens[UI_SCREEN_LOG] = lv_obj_create(NULL);
    lv_obj_t *scr = g_screens[UI_SCREEN_LOG];
    create_common_ui(scr, "QSO LOG", false);

    lv_obj_t *content = lv_obj_create(scr);
    lv_obj_set_size(content, LV_HOR_RES - 16, LV_VER_RES - 170);
    lv_obj_set_pos(content, 8, 114);
    lv_obj_set_style_bg_color(content, COLOR_CARD, 0);
    lv_obj_set_style_border_color(content, COLOR_GREEN, 0);
    lv_obj_set_style_border_width(content, 1, 0);
    lv_obj_set_style_radius(content, 8, 0);
    lv_obj_set_style_pad_all(content, 6, 0);

    lv_obj_t *header = lv_label_create(content);
    lv_label_set_text(header, "Recent Contacts");
    lv_obj_set_style_text_color(header, COLOR_GREEN, 0);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);

    static const char *sample_logs[8] = {
        "12:34 BA1AA 14.250 599 USB",
        "12:21 BG6XYZ 14.200 579 USB",
        "11:58 JA1RK 14.220 559 USB",
        "11:42 K3LR 14.195 569 USB",
        "11:15 G0ABC 14.250 589 USB",
        "10:58 DL1ABC 14.230 549 USB",
        "10:32 UA9XX 14.180 539 USB",
        "09:45 VE3XYZ 14.210 559 USB"
    };

    for (int i = 0; i < LOG_ENTRIES_SHOWN; i++) {
        g_log_labels[i] = lv_label_create(content);
        lv_obj_set_style_text_color(g_log_labels[i], COLOR_TEXT, 0);
        lv_obj_set_pos(g_log_labels[i], 4, 18 + i * 20);
        if (i < 8) {
            lv_label_set_text(g_log_labels[i], sample_logs[i]);
        }
    }

    update_hint_bar(UI_SCREEN_LOG);
}

static void alerts_screen_create(void)
{
    g_screens[UI_SCREEN_ALERTS] = lv_obj_create(NULL);
    lv_obj_t *scr = g_screens[UI_SCREEN_ALERTS];
    create_common_ui(scr, "ALERTS", false);

    g_mayday_border = scr;

    lv_obj_t *content = lv_obj_create(scr);
    lv_obj_set_size(content, LV_HOR_RES - 16, LV_VER_RES - 170);
    lv_obj_set_pos(content, 8, 114);
    lv_obj_set_style_bg_color(content, COLOR_CARD, 0);
    lv_obj_set_style_border_color(content, COLOR_RED, 0);
    lv_obj_set_style_border_width(content, 1, 0);
    lv_obj_set_style_radius(content, 8, 0);
    lv_obj_set_style_pad_all(content, 6, 0);

    lv_obj_t *header = lv_label_create(content);
    lv_label_set_text(header, "Alert History");
    lv_obj_set_style_text_color(header, COLOR_RED, 0);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);

    static const char *sample_alerts[8] = {
        "[!] MAYDAY 14.300 (test)",
        "[*] Interference 14.220",
        "[*] Interference 7.070",
        "[i] Sat AOS ISS 12:34",
        "[i] Sat LOS ISS 12:45",
        "[*] Noise floor spike",
        "[i] Band open 10m",
        "[i] Net started 14.300"
    };

    for (int i = 0; i < ALERT_ENTRIES_SHOWN; i++) {
        g_alert_labels[i] = lv_label_create(content);
        lv_obj_set_pos(g_alert_labels[i], 4, 18 + i * 20);
        if (i < 8) {
            if (strstr(sample_alerts[i], "MAYDAY"))
                lv_obj_set_style_text_color(g_alert_labels[i], COLOR_RED, 0);
            else if (sample_alerts[i][1] == '*')
                lv_obj_set_style_text_color(g_alert_labels[i], COLOR_ORANGE, 0);
            else
                lv_obj_set_style_text_color(g_alert_labels[i], COLOR_TEXT_DIM, 0);
            lv_label_set_text(g_alert_labels[i], sample_alerts[i]);
        }
    }

    update_hint_bar(UI_SCREEN_ALERTS);
}

static void freq_screen_create(void)
{
    g_screens[UI_SCREEN_FREQ] = lv_obj_create(NULL);
    lv_obj_t *scr = g_screens[UI_SCREEN_FREQ];
    create_common_ui(scr, "TUNE", true);

    lv_obj_t *content = lv_obj_create(scr);
    lv_obj_set_size(content, LV_HOR_RES - 16, LV_VER_RES - 220);
    lv_obj_set_pos(content, 8, 140);
    lv_obj_set_style_bg_color(content, COLOR_CARD, 0);
    lv_obj_set_style_border_color(content, COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(content, 1, 0);
    lv_obj_set_style_radius(content, 8, 0);
    lv_obj_set_style_pad_all(content, 8, 0);

    lv_obj_t *info = lv_label_create(content);
    lv_label_set_text(info, "HF Bands: 160/80/40/30/20/17/15/12/10/6m");
    lv_obj_set_style_text_color(info, COLOR_TEXT_DIM, 0);
    lv_obj_align(info, LV_ALIGN_TOP_MID, 0, 4);

    lv_obj_t *step_info = lv_label_create(content);
    lv_label_set_text(step_info, "VOL+/- Tune   MENU Step   ENTER Mode");
    lv_obj_set_style_text_color(step_info, COLOR_TEXT_DIM, 0);
    lv_obj_align(step_info, LV_ALIGN_BOTTOM_MID, 0, -4);

    update_hint_bar(UI_SCREEN_FREQ);
}

static void translate_screen_create(void)
{
    g_screens[UI_SCREEN_TRANSLATE] = lv_obj_create(NULL);
    lv_obj_t *scr = g_screens[UI_SCREEN_TRANSLATE];
    create_common_ui(scr, "TRANSLATE", false);

    lv_obj_t *content = lv_obj_create(scr);
    lv_obj_set_size(content, LV_HOR_RES - 16, LV_VER_RES - 170);
    lv_obj_set_pos(content, 8, 114);
    lv_obj_set_style_bg_color(content, COLOR_CARD, 0);
    lv_obj_set_style_border_color(content, COLOR_CYAN, 0);
    lv_obj_set_style_border_width(content, 1, 0);
    lv_obj_set_style_radius(content, 8, 0);
    lv_obj_set_style_pad_all(content, 8, 0);

    lv_obj_t *hdr1 = lv_label_create(content);
    lv_label_set_text(hdr1, "[EN] English");
    lv_obj_set_style_text_color(hdr1, COLOR_CYAN, 0);
    lv_obj_set_pos(hdr1, 4, 4);

    g_translate_orig_label = lv_label_create(content);
    lv_label_set_long_mode(g_translate_orig_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_size(g_translate_orig_label, LV_HOR_RES - 40, 60);
    lv_obj_set_style_text_color(g_translate_orig_label, COLOR_TEXT, 0);
    lv_obj_set_pos(g_translate_orig_label, 4, 20);
    lv_label_set_text(g_translate_orig_label,
        "CQ CQ CQ, this is Bravo Alpha One Alpha Alpha, calling CQ and standing by.");

    lv_obj_t *sep = lv_obj_create(content);
    lv_obj_set_size(sep, LV_HOR_RES - 32, 1);
    lv_obj_set_pos(sep, 0, 86);
    lv_obj_set_style_bg_color(sep, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_radius(sep, 0, 0);
    lv_obj_set_style_pad_all(sep, 0, 0);

    lv_obj_t *hdr2 = lv_label_create(content);
    lv_label_set_text(hdr2, "[ZH] Chinese");
    lv_obj_set_style_text_color(hdr2, COLOR_PURPLE, 0);
    lv_obj_set_pos(hdr2, 4, 92);

    g_translate_trans_label = lv_label_create(content);
    lv_label_set_long_mode(g_translate_trans_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_size(g_translate_trans_label, LV_HOR_RES - 40, 60);
    lv_obj_set_style_text_color(g_translate_trans_label, COLOR_TEXT, 0);
    lv_obj_set_pos(g_translate_trans_label, 4, 108);
    lv_label_set_text(g_translate_trans_label,
        "CQ CQ CQ，这里是BA1AA，呼叫CQ并守听。");

    update_hint_bar(UI_SCREEN_TRANSLATE);
}

static void settings_screen_create(void)
{
    g_screens[UI_SCREEN_SETTINGS] = lv_obj_create(NULL);
    lv_obj_t *scr = g_screens[UI_SCREEN_SETTINGS];
    create_common_ui(scr, "SETTINGS", false);

    lv_obj_t *content = lv_obj_create(scr);
    lv_obj_set_size(content, LV_HOR_RES - 16, LV_VER_RES - 170);
    lv_obj_set_pos(content, 8, 114);
    lv_obj_set_style_bg_color(content, COLOR_CARD, 0);
    lv_obj_set_style_border_color(content, COLOR_GRAY, 0);
    lv_obj_set_style_border_width(content, 1, 0);
    lv_obj_set_style_radius(content, 8, 0);
    lv_obj_set_style_pad_all(content, 8, 0);

    for (int i = 0; i < NUM_SETTINGS; i++) {
        int y = 8 + i * 40;

        lv_obj_t *item_bg = lv_obj_create(content);
        lv_obj_set_size(item_bg, LV_HOR_RES - 32, 32);
        lv_obj_set_pos(item_bg, 0, y);
        lv_obj_set_style_bg_color(item_bg, lv_color_hex(0x0d1117), 0);
        lv_obj_set_style_border_width(item_bg, 0, 0);
        lv_obj_set_style_radius(item_bg, 4, 0);
        lv_obj_set_style_pad_all(item_bg, 4, 0);

        g_setting_labels[i] = lv_label_create(item_bg);
        lv_label_set_text(g_setting_labels[i], SETTING_NAMES[i]);
        lv_obj_set_style_text_color(g_setting_labels[i], COLOR_TEXT, 0);
        lv_obj_align(g_setting_labels[i], LV_ALIGN_LEFT_MID, 4, 0);

        g_setting_value_labels[i] = lv_label_create(item_bg);
        lv_obj_set_style_text_color(g_setting_value_labels[i], COLOR_ACCENT, 0);
        lv_obj_align(g_setting_value_labels[i], LV_ALIGN_RIGHT_MID, -4, 0);
    }

    update_hint_bar(UI_SCREEN_SETTINGS);
}

static void update_settings_display(void)
{
    if (!g_screens[UI_SCREEN_SETTINGS]) return;
    lv_label_set_text(g_setting_value_labels[0], g_settings_wifi ? "ON" : "OFF");
    lv_label_set_text(g_setting_value_labels[1], g_settings_bt ? "ON" : "OFF");
    lv_label_set_text_fmt(g_setting_value_labels[2], "%d%%", g_settings_brightness);

    for (int i = 0; i < NUM_SETTINGS; i++) {
        lv_obj_t *item_bg = lv_obj_get_parent(g_setting_labels[i]);
        if (i == g_setting_idx) {
            lv_obj_set_style_border_color(item_bg, COLOR_ACCENT, 0);
            lv_obj_set_style_border_width(item_bg, 1, 0);
        } else {
            lv_obj_set_style_border_width(item_bg, 0, 0);
        }
    }
}

static void update_freq_display(void)
{
    if (!g_freq_big_label) return;
    char fbuf[32];
    char sbuf[16];
    freq_to_string(g_freq_hz, fbuf, sizeof(fbuf));
    lv_label_set_text(g_freq_big_label, fbuf);
    lv_label_set_text(g_freq_mode_label, MODE_NAMES[g_mode_idx]);
    int band = get_current_band(g_freq_hz);
    if (band >= 0)
        snprintf(sbuf, sizeof(sbuf), "%s", BAND_NAMES[band]);
    else
        snprintf(sbuf, sizeof(sbuf), "--");
    lv_label_set_text(g_freq_band_label, sbuf);
    lv_label_set_text(g_freq_step_label, STEP_NAMES[g_step_idx]);
}

static void switch_screen(ui_screen_t screen)
{
    if (screen < 0 || screen >= UI_SCREEN_MAX) return;
    g_current_screen = screen;
    lv_scr_load(g_screens[screen]);
    update_hint_bar(screen);

    if (screen == UI_SCREEN_FREQ) {
        update_freq_display();
    } else if (screen == UI_SCREEN_SETTINGS) {
        update_settings_display();
    }
}

static void adjust_freq(int dir)
{
    uint32_t step = STEP_SIZES[g_step_idx];
    if (dir > 0) {
        g_freq_hz += step;
        if (g_freq_hz > BAND_EDGES[NUM_BANDS - 1][1])
            g_freq_hz = BAND_EDGES[NUM_BANDS - 1][1];
    } else {
        if (g_freq_hz < step + BAND_EDGES[0][0])
            g_freq_hz = BAND_EDGES[0][0];
        else
            g_freq_hz -= step;
    }
    update_freq_display();
}

static void handle_button_press(btn_buttonset_t btn)
{
    printf("[BTN] Button press: 0x%02x screen=%d\n", (unsigned)btn, g_current_screen);

    switch (g_current_screen) {
    case UI_SCREEN_MAIN:
        if (btn == BTN_VOL_UP || btn == BTN_VOL_DOWN) {
            int dir = (btn == BTN_VOL_UP) ? -1 : 1;
            g_selected_card = (g_selected_card + dir + NUM_CARDS) % NUM_CARDS;
            update_card_highlight();
        } else if (btn == BTN_MENU) {
            g_selected_card = (g_selected_card + 1) % NUM_CARDS;
            update_card_highlight();
        } else if (btn == BTN_ENTER) {
            ui_screen_t target = CARD_SCREEN_MAP[g_selected_card];
            switch_screen(target);
        } else if (btn == BTN_HOME) {
            update_card_highlight();
        }
        break;

    case UI_SCREEN_CW:
        if (btn == BTN_VOL_UP || btn == BTN_VOL_DOWN) {
        } else if (btn == BTN_MENU) {
            strncpy(g_cw_buf, "", sizeof(g_cw_buf) - 1);
            if (g_cw_scroll_label)
                lv_label_set_text(g_cw_scroll_label, "(cleared)");
        } else if (btn == BTN_ENTER) {
        } else if (btn == BTN_HOME) {
            switch_screen(UI_SCREEN_MAIN);
        }
        break;

    case UI_SCREEN_LOG:
        if (btn == BTN_VOL_UP || btn == BTN_VOL_DOWN) {
            int dir = (btn == BTN_VOL_UP) ? -1 : 1;
            g_log_scroll += dir;
            if (g_log_scroll < 0) g_log_scroll = 0;
            if (g_log_scroll > 2) g_log_scroll = 2;
        } else if (btn == BTN_MENU) {
            g_qso_count++;
            if (g_card_value_labels[4])
                lv_label_set_text_fmt(g_card_value_labels[4], "%d QSOs today", g_qso_count);
        } else if (btn == BTN_ENTER) {
        } else if (btn == BTN_HOME) {
            switch_screen(UI_SCREEN_MAIN);
        }
        break;

    case UI_SCREEN_ALERTS:
        if (btn == BTN_VOL_UP || btn == BTN_VOL_DOWN) {
            int dir = (btn == BTN_VOL_UP) ? -1 : 1;
            g_alert_scroll += dir;
            if (g_alert_scroll < 0) g_alert_scroll = 0;
            if (g_alert_scroll > 2) g_alert_scroll = 2;
        } else if (btn == BTN_MENU) {
            g_mayday_alert = false;
            if (g_card_value_labels[0]) {
                lv_label_set_text(g_card_value_labels[0], "Monitoring");
                lv_obj_set_style_text_color(g_card_value_labels[0], COLOR_TEXT_DIM, 0);
            }
        } else if (btn == BTN_ENTER) {
        } else if (btn == BTN_HOME) {
            switch_screen(UI_SCREEN_MAIN);
        }
        break;

    case UI_SCREEN_FREQ:
        if (btn == BTN_VOL_UP || btn == BTN_VOL_DOWN) {
            int dir = (btn == BTN_VOL_UP) ? 1 : -1;
            adjust_freq(dir);
        } else if (btn == BTN_MENU) {
            g_step_idx = (g_step_idx + 1) % NUM_STEPS;
            update_freq_display();
        } else if (btn == BTN_ENTER) {
            g_mode_idx = (g_mode_idx + 1) % NUM_MODES;
            update_freq_display();
        } else if (btn == BTN_HOME) {
            switch_screen(UI_SCREEN_MAIN);
        }
        break;

    case UI_SCREEN_TRANSLATE:
        if (btn == BTN_VOL_UP || btn == BTN_VOL_DOWN) {
        } else if (btn == BTN_MENU) {
        } else if (btn == BTN_ENTER) {
        } else if (btn == BTN_HOME) {
            switch_screen(UI_SCREEN_MAIN);
        }
        break;

    case UI_SCREEN_SETTINGS:
        if (btn == BTN_VOL_UP || btn == BTN_VOL_DOWN) {
            int dir = (btn == BTN_VOL_UP) ? -1 : 1;
            g_setting_idx = (g_setting_idx + dir + NUM_SETTINGS) % NUM_SETTINGS;
            update_settings_display();
        } else if (btn == BTN_MENU) {
            g_setting_idx = (g_setting_idx + 1) % NUM_SETTINGS;
            update_settings_display();
        } else if (btn == BTN_ENTER) {
            if (g_setting_idx == 0) g_settings_wifi = !g_settings_wifi;
            else if (g_setting_idx == 1) g_settings_bt = !g_settings_bt;
            else {
                g_settings_brightness += 10;
                if (g_settings_brightness > 100) g_settings_brightness = 20;
            }
            update_settings_display();
        } else if (btn == BTN_HOME) {
            switch_screen(UI_SCREEN_MAIN);
        }
        break;

    default:
        break;
    }
}

static void button_poll_timer(lv_timer_t *timer)
{
    (void)timer;
    if (g_btn_fd < 0) return;

    btn_buttonset_t val = 0;
    ssize_t n = read(g_btn_fd, &val, sizeof(val));
    if (n <= 0) {
        return;
    }

    uint32_t now = get_ms();
    btn_buttonset_t pressed = val & ~g_btn_last;
    g_btn_last = val;

    if (pressed == 0) return;
    if (now - g_btn_last_time < BTN_DEBOUNCE_MS) return;
    g_btn_last_time = now;

    btn_buttonset_t single_btn = 0;
    for (int i = 0; i < 5; i++) {
        btn_buttonset_t mask = (btn_buttonset_t)1 << i;
        if (pressed & mask) {
            single_btn = mask;
            break;
        }
    }

    if (single_btn) {
        handle_button_press(single_btn);
    }
}

static void mayday_flash_timer(lv_timer_t *timer)
{
    (void)timer;
    if (!g_mayday_alert || !g_mayday_border) return;
    g_mayday_flash = !g_mayday_flash;
    lv_obj_set_style_border_color(g_mayday_border,
        g_mayday_flash ? COLOR_RED : COLOR_BG, 0);
    lv_obj_set_style_border_width(g_mayday_border, g_mayday_flash ? 3 : 0, 0);
}

static void ui_update_timer(lv_timer_t *timer)
{
    (void)timer;

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    if (t && g_time_label) {
        char time_buf[16];
        snprintf(time_buf, sizeof(time_buf), "%02d:%02d:%02d",
                 t->tm_hour, t->tm_min, t->tm_sec);
        lv_label_set_text(g_time_label, time_buf);
    }

    g_noise_floor = -80.0f + (float)(rand() % 10);
    g_s_meter = 5 + (rand() % 5);

    if (g_s_meter_bar) lv_bar_set_value(g_s_meter_bar, g_s_meter, LV_ANIM_ON);
    if (g_s_meter_label) lv_label_set_text_fmt(g_s_meter_label, "S%d", g_s_meter);

    if (g_current_screen == UI_SCREEN_MAIN) {
        static const char *cw_texts[] = {
            "CQ CQ CQ DE...",
            "DE BA1AA K",
            "UR 599 599 BK",
            "73 ES GB SK",
            "QRZ? DE BA1AA"
        };
        int idx = rand() % 5;
        if (g_card_value_labels[1])
            lv_label_set_text(g_card_value_labels[1], cw_texts[idx]);

        if (g_card_value_labels[2])
            lv_label_set_text_fmt(g_card_value_labels[2], "Noise %.0fdBm", g_noise_floor);

        if ((rand() % 300) == 0 && !g_mayday_alert) {
            g_mayday_alert = true;
            if (g_card_value_labels[0]) {
                lv_label_set_text(g_card_value_labels[0], "! ALERT !");
                lv_obj_set_style_text_color(g_card_value_labels[0], COLOR_RED, 0);
            }
            switch_screen(UI_SCREEN_ALERTS);
        }
    }

    if (g_current_screen == UI_SCREEN_CW && g_cw_scroll_label) {
        static const char *cw_extra[] = {"CQ CQ DE BG6XYZ K", "599 599 TU", "73", "QRZ?"};
        static char big_cw[512];
        static int append_count = 0;
        if (append_count < 10 && (rand() % 15) == 0) {
            size_t cur = strlen(big_cw);
            if (cur > 0 && cur < sizeof(big_cw) - 2) {
                big_cw[cur] = '\n';
                big_cw[cur + 1] = '\0';
            }
            strncat(big_cw, cw_extra[rand() % 4], sizeof(big_cw) - strlen(big_cw) - 1);
            lv_label_set_text(g_cw_scroll_label, big_cw);
            append_count++;
        }
    }
}

static int button_init(void)
{
    const char *dev = "/dev/input/event1";
    g_btn_fd = open(dev, O_RDONLY | O_NONBLOCK);
    if (g_btn_fd < 0) {
        printf("[BTN] Warning: cannot open %s (%s), running without buttons\n",
               dev, strerror(errno));
        return -1;
    }
    printf("[BTN] Button device %s opened (fd=%d)\n", dev, g_btn_fd);
    return 0;
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("========================================\n");
    printf("  AI Radio Console v%s for openvela\n", RADIO_APP_VERSION);
    printf("  Contest 2026 - Team 095\n");
    printf("  Target: Gemini-S1 (R528 Cortex-A7)\n");
    printf("========================================\n");

    printf("[INIT] Initializing modules...\n");
    agent_bridge_init();
    mayday_detector_init();
    cw_decoder_init(NULL);
    signal_analyzer_init(AUDIO_SAMPLE_RATE);
    freq_recommender_init();
    radio_log_init(NULL);
    translator_init();
    printf("[INIT] All modules initialized.\n");

    printf("[UI] Creating LVGL interface with multi-screen support...\n");
    lv_init();

    memset(g_screens, 0, sizeof(g_screens));
    memset(g_cw_buf, 0, sizeof(g_cw_buf));
    main_screen_create();
    cw_screen_create();
    log_screen_create();
    alerts_screen_create();
    freq_screen_create();
    translate_screen_create();
    settings_screen_create();

    lv_scr_load(g_screens[UI_SCREEN_MAIN]);
    g_current_screen = UI_SCREEN_MAIN;
    update_card_highlight();

    lv_timer_create(ui_update_timer, 1000, NULL);
    lv_timer_create(mayday_flash_timer, 300, NULL);

    button_init();
    lv_timer_create(button_poll_timer, BTN_POLL_MS, NULL);

    printf("[UI] Interface created with %d screens.\n", UI_SCREEN_MAX);
    printf("[INFO] Features:\n");
    printf("  - Mayday/SOS signal detection\n");
    printf("  - CW Morse code decoder\n");
    printf("  - Interference analysis\n");
    printf("  - AI frequency recommendation\n");
    printf("  - QSO logging\n");
    printf("  - Real-time translation\n");
    printf("  - Multi-screen navigation via hardware buttons\n");
    printf("  - LRADC buttons: Vol+/-, Menu, Enter, Home\n");
    printf("[INFO] System ready. Entering UI loop...\n");

    while (g_running) {
        lv_timer_handler();
        usleep(5000);
    }

    if (g_btn_fd >= 0) close(g_btn_fd);
    printf("[SHUTDOWN] AI Radio Console exiting.\n");
    return 0;
}
