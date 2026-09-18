/****************************************************************************
 * Copyright (C) 2026 Xiaomi Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>

#include <lvgl/lvgl.h>

#include "daynote_core.h"
#include "daynote_ui.h"
#include "daynote_mmdvm.h"
#include "infra/network_manager.h"
#include "llm/llm_router.h"
#ifdef CONFIG_AI_AGENT_BLE_GATT
#include "infra/ble_gatt.h"
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define UI_BG_COLOR 0x0a0a14
#define UI_ACCENT_COLOR 0x6c5ce7
#define UI_TEXT_MUTED 0x888899
#define UI_CARD_BG 0x16162a
#define MEMO_MAX_DISPLAY 20

/****************************************************************************
 * Private Data
 ****************************************************************************/

static int g_screen_w = 320;
static int g_screen_h = 240;

static lv_obj_t* g_tileview;
static lv_obj_t* g_tiles[MEMO_PAGE_COUNT]; /* Home, Voice, Review, Settings, DMR */
static lv_obj_t* g_nav_dots[MEMO_PAGE_COUNT];
static lv_obj_t* g_nav_bar;
static lv_obj_t* g_home_memo_count;
static lv_obj_t* g_home_todo_count;
static lv_obj_t* g_home_sched_count;
static lv_timer_t* g_flush_timer;
static lv_timer_t* g_remind_timer;
static lv_timer_t* g_dmr_refresh_timer;

/* DMR Hotspot page widgets */
static lv_obj_t* g_dmr_status_lbl;
static lv_obj_t* g_dmr_master_lbl;
static lv_obj_t* g_dmr_call_src_lbl;
static lv_obj_t* g_dmr_call_dst_lbl;
static lv_obj_t* g_dmr_call_slot_lbl;
static lv_obj_t* g_dmr_call_duration_lbl;
static lv_obj_t* g_dmr_call_rssi_lbl;
static lv_obj_t* g_dmr_history_list;
static lv_obj_t* g_dmr_active_card;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static const lv_font_t* get_font_large(void)
{
#if LV_FONT_MONTSERRAT_30
    return &lv_font_montserrat_30;
#else
    return LV_FONT_DEFAULT;
#endif
}

static const lv_font_t* get_font_medium(void)
{
#if LV_FONT_MONTSERRAT_20
    return &lv_font_montserrat_20;
#else
    return LV_FONT_DEFAULT;
#endif
}

static const lv_font_t* get_font_small(void)
{
#if LV_FONT_MONTSERRAT_16
    return &lv_font_montserrat_16;
#else
    return LV_FONT_DEFAULT;
#endif
}

/* -- Periodic flush timer ------------------------------- */

static void flush_timer_cb(lv_timer_t* timer)
{
    (void)timer;
    memo_store_flush();
}

/* -- Reminder check timer ------------------------------ */

static void remind_timer_cb(lv_timer_t* timer)
{
    /* Static to avoid ~21KB stack allocation on the small LVGL thread
     * stack (memo_item_t is ~1KB). LVGL timers are single-threaded, so
     * a static buffer is safe and non-reentrant. */
    static memo_item_t items[MEMO_MAX_DISPLAY];
    int count;
    int i;
    int64_t now;

    (void)timer;
    now = (int64_t)time(NULL);

    count = memo_store_get_due_reminders(now, items, MEMO_MAX_DISPLAY);
    for (i = 0; i < count; i++) {
        memo_ui_show_notification("Reminder", items[i].content);
        memo_store_mark_read(items[i].id);
    }
}

/* -- Navigation dots ----------------------------------- */

static void update_nav_dots(int active_idx)
{
    int i;

    for (i = 0; i < MEMO_PAGE_COUNT; i++) {
        if (g_nav_dots[i] == NULL) {
            continue;
        }
        if (i == active_idx) {
            lv_obj_set_style_bg_color(g_nav_dots[i],
                lv_color_hex(UI_ACCENT_COLOR), 0);
            lv_obj_set_size(g_nav_dots[i], 20, 8);
        } else {
            lv_obj_set_style_bg_color(g_nav_dots[i],
                lv_color_hex(UI_TEXT_MUTED), 0);
            lv_obj_set_size(g_nav_dots[i], 8, 8);
        }
    }
}

static void tileview_changed_cb(lv_event_t* e)
{
    lv_obj_t* tv = lv_event_get_target(e);
    lv_obj_t* tile = lv_tileview_get_tile_active(tv);
    int i;

    for (i = 0; i < MEMO_PAGE_COUNT; i++) {
        if (g_tiles[i] == tile) {
            update_nav_dots(i);
            break;
        }
    }
}

