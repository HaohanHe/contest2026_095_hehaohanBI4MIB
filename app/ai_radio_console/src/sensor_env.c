#include "sensor_env.h"
#include "radio_config.h"

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* When building for NuttX we can include the sensor framework headers, but the
 * code below does not rely on a specific sensor_event_s layout.  The drivers on
 * Gemini-S1 may expose simple float arrays or structured events; we therefore
 * treat every device as a generic character device and parse either a binary
 * float[] payload or whitespace-separated text values. */
#ifdef __NuttX__
#  include <nuttx/sensors/sensor.h>
#endif

#define SENSOR_DEV_PATH_MAX 64
#define SENSOR_READ_BUF_MAX 128

static char          g_shtc3_dev[SENSOR_DEV_PATH_MAX] = "";
static char          g_ltr553_dev[SENSOR_DEV_PATH_MAX] = "";
static int           g_interval_ms = SENSOR_POLL_INTERVAL_MS;

static volatile bool g_running = false;
static bool          g_inited = false;
static pthread_t     g_thread;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

static env_sensor_data_t g_data = { 0 };

static uint32_t get_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static int parse_float_text(const char *str, float *out, int max_count)
{
    char   tmp[SENSOR_READ_BUF_MAX];
    char  *saveptr = NULL;
    int    count = 0;

    strncpy(tmp, str, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    char *tok = strtok_r(tmp, " \t\r\n,", &saveptr);
    while (tok != NULL && count < max_count) {
        out[count++] = strtof(tok, NULL);
        tok = strtok_r(NULL, " \t\r\n,", &saveptr);
    }

    return count;
}

static bool read_floats_from_dev(const char *dev, float *out, int expected)
{
    if (dev == NULL || dev[0] == '\0' || expected <= 0 || expected > 4) {
        return false;
    }

    int fd = open(dev, O_RDONLY);
    if (fd < 0) {
        return false;
    }

    char   buf[SENSOR_READ_BUF_MAX];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);

    if (n <= 0) {
        return false;
    }
    buf[n] = '\0';

    /* Try a binary float array first.  This is the most generic protocol for
     * simple sensor character devices. */
    if ((size_t)n >= sizeof(float) * (size_t)expected) {
        memcpy(out, buf, sizeof(float) * (size_t)expected);
        bool sane = true;
        for (int i = 0; i < expected; i++) {
            if (isnan(out[i]) || isinf(out[i])) {
                sane = false;
                break;
            }
        }
        if (sane) {
            return true;
        }
    }

    /* Fall back to text values such as "23.5 56.7" or "123.4,0". */
    int got = parse_float_text(buf, out, expected);
    return got >= expected;
}

static bool temp_humi_valid(float temp, float humi)
{
    return (temp > -50.0f && temp < 150.0f) &&
           (humi >= 0.0f && humi <= 100.0f);
}

static bool light_prox_valid(float light, float prox)
{
    return (light >= 0.0f) && (prox >= 0.0f);
}

static void read_shtc3(env_sensor_data_t *out)
{
    float vals[2];

    if (read_floats_from_dev(g_shtc3_dev, vals, 2) &&
        temp_humi_valid(vals[0], vals[1])) {
        out->temperature = vals[0];
        out->humidity    = vals[1];
        out->valid_flags |= (ENV_VALID_TEMP | ENV_VALID_HUMI);
    }
}

static void read_ltr553(env_sensor_data_t *out)
{
    float vals[2];

    if (read_floats_from_dev(g_ltr553_dev, vals, 2) &&
        light_prox_valid(vals[0], vals[1])) {
        out->light          = vals[0];
        out->proximity      = (int)vals[1];
        out->proximity_near = (vals[1] > 0.0f);
        out->valid_flags   |= (ENV_VALID_LIGHT | ENV_VALID_PROX);
    }
}

static void read_composite(env_sensor_data_t *out)
{
    float vals[4];

    if (read_floats_from_dev(g_shtc3_dev, vals, 4) &&
        temp_humi_valid(vals[0], vals[1]) &&
        light_prox_valid(vals[2], vals[3])) {
        out->temperature    = vals[0];
        out->humidity       = vals[1];
        out->light          = vals[2];
        out->proximity      = (int)vals[3];
        out->proximity_near = (vals[3] > 0.0f);
        out->valid_flags   |= (ENV_VALID_TEMP | ENV_VALID_HUMI |
                               ENV_VALID_LIGHT | ENV_VALID_PROX);
    }
}

static void *sensor_env_thread(void *arg)
{
    (void)arg;

    while (g_running) {
        env_sensor_data_t sample = { 0 };

        if (g_shtc3_dev[0] != '\0' &&
            strcmp(g_shtc3_dev, g_ltr553_dev) == 0) {
            /* A single composite device exposes all four channels. */
            read_composite(&sample);
        } else {
            read_shtc3(&sample);
            read_ltr553(&sample);
        }

        sample.timestamp = get_ms();

        pthread_mutex_lock(&g_lock);
        g_data = sample;
        pthread_mutex_unlock(&g_lock);

        usleep((useconds_t)g_interval_ms * 1000U);
    }

    return NULL;
}

int sensor_env_init(void)
{
#if SENSOR_ENV_ENABLED
    strncpy(g_shtc3_dev, SENSOR_SHTC3_DEV, sizeof(g_shtc3_dev) - 1);
    g_shtc3_dev[sizeof(g_shtc3_dev) - 1] = '\0';

    strncpy(g_ltr553_dev, SENSOR_LTR553_DEV, sizeof(g_ltr553_dev) - 1);
    g_ltr553_dev[sizeof(g_ltr553_dev) - 1] = '\0';

    g_interval_ms = SENSOR_POLL_INTERVAL_MS;
    g_inited = true;
#else
    g_inited = false;
#endif

    return 0;
}

int sensor_env_start(void)
{
    if (!g_inited) {
        return 0;
    }
    if (g_running) {
        return 0;
    }

    g_running = true;

    int rc = pthread_create(&g_thread, NULL, sensor_env_thread, NULL);
    if (rc != 0) {
        g_running = false;
        return -1;
    }

    return 0;
}

int sensor_env_stop(void)
{
    if (!g_running) {
        return 0;
    }

    g_running = false;
    pthread_join(g_thread, NULL);
    return 0;
}

int sensor_env_get(env_sensor_data_t *out)
{
    if (out == NULL) {
        return -1;
    }

    pthread_mutex_lock(&g_lock);
    *out = g_data;
    pthread_mutex_unlock(&g_lock);

    return 0;
}
