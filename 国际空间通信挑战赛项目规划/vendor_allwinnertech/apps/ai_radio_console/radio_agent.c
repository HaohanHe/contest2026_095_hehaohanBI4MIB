/****************************************************************************
 * apps/vendor/allwinnertech/apps/ai_radio_console/radio_agent.c
 *
 * ai_agent integration via VelaClaw client.
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <semaphore.h>
#include <sys/stat.h>

#include <velaclaw/client.h>

#include "radio_agent.h"

#define RADIO_LOG_DIR "/data/agent/memory/radio"
#define RADIO_SKILL_PATH "/data/agent/skills/radio-log.md"
#define VELACLAW_OPEN_RETRIES 3
#define VELACLAW_ASK_RETRIES  3
#define VELACLAW_RETRY_DELAY_MS 500

static velaclaw_client_t *g_claw = NULL;
static sem_t g_claw_sem;
static char g_claw_response[4096];
static volatile bool g_claw_response_ready = false;

static const char *s_radio_skill =
    "# Radio Log\n\n"
    "description: Parse amateur radio QSO voice transcripts and save structured logs.\n\n"
    "Parse amateur radio QSO voice transcripts and save structured logs.\n\n"
    "## When to use\n\n"
    "When the user provides a transcribed radio communication and asks to save it as a QSO log, "
    "or when the transcript contains callsigns, frequencies, modes, signal reports, or other "
    "radio-related fields.\n\n"
    "## How to use\n\n"
    "1. Read the transcript provided by the user.\n"
    "2. Extract as many of these fields as possible:\n"
    "   - `datetime`: date and time of the QSO (ISO 8601 or user-provided)\n"
    "   - `callsign_local`: your callsign\n"
    "   - `callsign_remote`: the other station's callsign\n"
    "   - `frequency`: in MHz or kHz\n"
    "   - `mode`: e.g. FM, SSB, CW, AM, DMR\n"
    "   - `rst_sent`: signal report sent (e.g. 59)\n"
    "   - `rst_received`: signal report received\n"
    "   - `name`: operator name if mentioned\n"
    "   - `location`: location if mentioned\n"
    "   - `notes`: any other useful information\n"
    "3. Build a concise structured log entry in this format:\n\n"
    "```\n"
    "## QSO <datetime>\n"
    "- Local:    <callsign_local>\n"
    "- Remote:   <callsign_remote>\n"
    "- Frequency:<frequency>\n"
    "- Mode:     <mode>\n"
    "- RST Sent: <rst_sent>\n"
    "- RST Rcvd: <rst_received>\n"
    "- Name:     <name>\n"
    "- Location: <location>\n"
    "- Notes:    <notes>\n"
    "```\n\n"
    "4. Use `get_current_time` if no datetime is provided.\n"
    "5. Use `write_file` or `edit_file` to save the entry to `/data/agent/memory/radio/YYYY-MM-DD.md`. "
    "If the file exists, append the new entry. If it does not exist, create it with a `# Radio QSO Log` header.\n"
    "6. Respond with a short confirmation including the extracted callsigns and frequency.\n\n"
    "## Important\n\n"
    "- If a field is unknown, write `unknown` or leave it blank; do not invent information.\n"
    "- Keep notes short but useful.\n"
    "- Always save the log to `/data/agent/memory/radio/` so the companion app and LVGL UI can read it.\n\n"
    "## Example\n\n"
    "User: \"接收到业余电台通联语音，时间 2026-07-09 14:30:00，转写内容如下：\\\"BD2AAA 这里是 BD2BBB 在 438.500 FM 信号 59 收到 59\\\"\"\n\n"
    "→ get_current_time\n"
    "→ write_file path=\"/data/agent/memory/radio/2026-07-09.md\" "
    "content=\"# Radio QSO Log\\n\\n## QSO 2026-07-09 14:30:00\\n- Local:    BD2BBB\\n- Remote:   BD2AAA\\n- Frequency:438.500 MHz\\n- Mode:     FM\\n- RST Sent: 59\\n- RST Rcvd: 59\\n- Name:     unknown\\n- Location: unknown\\n- Notes:    Voice QSO via AI Radio Console\\n\"\n"
    "→ \"已保存通联日志：BD2BBB ↔ BD2AAA @ 438.500 MHz FM，信号 59/59。\"\n";

static int install_radio_skill(void)
{
    int fd = open(RADIO_SKILL_PATH, O_RDONLY);
    if (fd >= 0) {
        close(fd);
        return 0;
    }

    fd = open(RADIO_SKILL_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        printf("[radio_agent] cannot write skill: %d\n", errno);
        return -errno;
    }
    size_t len = strlen(s_radio_skill);
    if (write(fd, s_radio_skill, len) != (ssize_t)len) {
        close(fd);
        return -EIO;
    }
    close(fd);
    printf("[radio_agent] installed skill: %s\n", RADIO_SKILL_PATH);
    return 0;
}

static void claw_callback(int code, const char *text, void *cookie)
{
    (void)cookie;
    printf("[radio_agent] claw cb code=%d text=%s\n", code,
           text ? text : "(null)");

    if (code == 0 && text) {
        strncpy(g_claw_response, text, sizeof(g_claw_response) - 1);
        g_claw_response[sizeof(g_claw_response) - 1] = '\0';
    } else {
        snprintf(g_claw_response, sizeof(g_claw_response),
                 "agent error code=%d", code);
    }
    g_claw_response_ready = true;
    sem_post(&g_claw_sem);
}

static int ensure_dir(const char *path)
{
    int ret = mkdir(path, 0755);
    if (ret < 0 && errno != EEXIST) {
        return -errno;
    }
    return 0;
}

/* Fallback local log when ai_agent is unavailable or times out.
 * Keeps the competition demo safe: even if the agent fails, the
 * transcript is preserved and visible in the companion app. */
