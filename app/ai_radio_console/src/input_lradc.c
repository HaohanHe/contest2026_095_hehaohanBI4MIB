#include "input_lradc.h"
#include "asr_engine.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>

#define LRADC_DEVICE "/dev/input/event1"

static int g_lradc_fd = -1;
static pthread_t g_lradc_thread;
static volatile bool g_running = false;
static lradc_button_cb_t g_button_cb = NULL;
static void *g_button_cb_data = NULL;

static uint32_t get_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static btn_buttonset_t first_button(btn_buttonset_t mask)
{
    for (int i = 0; i < 5; i++) {
        btn_buttonset_t bit = (btn_buttonset_t)1 << i;
        if (mask & bit) {
            return bit;
        }
    }
    return 0;
}

static void handle_long_press(btn_buttonset_t btn)
{
    /* Notify external callback first */
    if (g_button_cb) {
        g_button_cb(btn, true, g_button_cb_data);
    }

    if (btn != LRADC_BTN_ENTER) {
        return;
    }

    if (asr_engine_get_state() == ASR_STATE_RECORDING) {
        printf("[input_lradc] PTT long press -> ASR stop\n");
        asr_engine_stop();
    } else {
        printf("[input_lradc] PTT long press -> ASR start\n");
        asr_engine_start();
    }
}

static void handle_short_press(btn_buttonset_t btn)
{
    /* Notify external callback first */
    if (g_button_cb) {
        g_button_cb(btn, false, g_button_cb_data);
    }

    if (btn == LRADC_BTN_MENU) {
        printf("[input_lradc] Menu short press\n");
    }
}

static void *lradc_thread_func(void *arg)
{
    (void)arg;

    btn_buttonset_t last_val = 0;
    btn_buttonset_t current_btn = 0;
    uint32_t press_time = 0;
    bool long_done = false;

    while (g_running) {
        btn_buttonset_t val = 0;
        ssize_t n = read(g_lradc_fd, &val, sizeof(val));

        if (n == (ssize_t)sizeof(val)) {
            uint32_t now = get_ms();
            btn_buttonset_t pressed = val & ~last_val;
            btn_buttonset_t released = last_val & ~val;

            if (pressed && current_btn == 0) {
                current_btn = first_button(pressed);
                if (current_btn != 0) {
                    press_time = now;
                    long_done = false;
                }
            }

            if (current_btn != 0 && !long_done &&
                (now - press_time) > LRADC_LONGPRESS_MS) {
                handle_long_press(current_btn);
                long_done = true;
            }

            if (released & current_btn) {
                uint32_t held = now - press_time;
                if (!long_done && held <= LRADC_LONGPRESS_MS) {
                    handle_short_press(current_btn);
                }
                current_btn = 0;
            }

            last_val = val;
        } else if (n < 0 && errno != EAGAIN && errno != EINTR) {
            /* Device error, throttle retries */
        }

        usleep(LRADC_POLL_MS * 1000);
    }

    return NULL;
}

int input_lradc_init(void)
{
    g_lradc_fd = open(LRADC_DEVICE, O_RDONLY | O_NONBLOCK);
    if (g_lradc_fd < 0) {
        printf("[input_lradc] Warning: cannot open %s (%s)\n",
               LRADC_DEVICE, strerror(errno));
        return -1;
    }

    printf("[input_lradc] Opened %s (fd=%d)\n", LRADC_DEVICE, g_lradc_fd);
    return 0;
}

int input_lradc_start(void)
{
    if (g_lradc_fd < 0) {
        return -1;
    }
    if (g_running) {
        return 0;
    }

    g_running = true;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    int ret = pthread_create(&g_lradc_thread, &attr, lradc_thread_func, NULL);
    pthread_attr_destroy(&attr);

    if (ret != 0) {
        g_running = false;
        printf("[input_lradc] Failed to create thread (%d)\n", ret);
        return -1;
    }

    printf("[input_lradc] Polling thread started\n");
    return 0;
}

int input_lradc_stop(void)
{
    g_running = false;

    /* Give the detached thread time to exit before closing the fd */
    usleep(LRADC_POLL_MS * 1000 * 3);

    if (g_lradc_fd >= 0) {
        close(g_lradc_fd);
        g_lradc_fd = -1;
    }

    printf("[input_lradc] Stopped\n");
    return 0;
}

void input_lradc_set_button_callback(lradc_button_cb_t cb, void *user_data)
{
    g_button_cb = cb;
    g_button_cb_data = user_data;
}
