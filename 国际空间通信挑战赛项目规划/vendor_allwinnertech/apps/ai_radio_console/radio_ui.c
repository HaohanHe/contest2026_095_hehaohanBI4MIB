/****************************************************************************
 * apps/vendor/allwinnertech/apps/ai_radio_console/radio_ui.c
 *
 * LVGL UI for AI Radio Console
 *
 * Thread-safety note: all lv_* calls must run on the LVGL thread.  Public
 * update functions (radio_ui_set_* and radio_ui_add_log) may be called from
 * worker threads; they allocate a small payload and use lv_async_call() to
 * marshal the update onto the LVGL thread.  This follows the same pattern
 * used in packages_demos/mini_memo/mini_memo_ui.c.
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <lvgl/lvgl.h>
#include <sys/boardctl.h>

#include "radio_ui.h"

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

#undef NEED_BOARDINIT
#if defined(CONFIG_BOARDCTL) && !defined(CONFIG_NSH_ARCHINIT)
#  define NEED_BOARDINIT 1
#endif

static lv_obj_t *g_status_label;
static lv_obj_t *g_state_label;
static lv_obj_t *g_transcript_label;
static lv_obj_t *g_log_label;
static lv_obj_t *g_ptt_btn;
static lv_obj_t *g_ptt_label;
static lv_obj_t *g_scr;

static radio_ui_state_t g_state = RADIO_UI_IDLE;
static bool g_ptt_pressed = false;

/* Chinese-capable fonts. FreeType MiSans is preferred; fall back to
 * built-in CJK font or Montserrat if unavailable. */
static lv_font_t *g_font_cjk_12;
static lv_font_t *g_font_cjk_16;

/* Async payload types: must be malloc'd because lv_async_call owns them. */
typedef struct {
    radio_ui_state_t state;
} ui_state_evt_t;

typedef struct {
    char text[256];
} ui_text_evt_t;

typedef struct {
    char timestamp[32];
    char text[256];
} ui_log_evt_t;

static const char *state_text(radio_ui_state_t state)
{
    switch (state) {
    case RADIO_UI_RECORDING:
        return "录音中...";
    case RADIO_UI_TRANSCRIBING:
        return "识别中...";
    case RADIO_UI_THINKING:
        return "AI 整理中...";
    case RADIO_UI_ERROR:
        return "错误";
    default:
        return "空闲";
    }
}

static lv_color_t state_color(radio_ui_state_t state)
{
    switch (state) {
    case RADIO_UI_RECORDING:
        return lv_color_hex(0xE74C3C);
    case RADIO_UI_TRANSCRIBING:
        return lv_color_hex(0xF1C40F);
    case RADIO_UI_THINKING:
        return lv_color_hex(0x3498DB);
    case RADIO_UI_ERROR:
        return lv_color_hex(0xE74C3C);
    default:
        return lv_color_hex(0x2ECC71);
    }
}

static void ptt_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) {
        g_ptt_pressed = true;
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        g_ptt_pressed = false;
    }
}

static void init_fonts(void)
{
#ifdef LV_USE_FREETYPE
    /* MiSans path matches ai_chat demo default: /data/res/fonts/MiSans-Normal.ttf */
    g_font_cjk_12 = lv_freetype_font_create("/data/res/fonts/MiSans-Normal.ttf",
                                            LV_FREETYPE_FONT_RENDER_MODE_BITMAP,
                                            12,
                                            LV_FREETYPE_FONT_STYLE_NORMAL);
    g_font_cjk_16 = lv_freetype_font_create("/data/res/fonts/MiSans-Normal.ttf",
                                            LV_FREETYPE_FONT_RENDER_MODE_BITMAP,
                                            16,
                                            LV_FREETYPE_FONT_STYLE_NORMAL);
    if (!g_font_cjk_12 || !g_font_cjk_16) {
        LV_LOG_WARN("MiSans load failed, falling back to built-in CJK font");
    }
#endif

    if (!g_font_cjk_12) {
#ifdef LV_FONT_SIMSUN_16_CJK
        g_font_cjk_12 = &lv_font_simsun_16_cjk;
#else
        g_font_cjk_12 = &lv_font_montserrat_12;
#endif
    }

    if (!g_font_cjk_16) {
#ifdef LV_FONT_SIMSUN_16_CJK
        g_font_cjk_16 = &lv_font_simsun_16_cjk;
#else
        g_font_cjk_16 = &lv_font_montserrat_16;
#endif
    }
}

