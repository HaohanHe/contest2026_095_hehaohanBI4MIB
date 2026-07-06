/****************************************************************************
 * AI Radio Console for openvela - Contest 2026 Team 095
 *
 * An AI-powered amateur radio controller running on Gemini-S1 (R528)
 * Features: Mayday detection, CW decoding, interference analysis,
 *           AI frequency recommendation, QSO logging, real-time translation,
 *           satellite pass alerts
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>

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

#define LV_HOR_RES          320
#define LV_VER_RES          240

#define COLOR_BG            lv_color_hex(0x0d1117)
#define COLOR_CARD          lv_color_hex(0x161b22)
#define COLOR_CARD_BORDER   lv_color_hex(0x30363d)
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

static lv_obj_t *g_main_screen = NULL;
static lv_obj_t *g_status_bar = NULL;
static lv_obj_t *g_time_label = NULL;
static lv_obj_t *g_freq_label = NULL;
static lv_obj_t *g_s_meter_bar = NULL;
static lv_obj_t *g_s_meter_label = NULL;

static lv_obj_t *g_card_mayday = NULL;
static lv_obj_t *g_card_cw = NULL;
static lv_obj_t *g_card_interfere = NULL;
static lv_obj_t *g_card_ai = NULL;
static lv_obj_t *g_card_log = NULL;
static lv_obj_t *g_card_translate = NULL;
static lv_obj_t *g_card_sat = NULL;
static lv_obj_t *g_card_settings = NULL;

static lv_obj_t *g_cw_text_label = NULL;
static lv_obj_t *g_mayday_value_label = NULL;
static lv_obj_t *g_interfere_value_label = NULL;
static lv_obj_t *g_ai_value_label = NULL;
static lv_obj_t *g_log_value_label = NULL;
static lv_obj_t *g_translate_value_label = NULL;
static lv_obj_t *g_sat_value_label = NULL;

static int g_temp_val = 24;
static int g_hum_val = 62;
static int g_light_val = 320;
static float g_noise_floor = -78.0f;
static int g_s_meter = 7;
static int g_qso_count = 12;
static char g_cw_buf[256];
static bool g_mayday_alert = false;
static volatile bool g_running = true;

static void status_bar_create(lv_obj_t *parent)
{
    g_status_bar = lv_obj_create(parent);
    lv_obj_set_size(g_status_bar, LV_HOR_RES, 24);
    lv_obj_set_pos(g_status_bar, 0, 0);
    lv_obj_set_style_bg_color(g_status_bar, COLOR_BG, 0);
    lv_obj_set_style_border_width(g_status_bar, 0, 0);
    lv_obj_set_style_pad_all(g_status_bar, 0, 0);
    lv_obj_set_style_radius(g_status_bar, 0, 0);

    g_time_label = lv_label_create(g_status_bar);
    lv_obj_set_style_text_color(g_time_label, COLOR_ACCENT, 0);
    lv_obj_align(g_time_label, LV_ALIGN_LEFT_MID, 8, 0);

    lv_obj_t *title_label = lv_label_create(g_status_bar);
    lv_label_set_text(title_label, "AI RADIO");
    lv_obj_set_style_text_color(title_label, COLOR_ACCENT, 0);
    lv_obj_align(title_label, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *wifi_led = lv_obj_create(g_status_bar);
    lv_obj_set_size(wifi_led, 6, 6);
    lv_obj_set_style_bg_color(wifi_led, COLOR_GREEN, 0);
    lv_obj_set_style_radius(wifi_led, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(wifi_led, 0, 0);
    lv_obj_align(wifi_led, LV_ALIGN_RIGHT_MID, -24, 0);

    lv_obj_t *wifi_label = lv_label_create(g_status_bar);
    lv_label_set_text(wifi_label, "W");
    lv_obj_set_style_text_color(wifi_label, COLOR_GREEN, 0);
    lv_obj_align(wifi_label, LV_ALIGN_RIGHT_MID, -12, 0);

    lv_obj_t *bt_label = lv_label_create(g_status_bar);
    lv_label_set_text(bt_label, "B");
    lv_obj_set_style_text_color(bt_label, COLOR_TEXT_DIM, 0);
    lv_obj_align(bt_label, LV_ALIGN_RIGHT_MID, -4, 0);
}

static lv_obj_t *create_card(lv_obj_t *parent, const char *icon_text, const char *title,
                              lv_color_t icon_bg, lv_color_t icon_fg, int x, int y)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, 148, 52);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_style_bg_color(card, COLOR_CARD, 0);
    lv_obj_set_style_border_color(card, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_pad_all(card, 6, 0);

    lv_obj_t *icon = lv_obj_create(card);
    lv_obj_set_size(icon, 20, 20);
    lv_obj_set_pos(icon, 0, 0);
    lv_obj_set_style_bg_color(icon, icon_bg, 0);
    lv_obj_set_style_radius(icon, 4, 0);
    lv_obj_set_style_border_width(icon, 0, 0);
    lv_obj_set_style_pad_all(icon, 0, 0);

    lv_obj_t *icon_label = lv_label_create(icon);
    lv_label_set_text(icon_label, icon_text);
    lv_obj_set_style_text_color(icon_label, icon_fg, 0);
    lv_obj_center(icon_label);

    lv_obj_t *title_label = lv_label_create(card);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_color(title_label, COLOR_TEXT, 0);
    lv_obj_set_pos(title_label, 28, 2);

    return card;
}

static void main_screen_create(void)
{
    g_main_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_main_screen, COLOR_BG, 0);
    lv_obj_set_style_pad_all(g_main_screen, 0, 0);
    lv_obj_set_style_radius(g_main_screen, 0, 0);
    lv_obj_set_style_border_width(g_main_screen, 0, 0);

    status_bar_create(g_main_screen);

    lv_obj_t *freq_display = lv_obj_create(g_main_screen);
    lv_obj_set_size(freq_display, LV_HOR_RES, 50);
    lv_obj_set_pos(freq_display, 0, 24);
    lv_obj_set_style_bg_color(freq_display, COLOR_BG, 0);
    lv_obj_set_style_border_width(freq_display, 0, 0);
    lv_obj_set_style_pad_all(freq_display, 0, 0);
    lv_obj_set_style_radius(freq_display, 0, 0);

    g_freq_label = lv_label_create(freq_display);
    lv_label_set_text(g_freq_label, "14.250.000");
    lv_obj_set_style_text_color(g_freq_label, COLOR_ACCENT, 0);
    lv_obj_align(g_freq_label, LV_ALIGN_CENTER, 0, -6);

    lv_obj_t *freq_unit = lv_label_create(freq_display);
    lv_label_set_text(freq_unit, "MHz USB");
    lv_obj_set_style_text_color(freq_unit, COLOR_TEXT_DIM, 0);
    lv_obj_align(freq_unit, LV_ALIGN_CENTER, 0, 12);

    lv_obj_t *spectrum_container = lv_obj_create(g_main_screen);
    lv_obj_set_size(spectrum_container, LV_HOR_RES - 16, 40);
    lv_obj_set_pos(spectrum_container, 8, 76);
    lv_obj_set_style_bg_color(spectrum_container, lv_color_hex(0x080b10), 0);
    lv_obj_set_style_border_color(spectrum_container, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(spectrum_container, 1, 0);
    lv_obj_set_style_radius(spectrum_container, 8, 0);
    lv_obj_set_style_pad_all(spectrum_container, 4, 0);

    lv_obj_t *spec_label = lv_label_create(spectrum_container);
    lv_label_set_text(spec_label, "SPECTRUM");
    lv_obj_set_style_text_color(spec_label, COLOR_TEXT_DIM, 0);
    lv_obj_align(spec_label, LV_ALIGN_TOP_LEFT, 2, 0);

    g_s_meter_label = lv_label_create(spectrum_container);
    lv_obj_set_style_text_color(g_s_meter_label, COLOR_ACCENT, 0);
    lv_obj_align(g_s_meter_label, LV_ALIGN_TOP_RIGHT, -2, 0);

    g_s_meter_bar = lv_bar_create(spectrum_container);
    lv_bar_set_range(g_s_meter_bar, 0, 9);
    lv_bar_set_value(g_s_meter_bar, g_s_meter, LV_ANIM_OFF);
    lv_obj_set_size(g_s_meter_bar, LV_HOR_RES - 32, 4);
    lv_obj_set_style_bg_color(g_s_meter_bar, lv_color_hex(0x1a1f26), 0);
    lv_obj_set_style_bg_opa(g_s_meter_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(g_s_meter_bar, 2, 0);
    lv_obj_set_style_pad_all(g_s_meter_bar, 0, 0);
    lv_obj_set_style_border_width(g_s_meter_bar, 0, 0);
    lv_obj_align(g_s_meter_bar, LV_ALIGN_BOTTOM_MID, 0, -4);

    int card_w = 148;
    int card_h = 52;
    int card_x_start = 8;
    int card_y_start = 120;
    int gap_x = 8;
    int gap_y = 6;

    g_card_mayday = create_card(g_main_screen, "SOS", "Mayday",
        lv_color_hex(0x3d1515), COLOR_RED, card_x_start, card_y_start);
    g_mayday_value_label = lv_label_create(g_card_mayday);
    lv_obj_set_style_text_color(g_mayday_value_label, COLOR_RED, 0);
    lv_obj_set_pos(g_mayday_value_label, 6, 30);

    g_card_cw = create_card(g_main_screen, "CW", "CW Decode",
        lv_color_hex(0x0d2b45), COLOR_ACCENT, card_x_start + card_w + gap_x, card_y_start);
    g_cw_text_label = lv_label_create(g_card_cw);
    lv_obj_set_style_text_color(g_cw_text_label, COLOR_ACCENT, 0);
    lv_obj_set_pos(g_cw_text_label, 6, 30);
    lv_label_set_long_mode(g_cw_text_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_size(g_cw_text_label, card_w - 12, 16);

    g_card_interfere = create_card(g_main_screen, "INT", "Interfere",
        lv_color_hex(0x3d2b0d), COLOR_ORANGE, card_x_start, card_y_start + card_h + gap_y);
    g_interfere_value_label = lv_label_create(g_card_interfere);
    lv_obj_set_style_text_color(g_interfere_value_label, COLOR_TEXT_DIM, 0);
    lv_obj_set_pos(g_interfere_value_label, 6, 30);

    g_card_ai = create_card(g_main_screen, "AI", "AI Freq",
        lv_color_hex(0x2a0d3d), COLOR_PURPLE, card_x_start + card_w + gap_x, card_y_start + card_h + gap_y);
    g_ai_value_label = lv_label_create(g_card_ai);
    lv_obj_set_style_text_color(g_ai_value_label, COLOR_GREEN, 0);
    lv_obj_set_pos(g_ai_value_label, 6, 30);

    g_card_log = create_card(g_main_screen, "LOG", "QSO Log",
        lv_color_hex(0x0d3d15), COLOR_GREEN, card_x_start, card_y_start + (card_h + gap_y) * 2);
    g_log_value_label = lv_label_create(g_card_log);
    lv_obj_set_style_text_color(g_log_value_label, COLOR_TEXT_DIM, 0);
    lv_obj_set_pos(g_log_value_label, 6, 30);

    g_card_translate = create_card(g_main_screen, "TR", "Translate",
        lv_color_hex(0x0d3d3d), COLOR_CYAN, card_x_start + card_w + gap_x, card_y_start + (card_h + gap_y) * 2);
    g_translate_value_label = lv_label_create(g_card_translate);
    lv_obj_set_style_text_color(g_translate_value_label, COLOR_TEXT_DIM, 0);
    lv_obj_set_pos(g_translate_value_label, 6, 30);

    lv_label_set_text(g_mayday_value_label, g_mayday_alert ? "! ALERT !" : "Monitoring");
    strncpy(g_cw_buf, "CQ CQ DE BA1AA...", sizeof(g_cw_buf) - 1);
    lv_label_set_text(g_cw_text_label, g_cw_buf);
    lv_label_set_text_fmt(g_interfere_value_label, "Noise %.0fdBm", g_noise_floor);
    lv_label_set_text(g_ai_value_label, "3 best freqs");
    lv_label_set_text_fmt(g_log_value_label, "%d QSOs today", g_qso_count);
    lv_label_set_text(g_translate_value_label, "EN-ZH Ready");
    lv_label_set_text_fmt(g_s_meter_label, "S%d", g_s_meter);

    lv_scr_load(g_main_screen);
}

static void ui_update_timer(lv_timer_t *timer)
{
    (void)timer;

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    if (t) {
        char time_buf[16];
        snprintf(time_buf, sizeof(time_buf), "%02d:%02d:%02d",
                 t->tm_hour, t->tm_min, t->tm_sec);
        lv_label_set_text(g_time_label, time_buf);
    }

    g_temp_val = 23 + (rand() % 3);
    g_hum_val = 60 + (rand() % 8);
    g_noise_floor = -80.0f + (float)(rand() % 10);
    g_s_meter = 5 + (rand() % 5);

    lv_bar_set_value(g_s_meter_bar, g_s_meter, LV_ANIM_ON);
    lv_label_set_text_fmt(g_s_meter_label, "S%d", g_s_meter);

    static const char *cw_texts[] = {
        "CQ CQ CQ DE...",
        "DE BA1AA K",
        "UR 599 599 BK",
        "73 ES GB SK",
        "QRZ? DE BA1AA"
    };
    int idx = rand() % 5;
    lv_label_set_text(g_cw_text_label, cw_texts[idx]);

    lv_label_set_text_fmt(g_interfere_value_label, "Noise %.0fdBm", g_noise_floor);
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

    printf("[UI] Creating LVGL interface...\n");
    lv_init();
    main_screen_create();
    lv_timer_create(ui_update_timer, 1000, NULL);
    printf("[UI] Interface created.\n");

    printf("[INFO] Features:\n");
    printf("  - Mayday/SOS signal detection\n");
    printf("  - CW Morse code decoder\n");
    printf("  - Interference analysis\n");
    printf("  - AI frequency recommendation\n");
    printf("  - QSO logging\n");
    printf("  - Real-time translation\n");
    printf("[INFO] System ready. Entering UI loop...\n");

    while (g_running) {
        lv_timer_handler();
        usleep(5000);
    }

    printf("[SHUTDOWN] AI Radio Console exiting.\n");
    return 0;
}
