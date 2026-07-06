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
#include <sys/ioctl.h>
#include <pthread.h>
#include <math.h>
#include <mqueue.h>

#include <nuttx/input/buttons.h>
#include <nuttx/audio/audio.h>

#include <lvgl.h>

#include "agent_bridge.h"
#include "radio_config.h"
#include "radio_log.h"
#include "location_service.h"
#include "input_lradc.h"
#include "wifi_auto_connect.h"
#include "audio_i2s.h"
#include "ui_ai_radio.h"

#define UI_REFRESH_MS       200
#define LCD_W               320
#define LCD_H               240

#define TOPBAR_H            26
#define SPECTRUM_H          76
#define CONTENT_H           106
#define BOTTOMBAR_H         32

#define SPECTRUM_Y          TOPBAR_H
#define CONTENT_Y           (TOPBAR_H + SPECTRUM_H)
#define BOTTOMBAR_Y         (CONTENT_Y + CONTENT_H)

#define CW_PANEL_W          155
#define CARD_START_X        160
#define CARD_COLS           2
#define CARD_ROWS           3
#define CARD_W              77
#define CARD_H              50
#define CARD_GAP_X          3
#define CARD_GAP_Y          3
#define CARD_PAD            2

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
#define COLOR_CYAN          lv_color_hex(0x00ffff)
#define COLOR_YELLOW        lv_color_hex(0xffdd33)
#define COLOR_SPEC_BG       lv_color_hex(0x060912)
#define COLOR_NOISE         lv_color_hex(0x405060)
#define COLOR_PEAK          lv_color_hex(0x00ffff)

#define BTN_VOL_DOWN        0x01
#define BTN_VOL_UP          0x02
#define BTN_MENU            0x04
#define BTN_ENTER           0x08
#define BTN_HOME            0x10

#define BTN_POLL_MS         50
#define BTN_DEBOUNCE_MS     200
#define BTN_LONGPRESS_MS    800

#define NUM_CARDS           (CARD_COLS * CARD_ROWS)
#define NUM_DEMOD_MODES     5
#define CW_LINES            4
#define SPEC_BARS           60

#define AUDIO_BPS           AUDIO_BITS_PER_SAMPLE
#define AUDIO_CHUNK_SAMPLES 160
#define AUDIO_RING_SIZE     512

#define FFT_SIZE            128
#define NUM_GOERTZEL        SPEC_BARS
#define CW_TONE_DEFAULT     700
#define CW_WPM_DEFAULT      15

#define SMOOTH_ALPHA_S      0.3f
#define SMOOTH_ALPHA_SPEC   0.5f
#define NOISE_FLOOR_ALPHA   0.05f
#define THRESHOLD_MULT      3.0f

typedef enum {
    CARD_SIG = 0,
    CARD_MAYDAY,
    CARD_AFC,
    CARD_FILTER,
    CARD_PTT,
    CARD_SETUP,
} card_idx_t;

typedef enum {
    DEMOD_USB = 0,
    DEMOD_LSB,
    DEMOD_CW,
    DEMOD_AM,
    DEMOD_FM
} demod_mode_t;

typedef struct {
    const char *code;
    char ch;
} morse_char_t;

static const char *MODE_NAMES[NUM_DEMOD_MODES] = {"USB", "LSB", "CW", "AM", "FM"};
static const char *MODE_FILTERS[NUM_DEMOD_MODES] = {"SSB 2.4k", "SSB 2.4k", "CW 700Hz", "AM 3.0k", "FM WIDE"};

static const morse_char_t MORSE_TABLE[] = {
    {".-", 'A'}, {"-...", 'B'}, {"-.-.", 'C'}, {"-..", 'D'}, {".", 'E'},
    {"..-.", 'F'}, {"--.", 'G'}, {"....", 'H'}, {"..", 'I'}, {".---", 'J'},
    {"-.-", 'K'}, {".-..", 'L'}, {"--", 'M'}, {"-.", 'N'}, {"---", 'O'},
    {".--.", 'P'}, {"--.-", 'Q'}, {".-.", 'R'}, {"...", 'S'}, {"-", 'T'},
    {"..-", 'U'}, {"...-", 'V'}, {".--", 'W'}, {"-..-", 'X'}, {"-.--", 'Y'},
    {"--..", 'Z'}, {".----", '1'}, {"..---", '2'}, {"...--", '3'}, {"....-", '4'},
    {".....", '5'}, {"-....", '6'}, {"--...", '7'}, {"---..", '8'}, {"----.", '9'},
    {"-----", '0'}, {"...---...", '!'}, {".-.-.", '+'}, {"-...-", '='},
    {"-..-.", '/'}, {"-.--.", '('}, {"-.--.-", ')'}, {".-.-.", '>'},
    {"...-.-", 'V'}, {NULL, 0}
};

static lv_obj_t *g_scr = NULL;
static lv_obj_t *g_time_label = NULL;
static lv_obj_t *g_freq_label = NULL;
static lv_obj_t *g_mode_label = NULL;
static lv_obj_t *g_s_meter_label = NULL;
static lv_obj_t *g_spec_bars[SPEC_BARS];
static lv_obj_t *g_noise_line = NULL;
static lv_obj_t *g_peak_line = NULL;
static lv_obj_t *g_spec_status_label = NULL;
static lv_obj_t *g_cw_status_label = NULL;
static lv_obj_t *g_cw_signal_dot = NULL;
static lv_obj_t *g_cw_labels[CW_LINES];
static lv_obj_t *g_cards[NUM_CARDS];
static lv_obj_t *g_card_val_labels[NUM_CARDS];
static lv_obj_t *g_hint_label = NULL;
static lv_obj_t *g_overlay = NULL;
static lv_obj_t *g_overlay_text = NULL;
static lv_obj_t *g_border_flash = NULL;

static uint32_t g_freq_hz = 14250000;
static int g_mode_idx = DEMOD_USB;
static int g_selected_card = 0;
static bool g_mayday_active = false;
static bool g_mayday_flash_on = false;
static bool g_ptt_active = false;
static int g_cw_tone_freq = CW_TONE_DEFAULT;
static int g_cw_wpm = CW_WPM_DEFAULT;
static float g_s_meter_db = -60.0f;
static int g_s_units = 0;
static int g_s_plus_db = 0;
static float g_noise_floor_mag = 100.0f;
static float g_spec_mag[SPEC_BARS];
static float g_spec_smooth[SPEC_BARS];
static int g_spec_peak_bin = 0;
static float g_spec_peak_mag = 0;
static int g_cw_line_idx = 0;
static char g_cw_lines[CW_LINES][48];
static bool g_cw_signal_present = false;
static float g_cw_afc_offset = 0;
static bool g_overlay_open = false;
static bool g_audio_ready = false;
static char g_audio_status[32];

