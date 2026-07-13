/****************************************************************************
 * apps/vendor/allwinnertech/apps/ai_radio_console/main.c
 *
 * AI Radio Console main application
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "radio_ui.h"
#include "audio_recorder.h"
#include "ptt_control.h"
#include "asr_client.h"
#include "radio_agent.h"
#include "radio_server.h"

#define SAMPLE_RATE     16000
#define CHANNELS        1
#define BPS             16
#define PTT_DEVICE      CONFIG_AI_RADIO_CONSOLE_PTT_GPIO
#define PTT_PIN         CONFIG_AI_RADIO_CONSOLE_PTT_PIN

#ifndef CONFIG_AI_RADIO_CONSOLE_USE_VOICE_CHANNEL
#define RECORD_DEVICE   CONFIG_AI_RADIO_CONSOLE_AUDIO_DEV
#define ASR_URL         CONFIG_AI_RADIO_CONSOLE_ASR_URL
#define ASR_MODEL       CONFIG_AI_RADIO_CONSOLE_ASR_MODEL
#endif

/* ASR API key is read from environment or config file.
 * Only used by the legacy nxrecorder + custom HTTP ASR path. */
#define ASR_KEY_FILE    "/data/agent/config/asr_key.txt"
#define RADIO_LOG_DIR   "/data/agent/memory/radio"

/* Match mini_memo's recommended worker stack size for LLM/ASR calls. */
#define WORKER_STACK_SIZE 40960

static bool g_ptt_was_pressed = false;
static bool g_worker_busy = false;

static pthread_mutex_t g_work_lock = PTHREAD_MUTEX_INITIALIZER;

static char g_api_key[128] = { 0 };

/* PTT self-test state. */
static int g_selftest_hold_ms = CONFIG_AI_RADIO_CONSOLE_PTT_SELFTEST_MS;
static bool g_selftest_active = false;
static int g_selftest_elapsed_ms = 0;

static int load_asr_key(void)
{
    FILE *f = fopen(ASR_KEY_FILE, "r");
    if (f) {
        if (fgets(g_api_key, sizeof(g_api_key), f)) {
            char *nl = strchr(g_api_key, '\n');
            if (nl) {
                *nl = '\0';
            }
        }
        fclose(f);
    }

    if (strlen(g_api_key) == 0) {
        const char *env = getenv("ASR_API_KEY");
        if (env) {
            strncpy(g_api_key, env, sizeof(g_api_key) - 1);
        }
    }

    if (strlen(g_api_key) == 0) {
        printf("[main] WARNING: ASR API key not set.\n"
               "       Create %s or set ASR_API_KEY env.\n", ASR_KEY_FILE);
        return -ENOENT;
    }

    return 0;
}

static void get_time_str(char *buf, size_t size)
{
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buf, size, "%H:%M:%S", tm_info);
}

