/****************************************************************************
 * apps/vendor/allwinnertech/apps/ai_radio_console/ptt_control.c
 *
 * PTT GPIO control using NuttX GPIO character driver
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/ioexpander/gpio.h>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "ptt_control.h"

struct ptt_ctx_s {
    char gpio_dev[64];
    int fd;
    bool key_state;
};

static struct ptt_ctx_s g_ptt = {
    .gpio_dev = "/dev/gpio0",
    .fd = -1,
    .key_state = false,
};

int ptt_init(const char *gpio_dev, int pin)
{
    (void)pin;

    if (g_ptt.fd >= 0) {
        close(g_ptt.fd);
        g_ptt.fd = -1;
    }

    if (gpio_dev) {
        strncpy(g_ptt.gpio_dev, gpio_dev, sizeof(g_ptt.gpio_dev) - 1);
    }
    g_ptt.key_state = false;

    g_ptt.fd = open(g_ptt.gpio_dev, O_RDWR);
    if (g_ptt.fd < 0) {
        printf("[radio_ptt] open %s failed: %d\n", g_ptt.gpio_dev, errno);
        return -errno;
    }

    int ret = ioctl(g_ptt.fd, GPIOC_SETPINTYPE, (unsigned long)GPIO_OUTPUT_PIN);
    if (ret < 0) {
        printf("[radio_ptt] GPIOC_SETPINTYPE failed: %d\n", errno);
        close(g_ptt.fd);
        g_ptt.fd = -1;
        return -errno;
    }

    ptt_unkey();
    printf("[radio_ptt] init ok on %s\n", g_ptt.gpio_dev);
    return 0;
}

int ptt_key(void)
{
    if (g_ptt.fd < 0) {
        return -ENODEV;
    }

    int ret = ioctl(g_ptt.fd, GPIOC_WRITE, (unsigned long)true);
    if (ret < 0) {
        printf("[radio_ptt] key failed: %d\n", errno);
        return -errno;
    }

    g_ptt.key_state = true;
    printf("[radio_ptt] keyed\n");
    return 0;
}

int ptt_unkey(void)
{
    if (g_ptt.fd < 0) {
        return -ENODEV;
    }

    int ret = ioctl(g_ptt.fd, GPIOC_WRITE, (unsigned long)false);
    if (ret < 0) {
        printf("[radio_ptt] unkey failed: %d\n", errno);
        return -errno;
    }

    g_ptt.key_state = false;
    printf("[radio_ptt] unkeyed\n");
    return 0;
}

bool ptt_is_pressed(void)
{
    return g_ptt.key_state;
}

void ptt_deinit(void)
{
    if (g_ptt.fd >= 0) {
        ptt_unkey();
        close(g_ptt.fd);
        g_ptt.fd = -1;
    }
    printf("[radio_ptt] deinit\n");
}