static void create_main_screen(void)
{
    g_scr = lv_scr_act();
    lv_obj_set_style_bg_color(g_scr, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_bg_opa(g_scr, LV_OPA_COVER, 0);

    /* Top status bar */
    lv_obj_t *top_bar = lv_obj_create(g_scr);
    lv_obj_set_size(top_bar, SCREEN_WIDTH, 32);
    lv_obj_set_pos(top_bar, 0, 0);
    lv_obj_set_style_bg_color(top_bar, lv_color_hex(0x16213E), 0);
    lv_obj_set_style_bg_opa(top_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(top_bar, 0, 0);
    lv_obj_set_style_radius(top_bar, 0, 0);
    lv_obj_set_style_pad_all(top_bar, 4, 0);

    g_state_label = lv_label_create(top_bar);
    lv_label_set_text(g_state_label, state_text(RADIO_UI_IDLE));
    lv_obj_set_style_text_color(g_state_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(g_state_label, g_font_cjk_12, 0);
    lv_obj_align(g_state_label, LV_ALIGN_LEFT_MID, 5, 0);

    g_status_label = lv_label_create(top_bar);
    lv_label_set_text(g_status_label, "AI Radio Console");
    lv_obj_set_style_text_color(g_status_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(g_status_label, g_font_cjk_12, 0);
    lv_obj_align(g_status_label, LV_ALIGN_RIGHT_MID, -5, 0);

    /* Log / transcript area */
    lv_obj_t *log_cont = lv_obj_create(g_scr);
    lv_obj_set_size(log_cont, SCREEN_WIDTH - 10, 130);
    lv_obj_set_pos(log_cont, 5, 37);
    lv_obj_set_style_bg_color(log_cont, lv_color_hex(0x0F3460), 0);
    lv_obj_set_style_bg_opa(log_cont, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(log_cont, 8, 0);
    lv_obj_set_style_border_width(log_cont, 0, 0);
    lv_obj_set_style_pad_all(log_cont, 6, 0);

    g_transcript_label = lv_label_create(log_cont);
    lv_label_set_text(g_transcript_label, "最近转写: (无)");
    lv_obj_set_style_text_color(g_transcript_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(g_transcript_label, g_font_cjk_12, 0);
    lv_obj_align(g_transcript_label, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_width(g_transcript_label, SCREEN_WIDTH - 26);
    lv_label_set_long_mode(g_transcript_label, LV_LABEL_LONG_WRAP);

    g_log_label = lv_label_create(log_cont);
    lv_label_set_text(g_log_label, "日志: (无)");
    lv_obj_set_style_text_color(g_log_label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_text_font(g_log_label, g_font_cjk_12, 0);
    lv_obj_align_to(g_log_label, g_transcript_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 6);
    lv_obj_set_width(g_log_label, SCREEN_WIDTH - 26);
    lv_label_set_long_mode(g_log_label, LV_LABEL_LONG_WRAP);

    /* Bottom PTT button */
    g_ptt_btn = lv_btn_create(g_scr);
    lv_obj_set_size(g_ptt_btn, SCREEN_WIDTH - 20, 60);
    lv_obj_align(g_ptt_btn, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_bg_color(g_ptt_btn, lv_color_hex(0x27AE60), 0);
    lv_obj_set_style_radius(g_ptt_btn, 12, 0);
    lv_obj_add_event_cb(g_ptt_btn, ptt_btn_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(g_ptt_btn, ptt_btn_event_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(g_ptt_btn, ptt_btn_event_cb, LV_EVENT_PRESS_LOST, NULL);

    g_ptt_label = lv_label_create(g_ptt_btn);
    lv_label_set_text(g_ptt_label, "按住录音");
    lv_obj_set_style_text_color(g_ptt_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(g_ptt_label, g_font_cjk_16, 0);
    lv_obj_center(g_ptt_label);
}

int radio_ui_init(void)
{
    if (lv_is_initialized()) {
        LV_LOG_ERROR("LVGL already initialized");
        return -1;
    }

#ifdef NEED_BOARDINIT
    boardctl(BOARDIOC_INIT, 0);
#endif

    lv_init();

    lv_nuttx_dsc_t info;
    lv_nuttx_result_t result;
    lv_nuttx_dsc_init(&info);

#ifdef CONFIG_LV_USE_NUTTX_LCD
    info.fb_path = "/dev/lcd0";
#endif
    if (CONFIG_AI_RADIO_CONSOLE_INPUT_DEV[0] != '\0') {
        info.input_path = CONFIG_AI_RADIO_CONSOLE_INPUT_DEV;
    }

    lv_nuttx_init(&info, &result);
    usleep(100000);

    if (result.disp == NULL) {
        LV_LOG_ERROR("LVGL init failed");
        return -1;
    }

    init_fonts();
    create_main_screen();
    return 0;
}

void radio_ui_run(void)
{
    while (1) {
        uint32_t idle = lv_timer_handler();
        usleep(idle ? idle * 1000 : 5000);
    }
}

/* -- LVGL-thread callbacks for async updates ----------------------------- */

static void async_set_state_cb(void *p)
{
    ui_state_evt_t *evt = (ui_state_evt_t *)p;
    if (!evt) {
        return;
    }

    g_state = evt->state;
    if (g_state_label) {
        lv_label_set_text(g_state_label, state_text(evt->state));
        lv_obj_set_style_text_color(g_state_label, state_color(evt->state), 0);
    }
    if (g_ptt_btn) {
        if (evt->state == RADIO_UI_RECORDING) {
            lv_obj_set_style_bg_color(g_ptt_btn, lv_color_hex(0xC0392B), 0);
            lv_label_set_text(g_ptt_label, "录音中...");
        } else {
            lv_obj_set_style_bg_color(g_ptt_btn, lv_color_hex(0x27AE60), 0);
            lv_label_set_text(g_ptt_label, "按住录音");
        }
    }
    free(evt);
}

static void async_set_status_cb(void *p)
{
    ui_text_evt_t *evt = (ui_text_evt_t *)p;
    if (!evt) {
        return;
    }
    if (g_status_label) {
        lv_label_set_text(g_status_label, evt->text);
    }
    free(evt);
}

static void async_set_transcript_cb(void *p)
{
    ui_text_evt_t *evt = (ui_text_evt_t *)p;
    if (!evt) {
        return;
    }
    if (g_transcript_label) {
        static char buf[256];
        snprintf(buf, sizeof(buf), "最近转写: %s", evt->text);
        lv_label_set_text(g_transcript_label, buf);
    }
    free(evt);
}

static void async_add_log_cb(void *p)
{
    ui_log_evt_t *evt = (ui_log_evt_t *)p;
    if (!evt) {
        return;
    }

    if (g_log_label) {
        static char log_buf[512];
        static int log_lines = 0;

        if (log_lines == 0) {
            log_buf[0] = '\0';
        }

        char line[192];
        snprintf(line, sizeof(line), "%s %s\n",
                 evt->timestamp[0] ? evt->timestamp : "", evt->text);

        size_t cur = strlen(log_buf);
        size_t add = strlen(line);
        if (cur + add >= sizeof(log_buf)) {
            /* Drop oldest line */
            char *next = strchr(log_buf, '\n');
            if (next) {
                memmove(log_buf, next + 1, strlen(next + 1) + 1);
                cur = strlen(log_buf);
            } else {
                log_buf[0] = '\0';
                cur = 0;
            }
            if (log_lines > 0) {
                log_lines--;
            }
        }

        snprintf(log_buf + cur, sizeof(log_buf) - cur, "%s", line);
        log_lines++;

        lv_label_set_text(g_log_label, log_buf);
    }
    free(evt);
}

void radio_ui_set_state(radio_ui_state_t state)
{
    ui_state_evt_t *evt = malloc(sizeof(*evt));
    if (!evt) {
        return;
    }
    evt->state = state;
    lv_async_call(async_set_state_cb, evt);
}

void radio_ui_add_log(const char *timestamp, const char *text)
{
    if (!text) {
        return;
    }

    ui_log_evt_t *evt = malloc(sizeof(*evt));
    if (!evt) {
        return;
    }

    strncpy(evt->timestamp, timestamp ? timestamp : "",
            sizeof(evt->timestamp) - 1);
    evt->timestamp[sizeof(evt->timestamp) - 1] = '\0';
    strncpy(evt->text, text, sizeof(evt->text) - 1);
    evt->text[sizeof(evt->text) - 1] = '\0';

    lv_async_call(async_add_log_cb, evt);
}

void radio_ui_set_transcript(const char *text)
{
    if (!text) {
        return;
    }

    ui_text_evt_t *evt = malloc(sizeof(*evt));
    if (!evt) {
        return;
    }
    strncpy(evt->text, text, sizeof(evt->text) - 1);
    evt->text[sizeof(evt->text) - 1] = '\0';

    lv_async_call(async_set_transcript_cb, evt);
}

void radio_ui_set_status(const char *text)
{
    if (!text) {
        return;
    }

    ui_text_evt_t *evt = malloc(sizeof(*evt));
    if (!evt) {
        return;
    }
    strncpy(evt->text, text, sizeof(evt->text) - 1);
    evt->text[sizeof(evt->text) - 1] = '\0';

    lv_async_call(async_set_status_cb, evt);
}

bool radio_ui_is_ptt_pressed(void)
{
    return g_ptt_pressed;
}
