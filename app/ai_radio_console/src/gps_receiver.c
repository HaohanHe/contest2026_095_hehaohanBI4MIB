#include "gps_receiver.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define GPS_LINE_MAX   256
#define GPS_MAX_FIELDS 24

static char g_dev[64] = "";
static int g_baud = 9600;
static int g_fd = -1;
static volatile bool g_running = false;
static pthread_t g_thread;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

static gps_fix_cb_t g_cb = NULL;
static void *g_cb_data = NULL;

/* Latest fix, only accessed by the GPS reader thread. */
static gps_fix_t g_last_fix = { .valid = false };

static double nmea_to_decimal(double raw, char dir)
{
    double deg = floor(raw / 100.0);
    double minutes = raw - deg * 100.0;
    double result = deg + minutes / 60.0;

    if (dir == 'S' || dir == 'W') {
        result = -result;
    }
    return result;
}

static bool nmea_checksum_ok(const char *line)
{
    const char *star = strchr(line, '*');
    if (!star || strlen(star) < 3) {
        return false;
    }

    unsigned char cs = 0;
    for (const char *p = line + 1; *p && p != star; ++p) {
        cs ^= (unsigned char)*p;
    }

    unsigned int parsed;
    if (sscanf(star + 1, "%2x", &parsed) != 1) {
        return false;
    }
    return cs == (unsigned char)parsed;
}

static int split_fields(char *payload, char **fields, int max_fields)
{
    int n = 0;
    char *saveptr = NULL;
    char *tok = strtok_r(payload, ",", &saveptr);

    while (tok != NULL && n < max_fields) {
        fields[n++] = tok;
        tok = strtok_r(NULL, ",", &saveptr);
    }
    return n;
}

static void parse_coordinates(gps_fix_t *fix,
                              const char *lat_raw, char lat_dir,
                              const char *lon_raw, char lon_dir)
{
    if (lat_raw != NULL && lat_raw[0] != '\0' &&
        (lat_dir == 'N' || lat_dir == 'S')) {
        fix->latitude = nmea_to_decimal(atof(lat_raw), lat_dir);
    }
    if (lon_raw != NULL && lon_raw[0] != '\0' &&
        (lon_dir == 'E' || lon_dir == 'W')) {
        fix->longitude = nmea_to_decimal(atof(lon_raw), lon_dir);
    }
}

static void emit_fix(void)
{
    pthread_mutex_lock(&g_lock);
    gps_fix_cb_t cb = g_cb;
    void *data = g_cb_data;
    pthread_mutex_unlock(&g_lock);

    if (cb != NULL) {
        cb(&g_last_fix, data);
    }
}

static void process_gga(char **fields, int count)
{
    if (count < 7) {
        return;
    }

    /* field[1] = UTC time, field[2/3] = lat/dir, field[4/5] = lon/dir,
     * field[6] = fix quality, field[7] = satellites, field[9] = altitude. */
    if (fields[1][0] != '\0') {
        strncpy(g_last_fix.utc_time, fields[1], sizeof(g_last_fix.utc_time) - 1);
        g_last_fix.utc_time[sizeof(g_last_fix.utc_time) - 1] = '\0';
    }

    parse_coordinates(&g_last_fix, fields[2], fields[3][0], fields[4], fields[5][0]);

    g_last_fix.fix_quality = (fields[6][0] != '\0') ? atoi(fields[6]) : 0;
    g_last_fix.num_sats = (count > 7 && fields[7][0] != '\0') ? atoi(fields[7]) : 0;

    if (count > 9 && fields[9][0] != '\0') {
        g_last_fix.altitude_m = (float)atof(fields[9]);
    }

    g_last_fix.valid = (g_last_fix.fix_quality > 0);
    emit_fix();
}

static void process_rmc(char **fields, int count)
{
    if (count < 7) {
        return;
    }

    /* field[1] = UTC time, field[2] = status, field[3/4] = lat/dir,
     * field[5/6] = lon/dir, field[7] = speed (knots), field[9] = date. */
    char status = fields[2][0];

    if (fields[1][0] != '\0') {
        strncpy(g_last_fix.utc_time, fields[1], sizeof(g_last_fix.utc_time) - 1);
        g_last_fix.utc_time[sizeof(g_last_fix.utc_time) - 1] = '\0';
    }

    parse_coordinates(&g_last_fix, fields[3], fields[4][0], fields[5], fields[6][0]);

    if (count > 7 && fields[7][0] != '\0') {
        g_last_fix.speed_kph = (float)(atof(fields[7]) * 1.852);
    }

    g_last_fix.valid = (status == 'A');
    emit_fix();
}

