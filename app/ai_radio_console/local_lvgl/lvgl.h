#ifndef LVGL_H_LOCAL_STUB
#define LVGL_H_LOCAL_STUB

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int16_t lv_coord_t;
typedef uint8_t lv_opa_t;
typedef uint32_t lv_color_t;
typedef uint32_t lv_res_t;
typedef struct _lv_obj_t lv_obj_t;
typedef struct _lv_timer_t lv_timer_t;
typedef void * lv_event_t;
typedef int   lv_event_code_t;
typedef void * lv_style_t;
struct _lv_font_t { int dummy; };
typedef struct _lv_font_t lv_font_t;
typedef void (*lv_timer_cb_t)(lv_timer_t *);
typedef void (*lv_event_cb_t)(lv_event_t *);

#define LV_OPA_COVER    255
#define LV_OPA_TRANSP   0
#define LV_RADIUS_CIRCLE 0x7FFF
#define LV_COORD_MAX    ((lv_coord_t)((1U << ((8 * sizeof(lv_coord_t)) - 1)) - 1))
#define LV_SIZE_CONTENT  LV_COORD_MAX

typedef enum {
    LV_ALIGN_DEFAULT = 0,
    LV_ALIGN_TOP_LEFT,
    LV_ALIGN_TOP_MID,
    LV_ALIGN_TOP_RIGHT,
    LV_ALIGN_BOTTOM_LEFT,
    LV_ALIGN_BOTTOM_MID,
    LV_ALIGN_BOTTOM_RIGHT,
    LV_ALIGN_LEFT_MID,
    LV_ALIGN_RIGHT_MID,
    LV_ALIGN_CENTER,
    _LV_ALIGN_LAST
} lv_align_t;

#define LV_ANIM_OFF 0
#define LV_ANIM_ON  1
#define LV_PART_MAIN      0x000000
#define LV_STATE_DEFAULT  0x000000

typedef enum {
    LV_LABEL_LONG_WRAP,
    LV_LABEL_LONG_DOT,
    LV_LABEL_LONG_SCROLL,
    LV_LABEL_LONG_SCROLL_CIRCULAR,
    LV_LABEL_LONG_CLIP,
    _LV_LABEL_LONG_MODE_LAST
} lv_label_long_mode_t;

#define LV_EVENT_CLICKED     0x04
#define LV_EVENT_READY      0x0A
#define LV_EVENT_CANCEL     0x0B
#define LV_EVENT_ALL        0x00
#define LV_OBJ_FLAG_CLICKABLE 0x00001000
#define LV_OBJ_FLAG_HIDDEN  0x00002000
#define LV_OBJ_FLAG_SCROLLABLE 0x00004000
#define LV_FONT_DEFAULT        ((const lv_font_t *)0)
static const lv_font_t lv_font_montserrat_10 = {0};
static const lv_font_t lv_font_montserrat_12 = {0};
static const lv_font_t lv_font_montserrat_14 = {0};
static const lv_font_t lv_font_montserrat_16 = {0};

#define MAX_LOCAL_OBJS 256
#define MAX_LOCAL_TIMERS 8
#define MAX_LOCAL_TEXT_LEN 256

struct _lv_obj_t {
    uint32_t id;
    bool is_label;
    bool is_bar;
    bool is_btn;
    char text[MAX_LOCAL_TEXT_LEN];
    lv_coord_t x, y, w, h;
    lv_color_t bg_color;
    lv_opa_t bg_opa;
    lv_coord_t radius;
    lv_coord_t border_width;
    lv_color_t border_color;
    lv_color_t text_color;
    lv_coord_t pad_all;
    int32_t bar_value;
    int32_t bar_min, bar_max;
};

struct _lv_timer_t {
    lv_timer_cb_t cb;
    uint32_t period;
    void * user_data;
};

static lv_obj_t g_objs[MAX_LOCAL_OBJS];
static int g_obj_count = 0;
static lv_timer_t g_timers[MAX_LOCAL_TIMERS];
static int g_timer_count = 0;
static int g_loop_count = 0;
static volatile bool *g_exit_flag = NULL;
static uint32_t g_tick_ms = 0;