static void create_nav_bar(lv_obj_t* parent)
{
    int i;

    g_nav_bar = lv_obj_create(parent);
    lv_obj_set_size(g_nav_bar, g_screen_w, 30);
    lv_obj_align(g_nav_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(g_nav_bar, lv_color_hex(UI_BG_COLOR), 0);
    lv_obj_set_style_bg_opa(g_nav_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_nav_bar, 0, 0);
    lv_obj_set_style_pad_all(g_nav_bar, 0, 0);
    lv_obj_set_flex_flow(g_nav_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(g_nav_bar, LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(g_nav_bar, 8, 0);
    lv_obj_clear_flag(g_nav_bar, LV_OBJ_FLAG_SCROLLABLE);

    for (i = 0; i < MEMO_PAGE_COUNT; i++) {
        g_nav_dots[i] = lv_obj_create(g_nav_bar);
        lv_obj_set_size(g_nav_dots[i], 8, 8);
        lv_obj_set_style_radius(g_nav_dots[i], 4, 0);
        lv_obj_set_style_bg_color(g_nav_dots[i],
            lv_color_hex(UI_TEXT_MUTED), 0);
        lv_obj_set_style_bg_opa(g_nav_dots[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(g_nav_dots[i], 0, 0);
        lv_obj_clear_flag(g_nav_dots[i], LV_OBJ_FLAG_SCROLLABLE);
    }

    update_nav_dots(0);
}

/* -- Page: Home ---------------------------------------- */

static lv_obj_t* create_stat_card(lv_obj_t* parent,
    const char* icon, const char* label, int count)
{
    char buf[32];
    lv_obj_t* card;
    lv_obj_t* lbl_icon;
    lv_obj_t* lbl_count;
    lv_obj_t* lbl_name;

    card = lv_obj_create(parent);
    lv_obj_set_size(card, (g_screen_w - 80) / 3, 100);
    lv_obj_set_style_bg_color(card, lv_color_hex(UI_CARD_BG), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 16, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lbl_icon = lv_label_create(card);
    lv_label_set_text(lbl_icon, icon);
    lv_obj_set_style_text_font(lbl_icon, get_font_large(), 0);
    lv_obj_set_style_text_color(lbl_icon, lv_color_hex(UI_ACCENT_COLOR), 0);

    snprintf(buf, sizeof(buf), "%d", count);
    lbl_count = lv_label_create(card);
    lv_label_set_text(lbl_count, buf);
    lv_obj_set_style_text_font(lbl_count, get_font_large(), 0);
    lv_obj_set_style_text_color(lbl_count, lv_color_white(), 0);

    lbl_name = lv_label_create(card);
    lv_label_set_text(lbl_name, label);
    lv_obj_set_style_text_font(lbl_name, get_font_small(), 0);
    lv_obj_set_style_text_color(lbl_name, lv_color_hex(UI_TEXT_MUTED), 0);

    return lbl_count;
}

static lv_obj_t* g_home_timeline;
static lv_obj_t* g_home_date_lbl;
static lv_obj_t* g_home_stat_lbl;

static void format_time_ago(int64_t timestamp, char* buf, size_t buf_sz)
{
    int64_t now = (int64_t)time(NULL);
    int64_t diff = now - timestamp;
    if (diff < 60) {
        snprintf(buf, buf_sz, "just now");
    } else if (diff < 3600) {
        snprintf(buf, buf_sz, "%lldm ago", (long long)(diff / 60));
    } else if (diff < 86400) {
        snprintf(buf, buf_sz, "%lldh ago", (long long)(diff / 3600));
    } else {
        snprintf(buf, buf_sz, "%lldd ago", (long long)(diff / 86400));
    }
}

static void refresh_home_timeline(void)
{
    /* Static to keep ~5KB memo array off the LVGL thread stack. */
    static memo_item_t items[5];
    int count;
    int i;
    char time_buf[32];

    if (!g_home_timeline) {
        return;
    }

    lv_obj_clean(g_home_timeline);

    count = memo_store_get_recent(items, 5);
    if (count == 0) {
        lv_obj_t* empty = lv_label_create(g_home_timeline);
        lv_label_set_text(empty, "No notes yet.\nSwipe to Record page to capture your first thought.");
        lv_obj_set_style_text_color(empty, lv_color_hex(UI_TEXT_MUTED), 0);
        lv_obj_set_style_text_font(empty, get_font_small(), 0);
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
        return;
    }

    for (i = count - 1; i >= 0; i--) {
        lv_obj_t* row = lv_obj_create(g_home_timeline);
        lv_obj_t* dot;
        lv_obj_t* content;
        lv_obj_t* meta;

        lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 6, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(row, 10, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        dot = lv_obj_create(row);
        lv_obj_set_size(dot, 8, 8);
        lv_obj_set_style_radius(dot, 4, 0);
        lv_obj_set_style_bg_color(dot, lv_color_hex(UI_ACCENT_COLOR), 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(dot, LV_ALIGN_TOP_LEFT, 0, 4);

        content = lv_label_create(row);
        lv_label_set_text(content, items[i].content);
        lv_obj_set_style_text_color(content, lv_color_white(), 0);
        lv_obj_set_style_text_font(content, get_font_small(), 0);
        lv_obj_set_width(content, LV_PCT(85));
        lv_label_set_long_mode(content, LV_LABEL_LONG_DOT);

        meta = lv_label_create(row);
        format_time_ago(items[i].timestamp, time_buf, sizeof(time_buf));
        lv_label_set_text(meta, time_buf);
        lv_obj_set_style_text_color(meta, lv_color_hex(UI_TEXT_MUTED), 0);
        lv_obj_set_style_text_font(meta, get_font_small(), 0);
    }
}

static void create_home_page(lv_obj_t* tile)
{
    lv_obj_t* title;
    lv_obj_t* stat_row;
    time_t now;
    struct tm* tm_info;
    char date_buf[64];
    char stat_buf[128];
    int total_cnt;

    lv_obj_set_style_pad_all(tile, 20, 0);
    lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(tile, 12, 0);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);

    /* Title + date */

    title = lv_label_create(tile);
    lv_label_set_text(title, LV_SYMBOL_HOME "  DayNote");
    lv_obj_set_style_text_font(title, get_font_large(), 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);

    now = time(NULL);
    tm_info = localtime(&now);
    strftime(date_buf, sizeof(date_buf), "%Y-%m-%d  %A", tm_info);
    g_home_date_lbl = lv_label_create(tile);
    lv_label_set_text(g_home_date_lbl, date_buf);
    lv_obj_set_style_text_color(g_home_date_lbl, lv_color_hex(UI_TEXT_MUTED), 0);
    lv_obj_set_style_text_font(g_home_date_lbl, get_font_small(), 0);

    /* Timeline */

    g_home_timeline = lv_obj_create(tile);
    lv_obj_set_size(g_home_timeline, LV_PCT(100), g_screen_h - 160);
    lv_obj_set_style_bg_color(g_home_timeline, lv_color_hex(UI_CARD_BG), 0);
    lv_obj_set_style_bg_opa(g_home_timeline, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(g_home_timeline, 12, 0);
    lv_obj_set_style_border_width(g_home_timeline, 0, 0);
    lv_obj_set_style_pad_all(g_home_timeline, 12, 0);
    lv_obj_set_flex_flow(g_home_timeline, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(g_home_timeline, 6, 0);

    refresh_home_timeline();

    /* Stats bar */

    stat_row = lv_obj_create(tile);
    lv_obj_set_size(stat_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(stat_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(stat_row, 0, 0);
    lv_obj_set_style_pad_all(stat_row, 0, 0);
    lv_obj_set_flex_flow(stat_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(stat_row, LV_FLEX_ALIGN_SPACE_BETWEEN,
        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(stat_row, LV_OBJ_FLAG_SCROLLABLE);

    total_cnt = memo_store_get_count(MEMO_TYPE_MEMO, false)
              + memo_store_get_count(MEMO_TYPE_TODO, false)
              + memo_store_get_count(MEMO_TYPE_SCHEDULE, false);
    snprintf(stat_buf, sizeof(stat_buf), "%d notes total", total_cnt);
    g_home_stat_lbl = lv_label_create(stat_row);
    lv_label_set_text(g_home_stat_lbl, stat_buf);
    lv_obj_set_style_text_color(g_home_stat_lbl, lv_color_hex(UI_ACCENT_COLOR), 0);
    lv_obj_set_style_text_font(g_home_stat_lbl, get_font_small(), 0);
}

/* -- Page: Voice --------------------------------------- */

static lv_obj_t* g_voice_status_lbl;
static lv_obj_t* g_ptt_btn;
static bool g_recording;
static lv_obj_t* g_review_list;
static lv_timer_t* g_ptt_selftest_start_timer;
static lv_timer_t* g_ptt_selftest_release_timer;
static uint32_t g_ptt_selftest_hold_ms;

static void refresh_review_list(void);
static void ptt_selftest_release_cb(lv_timer_t* timer);

static void ptt_selftest_start_cb(lv_timer_t* timer)
{
    lv_area_t coords;

    if (timer != NULL) {
        lv_timer_delete(timer);
        g_ptt_selftest_start_timer = NULL;
    }

    if (g_tileview == NULL || g_tiles[MEMO_PAGE_VOICE] == NULL || g_ptt_btn == NULL) {
        syslog(LOG_ERR, "PTT selftest: UI not ready\n");
        return;
    }

    lv_tileview_set_tile(g_tileview, g_tiles[MEMO_PAGE_VOICE], LV_ANIM_OFF);
    update_nav_dots(MEMO_PAGE_VOICE);
    lv_obj_update_layout(lv_screen_active());
    lv_obj_get_coords(g_ptt_btn, &coords);

    syslog(LOG_INFO,
        "PTT selftest: voice page ready, btn=(%ld,%ld)-(%ld,%ld), hold=%" PRIu32 "ms\n",
        (long)coords.x1, (long)coords.y1, (long)coords.x2, (long)coords.y2,
        g_ptt_selftest_hold_ms);

    lv_obj_send_event(g_ptt_btn, LV_EVENT_PRESSED, NULL);

    g_ptt_selftest_release_timer = lv_timer_create(ptt_selftest_release_cb,
        g_ptt_selftest_hold_ms, NULL);
    if (g_ptt_selftest_release_timer != NULL) {
        lv_timer_set_repeat_count(g_ptt_selftest_release_timer, 1);
    }
}

static void ptt_selftest_release_cb(lv_timer_t* timer)
{
    if (timer != NULL) {
        lv_timer_delete(timer);
        g_ptt_selftest_release_timer = NULL;
    }

    if (g_ptt_btn == NULL) {
        syslog(LOG_ERR, "PTT selftest: button missing on release\n");
        return;
    }

    syslog(LOG_INFO, "PTT selftest: releasing button\n");
    lv_obj_send_event(g_ptt_btn, LV_EVENT_RELEASED, NULL);
}

/* Payload for marshalling classify results onto the LVGL thread. */

typedef struct {
    char status_text[64];
    bool refresh;
} classify_ui_t;

/* Runs on the LVGL thread (via lv_async_call) where lv_* is safe. */

static void classify_done_async_cb(void* p)
{
    classify_ui_t* u = (classify_ui_t*)p;

    if (g_voice_status_lbl) {
        lv_label_set_text(g_voice_status_lbl, u->status_text);
    }

    if (u->refresh) {
        memo_ui_refresh_home();
        refresh_review_list();
    }

    free(u);
}

static void on_classify_done(int status, const classify_result_t* result,
    void* cookie)
{
    memo_item_t item;
    classify_ui_t* u;
    const char* type_name = "Memo";
    (void)cookie;

    /* This callback may run on the velaclaw SDK worker thread; lv_* is not
     * thread-safe, so all UI work is deferred to the LVGL thread. */

    u = malloc(sizeof(*u));
    if (!u) {
        syslog(LOG_ERR, "classify: ui payload alloc failed\n");
        return;
    }

    if (status != 0 || !result) {
        syslog(LOG_ERR, "classify failed status=%d\n", status);
        strncpy(u->status_text, "Classification failed",
            sizeof(u->status_text) - 1);
        u->status_text[sizeof(u->status_text) - 1] = '\0';
        u->refresh = false;
        lv_async_call(classify_done_async_cb, u);
        return;
    }

    /* Store the new memo (memo_store is thread-safe via internal mutex) */

    memset(&item, 0, sizeof(item));
    item.type = result->type;
    strncpy(item.content, result->content, sizeof(item.content) - 1);
    item.content[sizeof(item.content) - 1] = '\0';
    strncpy(item.transcript, result->transcript, sizeof(item.transcript) - 1);
    item.transcript[sizeof(item.transcript) - 1] = '\0';
    strncpy(item.summary, result->summary, sizeof(item.summary) - 1);
    item.summary[sizeof(item.summary) - 1] = '\0';
    strncpy(item.keywords, result->keywords, sizeof(item.keywords) - 1);
    item.keywords[sizeof(item.keywords) - 1] = '\0';
    item.timestamp = time(NULL);
    item.remind_at = result->remind_at;
    item.duration_ms = 0;
    item.is_read = false;

    memo_store_add(&item);

    if (result->type == MEMO_TYPE_TODO) {
        type_name = "Todo";
    } else if (result->type == MEMO_TYPE_SCHEDULE) {
        type_name = "Schedule";
    }

    snprintf(u->status_text, sizeof(u->status_text), "Saved as %s!",
        type_name);
    u->refresh = true;

    lv_async_call(classify_done_async_cb, u);
}

static void ptt_pressed_cb(lv_event_t* e)
{
    (void)e;
    int ret;

    if (g_recording) {
        return;
    }

    syslog(LOG_INFO, "PTT: pressed - start recording\n");
    g_recording = true;

    /* Pause VAD to free microphone for PTT */
    memo_vad_notify_ptt(true);

    lv_obj_set_style_bg_color(g_ptt_btn, lv_color_hex(0xd53a3a), 0);
    lv_label_set_text(g_voice_status_lbl, "Recording...");

    ret = memo_voice_start();
    if (ret < 0) {
        syslog(LOG_ERR, "PTT: voice_start failed: %d\n", ret);
        lv_label_set_text(g_voice_status_lbl, "Mic unavailable");
        lv_obj_set_style_bg_color(g_ptt_btn,
            lv_color_hex(UI_ACCENT_COLOR), 0);
        g_recording = false;
    }
}

static void ptt_released_cb(lv_event_t* e)
{
    (void)e;
    char text_buf[512];
    int ret;

    if (!g_recording) {
        return;
    }

    syslog(LOG_INFO, "PTT: released - stop recording\n");
    g_recording = false;

    /* Resume VAD auto-recording */
    memo_vad_notify_ptt(false);

    lv_obj_set_style_bg_color(g_ptt_btn, lv_color_hex(UI_ACCENT_COLOR), 0);
    lv_label_set_text(g_voice_status_lbl, "Recognizing...");

    ret = memo_voice_stop(text_buf, sizeof(text_buf));
    if (ret < 0 || text_buf[0] == '\0') {
        lv_label_set_text(g_voice_status_lbl,
            ret < 0 ? "ASR failed" : "No speech detected");
        return;
    }

    /* Show transcribed text */

    lv_label_set_text(g_voice_status_lbl, text_buf);

    /* Classify and store */

    memo_classify_async(text_buf, on_classify_done, NULL);
}

static lv_obj_t* g_auto_status_lbl;
static bool g_auto_record_enabled = true;

static void auto_record_toggle_cb(lv_event_t* e)
{
    lv_obj_t* card = lv_event_get_current_target(e);
    lv_obj_t* icon = lv_obj_get_child(card, 0);
    lv_obj_t* text = lv_obj_get_child(card, 1);

    g_auto_record_enabled = !g_auto_record_enabled;

    if (g_auto_record_enabled) {
        memo_vad_start();
        lv_label_set_text(icon, LV_SYMBOL_OK);
        lv_obj_set_style_text_color(icon, lv_color_hex(0x00d68f), 0);
        lv_label_set_text(text, "Auto-record: Listening");
    } else {
        memo_vad_stop();
        lv_label_set_text(icon, LV_SYMBOL_CLOSE);
        lv_obj_set_style_text_color(icon, lv_color_hex(UI_TEXT_MUTED), 0);
        lv_label_set_text(text, "Auto-record: Off");
    }
}

static void create_voice_page(lv_obj_t* tile)
{
    lv_obj_t* title;
    lv_obj_t* ptt_lbl;
    lv_obj_t* auto_card;
    lv_obj_t* auto_icon;
    lv_obj_t* auto_text;

    lv_obj_set_style_pad_all(tile, 20, 0);
    lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(tile, 16, 0);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);

    title = lv_label_create(tile);
    lv_label_set_text(title, LV_SYMBOL_AUDIO "  Record");
    lv_obj_set_style_text_font(title, get_font_large(), 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);

    /* Auto-record status card */

    auto_card = lv_obj_create(tile);
    lv_obj_set_size(auto_card, LV_PCT(90), 50);
    lv_obj_set_style_bg_color(auto_card, lv_color_hex(UI_CARD_BG), 0);
    lv_obj_set_style_bg_opa(auto_card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(auto_card, 12, 0);
    lv_obj_set_style_border_width(auto_card, 0, 0);
    lv_obj_set_flex_flow(auto_card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(auto_card, LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(auto_card, 12, 0);
    lv_obj_set_style_pad_column(auto_card, 10, 0);
    lv_obj_clear_flag(auto_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(auto_card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(auto_card, auto_record_toggle_cb,
        LV_EVENT_CLICKED, NULL);

    auto_icon = lv_label_create(auto_card);
    lv_label_set_text(auto_icon, g_auto_record_enabled ? LV_SYMBOL_OK : LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(auto_icon,
        lv_color_hex(g_auto_record_enabled ? 0x00d68f : UI_TEXT_MUTED), 0);
    lv_obj_set_style_text_font(auto_icon, get_font_medium(), 0);

    auto_text = lv_label_create(auto_card);
    lv_label_set_text(auto_text, g_auto_record_enabled ?
        "Auto-record: Listening" : "Auto-record: Off");
    lv_obj_set_style_text_color(auto_text, lv_color_white(), 0);
    lv_obj_set_style_text_font(auto_text, get_font_small(), 0);

    g_auto_status_lbl = auto_text;

    /* PTT button (circular, manual trigger) */

    g_ptt_btn = lv_obj_create(tile);
    lv_obj_set_size(g_ptt_btn, 90, 90);
    lv_obj_set_style_radius(g_ptt_btn, 45, 0);
    lv_obj_set_style_bg_color(g_ptt_btn, lv_color_hex(UI_ACCENT_COLOR), 0);
    lv_obj_set_style_bg_opa(g_ptt_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_ptt_btn, 0, 0);
    lv_obj_clear_flag(g_ptt_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_ptt_btn, LV_OBJ_FLAG_CLICKABLE);

    ptt_lbl = lv_label_create(g_ptt_btn);
    lv_label_set_text(ptt_lbl, LV_SYMBOL_AUDIO);
    lv_obj_set_style_text_font(ptt_lbl, get_font_large(), 0);
    lv_obj_set_style_text_color(ptt_lbl, lv_color_white(), 0);
    lv_obj_center(ptt_lbl);

    lv_obj_add_event_cb(g_ptt_btn, ptt_pressed_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(g_ptt_btn, ptt_released_cb, LV_EVENT_RELEASED, NULL);

    /* Status label */

    g_voice_status_lbl = lv_label_create(tile);
    lv_label_set_text(g_voice_status_lbl,
        "Auto-listening for speech.\nOr hold button to record manually.");
    lv_obj_set_style_text_font(g_voice_status_lbl, get_font_small(), 0);
    lv_obj_set_style_text_color(g_voice_status_lbl,
        lv_color_hex(UI_TEXT_MUTED), 0);
    lv_obj_set_width(g_voice_status_lbl, LV_PCT(90));
    lv_label_set_long_mode(g_voice_status_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(g_voice_status_lbl, LV_TEXT_ALIGN_CENTER, 0);

    g_recording = false;
}

/* -- Page: Review -------------------------------------- */

static void review_item_swipe_cb(lv_event_t* e)
{
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    lv_obj_t* row;
    uint32_t id;

    if (dir != LV_DIR_LEFT) {
        return;
    }

    row = lv_event_get_current_target(e);
    id = (uint32_t)(uintptr_t)lv_event_get_user_data(e);

    memo_store_delete(id);

    /* Animate row out then refresh */

    lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
    refresh_review_list();
    memo_ui_refresh_home();
}

static void refresh_review_list(void)
{
    /* Static: memo_item_t[20] is ~21KB, too large for the LVGL stack. */
    static memo_item_t items[MEMO_MAX_DISPLAY];
    int count;
    int i;

    if (!g_review_list) {
        return;
    }

    lv_obj_clean(g_review_list);

    count = memo_store_get_recent(items, MEMO_MAX_DISPLAY);
    if (count == 0) {
        lv_obj_t* empty = lv_label_create(g_review_list);
        lv_label_set_text(empty,
            "No memos yet.\nSwipe to Voice page to add one.");
        lv_obj_set_style_text_color(empty, lv_color_hex(UI_TEXT_MUTED), 0);
        lv_obj_set_style_text_font(empty, get_font_small(), 0);
        return;
    }

    /* Show items newest-first as cards */

    for (i = count - 1; i >= 0; i--) {
        lv_obj_t* card = lv_obj_create(g_review_list);
        lv_obj_t* header;
        lv_obj_t* type_lbl;
        lv_obj_t* content_lbl;
        lv_obj_t* kw_lbl;
        const char* type_icon;
        char time_buf[32];

        lv_obj_set_size(card, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(card, lv_color_hex(0x1a1a2e), 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(card, 10, 0);
        lv_obj_set_style_border_width(card, 0, 0);
        lv_obj_set_style_pad_all(card, 10, 0);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(card, 4, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_GESTURE_BUBBLE);

        /* Swipe left to delete */

        lv_obj_add_event_cb(card, review_item_swipe_cb,
            LV_EVENT_GESTURE,
            (void*)(uintptr_t)items[i].id);

        /* Header: type icon + time */

        header = lv_obj_create(card);
        lv_obj_set_size(header, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(header, 0, 0);
        lv_obj_set_style_pad_all(header, 0, 0);
        lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN,
            LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

        switch (items[i].type) {
        case MEMO_TYPE_TODO:
            type_icon = LV_SYMBOL_LIST " Task";
            break;
        case MEMO_TYPE_SCHEDULE:
            type_icon = LV_SYMBOL_BELL " Event";
            break;
        default:
            type_icon = LV_SYMBOL_FILE " Note";
            break;
        }

        type_lbl = lv_label_create(header);
        lv_label_set_text(type_lbl, type_icon);
        lv_obj_set_style_text_color(type_lbl,
            lv_color_hex(UI_ACCENT_COLOR), 0);
        lv_obj_set_style_text_font(type_lbl, get_font_small(), 0);

        format_time_ago(items[i].timestamp, time_buf, sizeof(time_buf));
        lv_obj_t* time_lbl = lv_label_create(header);
        lv_label_set_text(time_lbl, time_buf);
        lv_obj_set_style_text_color(time_lbl, lv_color_hex(UI_TEXT_MUTED), 0);
        lv_obj_set_style_text_font(time_lbl, get_font_small(), 0);

        /* Content (one-line summary) */

        content_lbl = lv_label_create(card);
        lv_label_set_text(content_lbl, items[i].content[0] ?
            items[i].content : "(no summary)");
        lv_obj_set_style_text_color(content_lbl, lv_color_white(), 0);
        lv_obj_set_style_text_font(content_lbl, get_font_small(), 0);
        lv_obj_set_width(content_lbl, LV_PCT(100));
        lv_label_set_long_mode(content_lbl, LV_LABEL_LONG_WRAP);

        /* Keywords */

        if (items[i].keywords[0]) {
            kw_lbl = lv_label_create(card);
            char kw_buf[160];
            snprintf(kw_buf, sizeof(kw_buf), "# %s", items[i].keywords);
            lv_label_set_text(kw_lbl, kw_buf);
            lv_obj_set_style_text_color(kw_lbl,
                lv_color_hex(0x8888aa), 0);
            lv_obj_set_style_text_font(kw_lbl, get_font_small(), 0);
            lv_obj_set_width(kw_lbl, LV_PCT(100));
            lv_label_set_long_mode(kw_lbl, LV_LABEL_LONG_WRAP);
        }
    }
}

static void create_review_page(lv_obj_t* tile)
{
    lv_obj_t* title;
    lv_obj_t* list;

    lv_obj_set_style_pad_all(tile, 30, 0);
    lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(tile, 10, 0);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);

    title = lv_label_create(tile);
    lv_label_set_text(title, LV_SYMBOL_LIST "  Review");
    lv_obj_set_style_text_font(title, get_font_large(), 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);

    /* Scrollable list of memos */

    list = lv_obj_create(tile);
    lv_obj_set_size(list, LV_PCT(100), g_screen_h - 130);
    lv_obj_set_style_bg_color(list, lv_color_hex(UI_CARD_BG), 0);
    lv_obj_set_style_bg_opa(list, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(list, 12, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 16, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, 8, 0);

    g_review_list = list;
    refresh_review_list();
}

/* -- Page: Settings ------------------------------------ */

static void create_settings_page(lv_obj_t* tile)
{
    lv_obj_t* title;
    lv_obj_t* card;
    lv_obj_t* info;
    char status_buf[512];
    int total_notes;
    bool agent_ok;

    lv_obj_set_style_pad_all(tile, 20, 0);
    lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(tile, 12, 0);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);

    title = lv_label_create(tile);
    lv_label_set_text(title, LV_SYMBOL_SETTINGS "  Settings");
    lv_obj_set_style_text_font(title, get_font_large(), 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);

    /* Status card */

    card = lv_obj_create(tile);
    lv_obj_set_size(card, LV_PCT(100), g_screen_h - 140);
    lv_obj_set_style_bg_color(card, lv_color_hex(UI_CARD_BG), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 14, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    agent_ok = memo_agent_is_connected();
    total_notes = memo_store_get_count(MEMO_TYPE_MEMO, false)
                + memo_store_get_count(MEMO_TYPE_TODO, false)
                + memo_store_get_count(MEMO_TYPE_SCHEDULE, false);

    bool wifi_ok = network_is_connected();
    const char *wifi_ip = wifi_ok ? network_get_ip() : "none";
#ifdef CONFIG_AI_AGENT_BLE_GATT
    bool ble_ok = ble_gatt_is_connected();
#else
    bool ble_ok = false;
#endif

    /* Get LLM backend status */
    char llm_status[64];
    llm_backend_t llm_be;
    if (llm_router_get_backend(0, &llm_be) == 0 && llm_be.enabled) {
        snprintf(llm_status, sizeof(llm_status), "%s/%s",
            llm_be.host, llm_be.model);
    } else {
        strncpy(llm_status, "Not configured", sizeof(llm_status) - 1);
    }

    /* Get DMR status */
    const char* dmr_state = "N/A";
    if (g_mmdvm) {
        dmr_state = mmdvm_state_str(mmdvm_get_state(g_mmdvm));
    }

    snprintf(status_buf, sizeof(status_buf),
        "WiFi: %s (%s)\n"
        "BLE: %s\n"
        "AI: %s\n"
        "LLM: %s\n"
        "DMR: %s (ID %u)\n"
        "Auto-record: %s\n"
        "Notes: %d / 100\n"
        "\n"
        "BLE: wifi_config/mmdvm_config/llm_config\n"
        "NSH: set_wifi / set_llm\n"
        "\n"
        "DayNote v1.1.0",
        wifi_ok ? "UP" : "DOWN",
        wifi_ip,
        ble_ok ? "Phone" : "Waiting",
        agent_ok ? "Ready" : "Offline",
        llm_status,
        dmr_state,
        g_mmdvm ? g_mmdvm->config.dmr_id : 0,
        g_auto_record_enabled ? "ON" : "OFF",
        total_notes);

    info = lv_label_create(card);
    lv_label_set_text(info, status_buf);
    lv_obj_set_style_text_font(info, get_font_small(), 0);
    lv_obj_set_style_text_color(info, lv_color_hex(UI_TEXT_MUTED), 0);
    lv_obj_set_style_text_line_space(info, 4, 0);
    lv_obj_set_width(info, LV_PCT(100));
    lv_label_set_long_mode(info, LV_LABEL_LONG_WRAP);
}

/* -- Page: DMR Hotspot ---------------------------------- */

static void dmr_refresh_timer_cb(lv_timer_t* timer)
{
    mmdvm_client_t *mmdvm;
    dmr_call_t call;
    char buf[128];
    bool has_call = false;
    int i;

    (void)timer;

    mmdvm = mmdvm_get_global();

    /* Update connection status */
    if (g_dmr_status_lbl) {
        mmdvm_state_t state = mmdvm ? mmdvm_get_state(mmdvm)
                                    : MMDVM_STATE_DISCONNECTED;
        lv_label_set_text_fmt(g_dmr_status_lbl, "Status: %s",
            mmdvm_state_str(state));
    }

    if (g_dmr_master_lbl && mmdvm) {
        lv_label_set_text_fmt(g_dmr_master_lbl, "Master: %s  ID: %u",
            mmdvm->config.master_addr, mmdvm->config.dmr_id);
    }

    /* Check for active call on either slot */
    if (mmdvm) {
        for (i = 0; i < 2; i++) {
            if (mmdvm_get_current_call(mmdvm, i, &call)) {
                has_call = true;
                break;
            }
        }
    }

    if (g_dmr_active_card) {
        if (has_call) {
            lv_obj_clear_flag(g_dmr_active_card, LV_OBJ_FLAG_HIDDEN);
            if (g_dmr_call_src_lbl) {
                lv_label_set_text_fmt(g_dmr_call_src_lbl, "%s  (%u)",
                    call.src_callsign, call.src_id);
            }
            if (g_dmr_call_dst_lbl) {
                lv_label_set_text_fmt(g_dmr_call_dst_lbl,
                    "TG %u  %s", call.dst_id,
                    call.call_type == DMR_CALL_GROUP ? "Group" : "Private");
            }
            if (g_dmr_call_slot_lbl) {
                lv_label_set_text_fmt(g_dmr_call_slot_lbl, "TS%u",
                    call.slot + 1);
            }
            if (g_dmr_call_duration_lbl) {
                unsigned int sec = call.duration_ms / 1000;
                lv_label_set_text_fmt(g_dmr_call_duration_lbl,
                    "%02u:%02u", sec / 60, sec % 60);
            }
            if (g_dmr_call_rssi_lbl) {
                lv_label_set_text_fmt(g_dmr_call_rssi_lbl,
                    "RSSI %u  BER %.1f%%", call.rssi,
                    (double)call.ber / 2.55);
            }
        } else {
            lv_obj_add_flag(g_dmr_active_card, LV_OBJ_FLAG_HIDDEN);
        }
    }

    /* Update history list — only rebuild when content actually changed to
     * avoid flicker and heap fragmentation from a 500ms full clean/re-add. */
    if (g_dmr_history_list && mmdvm) {
        static dmr_call_t hist[MMDVM_MAX_HISTORY];
        static int last_count = -1;
        static uint32_t last_sig = 0;
        int count = mmdvm_get_history(mmdvm, hist, MMDVM_MAX_HISTORY);
        uint32_t sig = 0;
        int n;

        /* Signature: count + src/dst/duration of the most recent entries */
        sig = (uint32_t)count * 2654435761u;
        for (n = 0; n < count && n < 5; n++) {
            sig = sig * 31u + hist[n].src_id;
            sig = sig * 31u + hist[n].dst_id;
            sig = sig * 31u + hist[n].duration_ms / 1000;
        }

        if (count != last_count || sig != last_sig) {
            last_count = count;
            last_sig = sig;

            lv_obj_clean(g_dmr_history_list);
            for (i = 0; i < count && i < 5; i++) {
                lv_obj_t *item = lv_label_create(g_dmr_history_list);
                unsigned int sec = hist[i].duration_ms / 1000;
                snprintf(buf, sizeof(buf), "%s -> TG%u  %02u:%02u",
                    hist[i].src_callsign, hist[i].dst_id,
                    sec / 60, sec % 60);
                lv_label_set_text(item, buf);
                lv_obj_set_style_text_font(item, get_font_small(), 0);
                lv_obj_set_style_text_color(item,
                    lv_color_hex(UI_TEXT_MUTED), 0);
            }
            if (count == 0) {
                lv_obj_t *empty = lv_label_create(g_dmr_history_list);
                lv_label_set_text(empty, "No calls yet. Waiting for DMR traffic...");
                lv_obj_set_style_text_font(empty, get_font_small(), 0);
                lv_obj_set_style_text_color(empty,
                    lv_color_hex(UI_TEXT_MUTED), 0);
            }
        }
    }
}

static void dmr_ptt_btn_cb(lv_event_t* e)
{
    (void)e;
    memo_ui_show_notification("DMR PTT",
        "RF frontend required.\n"
        "Stage 2: connect MMDVM_HS or OpenGD77 via UART.");
}

static void create_dmr_page(lv_obj_t* tile)
{
    lv_obj_t* title;
    lv_obj_t* card;
    lv_obj_t* ptt_btn;
    lv_obj_t* ptt_label;

    /* Title */
    title = lv_label_create(tile);
    lv_label_set_text(title, "DMR Hotspot");
    lv_obj_set_style_text_font(title, get_font_medium(), 0);
    lv_obj_set_style_text_color(title, lv_color_hex(UI_ACCENT_COLOR), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    /* Status card */
    card = lv_obj_create(tile);
    lv_obj_set_size(card, LV_PCT(90), 44);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 36);
    lv_obj_set_style_bg_color(card, lv_color_hex(UI_CARD_BG), 0);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 6, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    g_dmr_status_lbl = lv_label_create(card);
    lv_label_set_text(g_dmr_status_lbl, "Status: Disconnected");
    lv_obj_set_style_text_font(g_dmr_status_lbl, get_font_small(), 0);
    lv_obj_align(g_dmr_status_lbl, LV_ALIGN_TOP_LEFT, 4, 2);

    g_dmr_master_lbl = lv_label_create(card);
    lv_label_set_text(g_dmr_master_lbl, "Master: -  ID: -");
    lv_obj_set_style_text_font(g_dmr_master_lbl, get_font_small(), 0);
    lv_obj_set_style_text_color(g_dmr_master_lbl,
        lv_color_hex(UI_TEXT_MUTED), 0);
    lv_obj_align(g_dmr_master_lbl, LV_ALIGN_BOTTOM_LEFT, 4, 2);

    /* Active call card */
    g_dmr_active_card = lv_obj_create(tile);
    lv_obj_set_size(g_dmr_active_card, LV_PCT(90), 56);
    lv_obj_align(g_dmr_active_card, LV_ALIGN_TOP_MID, 0, 88);
    lv_obj_set_style_bg_color(g_dmr_active_card,
        lv_color_hex(0x1a1a3a), 0);
    lv_obj_set_style_radius(g_dmr_active_card, 8, 0);
    lv_obj_set_style_border_width(g_dmr_active_card, 1, 0);
    lv_obj_set_style_border_color(g_dmr_active_card,
        lv_color_hex(UI_ACCENT_COLOR), 0);
    lv_obj_set_style_pad_all(g_dmr_active_card, 6, 0);
    lv_obj_clear_flag(g_dmr_active_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_dmr_active_card, LV_OBJ_FLAG_HIDDEN);

    g_dmr_call_src_lbl = lv_label_create(g_dmr_active_card);
    lv_label_set_text(g_dmr_call_src_lbl, "-");
    lv_obj_set_style_text_font(g_dmr_call_src_lbl, get_font_small(), 0);
    lv_obj_align(g_dmr_call_src_lbl, LV_ALIGN_TOP_LEFT, 4, 2);

    g_dmr_call_dst_lbl = lv_label_create(g_dmr_active_card);
    lv_label_set_text(g_dmr_call_dst_lbl, "-");
    lv_obj_set_style_text_font(g_dmr_call_dst_lbl, get_font_small(), 0);
    lv_obj_set_style_text_color(g_dmr_call_dst_lbl,
        lv_color_hex(UI_TEXT_MUTED), 0);
    lv_obj_align(g_dmr_call_dst_lbl, LV_ALIGN_TOP_LEFT, 4, 18);

    g_dmr_call_slot_lbl = lv_label_create(g_dmr_active_card);
    lv_label_set_text(g_dmr_call_slot_lbl, "TS-");
    lv_obj_set_style_text_font(g_dmr_call_slot_lbl, get_font_small(), 0);
    lv_obj_align(g_dmr_call_slot_lbl, LV_ALIGN_TOP_RIGHT, -4, 2);

    g_dmr_call_duration_lbl = lv_label_create(g_dmr_active_card);
    lv_label_set_text(g_dmr_call_duration_lbl, "00:00");
    lv_obj_set_style_text_font(g_dmr_call_duration_lbl, get_font_small(), 0);
    lv_obj_set_style_text_color(g_dmr_call_duration_lbl,
        lv_color_hex(UI_ACCENT_COLOR), 0);
    lv_obj_align(g_dmr_call_duration_lbl, LV_ALIGN_TOP_RIGHT, -4, 18);

    g_dmr_call_rssi_lbl = lv_label_create(g_dmr_active_card);
    lv_label_set_text(g_dmr_call_rssi_lbl, "RSSI -  BER -");
    lv_obj_set_style_text_font(g_dmr_call_rssi_lbl, get_font_small(), 0);
    lv_obj_set_style_text_color(g_dmr_call_rssi_lbl,
        lv_color_hex(UI_TEXT_MUTED), 0);
    lv_obj_align(g_dmr_call_rssi_lbl, LV_ALIGN_BOTTOM_LEFT, 4, 2);

    /* History list */
    g_dmr_history_list = lv_obj_create(tile);
    lv_obj_set_size(g_dmr_history_list, LV_PCT(90), 50);
    lv_obj_align(g_dmr_history_list, LV_ALIGN_TOP_MID, 0, 152);
    lv_obj_set_style_bg_color(g_dmr_history_list,
        lv_color_hex(UI_CARD_BG), 0);
    lv_obj_set_style_radius(g_dmr_history_list, 8, 0);
    lv_obj_set_style_border_width(g_dmr_history_list, 0, 0);
    lv_obj_set_style_pad_all(g_dmr_history_list, 6, 0);
    lv_obj_set_flex_flow(g_dmr_history_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(g_dmr_history_list, 2, 0);

    /* PTT button */
    ptt_btn = lv_btn_create(tile);
    lv_obj_set_size(ptt_btn, 100, 32);
    lv_obj_align(ptt_btn, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_set_style_bg_color(ptt_btn, lv_color_hex(UI_ACCENT_COLOR), 0);
    lv_obj_set_style_radius(ptt_btn, 16, 0);
    lv_obj_add_event_cb(ptt_btn, dmr_ptt_btn_cb, LV_EVENT_CLICKED, NULL);

    ptt_label = lv_label_create(ptt_btn);
    lv_label_set_text(ptt_label, "PTT");
    lv_obj_set_style_text_font(ptt_label, get_font_small(), 0);
    lv_obj_center(ptt_label);

    /* Start refresh timer (500ms) */
    g_dmr_refresh_timer = lv_timer_create(dmr_refresh_timer_cb, 500, NULL);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int memo_ui_init(void)
{
    lv_obj_t* scr = lv_scr_act();
    lv_display_t* disp = lv_display_get_default();

    syslog(LOG_INFO, "memo_ui_init: creating %d-page UI\n", MEMO_PAGE_COUNT);

    /* Get actual display resolution */

    if (disp) {
        g_screen_w = lv_display_get_horizontal_resolution(disp);
        g_screen_h = lv_display_get_vertical_resolution(disp);
    }

    syslog(LOG_INFO, "memo_ui_init: using res=%dx%d\n", g_screen_w, g_screen_h);

    /* Dark background for screen */

    lv_obj_set_style_bg_color(scr, lv_color_hex(UI_BG_COLOR), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* Create tileview (horizontal swipe between pages) */

    g_tileview = lv_tileview_create(scr);
    lv_obj_set_size(g_tileview, g_screen_w, g_screen_h - 30);
    lv_obj_align(g_tileview, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(g_tileview, LV_OPA_TRANSP, 0);
    lv_obj_set_scroll_snap_x(g_tileview, LV_SCROLL_SNAP_CENTER);

    /* Add tiles: col 0-4, row 0 (horizontal only) */

    g_tiles[MEMO_PAGE_HOME] = lv_tileview_add_tile(
        g_tileview, 0, 0, LV_DIR_RIGHT);
    g_tiles[MEMO_PAGE_VOICE] = lv_tileview_add_tile(
        g_tileview, 1, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    g_tiles[MEMO_PAGE_REVIEW] = lv_tileview_add_tile(
        g_tileview, 2, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    g_tiles[MEMO_PAGE_SETTINGS] = lv_tileview_add_tile(
        g_tileview, 3, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    g_tiles[MEMO_PAGE_DMR] = lv_tileview_add_tile(
        g_tileview, 4, 0, LV_DIR_LEFT);

    /* Populate each page */

    create_home_page(g_tiles[MEMO_PAGE_HOME]);
    create_voice_page(g_tiles[MEMO_PAGE_VOICE]);
    create_review_page(g_tiles[MEMO_PAGE_REVIEW]);
    create_settings_page(g_tiles[MEMO_PAGE_SETTINGS]);
    create_dmr_page(g_tiles[MEMO_PAGE_DMR]);

    /* Ensure tiles don't eat horizontal scroll - tileview owns it */

    {
        int i;
        for (i = 0; i < MEMO_PAGE_COUNT; i++) {
            lv_obj_add_flag(g_tiles[i], LV_OBJ_FLAG_SCROLL_CHAIN_HOR);
            lv_obj_clear_flag(g_tiles[i], LV_OBJ_FLAG_SCROLL_ELASTIC);
        }
    }

    /* Navigation dots at bottom */

    create_nav_bar(scr);

    /* Listen for tile changes to update dots */

    lv_obj_add_event_cb(g_tileview, tileview_changed_cb,
        LV_EVENT_VALUE_CHANGED, NULL);

    /* Periodic flush timer (5 seconds) */

    g_flush_timer = lv_timer_create(flush_timer_cb, 5000, NULL);

    /* Reminder check timer (60 seconds) */

    g_remind_timer = lv_timer_create(remind_timer_cb, 60000, NULL);

    return 0;
}

void memo_ui_deinit(void)
{
    syslog(LOG_INFO, "memo_ui_deinit\n");
    if (g_ptt_selftest_start_timer) {
        lv_timer_delete(g_ptt_selftest_start_timer);
        g_ptt_selftest_start_timer = NULL;
    }
    if (g_ptt_selftest_release_timer) {
        lv_timer_delete(g_ptt_selftest_release_timer);
        g_ptt_selftest_release_timer = NULL;
    }
    if (g_flush_timer) {
        lv_timer_delete(g_flush_timer);
        g_flush_timer = NULL;
    }
    if (g_remind_timer) {
        lv_timer_delete(g_remind_timer);
        g_remind_timer = NULL;
    }
    if (g_dmr_refresh_timer) {
        lv_timer_delete(g_dmr_refresh_timer);
        g_dmr_refresh_timer = NULL;
    }
    g_tileview = NULL;
}

void memo_ui_show_notification(const char* title, const char* body)
{
    lv_obj_t* mbox;

    syslog(LOG_INFO, "memo_ui_show_notification: %s - %s\n", title, body);

    mbox = lv_msgbox_create(NULL);
    lv_msgbox_add_title(mbox, title);
    lv_msgbox_add_text(mbox, body);
    lv_msgbox_add_close_button(mbox);
    lv_obj_center(mbox);
}

void memo_ui_refresh_home(void)
{
    char stat_buf[128];
    int total_cnt;

    refresh_home_timeline();

    if (g_home_stat_lbl) {
        total_cnt = memo_store_get_count(MEMO_TYPE_MEMO, false)
                  + memo_store_get_count(MEMO_TYPE_TODO, false)
                  + memo_store_get_count(MEMO_TYPE_SCHEDULE, false);
        snprintf(stat_buf, sizeof(stat_buf), "%d notes total", total_cnt);
        lv_label_set_text(g_home_stat_lbl, stat_buf);
    }
}

void memo_ui_navigate_to(int page_index)
{
    if (page_index < 0 || page_index >= MEMO_PAGE_COUNT) {
        return;
    }

    if (g_tileview == NULL || g_tiles[page_index] == NULL) {
        return;
    }

    lv_tileview_set_tile(g_tileview, g_tiles[page_index], LV_ANIM_ON);
    update_nav_dots(page_index);
}

int memo_ui_start_ptt_selftest(unsigned int hold_ms)
{
    if (g_ptt_btn == NULL || g_tileview == NULL || g_tiles[MEMO_PAGE_VOICE] == NULL) {
        return -ENODEV;
    }

    if (g_ptt_selftest_start_timer != NULL || g_ptt_selftest_release_timer != NULL) {
        return -EBUSY;
    }

    g_ptt_selftest_hold_ms = hold_ms == 0 ? 1000 : hold_ms;
    g_ptt_selftest_start_timer = lv_timer_create(ptt_selftest_start_cb, 400, NULL);
    if (g_ptt_selftest_start_timer == NULL) {
        return -ENOMEM;
    }

    lv_timer_set_repeat_count(g_ptt_selftest_start_timer, 1);
    syslog(LOG_INFO, "PTT selftest: scheduled\n");
    return 0;
}
