#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/wait.h>

#include "wifi_auto_connect.h"

#define WAPI_CONF_PATH      "/data/etc/WiFi/wapi.conf"
#define WIFI_IFACE          "wlan0"
#define MAX_RETRIES         3
#define RETRY_INTERVAL_SEC  5
#define MAX_SSID_LEN        128
#define MAX_PSK_LEN         128
#define CMD_BUF_SIZE        512
#define LINE_BUF_SIZE       132
#define OUTPUT_BUF_SIZE     256

static int run_shell_cmd(const char *cmd)
{
    int status = system(cmd);
    if (status == -1) {
        printf("[WIFI] system() failed for: %s\n", cmd);
        return -1;
    }
    if (!WIFEXITED(status)) {
        return -1;
    }
    return WEXITSTATUS(status);
}

static void shell_escape(const char *src, char *dst, size_t dst_size)
{
    size_t j = 0;
    for (size_t i = 0; src[i] != '\0' && j < dst_size - 1; i++) {
        if (src[i] == '\\' || src[i] == '"') {
            if (j < dst_size - 2) {
                dst[j++] = '\\';
            }
        }
        dst[j++] = src[i];
    }
    dst[j] = '\0';
}

static int parse_wapi_conf(char *ssid, size_t ssid_size, char *psk, size_t psk_size)
{
    FILE *fp = fopen(WAPI_CONF_PATH, "r");
    if (!fp) {
        printf("[WIFI] Failed to open %s\n", WAPI_CONF_PATH);
        return -1;
    }

    char line[LINE_BUF_SIZE];
    int found_ssid = 0;
    int found_psk = 0;

    while (fgets(line, sizeof(line), fp) != NULL) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }

        if (strncmp(line, "ssid=", 5) == 0) {
            snprintf(ssid, ssid_size, "%s", line + 5);
            found_ssid = 1;
        } else if (strncmp(line, "psk=", 4) == 0) {
            snprintf(psk, psk_size, "%s", line + 4);
            found_psk = 1;
        }
    }

    fclose(fp);
    return (found_ssid && found_psk) ? 0 : -1;
}

static int wifi_do_connect(const char *ssid, const char *psk)
{
    char cmd[CMD_BUF_SIZE];
    char ssid_escaped[MAX_SSID_LEN * 2];
    char psk_escaped[MAX_PSK_LEN * 2];
    int ret;

    shell_escape(ssid, ssid_escaped, sizeof(ssid_escaped));
    shell_escape(psk, psk_escaped, sizeof(psk_escaped));

    ret = run_shell_cmd("wapi mode " WIFI_IFACE " 2");
    if (ret != 0) {
        printf("[WIFI] wapi mode failed: %d\n", ret);
        return -1;
    }

    ret = run_shell_cmd("wapi scan " WIFI_IFACE);
    if (ret != 0) {
        printf("[WIFI] wapi scan failed: %d\n", ret);
        return -1;
    }

    snprintf(cmd, sizeof(cmd), "wapi psk " WIFI_IFACE " \"%s\" 3", psk_escaped);
    ret = run_shell_cmd(cmd);
    if (ret != 0) {
        printf("[WIFI] wapi psk failed: %d\n", ret);
        return -1;
    }

    snprintf(cmd, sizeof(cmd), "wapi essid " WIFI_IFACE " \"%s\" 1", ssid_escaped);
    ret = run_shell_cmd(cmd);
    if (ret != 0) {
        printf("[WIFI] wapi essid failed: %d\n", ret);
        return -1;
    }

    ret = run_shell_cmd("renew " WIFI_IFACE);
    if (ret != 0) {
        printf("[WIFI] renew (DHCP) failed: %d\n", ret);
        return -1;
    }

    return 0;
}

static void *wifi_auto_connect_thread(void *arg)
{
    (void)arg;

    if (access(WAPI_CONF_PATH, F_OK) != 0) {
        printf("[WIFI] Warning: %s not found, skipping auto connect\n", WAPI_CONF_PATH);
        return NULL;
    }

    char ssid[MAX_SSID_LEN] = {0};
    char psk[MAX_PSK_LEN] = {0};

    if (parse_wapi_conf(ssid, sizeof(ssid), psk, sizeof(psk)) != 0) {
        printf("[WIFI] Warning: failed to parse ssid/psk from %s\n", WAPI_CONF_PATH);
        return NULL;
    }

    printf("[WIFI] Auto connect to ssid=%s\n", ssid);

    for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
        printf("[WIFI] Connection attempt %d/%d\n", attempt, MAX_RETRIES);
        if (wifi_do_connect(ssid, psk) == 0) {
            printf("[WIFI] Connected and DHCP succeeded\n");
            return NULL;
        }
        if (attempt < MAX_RETRIES) {
            printf("[WIFI] Retrying in %ds...\n", RETRY_INTERVAL_SEC);
            sleep(RETRY_INTERVAL_SEC);
        }
    }

    printf("[WIFI] Failed to connect after %d attempts\n", MAX_RETRIES);
    return NULL;
}

int wifi_auto_connect_start(void)
{
    pthread_t tid;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    int ret = pthread_create(&tid, &attr, wifi_auto_connect_thread, NULL);
    pthread_attr_destroy(&attr);

    if (ret != 0) {
        printf("[WIFI] Failed to create auto connect thread: %d\n", ret);
        return -1;
    }

    return 0;
}