static int g_audio_fd = -1;
static int g_btn_fd = -1;
static btn_buttonset_t g_btn_last = 0;
static uint32_t g_btn_last_time = 0;
static uint32_t g_btn_press_time = 0;
static btn_buttonset_t g_btn_current = 0;
static volatile bool g_running = true;
static pthread_t g_audio_thread;
static bool g_audio_thread_created = false;
static pthread_mutex_t g_dsp_mutex;

static int16_t g_ring_buffer[AUDIO_RING_SIZE];
static volatile int g_ring_head = 0;
static volatile int g_ring_tail = 0;
static volatile int g_ring_count = 0;

typedef struct {
    float coeff;
    float s1;
    float s2;
} goertzel_state_t;

static goertzel_state_t g_goertzel[SPEC_BARS + 1];
static goertzel_state_t g_goertzel_cw;

typedef enum {
    CW_STATE_IDLE = 0,
    CW_STATE_MARK,
    CW_STATE_SPACE,
    CW_STATE_INTER_CHAR,
    CW_STATE_INTER_WORD
} cw_state_t;

static cw_state_t g_cw_state = CW_STATE_IDLE;
static char g_cw_symbol_buf[16];
static int g_cw_symbol_len = 0;
static uint32_t g_cw_last_edge_time = 0;
static uint32_t g_cw_dit_ms = 80;
static int g_sos_count = 0;

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

static lv_color_t s_meter_color(int s, int plus)
{
    if (s <= 3) return COLOR_TEXT_DIM;
    if (s <= 5) return COLOR_GREEN;
    if (s <= 7) return COLOR_YELLOW;
    if (s <= 9) return COLOR_AMBER;
    return COLOR_RED;
}

static lv_color_t spec_bar_color(float mag_norm)
{
    if (mag_norm < 0.15f) return lv_color_hex(0x103060);
    if (mag_norm < 0.35f) return COLOR_ACCENT;
    if (mag_norm < 0.55f) return COLOR_GREEN;
    if (mag_norm < 0.78f) return COLOR_YELLOW;
    return COLOR_RED;
}

static int dbfs_to_sunits(float dbfs)
{
    float s0 = -54.0f;
    float s9 = -27.0f;
    float s9p20 = -7.0f;

    if (dbfs < s0) return 0;
    if (dbfs >= s9p20) return 9 + 20;

    if (dbfs < s9) {
        return (int)((dbfs - s0) / 3.0f) + 1;
    } else {
        return 9 + (int)((dbfs - s9) / 1.0f);
    }
}

static void s_units_to_str(int s_total, char *buf, size_t len)
{
    if (s_total <= 0) {
        snprintf(buf, len, "S0");
    } else if (s_total <= 9) {
        snprintf(buf, len, "S%d", s_total);
    } else {
        snprintf(buf, len, "S9+%d", s_total - 9);
    }
}

static void goertzel_init(goertzel_state_t *g, float freq_hz, int sample_rate, int n)
{
    float k = (int)(0.5f + (float)n * freq_hz / (float)sample_rate);
    float omega = 2.0f * (float)M_PI * k / (float)n;
    g->coeff = 2.0f * cosf(omega);
    g->s1 = 0.0f;
    g->s2 = 0.0f;
}

static float goertzel_mag(goertzel_state_t *g, const int16_t *samples, int n)
{
    g->s1 = 0.0f;
    g->s2 = 0.0f;
    for (int i = 0; i < n; i++) {
        float s0 = (float)samples[i] + g->coeff * g->s1 - g->s2;
        g->s2 = g->s1;
        g->s1 = s0;
    }
    float power = g->s1 * g->s1 + g->s2 * g->s2 - g->coeff * g->s1 * g->s2;
    if (power < 0) power = 0;
    return sqrtf(power) / (float)n;
}

static void ring_buffer_write(const int16_t *data, int count)
{
    for (int i = 0; i < count; i++) {
        g_ring_buffer[g_ring_head] = data[i];
        g_ring_head = (g_ring_head + 1) % AUDIO_RING_SIZE;
        if (g_ring_count < AUDIO_RING_SIZE) {
            g_ring_count++;
        } else {
            g_ring_tail = (g_ring_tail + 1) % AUDIO_RING_SIZE;
        }
    }
}

static int ring_buffer_read(int16_t *data, int count)
{
    int available = g_ring_count;
    int to_read = count < available ? count : available;
    for (int i = 0; i < to_read; i++) {
        data[i] = g_ring_buffer[g_ring_tail];
        g_ring_tail = (g_ring_tail + 1) % AUDIO_RING_SIZE;
    }
    g_ring_count -= to_read;
    return to_read;
}

static char morse_lookup(const char *symbol)
{
    if (strcmp(symbol, "...---...") == 0) return '!';
    for (int i = 0; MORSE_TABLE[i].code != NULL; i++) {
        if (strcmp(MORSE_TABLE[i].code, symbol) == 0) {
            return MORSE_TABLE[i].ch;
        }
    }
    return '?';
}

static void cw_append_char(char c)
{
    if (c == '!') {
        g_mayday_active = true;
        c = '!';
    }

    int len = strlen(g_cw_lines[g_cw_line_idx]);
    if (len >= 44 || c == '\n' || c == ' ') {
        g_cw_line_idx = (g_cw_line_idx + 1) % CW_LINES;
        memset(g_cw_lines[g_cw_line_idx], 0, sizeof(g_cw_lines[g_cw_line_idx]));
        for (int i = 0; i < CW_LINES; i++) {
            int li = (g_cw_line_idx - i + CW_LINES) % CW_LINES;
            lv_label_set_text(g_cw_labels[i], g_cw_lines[li]);
        }
        if (c == '\n' || c == ' ') return;
    }
    len = strlen(g_cw_lines[g_cw_line_idx]);
    if (len < 44) {
        g_cw_lines[g_cw_line_idx][len] = c;
        g_cw_lines[g_cw_line_idx][len + 1] = '\0';
        lv_label_set_text(g_cw_labels[0], g_cw_lines[g_cw_line_idx]);
    }
}

