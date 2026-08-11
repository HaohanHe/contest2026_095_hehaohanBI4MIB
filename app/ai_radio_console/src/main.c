/****************************************************************************
 * DayNote - Daily Voice Notes for openvela
 *
 * Product:全天候语音记忆设备
 * Core flow: VAD auto-record -> WAV -> ASR (SiliconFlow) -> LLM summary ->
 *           HippocampusIndex -> DailyDigest -> UI display -> Phone sync
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <pthread.h>

#include <nuttx/input/buttons.h>
#include <nuttx/audio/audio.h>

#include <lvgl.h>
#include <src/drivers/nuttx/lv_nuttx_entry.h>

#include "radio_config.h"
#include "input_lradc.h"
#include "wifi_auto_connect.h"
#include "siliconflow_client.h"
#include "spacelog_settings.h"
#include "ai_radio_font.h"
#include "note_store.h"
#include "memory_index.h"
#include "daily_digest.h"
#include "auto_recorder.h"
#include "ui_daynote.h"

/* Screen dimensions */
#define LCD_W               320
#define LCD_H              240

/* Colors */
#define COLOR_BG            lv_color_hex(0x0a0e1a)
#define COLOR_BAR           lv_color_hex(0x0d1220)
#define COLOR_TEXT          lv_color_hex(0xe0e8f0)
#define COLOR_TEXT_DIM      lv_color_hex(0x8899aa)
#define COLOR_ACCENT        lv_color_hex(0x00d4ff)
#define COLOR_GREEN         lv_color_hex(0x33dd66)
#define COLOR_RED           lv_color_hex(0xff3344)
#define COLOR_AMBER         lv_color_hex(0xffaa00)
#define COLOR_CARD          lv_color_hex(0x141c30)
#define COLOR_CARD_BORDER   lv_color_hex(0x253048)

/* Button constants */
#define BTN_POLL_MS         50
#define BTN_DEBOUNCE_MS     200
#define BTN_LONGPRESS_MS    800

/* Global state */
static lv_obj_t *g_scr = NULL;
static lv_obj_t *g_status_label = NULL;
static bool g_disp_available = true;
static volatile bool g_running = true;
static int g_audio_fd = -1;

/* Pending button event */
typedef struct {
    btn_buttonset_t btn;
    bool long_press;
    volatile bool pending;
} pending_btn_event_t;

static pending_btn_event_t g_pending_btn = {0, false, false};
static pthread_mutex_t g_btn_event_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Timer */
static lv_timer_t *g_ui_update_timer = NULL;
static lv_timer_t *g_button_poll_timer = NULL;

/****************************************************************************
 * Helper functions
 ****************************************************************************/

static lv_obj_t *create_label(lv_obj_t *parent, int x, int y, const char *txt,
                              const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, txt);
    lv_obj_set_style_text_color(lbl, color, 0);
    if (font) {
        lv_obj_set_style_text_font(lbl, font, 0);
    }
    if (x >= 0 && y >= 0) {
        lv_obj_set_pos(lbl, x, y);
    }
    return lbl;
}

/****************************************************************************
 * Audio setup (raw read mode for auto_recorder)
 ****************************************************************************/

