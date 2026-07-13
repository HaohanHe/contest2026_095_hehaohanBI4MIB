#include "radio_log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#if GPS_ENABLED
#include "location_service.h"
#endif

static bool g_initialized = false;
static char g_db_path[256] = LOG_DB_PATH;

static char g_current_net[128] = {0};
static uint32_t g_net_start_time = 0;
static char g_net_participants[2048] = {0};
static int g_net_participant_count = 0;

static void ensure_dir(const char *path) {
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s", path);
    char *p = strrchr(tmp, '/');
    if (p) {
        *p = '\0';
        mkdir(tmp, 0755);
    }
}

static int write_string_file(const char *path, const char *data, size_t len) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fwrite(data, 1, len, f);
    fclose(f);
    return 0;
}

static int read_string_file(const char *path, char *data, size_t max) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    size_t n = fread(data, 1, max - 1, f);
    data[n] = '\0';
    fclose(f);
    return (int)n;
}

int radio_log_init(const char *db_path) {
    if (db_path) strncpy(g_db_path, db_path, sizeof(g_db_path) - 1);
    ensure_dir(g_db_path);
    mkdir("/data/radio", 0755);
    mkdir("/data/radio/qso", 0755);
    mkdir("/data/radio/events", 0755);
    mkdir(AUDIO_CACHE_PATH, 0755);
    mkdir(RECORD_PATH, 0755);
    char path[512];
    snprintf(path, sizeof(path), "%s.idx", g_db_path);
    FILE *f = fopen(path, "a");
    if (f) {
        fclose(f);
    }
    g_initialized = true;
    return 0;
}

int radio_log_deinit(void) {
    g_initialized = false;
    return 0;
}

int radio_log_add_qso(const qso_entry_t *entry) {
    if (!entry) return -1;
    char path[512];
    snprintf(path, sizeof(path), "/data/radio/qso/%u_%s.dat", entry->timestamp, entry->callsign);
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    fwrite(entry, sizeof(qso_entry_t), 1, f);
    fclose(f);
    return 0;
}

int radio_log_get_qso(qso_entry_t *entries, size_t max_count, size_t *out_count) {
    if (!entries || !out_count) return -1;
    *out_count = 0;
    system("ls /data/radio/qso/*.dat 2>/dev/null | sort -r > /tmp/qso_list.txt");
    FILE *list = fopen("/tmp/qso_list.txt", "r");
    if (!list) return 0;
    char path[512];
    while (fgets(path, sizeof(path), list) && *out_count < max_count) {
        path[strcspn(path, "\n")] = '\0';
        FILE *f = fopen(path, "rb");
        if (f) {
            fread(&entries[*out_count], sizeof(qso_entry_t), 1, f);
            fclose(f);
            (*out_count)++;
        }
    }
    fclose(list);
    return 0;
}

int radio_log_get_qso_count(size_t *count) {
    if (!count) return -1;
    *count = 0;
    FILE *f = popen("ls /data/radio/qso/*.dat 2>/dev/null | wc -l", "r");
    if (f) {
        fscanf(f, "%zu", count);
        pclose(f);
    }
    return 0;
}

int radio_log_delete_qso(uint32_t timestamp) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "rm -f /data/radio/qso/%u_*.dat", timestamp);
    return system(cmd);
}

int radio_log_clear_all(void) {
    system("rm -f /data/radio/qso/*.dat");
    system("rm -f /data/radio/events/*.dat");
    return 0;
}

int radio_log_export_adif(const char *path) {
    if (!path) return -1;
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "ADIF Export by AI Radio Console\n");
    fprintf(f, "<ADIF_VER:5>3.1.0\n");
    fprintf(f, "<PROGRAMID:15>AI_RADIO_CONSOLE\n");
    fprintf(f, "<EOH>\n");
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "ls /data/radio/qso/*.dat 2>/dev/null");
    FILE *list = popen(cmd, "r");
    if (list) {
        char qpath[512];
        qso_entry_t qso;
        while (fgets(qpath, sizeof(qpath), list)) {
            qpath[strcspn(qpath, "\n")] = '\0';
            FILE *qf = fopen(qpath, "rb");
            if (qf) {
                fread(&qso, sizeof(qso_entry_t), 1, qf);
                fclose(qf);
                char date[16], time_str[16];
                struct tm *tm_info = gmtime((time_t *)&qso.timestamp);
                strftime(date, sizeof(date), "%Y%m%d", tm_info);
                strftime(time_str, sizeof(time_str), "%H%M%S", tm_info);
                fprintf(f, "<CALL:%zu>%s", strlen(qso.callsign), qso.callsign);
                fprintf(f, "<QSO_DATE:8>%s", date);
                fprintf(f, "<TIME_ON:6>%s", time_str);
                fprintf(f, "<FREQ:%.1f>%.3f", 0.0f, qso.frequency);
                fprintf(f, "<MODE:%zu>%s", strlen(qso.mode), qso.mode);
                fprintf(f, "<RST_SENT:%d>%d", 3, qso.signal_report);
                fprintf(f, "<QTH:%zu>%s", strlen(qso.qth), qso.qth);
                fprintf(f, "<EOR>\n");
            }
        }
        pclose(list);
    }
    fclose(f);
    return 0;
}