static void cw_process_symbol(void)
{
    if (g_cw_symbol_len == 0) return;
    g_cw_symbol_buf[g_cw_symbol_len] = '\0';
    char ch = morse_lookup(g_cw_symbol_buf);
    cw_append_char(ch);
    g_cw_symbol_len = 0;
}

static void dsp_process_chunk(const int16_t *samples, int n)
{
    pthread_mutex_lock(&g_dsp_mutex);

    double sum_sq = 0.0;

    for (int i = 0; i < n; i++) {
        float s = (float)samples[i] / 32768.0f;
        sum_sq += (double)s * (double)s;
    }

    float rms = sqrtf((float)(sum_sq / (double)n));
    float dbfs = 20.0f * log10f(rms + 1e-10f);

    float new_s = SMOOTH_ALPHA_S * dbfs + (1.0f - SMOOTH_ALPHA_S) * g_s_meter_db;
    g_s_meter_db = new_s;
    int s_total = dbfs_to_sunits(g_s_meter_db);
    if (s_total > 9) {
        g_s_units = 9;
        g_s_plus_db = s_total - 9;
    } else {
        g_s_units = s_total;
        g_s_plus_db = 0;
    }

    float max_mag = 0;
    int peak_bin = 0;
    for (int b = 0; b < SPEC_BARS; b++) {
        float freq = (float)b * (4000.0f / (float)(SPEC_BARS - 1));
        goertzel_init(&g_goertzel[b], freq, AUDIO_SAMPLE_RATE, n);
        float mag = goertzel_mag(&g_goertzel[b], samples, n);
        g_spec_mag[b] = mag;
        if (mag > max_mag) {
            max_mag = mag;
            peak_bin = b;
        }
    }

    if (max_mag > g_noise_floor_mag * 1.5f) {
        g_spec_peak_mag = max_mag;
        g_spec_peak_bin = peak_bin;
        g_cw_afc_offset = (float)peak_bin * (4000.0f / (float)(SPEC_BARS - 1)) - (float)g_cw_tone_freq;
    }

    g_noise_floor_mag = (1.0f - NOISE_FLOOR_ALPHA) * g_noise_floor_mag + NOISE_FLOOR_ALPHA * max_mag * 0.5f;

    for (int b = 0; b < SPEC_BARS; b++) {
        g_spec_smooth[b] = SMOOTH_ALPHA_SPEC * g_spec_mag[b] + (1.0f - SMOOTH_ALPHA_SPEC) * g_spec_smooth[b];
    }

    goertzel_init(&g_goertzel_cw, (float)g_cw_tone_freq, AUDIO_SAMPLE_RATE, n);
    float cw_mag = goertzel_mag(&g_goertzel_cw, samples, n);
    float cw_threshold = g_noise_floor_mag * THRESHOLD_MULT;

    uint32_t now = get_ms();
    bool cw_on = (cw_mag > cw_threshold);
    g_cw_signal_present = cw_on;

    if (g_cw_state == CW_STATE_IDLE) {
        if (cw_on) {
            g_cw_state = CW_STATE_MARK;
            g_cw_last_edge_time = now;
        }
    } else if (g_cw_state == CW_STATE_MARK) {
        if (!cw_on) {
            uint32_t dur = now - g_cw_last_edge_time;
            if (dur < g_cw_dit_ms * 2) {
                if (g_cw_symbol_len < 15) g_cw_symbol_buf[g_cw_symbol_len++] = '.';
            } else {
                if (g_cw_symbol_len < 15) g_cw_symbol_buf[g_cw_symbol_len++] = '-';
            }
            g_cw_state = CW_STATE_SPACE;
            g_cw_last_edge_time = now;
        }
    } else if (g_cw_state == CW_STATE_SPACE) {
        if (cw_on) {
            uint32_t gap = now - g_cw_last_edge_time;
            if (gap > g_cw_dit_ms * 5) {
                cw_process_symbol();
                cw_append_char(' ');
                g_cw_state = CW_STATE_MARK;
            } else if (gap > g_cw_dit_ms * 2) {
                cw_process_symbol();
                g_cw_state = CW_STATE_MARK;
            } else {
                g_cw_state = CW_STATE_MARK;
            }
            g_cw_last_edge_time = now;
        } else {
            uint32_t gap = now - g_cw_last_edge_time;
            if (gap > g_cw_dit_ms * 5) {
                cw_process_symbol();
                cw_append_char(' ');
                g_cw_state = CW_STATE_IDLE;
            } else if (gap > g_cw_dit_ms * 2) {
                cw_process_symbol();
                g_cw_state = CW_STATE_INTER_CHAR;
                g_cw_last_edge_time = now;
            }
        }
    } else if (g_cw_state == CW_STATE_INTER_CHAR) {
        if (cw_on) {
            g_cw_state = CW_STATE_MARK;
            g_cw_last_edge_time = now;
        } else {
            uint32_t gap = now - g_cw_last_edge_time;
            if (gap > g_cw_dit_ms * 5) {
                cw_append_char(' ');
                g_cw_state = CW_STATE_IDLE;
            }
        }
    }

    if (g_cw_symbol_len >= 3) {
        g_cw_symbol_buf[g_cw_symbol_len] = '\0';
        if (strcmp(g_cw_symbol_buf, "...") == 0) {
            g_sos_count++;
            if (g_sos_count >= 3) g_sos_count = 0;
        }
    }

    agent_bridge_send_audio(samples, n);
    agent_bridge_set_frequency((float)g_freq_hz);
    agent_bridge_set_mode(MODE_NAMES[g_mode_idx]);

    pthread_mutex_unlock(&g_dsp_mutex);
}