static int setup_audio(void)
{
    g_audio_fd = open(AUDIO_CAPTURE_DEV, O_RDWR);
    if (g_audio_fd < 0) {
        printf("[AUDIO] Failed to open %s: %d\n", AUDIO_CAPTURE_DEV, errno);
        return -1;
    }

    int ret = ioctl(g_audio_fd, AUDIOIOC_RESERVE, 0);
    if (ret < 0) {
        printf("[AUDIO] Reserve failed: %d\n", errno);
        close(g_audio_fd);
        g_audio_fd = -1;
        return -1;
    }

    struct audio_caps_desc_s cap_desc;
    memset(&cap_desc, 0, sizeof(cap_desc));
    cap_desc.caps.ac_len = sizeof(struct audio_caps_s);
    cap_desc.caps.ac_type = AUDIO_TYPE_INPUT;
    cap_desc.caps.ac_channels = AUDIO_CHANNELS;
    cap_desc.caps.ac_controls.hw[0] = AUDIO_SAMPLE_RATE;
    cap_desc.caps.ac_controls.b[3] = (AUDIO_SAMPLE_RATE >> 16) & 0xff;
    cap_desc.caps.ac_controls.b[2] = AUDIO_BITS_PER_SAMPLE;
    cap_desc.caps.ac_subtype = AUDIO_FMT_PCM;

    ret = ioctl(g_audio_fd, AUDIOIOC_CONFIGURE, (unsigned long)&cap_desc);
    if (ret < 0) {
        printf("[AUDIO] Configure failed: %d\n", errno);
        ioctl(g_audio_fd, AUDIOIOC_RELEASE, 0);
        close(g_audio_fd);
        g_audio_fd = -1;
        return -1;
    }

    ret = ioctl(g_audio_fd, AUDIOIOC_START, 0);
    if (ret < 0) {
        printf("[AUDIO] Start failed: %d\n", errno);
        ioctl(g_audio_fd, AUDIOIOC_STOP, 0);
        ioctl(g_audio_fd, AUDIOIOC_RELEASE, 0);
        close(g_audio_fd);
        g_audio_fd = -1;
        return -1;
    }

    printf("[AUDIO] Capture started, fd=%d, rate=%d\n", g_audio_fd, AUDIO_SAMPLE_RATE);
    return 0;
}

/****************************************************************************
 * Button handling
 ****************************************************************************/

static void lradc_button_handler(btn_buttonset_t btn, bool long_press, void *user_data)
{
    (void)user_data;
    pthread_mutex_lock(&g_btn_event_mutex);
    g_pending_btn.btn = btn;
    g_pending_btn.long_press = long_press;
    g_pending_btn.pending = true;
    pthread_mutex_unlock(&g_btn_event_mutex);
}

static void handle_button_press(btn_buttonset_t btn, bool long_press)
{
    if (!g_disp_available) return;

    if (spacelog_settings_is_visible()) {
        if (btn == LRADC_BTN_HOME) {
            spacelog_settings_hide();
        }
        return;
    }

    if (btn == LRADC_BTN_HOME && long_press) {
        spacelog_settings_show();
        return;
    }
}

static void button_poll_timer(lv_timer_t *timer)
{
    (void)timer;
    btn_buttonset_t btn = 0;
    bool long_press = false;
    bool has_event = false;

    pthread_mutex_lock(&g_btn_event_mutex);
    if (g_pending_btn.pending) {
        btn = g_pending_btn.btn;
        long_press = g_pending_btn.long_press;
        g_pending_btn.pending = false;
        has_event = true;
    }
    pthread_mutex_unlock(&g_btn_event_mutex);

    if (has_event) {
        handle_button_press(btn, long_press);
    }
}

/****************************************************************************
 * UI update timer
 ****************************************************************************/

static void ui_update_timer(lv_timer_t *timer)
{
    (void)timer;

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    if (tm_info) {
        char time_buf[16];
        strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tm_info);
        if (g_status_label) {
            lv_label_set_text(g_status_label, time_buf);
        }
    }

    if (ui_daynote_is_visible()) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d notes", note_store_count());
        lv_obj_t *count_label = ui_daynote_get_count_label();
        if (count_label) {
            lv_label_set_text(count_label, buf);
        }

        int count = note_store_count();
        if (count > 0) {
            const note_t *last = note_store_get_last();
            if (last && last->transcript && last->summary) {
                ui_daynote_set_result(last->transcript, last->summary);
            }
        }
    }
}

/****************************************************************************
 * Create main screen
 ****************************************************************************/

