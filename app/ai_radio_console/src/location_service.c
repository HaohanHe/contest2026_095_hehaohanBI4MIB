#include <pthread.h>
#include <stdbool.h>
#include <string.h>

#include "radio_config.h"
#include "gps_receiver.h"
#include "location_service.h"

static gps_fix_t g_latest_fix;
static pthread_mutex_t g_fix_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool g_started = false;

static void on_gps_fix(const gps_fix_t *fix, void *user_data)
{
    (void)user_data;
    if (fix == NULL) {
        return;
    }

    pthread_mutex_lock(&g_fix_mutex);
    memcpy(&g_latest_fix, fix, sizeof(g_latest_fix));
    pthread_mutex_unlock(&g_fix_mutex);
}

int location_service_init(void)
{
#if GPS_ENABLED
    pthread_mutex_init(&g_fix_mutex, NULL);
    memset(&g_latest_fix, 0, sizeof(g_latest_fix));
    g_latest_fix.valid = false;
    g_started = false;
    return 0;
#else
    memset(&g_latest_fix, 0, sizeof(g_latest_fix));
    g_latest_fix.valid = false;
    g_started = false;
    return 0;
#endif
}

int location_service_start(void)
{
#if GPS_ENABLED
    if (g_started) {
        return 0;
    }

    if (gps_receiver_init(GPS_UART_DEV, GPS_BAUD_RATE) != 0) {
        return -1;
    }

    gps_receiver_set_callback(on_gps_fix, NULL);

    if (gps_receiver_start() != 0) {
        return -1;
    }

    g_started = true;
    return 0;
#else
    return 0;
#endif
}

int location_service_stop(void)
{
#if GPS_ENABLED
    if (!g_started) {
        return 0;
    }

    gps_receiver_stop();
    g_started = false;
    return 0;
#else
    return 0;
#endif
}

bool location_service_get_fix(gps_fix_t *out_fix)
{
    if (out_fix == NULL) {
        return false;
    }

    pthread_mutex_lock(&g_fix_mutex);
    memcpy(out_fix, &g_latest_fix, sizeof(*out_fix));
    pthread_mutex_unlock(&g_fix_mutex);

    return out_fix->valid;
}

bool location_service_has_valid_fix(void)
{
    bool valid;

    pthread_mutex_lock(&g_fix_mutex);
    valid = g_latest_fix.valid;
    pthread_mutex_unlock(&g_fix_mutex);

    return valid;
}