static void *audio_thread_func(void *arg)
{
    (void)arg;
    int ret;
    int retries = 0;
    bool audio_failed = false;
    mqd_t mq = (mqd_t)-1;
    struct ap_buffer_s *buffers[8] = {NULL};
    int num_bufs = 0;

    while (g_running) {
        if (audio_failed) {
            usleep(100000);
            int16_t proc_buf[FFT_SIZE];
            while (g_ring_count >= FFT_SIZE) {
                int got = ring_buffer_read(proc_buf, FFT_SIZE);
                if (got == FFT_SIZE) {
                    dsp_process_chunk(proc_buf, FFT_SIZE);
                }
            }
            continue;
        }

        if (g_audio_fd < 0) {
            snprintf(g_audio_status, sizeof(g_audio_status), "WAITING FOR AUDIO...");
            g_audio_ready = false;
            sleep(2);

            g_audio_fd = open("/dev/audio/pcm0c", O_RDWR);
            if (g_audio_fd < 0) {
                g_audio_fd = open("/dev/pcmC0D0c", O_RDWR);
            }
            if (g_audio_fd < 0) {
                g_audio_fd = open("/dev/audio_in", O_RDWR);
            }
            if (g_audio_fd < 0) {
                retries++;
                printf("[AUDIO] Failed to open device, retries=%d\n", retries);
                if (retries >= 3) {
                    audio_failed = true;
                    snprintf(g_audio_status, sizeof(g_audio_status), "NO AUDIO HW");
                    g_audio_ready = false;
                    printf("[AUDIO] No audio hardware, disabling\n");
                }
                continue;
            }
            retries = 0;

            ret = ioctl(g_audio_fd, AUDIOIOC_RESERVE, 0);
            if (ret < 0) {
                printf("[AUDIO] AUDIOIOC_RESERVE failed: %d\n", errno);
                close(g_audio_fd);
                g_audio_fd = -1;
                continue;
            }

            struct audio_caps_desc_s cap_desc;
            memset(&cap_desc, 0, sizeof(cap_desc));
            cap_desc.caps.ac_len = sizeof(struct audio_caps_s);
            cap_desc.caps.ac_type = AUDIO_TYPE_INPUT;
            cap_desc.caps.ac_channels = AUDIO_CHANNELS;
            cap_desc.caps.ac_chmap = 0;
            cap_desc.caps.ac_controls.hw[0] = AUDIO_SAMPLE_RATE;
            cap_desc.caps.ac_controls.b[3] = (AUDIO_SAMPLE_RATE >> 16) & 0xff;
            cap_desc.caps.ac_controls.b[2] = AUDIO_BPS;
            cap_desc.caps.ac_subtype = AUDIO_FMT_PCM;

            ret = ioctl(g_audio_fd, AUDIOIOC_CONFIGURE, (unsigned long)&cap_desc);
            if (ret < 0) {
                printf("[AUDIO] AUDIOIOC_CONFIGURE failed: %d\n", errno);
                ioctl(g_audio_fd, AUDIOIOC_RELEASE, 0);
                close(g_audio_fd);
                g_audio_fd = -1;
                continue;
            }

            struct ap_buffer_info_s buf_info;
            if (ioctl(g_audio_fd, AUDIOIOC_GETBUFFERINFO, (unsigned long)&buf_info) != 0) {
                buf_info.nbuffers = 4;
                buf_info.buffer_size = 1024;
            }
            if (buf_info.nbuffers > 8) buf_info.nbuffers = 8;

            char mq_name[32];
            snprintf(mq_name, sizeof(mq_name), "/ai_radio_mq_%d", g_audio_fd);
            mq_unlink(mq_name);
            struct mq_attr attr;
            attr.mq_maxmsg = buf_info.nbuffers + 8;
            attr.mq_msgsize = sizeof(struct audio_msg_s);
            attr.mq_curmsgs = 0;
            attr.mq_flags = 0;
            mq = mq_open(mq_name, O_RDWR | O_CREAT, 0644, &attr);
            if (mq == (mqd_t)-1) {
                printf("[AUDIO] mq_open failed: %d\n", errno);
                ioctl(g_audio_fd, AUDIOIOC_RELEASE, 0);
                close(g_audio_fd);
                g_audio_fd = -1;
                continue;
            }

            ret = ioctl(g_audio_fd, AUDIOIOC_REGISTERMQ, (unsigned long)mq);
            if (ret < 0) {
                printf("[AUDIO] AUDIOIOC_REGISTERMQ failed: %d\n", errno);
                mq_close(mq);
                mq_unlink(mq_name);
                mq = (mqd_t)-1;
                ioctl(g_audio_fd, AUDIOIOC_RELEASE, 0);
                close(g_audio_fd);
                g_audio_fd = -1;
                continue;
            }

            num_bufs = 0;
            for (int i = 0; i < buf_info.nbuffers; i++) {
                struct audio_buf_desc_s bufdesc;
                memset(&bufdesc, 0, sizeof(bufdesc));
                bufdesc.numbytes = buf_info.buffer_size;
                bufdesc.u.pbuffer = &buffers[i];
                ret = ioctl(g_audio_fd, AUDIOIOC_ALLOCBUFFER, (unsigned long)&bufdesc);
                if (ret != sizeof(bufdesc) || buffers[i] == NULL) {
                    printf("[AUDIO] AUDIOIOC_ALLOCBUFFER %d failed: %d\n", i, errno);
                    break;
                }
                num_bufs++;
            }
            if (num_bufs < 2) {
                printf("[AUDIO] Not enough buffers allocated\n");
                for (int i = 0; i < num_bufs; i++) {
                    struct audio_buf_desc_s bufdesc;
                    memset(&bufdesc, 0, sizeof(bufdesc));
                    bufdesc.u.buffer = buffers[i];
                    ioctl(g_audio_fd, AUDIOIOC_FREEBUFFER, (unsigned long)&bufdesc);
                    buffers[i] = NULL;
                }
                num_bufs = 0;
                ioctl(g_audio_fd, AUDIOIOC_UNREGISTERMQ, (unsigned long)mq);
                mq_close(mq);
                mq_unlink(mq_name);
                mq = (mqd_t)-1;
                ioctl(g_audio_fd, AUDIOIOC_RELEASE, 0);
                close(g_audio_fd);
                g_audio_fd = -1;
                continue;
            }

            for (int i = 0; i < num_bufs; i++) {
                buffers[i]->curbyte = 0;
                buffers[i]->flags = 0;
                buffers[i]->nbytes = buffers[i]->nmaxbytes;
                struct audio_buf_desc_s bufdesc;
                memset(&bufdesc, 0, sizeof(bufdesc));
                bufdesc.numbytes = buffers[i]->nbytes;
                bufdesc.u.buffer = buffers[i];
                ret = ioctl(g_audio_fd, AUDIOIOC_ENQUEUEBUFFER, (unsigned long)&bufdesc);
                if (ret < 0) {
                    printf("[AUDIO] AUDIOIOC_ENQUEUEBUFFER %d failed: %d\n", i, errno);
                }
            }

            ret = ioctl(g_audio_fd, AUDIOIOC_START, 0);
            if (ret < 0) {
                printf("[AUDIO] AUDIOIOC_START failed: %d\n", errno);
                ioctl(g_audio_fd, AUDIOIOC_STOP, 0);
                for (int i = 0; i < num_bufs; i++) {
                    struct audio_buf_desc_s bufdesc;
                    memset(&bufdesc, 0, sizeof(bufdesc));
                    bufdesc.u.buffer = buffers[i];
                    ioctl(g_audio_fd, AUDIOIOC_FREEBUFFER, (unsigned long)&bufdesc);
                    buffers[i] = NULL;
                }
                num_bufs = 0;
                ioctl(g_audio_fd, AUDIOIOC_UNREGISTERMQ, (unsigned long)mq);
                mq_close(mq);
                mq_unlink(mq_name);
                mq = (mqd_t)-1;
                ioctl(g_audio_fd, AUDIOIOC_RELEASE, 0);
                close(g_audio_fd);
                g_audio_fd = -1;
                continue;
            }

            g_audio_ready = true;
            snprintf(g_audio_status, sizeof(g_audio_status), "AUDIO OK");
            printf("[AUDIO] Capture started fd=%d, bufs=%d, size=%d, rate=%d\n",
                   g_audio_fd, num_bufs, buf_info.buffer_size, AUDIO_SAMPLE_RATE);
            continue;
        }

        struct audio_msg_s msg;
        unsigned int prio;
        ssize_t n = mq_receive(mq, (char *)&msg, sizeof(msg), &prio);
        if (n != sizeof(msg)) {
            if (errno == EINTR) continue;
            printf("[AUDIO] mq_receive failed: %d\n", errno);
            ioctl(g_audio_fd, AUDIOIOC_STOP, 0);
            for (int i = 0; i < num_bufs; i++) {
                struct audio_buf_desc_s bufdesc;
                memset(&bufdesc, 0, sizeof(bufdesc));
                bufdesc.u.buffer = buffers[i];
                ioctl(g_audio_fd, AUDIOIOC_FREEBUFFER, (unsigned long)&bufdesc);
                buffers[i] = NULL;
            }
            num_bufs = 0;
            char mq_name[32];
            snprintf(mq_name, sizeof(mq_name), "/ai_radio_mq_%d", g_audio_fd);
            ioctl(g_audio_fd, AUDIOIOC_UNREGISTERMQ, (unsigned long)mq);
            mq_close(mq);
            mq_unlink(mq_name);
            mq = (mqd_t)-1;
            ioctl(g_audio_fd, AUDIOIOC_RELEASE, 0);
            close(g_audio_fd);
            g_audio_fd = -1;
            g_audio_ready = false;
            continue;
        }

        switch (msg.msg_id) {
        case AUDIO_MSG_DEQUEUE: {
            struct ap_buffer_s *apb = (struct ap_buffer_s *)msg.u.ptr;
            if (apb && apb->nbytes > 0) {
                int samples = apb->nbytes / sizeof(int16_t);
                int16_t *samps = (int16_t *)apb->samp;
                ring_buffer_write(samps, samples);

                int16_t proc_buf[FFT_SIZE];
                while (g_ring_count >= FFT_SIZE) {
                    int got = ring_buffer_read(proc_buf, FFT_SIZE);
                    if (got == FFT_SIZE) {
                        dsp_process_chunk(proc_buf, FFT_SIZE);
                    }
                }
            }

            if (apb) {
                apb->curbyte = 0;
                apb->flags = 0;
                apb->nbytes = apb->nmaxbytes;
                struct audio_buf_desc_s bufdesc;
                memset(&bufdesc, 0, sizeof(bufdesc));
                bufdesc.numbytes = apb->nbytes;
                bufdesc.u.buffer = apb;
                ioctl(g_audio_fd, AUDIOIOC_ENQUEUEBUFFER, (unsigned long)&bufdesc);
            }
            break;
        }

        case AUDIO_MSG_STOP:
        case AUDIO_MSG_COMPLETE:
        case AUDIO_MSG_IOERR:
            printf("[AUDIO] Received stop/complete/ioerr message: %d\n", msg.msg_id);
            ioctl(g_audio_fd, AUDIOIOC_STOP, 0);
            for (int i = 0; i < num_bufs; i++) {
                struct audio_buf_desc_s bufdesc;
                memset(&bufdesc, 0, sizeof(bufdesc));
                bufdesc.u.buffer = buffers[i];
                ioctl(g_audio_fd, AUDIOIOC_FREEBUFFER, (unsigned long)&bufdesc);
                buffers[i] = NULL;
            }
            num_bufs = 0;
            {
                char mq_name[32];
                snprintf(mq_name, sizeof(mq_name), "/ai_radio_mq_%d", g_audio_fd);
                ioctl(g_audio_fd, AUDIOIOC_UNREGISTERMQ, (unsigned long)mq);
                mq_close(mq);
                mq_unlink(mq_name);
            }
            mq = (mqd_t)-1;
            ioctl(g_audio_fd, AUDIOIOC_RELEASE, 0);
            close(g_audio_fd);
            g_audio_fd = -1;
            g_audio_ready = false;
            break;

        default:
            break;
        }
    }

    if (g_audio_fd >= 0) {
        ioctl(g_audio_fd, AUDIOIOC_STOP, 0);
        for (int i = 0; i < num_bufs; i++) {
            if (buffers[i]) {
                struct audio_buf_desc_s bufdesc;
                memset(&bufdesc, 0, sizeof(bufdesc));
                bufdesc.u.buffer = buffers[i];
                ioctl(g_audio_fd, AUDIOIOC_FREEBUFFER, (unsigned long)&bufdesc);
            }
        }
        if (mq != (mqd_t)-1) {
            char mq_name[32];
            snprintf(mq_name, sizeof(mq_name), "/ai_radio_mq_%d", g_audio_fd);
            ioctl(g_audio_fd, AUDIOIOC_UNREGISTERMQ, (unsigned long)mq);
            mq_close(mq);
            mq_unlink(mq_name);
        }
        ioctl(g_audio_fd, AUDIOIOC_RELEASE, 0);
        close(g_audio_fd);
        g_audio_fd = -1;
    }
    return NULL;
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
    set_mode_label_color();
}