static void create_main_screen(void)
{
    g_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_scr, COLOR_BG, 0);
    lv_obj_set_style_pad_all(g_scr, 0, 0);
    lv_obj_set_style_radius(g_scr, 0, 0);
    lv_obj_set_style_border_width(g_scr, 0, 0);
    lv_obj_set_style_text_font(g_scr, &ai_radio_font, 0);

    /* Top bar: time */
    lv_obj_t *topbar = lv_obj_create(g_scr);
    lv_obj_set_size(topbar, LCD_W, 26);
    lv_obj_set_pos(topbar, 0, 0);
    lv_obj_set_style_bg_color(topbar, COLOR_BAR, 0);
    lv_obj_set_style_border_width(topbar, 0, 0);
    lv_obj_set_style_pad_all(topbar, 0, 0);
    lv_obj_set_style_radius(topbar, 0, 0);

    g_status_label = create_label(topbar, 4, 5, "00:00:00", NULL, COLOR_TEXT);

    lv_scr_load(g_scr);
}

/****************************************************************************
 * Main entry point
 ****************************************************************************/

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("========================================\n");
    printf("  DayNote v1.0 for openvela\n");
    printf("  Daily Voice Notes - Gemini-S1 (R528)\n");
    printf("  Landscape 320x240\n");
    printf("========================================\n");

    /* Load AI config and init SiliconFlow client */
    ai_config_t ai_config;
    radio_config_store_load(&ai_config);
    sf_client_init(&ai_config);
    printf("[INIT] SiliconFlow client initialized (ASR=%s, LLM=%s)\n",
           ai_config.asr_model, ai_config.llm_model);

    /* Initialize subsystems */
    note_store_init();
    memory_index_load();
    printf("[INIT] Note store and memory index loaded\n");

    /* Defer daily digest to avoid blocking startup on LLM */
    printf("[INIT] Skipping daily digest at startup (will generate on first note)\n");

    wifi_auto_connect_start();

    /* Setup audio and start auto recorder */
    if (setup_audio() == 0) {
        auto_recorder_init(g_audio_fd, AUDIO_SAMPLE_RATE);
        printf("[INIT] Auto recorder started\n");
    }

    printf("[UI] Creating LVGL interface...\n");
    lv_init();

    lv_nuttx_dsc_t dsc;
    lv_nuttx_result_t nuttx_res;
    lv_nuttx_dsc_init(&dsc);
    dsc.fb_path = "/dev/fb0";
    dsc.input_path = "/dev/input0";
    dsc.utouch_path = NULL;
    lv_nuttx_init(&dsc, &nuttx_res);

    if (nuttx_res.disp == NULL) {
        syslog(LOG_ERR, "daynote: failed to open /dev/lcd0\n");
        g_disp_available = false;
    }

    if (g_disp_available) {
        create_main_screen();
        ui_daynote_init();
        ui_daynote_show();
        spacelog_settings_init();
        spacelog_settings_create(ui_daynote_get_screen());
        g_ui_update_timer = lv_timer_create(ui_update_timer, 100, NULL);
        g_button_poll_timer = lv_timer_create(button_poll_timer, BTN_POLL_MS, NULL);
    }

    if (input_lradc_init() != 0) {
        printf("[INIT] input_lradc_init failed\n");
    }
    input_lradc_set_button_callback(lradc_button_handler, NULL);
    input_lradc_start();

    printf("[UI] Interface created. Entering main loop...\n");

    while (g_running) {
        if (g_disp_available) {
            uint32_t idle = lv_timer_handler();
            if (idle < 1) idle = 1;
            if (idle > BTN_POLL_MS) idle = BTN_POLL_MS;
            usleep(idle * 1000);
        } else {
            usleep(BTN_POLL_MS * 1000);
        }
    }

    g_running = false;
    if (g_ui_update_timer) lv_timer_del(g_ui_update_timer);
    if (g_button_poll_timer) lv_timer_del(g_button_poll_timer);

    input_lradc_stop();
    auto_recorder_deinit();

    if (g_audio_fd >= 0) {
        ioctl(g_audio_fd, AUDIOIOC_STOP, 0);
        ioctl(g_audio_fd, AUDIOIOC_RELEASE, 0);
        close(g_audio_fd);
        g_audio_fd = -1;
    }

    note_store_deinit();
    memory_index_save();

    printf("[SHUTDOWN] Exiting.\n");
    return 0;
}