static int write_raw_fallback_log(const char *transcript)
{
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[32];
    char path[128];
    FILE *f;

    ensure_dir(RADIO_LOG_DIR);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    snprintf(path, sizeof(path), RADIO_LOG_DIR "/fallback_%ld.md", (long)now);

    f = fopen(path, "w");
    if (!f) {
        printf("[radio_agent] fallback log open failed: %d\n", errno);
        return -errno;
    }
    fprintf(f, "## QSO %s\n- Raw transcript: %s\n- Notes: saved locally (agent unavailable)\n",
            time_str, transcript);
    fclose(f);
    printf("[radio_agent] fallback log saved: %s\n", path);
    return 0;
}

int radio_agent_save_raw_log(const char *transcript)
{
    if (!transcript) {
        return -EINVAL;
    }
    return write_raw_fallback_log(transcript);
}

static void sleep_ms(unsigned int ms)
{
    usleep(ms * 1000);
}

int radio_agent_init(void)
{
    int retry;

    if (g_claw) {
        return 0;
    }

    sem_init(&g_claw_sem, 0, 0);
    g_claw_response[0] = '\0';
    g_claw_response_ready = false;

    for (retry = 0; retry < VELACLAW_OPEN_RETRIES; retry++) {
        g_claw = velaclaw_client_open("ai_radio_console");
        if (g_claw) {
            break;
        }
        printf("[radio_agent] velaclaw_client_open failed, retry %d/%d\n",
               retry + 1, VELACLAW_OPEN_RETRIES);
        if (retry + 1 < VELACLAW_OPEN_RETRIES) {
            sleep_ms(VELACLAW_RETRY_DELAY_MS);
        }
    }

    if (!g_claw) {
        printf("[radio_agent] velaclaw_client_open failed after %d retries\n",
               VELACLAW_OPEN_RETRIES);
        sem_destroy(&g_claw_sem);
        return -ENODEV;
    }

    ensure_dir("/data/agent/skills");
    ensure_dir(RADIO_LOG_DIR);
    install_radio_skill();
    printf("[radio_agent] init ok\n");
    return 0;
}

int radio_agent_save_log(const char *transcript, char **response_out)
{
    if (!g_claw || !transcript) {
        return -EINVAL;
    }

    /* Build a prompt that asks the agent to save a structured radio log.
     * The radio-log skill (if installed) will intercept this and format it.
     */
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    const char *prompt_fmt =
        "接收到业余电台通联语音，时间 %s，转写内容如下：\"%s\"\n"
        "请识别呼号、频率、模式、信号报告等字段，保存为一条通联日志。"
        "如果没有识别到完整字段，请尽量提取已知信息并保存。";
    int prompt_len = snprintf(NULL, 0, prompt_fmt, time_str, transcript);
    if (prompt_len < 0) {
        return -EIO;
    }

    char *prompt = malloc(prompt_len + 1);
    if (!prompt) {
        return -ENOMEM;
    }
    snprintf(prompt, prompt_len + 1, prompt_fmt, time_str, transcript);

    velaclaw_ask_req_t req = {
        .text = prompt,
        .timeout_ms = 30000,
    };

    int ret = -EIO;
    int retry;

    for (retry = 0; retry < VELACLAW_ASK_RETRIES; retry++) {
        g_claw_response_ready = false;
        g_claw_response[0] = '\0';

        /* Flush any stale posts from previous timed-out callbacks so the
         * next sem_timedwait waits for the current request. */
        while (sem_trywait(&g_claw_sem) == 0) {
        }

        ret = velaclaw_ask(g_claw, &req, claw_callback, NULL);
        if (ret < 0) {
            printf("[radio_agent] velaclaw_ask failed: %d (retry %d/%d)\n",
                   ret, retry + 1, VELACLAW_ASK_RETRIES);
            if (retry + 1 < VELACLAW_ASK_RETRIES) {
                sleep_ms(VELACLAW_RETRY_DELAY_MS);
            }
            continue;
        }

        /* Wait for the async callback (30 s timeout). */
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 30;

        if (sem_timedwait(&g_claw_sem, &ts) != 0) {
            printf("[radio_agent] wait for response timed out (retry %d/%d)\n",
                   retry + 1, VELACLAW_ASK_RETRIES);
            ret = -ETIMEDOUT;
            if (retry + 1 < VELACLAW_ASK_RETRIES) {
                sleep_ms(VELACLAW_RETRY_DELAY_MS);
                continue;
            }
            if (response_out) {
                *response_out = strdup("AI Agent 处理超时");
            }
            goto done;
        }

        /* Success path. */
        if (response_out) {
            *response_out = strdup(g_claw_response_ready ? g_claw_response
                                                           : "AI Agent 无响应");
        }
        break;
    }

    /* Also save a raw local backup */
    char path[128];
    snprintf(path, sizeof(path), RADIO_LOG_DIR "/raw_%ld.txt", (long)now);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "[%s] %s\n", time_str, transcript);
        fclose(f);
    }

done:
    free(prompt);
    return ret;
}

void radio_agent_deinit(void)
{
    if (g_claw) {
        velaclaw_client_close(g_claw);
        g_claw = NULL;
    }
    sem_destroy(&g_claw_sem);
    printf("[radio_agent] deinit\n");
}