static void process_line(const char *line)
{
    if (line[0] != '$') {
        return;
    }
    if (!nmea_checksum_ok(line)) {
        return;
    }

    char payload[GPS_LINE_MAX];
    strncpy(payload, line + 1, sizeof(payload) - 1);
    payload[sizeof(payload) - 1] = '\0';

    char *star = strchr(payload, '*');
    if (star != NULL) {
        *star = '\0';
    }

    char *fields[GPS_MAX_FIELDS];
    int n = split_fields(payload, fields, GPS_MAX_FIELDS);
    if (n < 1) {
        return;
    }

    size_t tlen = strlen(fields[0]);
    if (tlen < 3) {
        return;
    }

    const char *suffix = fields[0] + tlen - 3;
    if (strcmp(suffix, "GGA") == 0) {
        process_gga(fields, n);
    } else if (strcmp(suffix, "RMC") == 0) {
        process_rmc(fields, n);
    }
}

static speed_t baud_to_speed(int baud)
{
    switch (baud) {
    case 4800:   return B4800;
    case 9600:   return B9600;
    case 19200:  return B19200;
    case 38400:  return B38400;
    case 57600:  return B57600;
    case 115200: return B115200;
    case 230400: return B230400;
    default:     return (speed_t)-1;
    }
}

static void *gps_thread(void *arg)
{
    (void)arg;

    int fd = open(g_dev, O_RDWR | O_NOCTTY);
    if (fd < 0) {
#ifdef LOCAL_BUILD
        fprintf(stderr, "gps_receiver: cannot open %s (%s), GPS disabled in local build\n",
                g_dev, strerror(errno));
#else
        fprintf(stderr, "gps_receiver: cannot open %s (%s)\n", g_dev, strerror(errno));
#endif
        g_running = false;
        return NULL;
    }

    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    if (tcgetattr(fd, &tty) != 0) {
        fprintf(stderr, "gps_receiver: tcgetattr failed: %s\n", strerror(errno));
        close(fd);
        g_running = false;
        return NULL;
    }

    speed_t speed = baud_to_speed(g_baud);
    if (speed == (speed_t)-1) {
        fprintf(stderr, "gps_receiver: unsupported baud rate %d\n", g_baud);
        close(fd);
        g_running = false;
        return NULL;
    }

    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);

    tty.c_cflag = CS8 | CLOCAL | CREAD;
    tty.c_iflag = IGNPAR;
    tty.c_oflag = 0;
    tty.c_lflag = 0;
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 5; /* 500 ms timeout so stop() can break out quickly */

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        fprintf(stderr, "gps_receiver: tcsetattr failed: %s\n", strerror(errno));
        close(fd);
        g_running = false;
        return NULL;
    }
    tcflush(fd, TCIOFLUSH);

    pthread_mutex_lock(&g_lock);
    g_fd = fd;
    pthread_mutex_unlock(&g_lock);

    char buf[GPS_LINE_MAX];
    size_t len = 0;

    while (g_running) {
        char c;
        ssize_t n = read(g_fd, &c, 1);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (n == 0) {
            continue; /* timeout, loop back to check g_running */
        }
        if (c == '\r' || c == '\n') {
            if (len > 0) {
                buf[len] = '\0';
                process_line(buf);
                len = 0;
            }
            continue;
        }
        if (len < sizeof(buf) - 1) {
            buf[len++] = c;
        }
    }

    return NULL;
}

int gps_receiver_init(const char *dev, int baud)
{
    if (dev == NULL || baud <= 0) {
        return -1;
    }
    strncpy(g_dev, dev, sizeof(g_dev) - 1);
    g_dev[sizeof(g_dev) - 1] = '\0';
    g_baud = baud;
    return 0;
}

int gps_receiver_start(void)
{
    if (g_running) {
        return 0;
    }
    if (g_dev[0] == '\0') {
        return -1;
    }

    g_running = true;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    int rc = pthread_create(&g_thread, &attr, gps_thread, NULL);
    pthread_attr_destroy(&attr);

    if (rc != 0) {
        g_running = false;
        return -1;
    }
    return 0;
}

int gps_receiver_stop(void)
{
    if (!g_running) {
        return 0;
    }

    g_running = false;

    pthread_mutex_lock(&g_lock);
    int fd = g_fd;
    g_fd = -1;
    pthread_mutex_unlock(&g_lock);

    if (fd >= 0) {
        close(fd);
    }
    return 0;
}

void gps_receiver_set_callback(gps_fix_cb_t cb, void *user_data)
{
    pthread_mutex_lock(&g_lock);
    g_cb = cb;
    g_cb_data = user_data;
    pthread_mutex_unlock(&g_lock);
}