static void update_cards_display(void)
{
    char s_str[16];
    if (g_s_plus_db > 0) {
        snprintf(s_str, sizeof(s_str), "S9+%d", g_s_plus_db);
    } else {
        snprintf(s_str, sizeof(s_str), "S%d", g_s_units);
    }
    lv_label_set_text(g_card_val_labels[CARD_SIG], s_str);
    lv_obj_set_style_text_color(g_card_val_labels[CARD_SIG],
        s_meter_color(g_s_units, g_s_plus_db), 0);

    if (g_mayday_active) {
        lv_label_set_text(g_card_val_labels[CARD_MAYDAY], "ALERT!");
        lv_obj_set_style_text_color(g_card_val_labels[CARD_MAYDAY], COLOR_RED, 0);
    } else {
        lv_label_set_text(g_card_val_labels[CARD_MAYDAY], "OK");
        lv_obj_set_style_text_color(g_card_val_labels[CARD_MAYDAY], COLOR_GREEN, 0);
    }

    char afc_str[16];
    if (fabsf(g_cw_afc_offset) < 5.0f) {
        snprintf(afc_str, sizeof(afc_str), "LOCK");
    } else if (g_cw_afc_offset > 0) {
        snprintf(afc_str, sizeof(afc_str), "%+dHz", (int)g_cw_afc_offset);
    } else {
        snprintf(afc_str, sizeof(afc_str), "%dHz", (int)g_cw_afc_offset);
    }
    lv_label_set_text(g_card_val_labels[CARD_AFC], afc_str);
    lv_obj_set_style_text_color(g_card_val_labels[CARD_AFC], COLOR_CYAN, 0);

    lv_label_set_text(g_card_val_labels[CARD_FILTER], MODE_FILTERS[g_mode_idx]);
    lv_obj_set_style_text_color(g_card_val_labels[CARD_FILTER], COLOR_AMBER, 0);

    if (g_ptt_active) {
        lv_label_set_text(g_card_val_labels[CARD_PTT], "TX");
        lv_obj_set_style_text_color(g_card_val_labels[CARD_PTT], COLOR_RED, 0);
    } else {
        lv_label_set_text(g_card_val_labels[CARD_PTT], "RX");
        lv_obj_set_style_text_color(g_card_val_labels[CARD_PTT], COLOR_GREEN, 0);
    }

    lv_label_set_text(g_card_val_labels[CARD_SETUP], "SETUP");
    lv_obj_set_style_text_color(g_card_val_labels[CARD_SETUP], COLOR_TEXT_DIM, 0);
}

