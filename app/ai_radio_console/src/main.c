/****************************************************************************
 * AI Radio Console for openvela - Contest 2026 Team 095
 *
 * An AI-powered amateur radio controller running on Gemini-S1 (R528)
 * Landscape 320x240 ILI9341 SPI LCD + TPADC resistive touch + 5 LRADC buttons
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
#include <syslog.h>
#include <sys/types.h>

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

#undef UI_REFRESH_MS
#define UI_REFRESH_MS       500

#define LCD_W               320
#define LCD_H               240

#define TOPBAR_H            26
#define SPECTRUM_H          80
#define CONTENT_H           104
#define BOTTOMBAR_H         24

#define SPECTRUM_Y          TOPBAR_H
#define CONTENT_Y           (TOPBAR_H + SPECTRUM_H)
#define BOTTOMBAR_Y         (CONTENT_Y + CONTENT_H)

#define CARDS_START_X       160
#define CARDS_W             160
#define CARD_COLS           2
#define CARD_ROWS           3
#define CARD_W              77
#define CARD_H              50
#define CARD_GAP_X          3
#define CARD_GAP_Y          2
#define CARD_PAD            4

#define COLOR_BG            lv_color_hex(0x0a0e1a)
#define COLOR_BAR           lv_color_hex(0x0d1220)
#define COLOR_CARD          lv_color_hex(0x141c30)
#define COLOR_CARD_BORDER   lv_color_hex(0x253048)
#define COLOR_SEL_BORDER    lv_color_hex(0x00d4ff)
#define COLOR_ACCENT        lv_color_hex(0x00d4ff)
#define COLOR_GREEN         lv_color_hex(0x33dd66)
#define COLOR_RED           lv_color_hex(0xff3344)
#define COLOR_AMBER         lv_color_hex(0xffaa00)
#define COLOR_MAGENTA       lv_color_hex(0xdd55ff)
#define COLOR_TEXT          lv_color_hex(0xe0e8f0)
#define COLOR_TEXT_DIM      lv_color_hex(0x8899aa)
#define COLOR_CYAN          lv_color_hex(0x00d4ff)
#define COLOR_YELLOW        lv_color_hex(0xffdd33)
#define COLOR_SPEC_BG       lv_color_hex(0x060912)

#define BTN_VOL_DOWN        0x01
#define BTN_VOL_UP          0x02
#define BTN_MENU            0x04
#define BTN_ENTER           0x08
#define BTN_HOME            0x10

#define BTN_POLL_MS         50
#define BTN_DEBOUNCE_MS     200

#define NUM_CARDS           (CARD_COLS * CARD_ROWS)
#define NUM_STEPS           7
#define NUM_DEMOD_MODES     5
#define NUM_BANDS           10
#define CW_LINES            4
#define SPEC_BARS           60

typedef enum {
    CARD_MAYDAY = 0,
    CARD_QSO,
    CARD_AI_FREQ,
    CARD_XVERSE,
    CARD_SAT,
    CARD_SETUP,
} card_idx_t;

typedef enum {
    DEMOD_USB = 0,
    DEMOD_LSB,
    DEMOD_CW,
    DEMOD_AM,
    DEMOD_FM
} demod_mode_t;

static const uint32_t STEP_SIZES[NUM_STEPS] = {100, 500, 1000, 5000, 10000, 100000, 1000000};
static const char *STEP_NAMES[NUM_STEPS] = {"100Hz", "500Hz", "1kHz", "5kHz", "10kHz", "100kHz", "1MHz"};
static const char *MODE_NAMES[NUM_DEMOD_MODES] = {"USB", "LSB", "CW", "AM", "FM"};
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

static const char *CARD_TITLES[NUM_CARDS] = {"MAYDAY", "QSO", "AI FREQ", "X-VERSE", "SAT", "SETUP"};

static lv_obj_t *g_scr = NULL;
static lv_obj_t *g_time_label = NULL;
static lv_obj_t *g_freq_label = NULL;
static lv_obj_t *g_mode_label = NULL;
static lv_obj_t *g_step_label = NULL;
static lv_obj_t *g_wifi_dot = NULL;
static lv_obj_t *g_bt_dot = NULL;
static lv_obj_t *g_s_meter_label = NULL;
static lv_obj_t *g_spec_bars[SPEC_BARS];
static lv_obj_t *g_noise_line = NULL;
static lv_obj_t *g_cw_labels[CW_LINES];
static lv_obj_t *g_cards[NUM_CARDS];
static lv_obj_t *g_card_val_labels[NUM_CARDS];
static lv_obj_t *g_hint_label = NULL;
static lv_obj_t *g_overlay = NULL;
static lv_obj_t *g_overlay_text = NULL;
static lv_obj_t *g_border_flash = NULL;

static uint32_t g_freq_hz = 14250000;
static int g_step_idx = 2;
static int g_mode_idx = DEMOD_USB;
static int g_selected_card = 0;
static bool g_mayday_active = false;
static bool g_mayday_flash_on = false;
static int g_s_meter = 6;
static float g_noise_floor = -76.0f;
static int g_qso_count = 27;
static int g_cw_line_idx = 0;
static char g_cw_lines[CW_LINES][48];
static bool g_wifi_on = true;
static bool g_bt_on = false;
static bool g_overlay_open = false;

static int g_btn_fd = -1;
static btn_buttonset_t g_btn_last = 0;
static uint32_t g_btn_last_time = 0;
static uint32_t g_sim_time_sec = 8 * 3600 + 1 * 60 + 15;
static int g_spec_values[SPEC_BARS];
static volatile bool g_running = true;

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

static lv_color_t s_meter_color(int s)
{
    if (s <= 3) return COLOR_TEXT_DIM;
    if (s <= 5) return COLOR_GREEN;
    if (s <= 7) return COLOR_YELLOW;
    if (s <= 9) return COLOR_AMBER;
    return COLOR_RED;
}

static lv_color_t spec_bar_color(int height_pct)
{
    if (height_pct < 25) return lv_color_hex(0x103060);
    if (height_pct < 50) return COLOR_ACCENT;
    if (height_pct < 70) return COLOR_GREEN;
    if (height_pct < 88) return COLOR_YELLOW;
    return COLOR_RED;
}

static int get_current_band(uint32_t hz)
{
    for (int i = 0; i < NUM_BANDS; i++) {
        if (hz >= BAND_EDGES[i][0] && hz <= BAND_EDGES[i][1])
            return i;
    }
    return -1;
}

static void update_card_highlight(void)
{
    for (int i = 0; i < NUM_CARDS; i++) {
        if (i == g_selected_card && !g_overlay_open) {
            lv_obj_set_style_border_color(g_cards[i], COLOR_SEL_BORDER, 0);
            lv_obj_set_style_border_width(g_cards[i], 2, 0);
        } else {
            lv_obj_set_style_border_color(g_cards[i], COLOR_CARD_BORDER, 0);
            lv_obj_set_style_border_width(g_cards[i], 1, 0);
        }
    }
}

static void close_overlay(void)
{
    if (g_overlay) {
        lv_obj_del(g_overlay);
        g_overlay = NULL;
        g_overlay_text = NULL;
    }
    g_overlay_open = false;
    update_card_highlight();
}

static void open_overlay(const char *title, const char *text)
{
    close_overlay();
    g_overlay_open = true;

    g_overlay = lv_obj_create(g_scr);
    lv_obj_set_size(g_overlay, 280, 140);
    lv_obj_center(g_overlay);
    lv_obj_set_style_bg_color(g_overlay, COLOR_CARD, 0);
    lv_obj_set_style_border_color(g_overlay, COLOR_SEL_BORDER, 0);
    lv_obj_set_style_border_width(g_overlay, 2, 0);
    lv_obj_set_style_radius(g_overlay, 6, 0);
    lv_obj_set_style_pad_all(g_overlay, 8, 0);

    lv_obj_t *title_lbl = lv_label_create(g_overlay);
    lv_label_set_text(title_lbl, title);
    lv_obj_set_style_text_color(title_lbl, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(title_lbl, LV_ALIGN_TOP_MID, 0, 0);

    g_overlay_text = lv_label_create(g_overlay);
    lv_label_set_text(g_overlay_text, text);
    lv_obj_set_style_text_color(g_overlay_text, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(g_overlay_text, &lv_font_montserrat_12, 0);
    lv_label_set_long_mode(g_overlay_text, LV_LABEL_LONG_WRAP);
    lv_obj_set_size(g_overlay_text, 260, 90);
    lv_obj_align(g_overlay_text, LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_t *hint = lv_label_create(g_overlay);
    lv_label_set_text(hint, "HOME: Back");
    lv_obj_set_style_text_color(hint, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, 0);

    update_card_highlight();
}

static void set_mode_label_color(void)
{
    if (!g_mode_label) return;
    switch (g_mode_idx) {
    case DEMOD_USB: lv_obj_set_style_text_color(g_mode_label, COLOR_ACCENT, 0); break;
    case DEMOD_LSB: lv_obj_set_style_text_color(g_mode_label, lv_color_hex(0x5588ff), 0); break;
    case DEMOD_CW:  lv_obj_set_style_text_color(g_mode_label, COLOR_GREEN, 0); break;
    case DEMOD_AM:  lv_obj_set_style_text_color(g_mode_label, COLOR_AMBER, 0); break;
    case DEMOD_FM:  lv_obj_set_style_text_color(g_mode_label, COLOR_MAGENTA, 0); break;
    }
}

static void update_freq_display(void)
{
    char fbuf[24];
    freq_to_string(g_freq_hz, fbuf, sizeof(fbuf));
    lv_label_set_text(g_freq_label, fbuf);
    lv_label_set_text(g_mode_label, MODE_NAMES[g_mode_idx]);
    lv_label_set_text(g_step_label, STEP_NAMES[g_step_idx]);
    set_mode_label_color();
}

static void cw_append_char(char c)
{
    int len = strlen(g_cw_lines[g_cw_line_idx]);
    if (len >= 46 || c == '\n') {
        g_cw_line_idx = (g_cw_line_idx + 1) % CW_LINES;
        memset(g_cw_lines[g_cw_line_idx], 0, sizeof(g_cw_lines[g_cw_line_idx]));
        for (int i = 0; i < CW_LINES; i++) {
            int li = (g_cw_line_idx - i + CW_LINES) % CW_LINES;
            lv_label_set_text(g_cw_labels[i], g_cw_lines[li]);
        }
        if (c == '\n') return;
    }
    len = strlen(g_cw_lines[g_cw_line_idx]);
    if (len < 46) {
        g_cw_lines[g_cw_line_idx][len] = c;
        g_cw_lines[g_cw_line_idx][len + 1] = '\0';
        lv_label_set_text(g_cw_labels[0], g_cw_lines[g_cw_line_idx]);
    }
}

static void cw_append_word(const char *word)
{
    while (*word) {
        cw_append_char(*word++);
    }
}

static void update_cards_display(void)
{
    if (g_mayday_active) {
        lv_label_set_text(g_card_val_labels[CARD_MAYDAY], "!! ACTIVE !!");
        lv_obj_set_style_text_color(g_card_val_labels[CARD_MAYDAY], COLOR_RED, 0);
    } else {
        lv_label_set_text(g_card_val_labels[CARD_MAYDAY], "Monitoring");
        lv_obj_set_style_text_color(g_card_val_labels[CARD_MAYDAY], COLOR_GREEN, 0);
    }

    lv_label_set_text_fmt(g_card_val_labels[CARD_QSO], "%d QSOs", g_qso_count);

    int band = get_current_band(g_freq_hz);
    if (band >= 4 && band <= 8) {
        int h = (g_sim_time_sec / 3600) % 24;
        if (h >= 12 && h <= 16) band = 4;
        else if (h >= 18 || h < 6) band = 8;
        else band = 6;
    }
    lv_label_set_text(g_card_val_labels[CARD_AI_FREQ], BAND_NAMES[band < 0 ? 4 : band]);

    const char *translations[] = {"QSY 14.225", "Good Signal", "QRZ?", "73 TU", "599 BK"};
    lv_label_set_text(g_card_val_labels[CARD_XVERSE], translations[rand() % 5]);

    int sat_min = (12 * 60 + 34 + (g_sim_time_sec / 60)) % (24 * 60);
    lv_label_set_text_fmt(g_card_val_labels[CARD_SAT], "ISS %02d:%02d",
                          sat_min / 60, sat_min % 60);

    lv_label_set_text(g_card_val_labels[CARD_SETUP], g_wifi_on ? "CFG:W" : "CFG:-");
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
}

static void handle_card_enter(void)
{
    switch (g_selected_card) {
    case CARD_MAYDAY:
        g_mayday_active = true;
        open_overlay("MAYDAY ALERT",
            "Distress signal detected!\n"
            "Freq: 14.300 MHz USB\n"
            "Press ENTER to cancel\n"
            "HOME to return");
        break;
    case CARD_QSO:
        open_overlay("QSO Log",
            "Recent contacts:\n"
            "12:34 BA1AA 14.250 599\n"
            "12:21 BG6XYZ 14.200\n"
            "11:58 JA1RK 14.220\n"
            "Total: 27 QSOs today");
        break;
    case CARD_AI_FREQ:
        open_overlay("AI Freq Recommend",
            "Based on UTC time, conditions:\n"
            "- 20m (14.225) Open - Daytime\n"
            "- 10m (28.400) Watch for open\n"
            "- 40m (7.070) Best after dark\n"
            "SFI=142 K=2 A=8");
        break;
    case CARD_XVERSE:
        open_overlay("X-VERSE Translate",
            "EN->ZH Live translation\n"
            "RX: \"CQ CQ this is K3LR\"\n"
            "-> \"CQ CQ 这里是K3LR\"\n"
            "TX-ready (ENTER=PTT)");
        break;
    case CARD_SAT:
        open_overlay("Satellite Passes",
            "Next passes:\n"
            "ISS  AOS 12:34  LOS 12:45\n"
            "SO-50 AOS 14:22  LOS 14:35\n"
            "FO-29 AOS 18:05  LOS 18:18\n"
            "FO-29 435.800/145.900");
        break;
    case CARD_SETUP:
        open_overlay("Settings",
            "WiFi: ON  (press MENU toggle)\n"
            "BT:   OFF\n"
            "Brightness: 70%\n"
            "Step: 1kHz\n"
            "Mode: USB");
        break;
    }
}

static void handle_button_press(btn_buttonset_t btn)
{
    printf("[BTN] 0x%02x\n", (unsigned)btn);

    if (g_overlay_open) {
        if (btn == BTN_HOME) {
            close_overlay();
            if (g_selected_card == CARD_MAYDAY) {
                g_mayday_active = false;
            }
        } else if (btn == BTN_ENTER) {
            if (g_selected_card == CARD_MAYDAY) {
                g_mayday_active = false;
                close_overlay();
            }
        }
        return;
    }

    if (btn == BTN_VOL_UP || btn == BTN_VOL_DOWN) {
        int dir = (btn == BTN_VOL_UP) ? 1 : -1;
        if (g_selected_card + dir >= 0 && g_selected_card + dir < NUM_CARDS) {
            g_selected_card += dir;
        } else if (btn == BTN_VOL_UP) {
            adjust_freq(1);
        } else {
            adjust_freq(-1);
        }
        update_card_highlight();
        update_freq_display();
    } else if (btn == BTN_MENU) {
        static int menu_mode = 0;
        menu_mode = (menu_mode + 1) % 3;
        if (menu_mode == 0) {
            g_step_idx = (g_step_idx + 1) % NUM_STEPS;
        } else if (menu_mode == 1) {
            g_mode_idx = (g_mode_idx + 1) % NUM_DEMOD_MODES;
        } else {
            g_wifi_on = !g_wifi_on;
        }
        update_freq_display();
        update_cards_display();
        lv_obj_set_style_bg_color(g_wifi_dot, g_wifi_on ? COLOR_GREEN : COLOR_TEXT_DIM, 0);
    } else if (btn == BTN_ENTER) {
        handle_card_enter();
    } else if (btn == BTN_HOME) {
        g_selected_card = 0;
        g_step_idx = 2;
        g_mode_idx = DEMOD_USB;
        g_freq_hz = 14250000;
        update_card_highlight();
        update_freq_display();
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
    if (!g_mayday_active || !g_border_flash) return;
    g_mayday_flash_on = !g_mayday_flash_on;
    lv_obj_set_style_border_color(g_border_flash,
        g_mayday_flash_on ? COLOR_RED : COLOR_BG, 0);
    lv_obj_set_style_border_width(g_border_flash, g_mayday_flash_on ? 3 : 0, 0);
}

static void update_spectrum(void)
{
    static int spec_drift[SPEC_BARS];
    for (int i = 0; i < SPEC_BARS; i++) {
        int change = (rand() % 5) - 2;
        spec_drift[i] += change;
        if (spec_drift[i] < 5) spec_drift[i] = 5;
        if (spec_drift[i] > 95) spec_drift[i] = 95;
        g_spec_values[i] = spec_drift[i];
    }

    int peak_pos = SPEC_BARS / 2 + (rand() % 11) - 5;
    int peak_h = 60 + (rand() % 35);
    if (peak_pos >= 0 && peak_pos < SPEC_BARS) {
        g_spec_values[peak_pos] = peak_h;
        if (peak_pos > 0) g_spec_values[peak_pos - 1] = peak_h - 10;
        if (peak_pos < SPEC_BARS - 1) g_spec_values[peak_pos + 1] = peak_h - 10;
    }

    int bar_w = (LCD_W - 50) / SPEC_BARS;
    int bar_area_h = SPECTRUM_H - 28;
    for (int i = 0; i < SPEC_BARS; i++) {
        int h = (g_spec_values[i] * bar_area_h) / 100;
        lv_obj_set_size(g_spec_bars[i], bar_w - 1, h);
        lv_obj_set_pos(g_spec_bars[i], 40 + i * bar_w, SPECTRUM_H - 12 - h);
        lv_obj_set_style_bg_color(g_spec_bars[i], spec_bar_color(g_spec_values[i]), 0);
    }

    int noise_y = SPECTRUM_H - 12 - (int)((20 + (rand() % 8)) * bar_area_h / 100);
    lv_obj_set_pos(g_noise_line, 40, noise_y);
}

static void ui_update_timer(lv_timer_t *timer)
{
    (void)timer;

    g_sim_time_sec++;
    int hrs = (g_sim_time_sec / 3600) % 24;
    int mins = (g_sim_time_sec / 60) % 60;
    int secs = g_sim_time_sec % 60;
    char time_buf[16];
    snprintf(time_buf, sizeof(time_buf), "%02d:%02d:%02d", hrs, mins, secs);
    lv_label_set_text(g_time_label, time_buf);

    g_noise_floor = -80.0f + (float)(rand() % 10);
    g_s_meter = 5 + (rand() % 4);
    if ((rand() % 40) == 0) g_s_meter = 9;
    lv_label_set_text_fmt(g_s_meter_label, "S%d", g_s_meter);
    lv_obj_set_style_text_color(g_s_meter_label, s_meter_color(g_s_meter), 0);

    update_spectrum();

    static const char *cw_words[] = {
        "CQ ", "DE ", "BA1AA ", "K ", "UR ", "599 ", "BK ", "73 ",
        "QRZ? ", "ES ", "GB ", "SK ", "RST ", "TU ", "FB "
    };
    if ((rand() % 20) == 0) {
        cw_append_word(cw_words[rand() % (sizeof(cw_words) / sizeof(cw_words[0]))]);
    }

    if ((rand() % 150) == 0) {
        g_qso_count++;
    }

    update_cards_display();

    if ((rand() % 500) == 0 && !g_mayday_active && !g_overlay_open) {
        g_mayday_active = true;
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

static lv_obj_t *create_bar(lv_obj_t *parent, int x, int y, int w, int h, lv_color_t c)
{
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_set_size(bar, w, h);
    lv_obj_set_pos(bar, x, y);
    lv_obj_set_style_bg_color(bar, c, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    return bar;
}

static lv_obj_t *create_label(lv_obj_t *parent, int x, int y, const char *txt,
                               const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, txt);
    lv_obj_set_style_text_color(lbl, color, 0);
    lv_obj_set_style_text_font(lbl, font, 0);
    if (x >= 0 && y >= 0) {
        lv_obj_set_pos(lbl, x, y);
    }
    return lbl;
}

static void create_main_screen(void)
{
    g_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_scr, COLOR_BG, 0);
    lv_obj_set_style_pad_all(g_scr, 0, 0);
    lv_obj_set_style_radius(g_scr, 0, 0);
    lv_obj_set_style_border_width(g_scr, 0, 0);

    g_border_flash = lv_obj_create(g_scr);
    lv_obj_set_size(g_border_flash, LCD_W, LCD_H);
    lv_obj_set_pos(g_border_flash, 0, 0);
    lv_obj_set_style_bg_opa(g_border_flash, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(g_border_flash, COLOR_BG, 0);
    lv_obj_set_style_border_width(g_border_flash, 0, 0);
    lv_obj_set_style_radius(g_border_flash, 0, 0);
    lv_obj_set_style_pad_all(g_border_flash, 0, 0);

    lv_obj_t *topbar = lv_obj_create(g_scr);
    lv_obj_set_size(topbar, LCD_W, TOPBAR_H);
    lv_obj_set_pos(topbar, 0, 0);
    lv_obj_set_style_bg_color(topbar, COLOR_BAR, 0);
    lv_obj_set_style_border_width(topbar, 0, 0);
    lv_obj_set_style_pad_all(topbar, 0, 0);
    lv_obj_set_style_radius(topbar, 0, 0);

    g_time_label = create_label(topbar, 6, 5, "08:01:15",
                                &lv_font_montserrat_12, COLOR_TEXT);

    g_freq_label = create_label(topbar, -1, 2, "14.250.000",
                                &lv_font_montserrat_18, COLOR_ACCENT);
    lv_obj_center(g_freq_label);
    lv_obj_set_pos(g_freq_label, -20, 2);

    g_mode_label = create_label(topbar, -1, 8, "USB",
                                &lv_font_montserrat_10, COLOR_ACCENT);
    lv_obj_align(g_mode_label, LV_ALIGN_CENTER, 58, 0);

    g_step_label = create_label(topbar, -1, 10, "1kHz",
                                &lv_font_montserrat_10, COLOR_AMBER);
    lv_obj_align(g_step_label, LV_ALIGN_CENTER, 80, 8);

    g_wifi_dot = lv_obj_create(topbar);
    lv_obj_set_size(g_wifi_dot, 5, 5);
    lv_obj_set_style_bg_color(g_wifi_dot, g_wifi_on ? COLOR_GREEN : COLOR_TEXT_DIM, 0);
    lv_obj_set_style_radius(g_wifi_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(g_wifi_dot, 0, 0);
    lv_obj_set_style_pad_all(g_wifi_dot, 0, 0);
    lv_obj_align(g_wifi_dot, LV_ALIGN_RIGHT_MID, -24, 0);
    create_label(topbar, -1, -1, "W", &lv_font_montserrat_10,
                 g_wifi_on ? COLOR_GREEN : COLOR_TEXT_DIM);
    lv_obj_t *w_lbl = lv_obj_get_child(topbar, -1);
    lv_obj_align(w_lbl, LV_ALIGN_RIGHT_MID, -16, 0);

    g_bt_dot = lv_obj_create(topbar);
    lv_obj_set_size(g_bt_dot, 5, 5);
    lv_obj_set_style_bg_color(g_bt_dot, g_bt_on ? COLOR_CYAN : COLOR_TEXT_DIM, 0);
    lv_obj_set_style_radius(g_bt_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(g_bt_dot, 0, 0);
    lv_obj_set_style_pad_all(g_bt_dot, 0, 0);
    lv_obj_align(g_bt_dot, LV_ALIGN_RIGHT_MID, -10, 0);
    create_label(topbar, -1, -1, "B", &lv_font_montserrat_10,
                 g_bt_on ? COLOR_CYAN : COLOR_TEXT_DIM);
    lv_obj_t *b_lbl = lv_obj_get_child(topbar, -1);
    lv_obj_align(b_lbl, LV_ALIGN_RIGHT_MID, -2, 0);

    lv_obj_t *spectrum_bg = lv_obj_create(g_scr);
    lv_obj_set_size(spectrum_bg, LCD_W, SPECTRUM_H);
    lv_obj_set_pos(spectrum_bg, 0, SPECTRUM_Y);
    lv_obj_set_style_bg_color(spectrum_bg, COLOR_SPEC_BG, 0);
    lv_obj_set_style_border_width(spectrum_bg, 0, 0);
    lv_obj_set_style_pad_all(spectrum_bg, 0, 0);
    lv_obj_set_style_radius(spectrum_bg, 0, 0);

    g_s_meter_label = create_label(spectrum_bg, 6, 4, "S6",
                                   &lv_font_montserrat_12, COLOR_GREEN);

    for (int i = 0; i < 9; i++) {
        char s[4];
        snprintf(s, sizeof(s), "%d", i + 1);
        lv_color_t c = s_meter_color(i + 1);
        create_label(spectrum_bg, 8 + i * 4, SPECTRUM_H - 10, s,
                     &lv_font_montserrat_10, COLOR_TEXT_DIM);
        (void)c;
    }
    create_label(spectrum_bg, 8 + 8 * 4, SPECTRUM_H - 10, "+",
                 &lv_font_montserrat_10, COLOR_TEXT_DIM);
    create_label(spectrum_bg, 8 + 8 * 4 + 6, SPECTRUM_H - 10, "10",
                 &lv_font_montserrat_10, COLOR_RED);

    create_label(spectrum_bg, 4, 20, "S", &lv_font_montserrat_10, COLOR_TEXT_DIM);

    for (int i = 0; i < SPEC_BARS; i++) {
        g_spec_bars[i] = create_bar(spectrum_bg, 40 + i * 4, SPECTRUM_H - 20,
                                    3, 10, lv_color_hex(0x103060));
        g_spec_values[i] = 20;
    }

    g_noise_line = create_bar(spectrum_bg, 40, SPECTRUM_H - 25, LCD_W - 45, 1,
                              lv_color_hex(0x405060));

    lv_obj_t *content_bg = lv_obj_create(g_scr);
    lv_obj_set_size(content_bg, LCD_W, CONTENT_H);
    lv_obj_set_pos(content_bg, 0, CONTENT_Y);
    lv_obj_set_style_bg_color(content_bg, COLOR_BG, 0);
    lv_obj_set_style_border_width(content_bg, 0, 0);
    lv_obj_set_style_pad_all(content_bg, 0, 0);
    lv_obj_set_style_radius(content_bg, 0, 0);

    lv_obj_t *cw_box = lv_obj_create(content_bg);
    lv_obj_set_size(cw_box, CARDS_START_X - 4, CONTENT_H - 4);
    lv_obj_set_pos(cw_box, 4, 2);
    lv_obj_set_style_bg_color(cw_box, COLOR_SPEC_BG, 0);
    lv_obj_set_style_border_color(cw_box, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(cw_box, 1, 0);
    lv_obj_set_style_radius(cw_box, 4, 0);
    lv_obj_set_style_pad_all(cw_box, 4, 0);

    create_label(cw_box, 4, 2, "CW DECODE", &lv_font_montserrat_10, COLOR_ACCENT);
    memset(g_cw_lines, 0, sizeof(g_cw_lines));
    strncpy(g_cw_lines[0], "CQ CQ DE BA1AA...", sizeof(g_cw_lines[0]) - 1);
    for (int i = 0; i < CW_LINES; i++) {
        g_cw_labels[i] = create_label(cw_box, 4, 14 + i * 20, g_cw_lines[i],
                                      &lv_font_montserrat_10, COLOR_TEXT);
    }

    const lv_color_t card_title_colors[NUM_CARDS] = {
        COLOR_RED, COLOR_GREEN, COLOR_ACCENT, COLOR_CYAN, COLOR_AMBER, COLOR_TEXT_DIM
    };

    for (int row = 0; row < CARD_ROWS; row++) {
        for (int col = 0; col < CARD_COLS; col++) {
            int idx = row * CARD_COLS + col;
            int x = CARDS_START_X + CARD_PAD + col * (CARD_W + CARD_GAP_X);
            int y = CARD_PAD + row * (CARD_H + CARD_GAP_Y);
            g_cards[idx] = lv_obj_create(content_bg);
            lv_obj_set_size(g_cards[idx], CARD_W, CARD_H);
            lv_obj_set_pos(g_cards[idx], x, y);
            lv_obj_set_style_bg_color(g_cards[idx], COLOR_CARD, 0);
            lv_obj_set_style_border_color(g_cards[idx], COLOR_CARD_BORDER, 0);
            lv_obj_set_style_border_width(g_cards[idx], 1, 0);
            lv_obj_set_style_radius(g_cards[idx], 4, 0);
            lv_obj_set_style_pad_all(g_cards[idx], 4, 0);

            create_label(g_cards[idx], 4, 1, CARD_TITLES[idx],
                         &lv_font_montserrat_10, card_title_colors[idx]);

            g_card_val_labels[idx] = create_label(g_cards[idx], 4, 18, "",
                                                  &lv_font_montserrat_12, COLOR_TEXT);
        }
    }

    lv_obj_t *bottombar = lv_obj_create(g_scr);
    lv_obj_set_size(bottombar, LCD_W, BOTTOMBAR_H);
    lv_obj_set_pos(bottombar, 0, BOTTOMBAR_Y);
    lv_obj_set_style_bg_color(bottombar, COLOR_BAR, 0);
    lv_obj_set_style_border_width(bottombar, 0, 0);
    lv_obj_set_style_pad_all(bottombar, 0, 0);
    lv_obj_set_style_radius(bottombar, 0, 0);

    g_hint_label = create_label(bottombar, -1, -1,
        "VOL+:Tune  MENU:Mode  ENTER:Select  HOME:Main",
        &lv_font_montserrat_10, COLOR_TEXT_DIM);
    lv_obj_center(g_hint_label);

    lv_scr_load(g_scr);
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("========================================\n");
    printf("  AI Radio Console v%s for openvela\n", RADIO_APP_VERSION);
    printf("  Landscape 320x240 - Gemini-S1 (R528)\n");
    printf("  Contest 2026 - Team 095 (BI4MIB)\n");
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

    printf("[UI] Creating LVGL landscape 320x240 interface...\n");
    lv_init();

    lv_nuttx_dsc_t dsc;
    lv_nuttx_result_t nuttx_res;
    lv_nuttx_dsc_init(&dsc);
    dsc.fb_path = "/dev/lcd0";
    dsc.input_path = "/dev/input0";
    dsc.utouch_path = NULL;
    lv_nuttx_init(&dsc, &nuttx_res);

    if(nuttx_res.disp == NULL) {
        syslog(LOG_ERR, "ai_radio: failed to open /dev/lcd0\n");
    }
    if(nuttx_res.indev == NULL) {
        syslog(LOG_WARNING, "ai_radio: failed to open /dev/input0 (touch)\n");
    }

    create_main_screen();
    update_freq_display();
    update_cards_display();
    update_card_highlight();
    update_spectrum();

    lv_timer_create(ui_update_timer, UI_REFRESH_MS, NULL);
    lv_timer_create(mayday_flash_timer, 300, NULL);

    button_init();
    lv_timer_create(button_poll_timer, BTN_POLL_MS, NULL);

    printf("[UI] Interface created.\n");
    printf("[INFO] LRADC buttons: Vol-/+, Menu, Enter, Home\n");
    printf("[INFO] System ready. Entering UI loop...\n");

    while (g_running) {
        uint32_t idle = lv_timer_handler();
        if(idle < 1) idle = 1;
        if(idle > BTN_POLL_MS) idle = BTN_POLL_MS;
        usleep(idle * 1000);
    }

    if (g_btn_fd >= 0) close(g_btn_fd);
    printf("[SHUTDOWN] AI Radio Console exiting.\n");
    return 0;
}