static inline uint32_t lv_tick_get(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static inline void lv_init(void) {
    g_obj_count = 0;
    g_timer_count = 0;
    g_loop_count = 0;
    g_tick_ms = 0;
    memset(g_objs, 0, sizeof(g_objs));
    memset(g_timers, 0, sizeof(g_timers));
}

static inline void lv_deinit(void) {}

static inline lv_obj_t * lv_scr_act(void) { return &g_objs[0]; }
static inline void lv_scr_load(lv_obj_t * scr) { (void)scr; }

static inline lv_obj_t * lv_obj_create(lv_obj_t * parent) {
    (void)parent;
    if (g_obj_count >= MAX_LOCAL_OBJS) return NULL;
    lv_obj_t * obj = &g_objs[g_obj_count];
    memset(obj, 0, sizeof(*obj));
    obj->id = g_obj_count;
    obj->w = 100;
    obj->h = 100;
    obj->bg_opa = LV_OPA_COVER;
    g_obj_count++;
    return obj;
}

static inline void lv_obj_del(lv_obj_t * obj) { (void)obj; }
static inline void lv_obj_set_size(lv_obj_t * obj, lv_coord_t w, lv_coord_t h) { if(obj){obj->w=w;obj->h=h;} }
static inline void lv_obj_set_width(lv_obj_t * obj, lv_coord_t w) { if(obj)obj->w=w; }
static inline void lv_obj_set_height(lv_obj_t * obj, lv_coord_t h) { if(obj)obj->h=h; }
static inline void lv_obj_set_pos(lv_obj_t * obj, lv_coord_t x, lv_coord_t y) { if(obj){obj->x=x;obj->y=y;} }
static inline void lv_obj_set_x(lv_obj_t * obj, lv_coord_t x) { if(obj)obj->x=x; }
static inline void lv_obj_set_y(lv_obj_t * obj, lv_coord_t y) { if(obj)obj->y=y; }
static inline void lv_obj_align(lv_obj_t * obj, lv_align_t align, lv_coord_t x_ofs, lv_coord_t y_ofs) {
    (void)obj; (void)align; (void)x_ofs; (void)y_ofs;
}
static inline void lv_obj_center(lv_obj_t * obj) { (void)obj; }

static inline void lv_obj_set_style_bg_color(lv_obj_t * obj, lv_color_t color, int sel) { (void)sel; if(obj)obj->bg_color=color; }
static inline void lv_obj_set_style_bg_opa(lv_obj_t * obj, lv_opa_t opa, int sel) { (void)sel; if(obj)obj->bg_opa=opa; }
static inline void lv_obj_set_style_radius(lv_obj_t * obj, lv_coord_t r, int sel) { (void)sel; if(obj)obj->radius=r; }
static inline void lv_obj_set_style_border_width(lv_obj_t * obj, lv_coord_t w, int sel) { (void)sel; if(obj)obj->border_width=w; }
static inline void lv_obj_set_style_border_color(lv_obj_t * obj, lv_color_t color, int sel) { (void)sel; if(obj)obj->border_color=color; }
static inline void lv_obj_set_style_border_opa(lv_obj_t * obj, lv_opa_t opa, int sel) { (void)obj; (void)opa; (void)sel; }
static inline void lv_obj_set_style_pad_all(lv_obj_t * obj, lv_coord_t pad, int sel) { (void)sel; if(obj)obj->pad_all=pad; }
static inline void lv_obj_set_style_text_color(lv_obj_t * obj, lv_color_t color, int sel) { (void)sel; if(obj)obj->text_color=color; }
static inline void lv_obj_set_style_text_font(lv_obj_t * obj, const void * font, int sel) { (void)obj; (void)font; (void)sel; }
static inline void lv_obj_set_style_text_align(lv_obj_t * obj, int align, int sel) { (void)obj; (void)align; (void)sel; }
static inline void lv_obj_add_flag(lv_obj_t * obj, int flag) { (void)obj; (void)flag; }
static inline void lv_obj_clear_flag(lv_obj_t * obj, int flag) { (void)obj; (void)flag; }
static inline void lv_obj_add_event_cb(lv_obj_t * obj, lv_event_cb_t cb, int event, void * data) {
    (void)obj; (void)cb; (void)event; (void)data;
}

static inline lv_color_t lv_color_hex(uint32_t c) { return (lv_color_t)c; }
static inline lv_color_t lv_color_make(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r<<16)|((uint32_t)g<<8)|(uint32_t)b;
}

static inline lv_obj_t * lv_label_create(lv_obj_t * parent) {
    lv_obj_t * obj = lv_obj_create(parent);
    if(obj) obj->is_label = true;
    return obj;
}
static inline void lv_label_set_text(lv_obj_t * label, const char * text) {
    if(label && text) strncpy(label->text, text, MAX_LOCAL_TEXT_LEN-1);
}
static inline void lv_label_set_text_fmt(lv_obj_t * label, const char * fmt, ...) {
    if(!label) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(label->text, MAX_LOCAL_TEXT_LEN, fmt, ap);
    va_end(ap);
}
static inline void lv_label_set_long_mode(lv_obj_t * label, int mode) { (void)label; (void)mode; }

static inline lv_obj_t * lv_btn_create(lv_obj_t * parent) {
    lv_obj_t * obj = lv_obj_create(parent);
    if(obj) obj->is_btn = true;
    return obj;
}

static inline lv_obj_t * lv_bar_create(lv_obj_t * parent) {
    lv_obj_t * obj = lv_obj_create(parent);
    if(obj) {
        obj->is_bar = true;
        obj->bar_min = 0;
        obj->bar_max = 100;
        obj->bar_value = 0;
    }
    return obj;
}
static inline void lv_bar_set_value(lv_obj_t * bar, int32_t v, int anim) {
    (void)anim;
    if(bar) bar->bar_value = v;
}
static inline void lv_bar_set_range(lv_obj_t * bar, int32_t min, int32_t max) {
    if(bar) { bar->bar_min=min; bar->bar_max=max; }
}