static void toggle_ptt(void)
{
    g_ptt_active = !g_ptt_active;
    printf("[PTT] %s\n", g_ptt_active ? "TX" : "RX");
}

static void handle_card_enter(void)
{
    switch (g_selected_card) {
    case CARD_MAYDAY:
        g_mayday_active = true;
        open_overlay("MAYDAY ALERT",
            "SOS distress signal detected!\n"
            "Monitor 2182kHz / 14300kHz\n"
            "Press ENTER to cancel\n"
            "HOME to return");
        break;
    case CARD_PTT:
        toggle_ptt();
        break;
    case CARD_SETUP:
        open_overlay("Settings",
            "Mode: USB/LSB/CW/AM/FM (MENU)\n"
            "CW Tone: 500-1000Hz\n"
            "WPM: 10-25\n"
            "Freq Step: 100Hz-1MHz");
        break;
    default:
        break;
    }
}

static void handle_button_press(btn_buttonset_t btn, bool long_press)
{
    printf("[BTN] 0x%02x %s\n", (unsigned)btn, long_press ? "LONG" : "SHORT");

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
        } else {
            uint32_t steps[] = {100, 500, 1000, 5000, 10000, 100000, 1000000};
            g_freq_hz += dir * steps[2];
            if (g_freq_hz < 1800000) g_freq_hz = 1800000;
            if (g_freq_hz > 54000000) g_freq_hz = 54000000;
        }
        update_card_highlight();
        update_freq_display();
    } else if (btn == BTN_MENU) {
        if (long_press) {
            g_cw_tone_freq += 50;
            if (g_cw_tone_freq > 1000) g_cw_tone_freq = 500;
        } else {
            g_mode_idx = (g_mode_idx + 1) % NUM_DEMOD_MODES;
        }
        update_freq_display();
    } else if (btn == BTN_ENTER) {
        if (long_press) {
            handle_card_enter();
        } else {
            toggle_ptt();
        }
    } else if (btn == BTN_HOME) {
        if (long_press) {
            g_selected_card = CARD_SETUP;
            handle_card_enter();
        } else {
            g_selected_card = 0;
            g_mode_idx = DEMOD_USB;
            g_freq_hz = 14250000;
            g_ptt_active = false;
            g_mayday_active = false;
            update_card_highlight();
            update_freq_display();
        }
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
    btn_buttonset_t released = g_btn_last & ~val;

    if (pressed && g_btn_current == 0) {
        g_btn_current = pressed;
        g_btn_press_time = now;
    }

    if (released & g_btn_current) {
        uint32_t held = now - g_btn_press_time;
        bool is_long = (held >= BTN_LONGPRESS_MS);

        btn_buttonset_t single_btn = 0;
        for (int i = 0; i < 5; i++) {
            btn_buttonset_t mask = (btn_buttonset_t)1 << i;
            if (g_btn_current & mask) {
                single_btn = mask;
                break;
            }
        }

        if (single_btn && now - g_btn_last_time >= BTN_DEBOUNCE_MS) {
            g_btn_last_time = now;
            handle_button_press(single_btn, is_long);
        }
        g_btn_current = 0;
    }

    g_btn_last = val;
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

static void ui_update_timer(lv_timer_t *timer)
{
    (void)timer;
    static time_t start_time = 0;
    if (start_time == 0) start_time = time(NULL);
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    if (tm_info) {
        char time_buf[16];
        strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tm_info);
        lv_label_set_text(g_time_label, time_buf);
    }

    char s_str[16];
    int s_total = g_s_units + g_s_plus_db;
    s_units_to_str(s_total, s_str, sizeof(s_str));
    lv_label_set_text(g_s_meter_label, s_str);
    lv_obj_set_style_text_color(g_s_meter_label,
        s_meter_color(g_s_units, g_s_plus_db), 0);

    int bar_w = (LCD_W - 50) / SPEC_BARS;
    int bar_area_h = SPECTRUM_H - 30;
    pthread_mutex_lock(&g_dsp_mutex);

    float max_smooth = 0.001f;
    for (int i = 0; i < SPEC_BARS; i++) {
        if (g_spec_smooth[i] > max_smooth) max_smooth = g_spec_smooth[i];
    }

    for (int i = 0; i < SPEC_BARS; i++) {
        float norm = g_spec_smooth[i] / (g_noise_floor_mag * 8.0f);
        if (norm > 1.0f) norm = 1.0f;
        int h = (int)(norm * (float)bar_area_h);
        if (h < 1) h = 1;
        lv_obj_set_size(g_spec_bars[i], bar_w - 1, h);
        lv_obj_set_pos(g_spec_bars[i], 40 + i * bar_w, SPECTRUM_H - 14 - h);
        lv_obj_set_style_bg_color(g_spec_bars[i], spec_bar_color(norm), 0);
    }

    float noise_norm = g_noise_floor_mag / (g_noise_floor_mag * 8.0f) * 0.8f;
    int noise_y = SPECTRUM_H - 14 - (int)(noise_norm * (float)bar_area_h);
    lv_obj_set_pos(g_noise_line, 40, noise_y);

    int peak_x = 40 + g_spec_peak_bin * bar_w;
    lv_obj_set_pos(g_peak_line, peak_x, SPECTRUM_H - 14 - bar_area_h - 2);

    pthread_mutex_unlock(&g_dsp_mutex);

    if (g_audio_ready) {
        lv_label_set_text(g_spec_status_label, "AUDIO SPECTRUM 0-4kHz");
    } else {
        lv_label_set_text(g_spec_status_label, g_audio_status);
    }

    char cw_status[32];
    snprintf(cw_status, sizeof(cw_status), "CW %dHz %dWPM", g_cw_tone_freq, g_cw_wpm);
    lv_label_set_text(g_cw_status_label, cw_status);
    lv_obj_set_style_bg_color(g_cw_signal_dot,
        g_cw_signal_present ? COLOR_RED : COLOR_TEXT_DIM, 0);

    update_cards_display();
    update_freq_display();
}