static void *process_worker(void *arg)
{
    (void)arg;

    const uint8_t *pcm = NULL;
    size_t pcm_len = 0;
    char local_text[1024];
    char *asr_text = NULL;
    char *text = NULL;
    char time_str[32];
    int ret;

    /* 1. Try to get text directly (voice_channel path). */
    radio_ui_set_state(RADIO_UI_TRANSCRIBING);
    radio_server_broadcast_status("transcribing");

    ret = audio_recorder_get_text(local_text, sizeof(local_text));
    if (ret == 0) {
        /* voice_channel path: direct ASR text. Empty text is valid
         * (no speech detected) and should not fall back to raw PCM. */
        if (local_text[0] == '\0') {
            printf("[main] voice_channel returned empty text\n");
            radio_ui_set_state(RADIO_UI_IDLE);
            radio_ui_set_status("未识别到语音");
            radio_server_broadcast_status("idle");
            goto done;
        }
        text = local_text;
        printf("[main] voice_channel text: %s\n", text);
    } else if (ret == -ENOTSUP) {
        /* 2. Fall back to raw PCM + custom ASR (nxrecorder path). */
        if (audio_recorder_get_data(&pcm, &pcm_len) < 0 || pcm_len == 0) {
            printf("[main] no audio data\n");
            radio_ui_set_state(RADIO_UI_ERROR);
            radio_ui_set_status("无音频数据");
            radio_server_broadcast_status("error: no audio");
            goto done;
        }

        printf("[main] processing %zu bytes of PCM\n", pcm_len);

        if (asr_transcribe(pcm, pcm_len, SAMPLE_RATE, &asr_text) < 0 ||
            !asr_text || asr_text[0] == '\0') {
            printf("[main] ASR failed\n");
            radio_ui_set_state(RADIO_UI_ERROR);
            radio_ui_set_status("识别失败");
            radio_server_broadcast_status("error: asr failed");
            goto done;
        }

        text = asr_text;
    } else {
        printf("[main] audio_recorder_get_text failed: %d\n", ret);
        radio_ui_set_state(RADIO_UI_ERROR);
        radio_ui_set_status("获取转写失败");
        radio_server_broadcast_status("error: get text failed");
        goto done;
    }

    get_time_str(time_str, sizeof(time_str));
    printf("[main] ASR result: %s\n", text);
    radio_ui_set_transcript(text);
    radio_ui_add_log(time_str, text);
    radio_server_broadcast_log(time_str, text);

    /* 3. Save log via ai_agent */
    radio_ui_set_state(RADIO_UI_THINKING);
    radio_server_broadcast_status("thinking");
    char *agent_resp = NULL;
    if (radio_agent_save_log(text, &agent_resp) < 0) {
        printf("[main] agent save failed\n");
        radio_ui_set_state(RADIO_UI_ERROR);
        radio_ui_set_status("保存日志失败");
        radio_server_broadcast_status("error: save failed");
        /* Fallback: keep a raw local copy so the log is not lost. */
        radio_agent_save_raw_log(text);
    } else {
        printf("[main] agent: %s\n", agent_resp ? agent_resp : "ok");
        radio_ui_set_state(RADIO_UI_IDLE);
        radio_ui_set_status("已保存");
        radio_server_broadcast_status("idle");
        free(agent_resp);
    }

done:
    if (asr_text) {
        free(asr_text);
    }

    pthread_mutex_lock(&g_work_lock);
    g_worker_busy = false;
    pthread_mutex_unlock(&g_work_lock);

    return NULL;
}

static void start_processing(void)
{
    pthread_t tid;
    pthread_attr_t attr;
    int ret;

    pthread_mutex_lock(&g_work_lock);
    if (g_worker_busy) {
        pthread_mutex_unlock(&g_work_lock);
        printf("[main] worker still busy\n");
        return;
    }
    g_worker_busy = true;
    pthread_mutex_unlock(&g_work_lock);

    pthread_attr_init(&attr);
    /* 40 KiB stack: worker does ASR + VelaClaw + JSON parsing. */
    pthread_attr_setstacksize(&attr, WORKER_STACK_SIZE);
    ret = pthread_create(&tid, &attr, process_worker, NULL);
    pthread_attr_destroy(&attr);
    if (ret != 0) {
        pthread_mutex_lock(&g_work_lock);
        g_worker_busy = false;
        pthread_mutex_unlock(&g_work_lock);
        printf("[main] worker thread create failed: %d\n", ret);
        return;
    }
    pthread_detach(tid);
}

static void ptt_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    bool pressed = radio_ui_is_ptt_pressed();

    /* Optional: also read physical PTT if implemented */
    if (!pressed) {
        pressed = ptt_is_pressed();
    }

    /* Self-test mode: simulate a PTT press of configured duration. */
    if (g_selftest_active) {
        g_selftest_elapsed_ms += 100; /* timer period is 100 ms */
        if (!g_ptt_was_pressed && g_selftest_elapsed_ms < g_selftest_hold_ms) {
            pressed = true;
            printf("[main] selftest: PTT keyed (%d/%d ms)\n",
                   g_selftest_elapsed_ms, g_selftest_hold_ms);
        } else if (g_ptt_was_pressed &&
                   g_selftest_elapsed_ms >= g_selftest_hold_ms) {
            pressed = false;
            printf("[main] selftest: PTT unkeyed after %d ms\n",
                   g_selftest_elapsed_ms);
            g_selftest_active = false;
        }
    }

    if (pressed && !g_ptt_was_pressed) {
        /* PTT just pressed */
        printf("[main] PTT pressed\n");
        /* Start audio first; only key the external radio if capture
         * actually started. This avoids accidental transmissions when
         * the audio path is not ready. */
        if (audio_recorder_start() < 0) {
            radio_ui_set_state(RADIO_UI_ERROR);
            radio_ui_set_status("录音启动失败");
            radio_server_broadcast_status("error: audio start failed");
            g_ptt_was_pressed = false;
            return;
        }
        radio_ui_set_state(RADIO_UI_RECORDING);
        radio_ui_set_status("录音中");
        radio_server_broadcast_status("recording");
        ptt_key();
    } else if (!pressed && g_ptt_was_pressed) {
        /* PTT just released */
        printf("[main] PTT released\n");
        ptt_unkey();
        audio_recorder_stop();
        radio_server_broadcast_status("processing");
        start_processing();
    }

    g_ptt_was_pressed = pressed;
}