static inline void local_lv_set_exit_flag(volatile bool *flag) { g_exit_flag = flag; }

static inline lv_timer_t * lv_timer_create(lv_timer_cb_t cb, uint32_t period, void * data) {
    if(g_timer_count >= MAX_LOCAL_TIMERS) return NULL;
    lv_timer_t * t = &g_timers[g_timer_count];
    t->cb = cb;
    t->period = period;
    t->user_data = data;
    g_timer_count++;
    return t;
}
static inline void lv_timer_del(lv_timer_t * t) { (void)t; }

static inline uint32_t lv_timer_handler(void) {
    g_loop_count++;
    for(int i=0;i<g_timer_count;i++){
        if(g_timers[i].cb) g_timers[i].cb(&g_timers[i]);
    }
    fflush(stdout);
    if(g_loop_count > 6) {
        printf("\n[LOCAL TEST] Ran %d UI update cycles, exiting successfully.\n", g_loop_count);
        exit(0);
    }
    usleep(10000);
    return 5;
}

static inline void lv_tick_inc(uint32_t ms) { (void)ms; }
static inline void lv_style_init(lv_style_t * style) { (void)style; }

typedef struct {
    const char *fb_path;
    const char *input_path;
    const char *utouch_path;
} lv_nuttx_dsc_t;

typedef struct {
    void *disp;
    void *indev;
} lv_nuttx_result_t;

static inline void lv_nuttx_dsc_init(lv_nuttx_dsc_t *dsc) { memset(dsc, 0, sizeof(*dsc)); }
static inline void lv_nuttx_init(lv_nuttx_dsc_t *dsc, lv_nuttx_result_t *res) {
    (void)dsc;
    res->disp = (void*)1;
    res->indev = (void*)1;
}

/* ── Event accessors ─────────────────────────────────────── */
static inline lv_obj_t * lv_event_get_target(lv_event_t * e) { (void)e; return NULL; }
static inline int lv_event_get_code(lv_event_t * e) { (void)e; return 0; }
static inline void * lv_event_get_user_data(lv_event_t * e) { (void)e; return NULL; }

/* ── Object manipulation ──────────────────────────────────── */
static inline void lv_obj_move_foreground(lv_obj_t * obj) { (void)obj; }
static inline lv_coord_t lv_obj_get_x(lv_obj_t * obj) { return obj ? obj->x : 0; }
static inline lv_coord_t lv_obj_get_y(lv_obj_t * obj) { return obj ? obj->y : 0; }

/* ── Flex layout ──────────────────────────────────────────── */
#define LV_FLEX_FLOW_COLUMN   0x01
#define LV_FLEX_FLOW_ROW      0x02
#define LV_FLEX_ALIGN_START   0
#define LV_FLEX_ALIGN_CENTER   1
#define LV_FLEX_ALIGN_END      2
static inline void lv_obj_set_flex_flow(lv_obj_t * obj, int flow) { (void)obj; (void)flow; }
static inline void lv_obj_set_flex_align(lv_obj_t * obj, int main, int cross, int track) { (void)obj; (void)main; (void)cross; (void)track; }
static inline void lv_obj_set_style_pad_row(lv_obj_t * obj, lv_coord_t pad, int sel) { (void)obj; (void)pad; (void)sel; }

/* ── Textarea ─────────────────────────────────────────────── */
static inline lv_obj_t * lv_textarea_create(lv_obj_t * parent) {
    return lv_obj_create(parent);
}
static inline void lv_textarea_set_text(lv_obj_t * ta, const char * text) {
    if(ta && text) strncpy(ta->text, text, MAX_LOCAL_TEXT_LEN-1);
}
static inline const char * lv_textarea_get_text(lv_obj_t * ta) {
    return ta ? ta->text : NULL;
}
static inline void lv_textarea_set_placeholder_text(lv_obj_t * ta, const char * txt) { (void)ta; (void)txt; }
static inline void lv_textarea_set_password_mode(lv_obj_t * ta, bool en) { (void)ta; (void)en; }

/* ── Keyboard ─────────────────────────────────────────────── */
static inline lv_obj_t * lv_keyboard_create(lv_obj_t * parent) {
    return lv_obj_create(parent);
}
static inline void lv_keyboard_set_textarea(lv_obj_t * kb, lv_obj_t * ta) { (void)kb; (void)ta; }

/* ── Symbols ──────────────────────────────────────────────── */
#define LV_SYMBOL_SETTINGS  "\xEF\x97\x92"
#define LV_SYMBOL_HOME      "\xEF\x82\xAB"

#ifdef __cplusplus
}
#endif

#endif