static int button_init(void)
{
    /* Inline LVGL button polling is disabled; LRADC input is handled
     * by the input_lradc module to avoid sharing /dev/input/event1. */
    g_btn_fd = -1;
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

    g_time_label = create_label(topbar, 4, 5, "00:00:00",
                                &lv_font_montserrat_10, COLOR_TEXT);

    g_freq_label = create_label(topbar, -1, 2, "14.250.000",
                                &lv_font_montserrat_16, COLOR_ACCENT);
    lv_obj_center(g_freq_label);
    lv_obj_set_pos(g_freq_label, -25, 3);

    g_mode_label = create_label(topbar, -1, 8, "USB",
                                &lv_font_montserrat_10, COLOR_ACCENT);
    lv_obj_align(g_mode_label, LV_ALIGN_CENTER, 52, 0);

    g_s_meter_label = create_label(topbar, LCD_W - 38, 7, "S0",
                                   &lv_font_montserrat_10, COLOR_TEXT_DIM);

    lv_obj_t *spectrum_bg = lv_obj_create(g_scr);
    lv_obj_set_size(spectrum_bg, LCD_W, SPECTRUM_H);
    lv_obj_set_pos(spectrum_bg, 0, SPECTRUM_Y);
    lv_obj_set_style_bg_color(spectrum_bg, COLOR_SPEC_BG, 0);
    lv_obj_set_style_border_width(spectrum_bg, 0, 0);
    lv_obj_set_style_pad_all(spectrum_bg, 0, 0);
    lv_obj_set_style_radius(spectrum_bg, 0, 0);

    g_spec_status_label = create_label(spectrum_bg, 4, 2, "AUDIO SPECTRUM 0-4kHz",
                                       &lv_font_montserrat_10, COLOR_TEXT_DIM);

    create_label(spectrum_bg, 38, SPECTRUM_H - 12, "0", &lv_font_montserrat_10, COLOR_TEXT_DIM);
    create_label(spectrum_bg, 80, SPECTRUM_H - 12, "1k", &lv_font_montserrat_10, COLOR_TEXT_DIM);
    create_label(spectrum_bg, 140, SPECTRUM_H - 12, "2k", &lv_font_montserrat_10, COLOR_TEXT_DIM);
    create_label(spectrum_bg, 200, SPECTRUM_H - 12, "3k", &lv_font_montserrat_10, COLOR_TEXT_DIM);
    create_label(spectrum_bg, LCD_W - 28, SPECTRUM_H - 12, "4k", &lv_font_montserrat_10, COLOR_TEXT_DIM);

    int bar_w = (LCD_W - 50) / SPEC_BARS;
    for (int i = 0; i < SPEC_BARS; i++) {
        g_spec_bars[i] = create_bar(spectrum_bg, 40 + i * bar_w, SPECTRUM_H - 15,
                                    bar_w - 1, 2, lv_color_hex(0x103060));
        g_spec_smooth[i] = 0;
    }

    g_noise_line = create_bar(spectrum_bg, 40, SPECTRUM_H - 20, LCD_W - 45, 1, COLOR_NOISE);
    g_peak_line = create_bar(spectrum_bg, 160, SPECTRUM_H - 48, 2, 3, COLOR_PEAK);

    lv_obj_t *content_bg = lv_obj_create(g_scr);
    lv_obj_set_size(content_bg, LCD_W, CONTENT_H);
    lv_obj_set_pos(content_bg, 0, CONTENT_Y);
    lv_obj_set_style_bg_color(content_bg, COLOR_BG, 0);
    lv_obj_set_style_border_width(content_bg, 0, 0);
    lv_obj_set_style_pad_all(content_bg, 0, 0);
    lv_obj_set_style_radius(content_bg, 0, 0);

    lv_obj_t *cw_box = lv_obj_create(content_bg);
    lv_obj_set_size(cw_box, CW_PANEL_W - 4, CONTENT_H - 4);
    lv_obj_set_pos(cw_box, 4, 2);
    lv_obj_set_style_bg_color(cw_box, COLOR_SPEC_BG, 0);
    lv_obj_set_style_border_color(cw_box, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(cw_box, 1, 0);
    lv_obj_set_style_radius(cw_box, 4, 0);
    lv_obj_set_style_pad_all(cw_box, 4, 0);

    g_cw_status_label = create_label(cw_box, 4, 2, "CW 700Hz 15WPM",
                                     &lv_font_montserrat_10, COLOR_GREEN);

    g_cw_signal_dot = lv_obj_create(cw_box);
    lv_obj_set_size(g_cw_signal_dot, 6, 6);
    lv_obj_set_style_bg_color(g_cw_signal_dot, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_radius(g_cw_signal_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(g_cw_signal_dot, 0, 0);
    lv_obj_set_style_pad_all(g_cw_signal_dot, 0, 0);
    lv_obj_set_pos(g_cw_signal_dot, CW_PANEL_W - 22, 4);

    memset(g_cw_lines, 0, sizeof(g_cw_lines));
    strncpy(g_cw_lines[0], "...", sizeof(g_cw_lines[0]) - 1);
    for (int i = 0; i < CW_LINES; i++) {
        g_cw_labels[i] = create_label(cw_box, 4, 18 + i * 20, g_cw_lines[i],
                                      &lv_font_montserrat_10, COLOR_TEXT);
    }

    const char *CARD_TITLES[NUM_CARDS] = {"SIG", "MAYDAY", "AFC", "FILTER", "PTT", "SETUP"};
    const lv_color_t card_title_colors[NUM_CARDS] = {
        COLOR_GREEN, COLOR_RED, COLOR_CYAN, COLOR_AMBER, COLOR_ACCENT, COLOR_TEXT_DIM
    };

    for (int row = 0; row < CARD_ROWS; row++) {
        for (int col = 0; col < CARD_COLS; col++) {
            int idx = row * CARD_COLS + col;
            int x = CARD_START_X + CARD_PAD + col * (CARD_W + CARD_GAP_X);
            int y = CARD_PAD + row * (CARD_H + CARD_GAP_Y);
            g_cards[idx] = lv_obj_create(content_bg);
            lv_obj_set_size(g_cards[idx], CARD_W, CARD_H);
            lv_obj_set_pos(g_cards[idx], x, y);
            lv_obj_set_style_bg_color(g_cards[idx], COLOR_CARD, 0);
            lv_obj_set_style_border_color(g_cards[idx], COLOR_CARD_BORDER, 0);
            lv_obj_set_style_border_width(g_cards[idx], 1, 0);
            lv_obj_set_style_radius(g_cards[idx], 4, 0);
            lv_obj_set_style_pad_all(g_cards[idx], 3, 0);

            create_label(g_cards[idx], 3, 1, CARD_TITLES[idx],
                         &lv_font_montserrat_10, card_title_colors[idx]);

            g_card_val_labels[idx] = create_label(g_cards[idx], 3, 16, "",
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
        "VOL+-:Tune  MENU:Mode  ENTER:PTT  HOME:Hold=Setup",
        &lv_font_montserrat_10, COLOR_TEXT_DIM);
    lv_obj_center(g_hint_label);

    lv_scr_load(g_scr);
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("========================================\n");
    printf("  AI Radio Console v2.0 for openvela\n");
    printf("  REAL AUDIO DSP - Gemini-S1 (R528)\n");
    printf("  Landscape 320x240 - Contest 2026\n");
    printf("  Voice AI powered by SiliconFlow ASR+LLM\n");
    printf("========================================\n");

    wifi_auto_connect_start();

    pthread_mutex_init(&g_dsp_mutex, NULL);
    snprintf(g_audio_status, sizeof(g_audio_status), "WAITING FOR AUDIO...");

    radio_log_init(NULL);
    agent_bridge_init();
    agent_bridge_set_frequency((float)g_freq_hz);
    agent_bridge_set_mode(MODE_NAMES[g_mode_idx]);

    if (location_service_init() != 0) {
        printf("[INIT] location_service_init failed\n");
    }

    printf("[INIT] Starting audio capture thread...\n");
#if AUDIO_CAPTURE_FROM_I2S
    if (audio_i2s_init() == 0) {
        audio_i2s_start();
    } else
#endif
    {
        pthread_create(&g_audio_thread, NULL, audio_thread_func, NULL);
        g_audio_thread_created = true;
    }

    printf("[UI] Creating LVGL landscape 320x240 interface...\n");
    lv_init();

    lv_nuttx_dsc_t dsc;
    lv_nuttx_result_t nuttx_res;
    lv_nuttx_dsc_init(&dsc);
    dsc.fb_path = "/dev/lcd0";
    dsc.input_path = "/dev/input0";
    dsc.utouch_path = NULL;
    lv_nuttx_init(&dsc, &nuttx_res);

    if (nuttx_res.disp == NULL) {
        syslog(LOG_ERR, "ai_radio: failed to open /dev/lcd0\n");
    }
    if (nuttx_res.indev == NULL) {
        syslog(LOG_WARNING, "ai_radio: failed to open /dev/input0 (touch)\n");
    }

    g_cw_dit_ms = 1200 / g_cw_wpm;

    create_main_screen();
    update_freq_display();
    update_cards_display();
    update_card_highlight();

    ui_ai_radio_init();

    lv_timer_create(ui_update_timer, UI_REFRESH_MS, NULL);
    lv_timer_create(mayday_flash_timer, 300, NULL);

    button_init();
    lv_timer_create(button_poll_timer, BTN_POLL_MS, NULL);

    input_lradc_init();
    input_lradc_start();

    if (location_service_start() != 0) {
        printf("[INIT] location_service_start failed\n");
    }

    printf("[UI] Interface created.\n");
    printf("[INFO] LRADC buttons: Vol-/+, Menu, Enter, Home\n");
    printf("[INFO] Entering main loop...\n");

    while (g_running) {
        uint32_t idle = lv_timer_handler();
        ui_ai_radio_refresh();
        if (idle < 1) idle = 1;
        if (idle > BTN_POLL_MS) idle = BTN_POLL_MS;
        usleep(idle * 1000);
    }

    if (g_btn_fd >= 0) close(g_btn_fd);
    input_lradc_stop();
    audio_i2s_stop();
    if (g_audio_thread_created) {
        pthread_join(g_audio_thread, NULL);
    }
    location_service_stop();
    agent_bridge_deinit();
    radio_log_deinit();
    pthread_mutex_destroy(&g_dsp_mutex);
    printf("[SHUTDOWN] AI Radio Console exiting.\n");
    return 0;
}