int ai_radio_console_main(int argc, FAR char *argv[])
{
    int i;
    int ret = 0;

    printf("\n=== AI Radio Console starting ===\n");

    /* Parse command line. Supported: --ptt-selftest [ms] */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--ptt-selftest") == 0) {
            bool has_ms = false;
            if (i + 1 < argc) {
                int ms = atoi(argv[i + 1]);
                if (ms > 0) {
                    g_selftest_hold_ms = ms;
                    i++;
                    has_ms = true;
                }
            }
            /* If user asks for self-test without a duration and Kconfig default
             * is 0, use a sensible fallback so the test actually runs. */
            if (!has_ms && g_selftest_hold_ms <= 0) {
                g_selftest_hold_ms = 3000;
            }
            g_selftest_active = true;
            g_selftest_elapsed_ms = 0;
            printf("[main] PTT self-test enabled: %d ms\n", g_selftest_hold_ms);
        }
    }

    /* Kconfig default self-test. */
    if (!g_selftest_active && g_selftest_hold_ms > 0) {
        g_selftest_active = true;
        g_selftest_elapsed_ms = 0;
        printf("[main] PTT self-test enabled by Kconfig: %d ms\n",
               g_selftest_hold_ms);
    }

    /* Ensure log directory exists */
    mkdir(RADIO_LOG_DIR, 0755);

#ifndef CONFIG_AI_RADIO_CONSOLE_USE_VOICE_CHANNEL
    /* ASR key is only needed for the legacy nxrecorder + HTTP ASR path. */
    load_asr_key();
#else
    printf("[main] voice_channel enabled; skipping board-side ASR key load\n");
#endif

    /* Initialize UI */
    if (radio_ui_init() < 0) {
        printf("[main] UI init failed\n");
        return 1;
    }

    /* Initialize audio recorder (backend selected by Kconfig) */
#ifdef CONFIG_AI_RADIO_CONSOLE_USE_VOICE_CHANNEL
    if (audio_recorder_init(NULL, SAMPLE_RATE, CHANNELS, BPS) < 0) {
#else
    if (audio_recorder_init(RECORD_DEVICE, SAMPLE_RATE, CHANNELS, BPS) < 0) {
#endif
        radio_ui_set_status("音频初始化失败");
        printf("[main] audio init failed\n");
    }

    /* Initialize PTT GPIO */
    if (ptt_init(PTT_DEVICE, PTT_PIN) < 0) {
        radio_ui_set_status("PTT GPIO 未就绪");
        printf("[main] PTT init failed\n");
    }

#ifndef CONFIG_AI_RADIO_CONSOLE_USE_VOICE_CHANNEL
    /* Initialize custom HTTP ASR client (legacy path). */
    if (asr_client_init(ASR_URL, ASR_MODEL, g_api_key) < 0) {
        radio_ui_set_status("ASR 未配置");
        printf("[main] ASR init failed\n");
    }
#else
    printf("[main] voice_channel enabled; skipping custom ASR client init\n");
#endif

    /* Initialize ai_agent connection */
    if (radio_agent_init() < 0) {
        radio_ui_set_status("AI Agent 未就绪");
        printf("[main] agent init failed\n");
    }

    /* Start optional companion app server (default port 28790).
     * ai_agent itself provides the primary com.agent.coapp API on 28789. */
    if (radio_server_start() < 0) {
        radio_ui_set_status("App 服务器未启动");
        printf("[main] server start failed\n");
    }

    /* Create PTT poll timer */
    lv_timer_t *ptt_timer = lv_timer_create(ptt_timer_cb, 100, NULL);
    if (!ptt_timer) {
        printf("[main] timer create failed\n");
        ret = 1;
        goto cleanup;
    }
    lv_timer_set_repeat_count(ptt_timer, -1);

    printf("[main] entering UI loop\n");
    radio_ui_run();

    /* Cleanup (not reached in normal loop) */
    lv_timer_del(ptt_timer);

cleanup:
    radio_server_stop();
    radio_agent_deinit();
#ifndef CONFIG_AI_RADIO_CONSOLE_USE_VOICE_CHANNEL
    asr_client_deinit();
#endif
    ptt_deinit();
    audio_recorder_deinit();

    return ret;
}