int radio_log_import_adif(const char *path) {
    (void)path;
    return 0;
}

int radio_log_start_net(const char *net_name) {
    if (!net_name) return -1;
    strncpy(g_current_net, net_name, sizeof(g_current_net) - 1);
    g_net_start_time = (uint32_t)time(NULL);
    g_net_participants[0] = '\0';
    g_net_participant_count = 0;
    return 0;
}

int radio_log_end_net(void) {
    g_current_net[0] = '\0';
    g_net_start_time = 0;
    g_net_participants[0] = '\0';
    g_net_participant_count = 0;
    return 0;
}

int radio_log_add_net_participant(const char *callsign) {
    if (!callsign) return -1;
    if (g_net_participant_count > 0) {
        strncat(g_net_participants, ", ", sizeof(g_net_participants) - strlen(g_net_participants) - 1);
    }
    strncat(g_net_participants, callsign, sizeof(g_net_participants) - strlen(g_net_participants) - 1);
    g_net_participant_count++;
    return 0;
}

int radio_log_get_net_summary(char *summary, size_t max_len) {
    if (!summary) return -1;
    if (g_current_net[0] == '\0') {
        snprintf(summary, max_len, "No active net");
        return 0;
    }
    snprintf(summary, max_len, "Net: %s | Participants(%d): %s",
             g_current_net, g_net_participant_count, g_net_participants);
    return 0;
}

int radio_log_add_event(const alert_event_t *event) {
    if (!event) return -1;
    char path[512];
    snprintf(path, sizeof(path), "/data/radio/events/%u.dat", event->timestamp);
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    fwrite(event, sizeof(alert_event_t), 1, f);
    fclose(f);
    return 0;
}

int radio_log_get_events(alert_event_t *events, size_t max_count, size_t *out_count) {
    if (!events || !out_count) return -1;
    *out_count = 0;
    FILE *list = popen("ls /data/radio/events/*.dat 2>/dev/null | sort -r", "r");
    if (!list) return 0;
    char path[512];
    while (fgets(path, sizeof(path), list) && *out_count < max_count) {
        path[strcspn(path, "\n")] = '\0';
        FILE *f = fopen(path, "rb");
        if (f) {
            fread(&events[*out_count], sizeof(alert_event_t), 1, f);
            fclose(f);
            (*out_count)++;
        }
    }
    pclose(list);
    return 0;
}

static const char *TRANSCRIPT_PATH = "/data/radio/transcript.txt";
static FILE *g_transcript_fp = NULL;

int radio_log_qso_text(float freq, const char *mode, const char *text) {
    if (!text) return -1;
    ensure_dir(TRANSCRIPT_PATH);
    if (!g_transcript_fp) {
        g_transcript_fp = fopen(TRANSCRIPT_PATH, "a");
    }
    if (!g_transcript_fp) return -1;
    time_t now = time(NULL);
    char tbuf[32];
    struct tm *tm_info = gmtime(&now);
    if (tm_info) {
        strftime(tbuf, sizeof(tbuf), "%Y-%m-%dT%H:%M:%SZ", tm_info);
    } else {
        snprintf(tbuf, sizeof(tbuf), "---");
    }

#if GPS_ENABLED
    gps_fix_t fix;
    bool has_fix = location_service_get_fix(&fix);
    if (has_fix && fix.valid) {
        fprintf(g_transcript_fp,
                "[%s] %.3f MHz %s: \"%s\" lat=%.4f lon=%.4f alt=%.1f\n",
                tbuf, freq / 1000000.0f, mode ? mode : "USB", text,
                fix.latitude, fix.longitude, fix.altitude_m);
    } else {
        fprintf(g_transcript_fp,
                "[%s] %.3f MHz %s: \"%s\" lat=--- lon=--- alt=---\n",
                tbuf, freq / 1000000.0f, mode ? mode : "USB", text);
    }
#else
    fprintf(g_transcript_fp, "[%s] %.3fMHz %s: %s\n", tbuf, freq / 1000000.0f, mode ? mode : "USB", text);
#endif

    fflush(g_transcript_fp);
    return 0;
}

int radio_log_get_transcript(char *buffer, size_t max_len, size_t *out_len) {
    if (!buffer || max_len == 0) return -1;
    if (g_transcript_fp) fflush(g_transcript_fp);
    FILE *f = fopen(TRANSCRIPT_PATH, "r");
    if (!f) { *out_len = 0; buffer[0] = '\0'; return 0; }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    size_t to_read = (size_t)fsize < max_len - 1 ? (size_t)fsize : max_len - 1;
    if (to_read > 0) {
        fseek(f, fsize - (long)to_read, SEEK_SET);
        *out_len = fread(buffer, 1, to_read, f);
    } else {
        *out_len = 0;
    }
    buffer[*out_len] = '\0';
    fclose(f);
    return 0;
}

int radio_log_clear_transcript(void) {
    if (g_transcript_fp) { fclose(g_transcript_fp); g_transcript_fp = NULL; }
    return remove(TRANSCRIPT_PATH);
}
