/*
 * Copyright (C) 2026 Xiaomi Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "tools/tool_media.h"
#include "core/message_bus.h"
#include "infra/vela_tls.h"
#include "agent_compat.h"

#include <math.h>
#include <semaphore.h>
#include <stdarg.h>
#include <string.h>
#include <syslog.h>

#include "cJSON.h"

#include <media_player.h>
#include <media_policy.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define TAG "AGENT_MUSIC"

#define MEDIA_URL_MAX 512
#define MEDIA_OPTIONS_MAX 256
#define MEDIA_VOL_MIN_IDX 0
#define MEDIA_VOL_MAX_IDX 15
#define MEDIA_VOL_USER_MIN 0
#define MEDIA_VOL_USER_MAX 100

/* Timeouts for sem_timedwait (milliseconds) */

#define TIMEOUT_PREPARE_MS 10000
#define TIMEOUT_START_MS 3000
#define TIMEOUT_PAUSE_MS 3000
#define TIMEOUT_STOP_MS 3000
#define TIMEOUT_SEEK_MS 3000

/****************************************************************************
 * Private Types
 ****************************************************************************/

typedef enum {
    MEDIA_STATE_IDLE = 0,
    MEDIA_STATE_PREPARED,
    MEDIA_STATE_PLAYING,
    MEDIA_STATE_PAUSED,
    MEDIA_STATE_ERROR,
} media_state_e;

typedef struct
{
    pthread_mutex_t lock;
    media_state_e state;
    void* handle;
    uint32_t session_id;
    char url[MEDIA_URL_MAX];
    int volume; /* user-facing 0..100 */
    bool exiting; /* module teardown flag */
    bool is_live_stream; /* rtsp/rtmp → duration not supported */

    /* Event confirmation via semaphore */

    sem_t sem; /* posted by event callback */
    int last_event; /* MEDIA_EVENT_* from callback */
    int last_result; /* result code from callback */
} media_ctx_t;

/****************************************************************************
 * Private Data
 ****************************************************************************/

static media_ctx_t g_media;
static bool g_media_initialized = false;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void media_ctx_init_once(void)
{
    if (g_media_initialized) {
        return;
    }

    memset(&g_media, 0, sizeof(g_media));
    pthread_mutex_init(&g_media.lock, NULL);
    sem_init(&g_media.sem, 0, 0);
    g_media.state = MEDIA_STATE_IDLE;
    g_media.handle = NULL;
    g_media.session_id = 0;
    g_media.volume = 50;
    g_media.exiting = false;
    g_media.last_event = 0;
    g_media.last_result = 0;
    g_media_initialized = true;
}

static const char* state_str(media_state_e s)
{
    switch (s) {
    case MEDIA_STATE_IDLE:
        return "IDLE";
    case MEDIA_STATE_PREPARED:
        return "PREPARED";
    case MEDIA_STATE_PLAYING:
        return "PLAYING";
    case MEDIA_STATE_PAUSED:
        return "PAUSED";
    case MEDIA_STATE_ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

/**
 * Sanitize URL for logging — strip query string to avoid token leaks.
 */

static void log_url(const char* url, char* buf, size_t bufsz)
{
    const char* q = strchr(url, '?');
    if (q) {
        size_t len = (size_t)(q - url);
        if (len >= bufsz) {
            len = bufsz - 1;
        }

        memcpy(buf, url, len);
        buf[len] = '\0';
    } else {
        snprintf(buf, bufsz, "%s", url);
    }
}

/**
 * Force-close current session (stop + close). Caller must hold lock.
 * Does NOT wait for events — used for error recovery and cleanup.
 */

static void force_close_locked(void)
{
    if (g_media.handle) {
        syslog(LOG_INFO, "[%s] Force close session %" PRIu32 "\n",
            TAG, g_media.session_id);
        media_player_stop(g_media.handle);

        /* Brief yield to let the player drain its internal queue
         * before close — prevents "play/resume failed: End of file"
         * log spam when the player tries to read after EOF. */

        pthread_mutex_unlock(&g_media.lock);
        usleep(50 * 1000); /* 50ms */
        pthread_mutex_lock(&g_media.lock);

        if (g_media.handle) {
            media_player_close(g_media.handle, 0);
            g_media.handle = NULL;
        }
    }

    g_media.state = MEDIA_STATE_IDLE;
    g_media.url[0] = '\0';
}

/**
 * Map user volume (0..100) to hardware index.
 */

static int volume_to_index(int vol)
{
    if (vol <= MEDIA_VOL_USER_MIN) {
        return MEDIA_VOL_MIN_IDX;
    }

    if (vol >= MEDIA_VOL_USER_MAX) {
        return MEDIA_VOL_MAX_IDX;
    }

    int idx = MEDIA_VOL_MIN_IDX + (int)round((double)vol * (MEDIA_VOL_MAX_IDX - MEDIA_VOL_MIN_IDX) / 100.0);

    if (idx < MEDIA_VOL_MIN_IDX) {
        idx = MEDIA_VOL_MIN_IDX;
    }

    if (idx > MEDIA_VOL_MAX_IDX) {
        idx = MEDIA_VOL_MAX_IDX;
    }

    return idx;
}

/**
 * Push a notification to the agent loop so the LLM can inform the user
 * about asynchronous playback events (EOF, errors).
 * Must be called WITHOUT g_media.lock held (push may block briefly).
 */

static void notify_agent(const char* fmt, ...)
{
    char buf[256];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    agent_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    strncpy(msg.channel, "system", sizeof(msg.channel) - 1);
    strncpy(msg.chat_id, "media", sizeof(msg.chat_id) - 1);
    msg.content = strdup(buf);
    if (msg.content) {
        if (message_bus_push_inbound(&msg) != OK) {
            syslog(LOG_WARNING, "[%s] Failed to push media notification\n", TAG);
            free(msg.content);
        }
    }
}

/**
 * Media event callback — called from media framework thread.
 * Records event + result, then posts semaphore for waiting execute.
 * Also handles autonomous events (COMPLETED / async errors).
 */

static void media_event_cb(void* cookie, int event, int result,
    const char* extra)
{
    uint32_t cb_session = (uint32_t)(uintptr_t)cookie;

    pthread_mutex_lock(&g_media.lock);

    /* Discard stale session events */

    if (cb_session != g_media.session_id) {
        syslog(LOG_DEBUG,
            "[%s] Stale event %d for session %" PRIu32
            " (current %" PRIu32 "), discarded\n",
            TAG, event, cb_session, g_media.session_id);
        pthread_mutex_unlock(&g_media.lock);
        return;
    }

    if (g_media.exiting) {
        pthread_mutex_unlock(&g_media.lock);
        return;
    }

    /* Discard events after force_close (e.g. STOPPED from cleanup) */

    if (g_media.state == MEDIA_STATE_IDLE && g_media.handle == NULL) {
        syslog(LOG_DEBUG,
            "[%s] Event %d after cleanup, discarded\n",
            TAG, event);
        pthread_mutex_unlock(&g_media.lock);
        return;
    }

    syslog(LOG_INFO, "[%s] Event %d result %d extra=%s\n",
        TAG, event, result, extra ? extra : "");

    /* Ignore NOP events — audio_output fires these continuously
     * during playback.  If we let them through they poison
     * last_event and wake sem, breaking wait_event() for real
     * commands (stop / pause / seek / switch). */

    if (event == MEDIA_EVENT_NOP) {
        pthread_mutex_unlock(&g_media.lock);
        return;
    }

    /* Handle autonomous COMPLETED (EOF or error-complete) */

    if (event == MEDIA_EVENT_COMPLETED) {
        char url_copy[MEDIA_URL_MAX];
        snprintf(url_copy, sizeof(url_copy), "%s", g_media.url);
        uint32_t sid = g_media.session_id;

        if (result >= 0) {
            /* Normal playback finished -> IDLE */

            syslog(LOG_INFO, "[%s] EOF — auto stop session %" PRIu32 "\n",
                TAG, sid);

            /* Bump session_id so STOPPED from force_close is discarded */

            g_media.session_id++;
            force_close_locked();
            pthread_mutex_unlock(&g_media.lock);

            notify_agent("[music] Playback finished (session %" PRIu32 ")."
                         " The track has ended normally.",
                sid);
        } else {
            /* Error during playback -> close and go IDLE */

            syslog(LOG_ERR,
                "[%s] COMPLETED with error result=%d session %" PRIu32 "\n",
                TAG, result, sid);

            /* Bump session so any further stale events are discarded */

            g_media.session_id++;
            force_close_locked();
            pthread_mutex_unlock(&g_media.lock);

            notify_agent("[music] Playback failed (session %" PRIu32
                         ", error=%d). Please inform the user that"
                         " the music could not be played successfully.",
                sid, result);
        }

        return;
    }

    /* Record event + result for the waiting execute function */

    g_media.last_event = event;
    g_media.last_result = result;

    /* Update state based on confirmed events */

    if (result >= 0) {
        switch (event) {
        case MEDIA_EVENT_PREPARED:
            g_media.state = MEDIA_STATE_PREPARED;
            break;

        case MEDIA_EVENT_STARTED:
            g_media.state = MEDIA_STATE_PLAYING;
            break;

        case MEDIA_EVENT_PAUSED:
            g_media.state = MEDIA_STATE_PAUSED;
            break;

        case MEDIA_EVENT_STOPPED:
            /* stop event — state will be set to IDLE by force_close */
            break;

        default:
            break;
        }
    } else {
        /* Async error */

        syslog(LOG_ERR,
            "[%s] Async error event=%d result=%d extra=%s\n",
            TAG, event, result, extra ? extra : "");
        g_media.state = MEDIA_STATE_ERROR;
    }

    pthread_mutex_unlock(&g_media.lock);

    /* Wake up the waiting execute function */

    sem_post(&g_media.sem);
}

/**
 * Wait for a specific media event with timeout.
 * Caller must hold g_media.lock. Lock is released during wait
 * to allow the callback to run, then re-acquired.
 *
 * Returns 0 on success (expected event with result >= 0).
 * Returns -1 on timeout, wrong event, or error result.
 */

static int wait_event(int expected_event, int timeout_ms)
{
    struct timespec ts;

    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000L;
    }

    /* Clear stale event data */

    g_media.last_event = -1;
    g_media.last_result = 0;

    /* Release lock so callback can run */

    pthread_mutex_unlock(&g_media.lock);

    int ret = sem_timedwait(&g_media.sem, &ts);

    /* Re-acquire lock */

    pthread_mutex_lock(&g_media.lock);

    if (ret != 0) {
        syslog(LOG_ERR, "[%s] Timeout waiting for event %d (%d ms)\n",
            TAG, expected_event, timeout_ms);
        return -1;
    }

    /* Check if we got the expected event with success result */

    if (g_media.last_result < 0) {
        syslog(LOG_ERR,
            "[%s] Event %d failed with result %d (expected %d)\n",
            TAG, g_media.last_event, g_media.last_result,
            expected_event);
        return -1;
    }

    if (g_media.last_event != expected_event) {
        syslog(LOG_WARNING,
            "[%s] Got event %d but expected %d (result=%d)\n",
            TAG, g_media.last_event, expected_event,
            g_media.last_result);

        /* Accept it if result is OK — some events may arrive
         * in slightly different order */

        return 0;
    }

    syslog(LOG_INFO, "[%s] Event %d confirmed OK\n", TAG, expected_event);
    return 0;
}

/**
 * JSON error response helper.
 */

static int respond_error(char* output, size_t output_size,
    const char* code, const char* message)
{
    snprintf(output, output_size,
        "{\"ok\":false,\"error\":{\"code\":\"%s\",\"message\":\"%s\"}}",
        code, message);
    return ERROR;
}

/**
 * JSON ok response with state.
 */

static int respond_ok_state(char* output, size_t output_size,
    const char* extra)
{
    if (extra) {
        snprintf(output, output_size,
            "{\"ok\":true,\"state\":\"%s\",%s}",
            state_str(g_media.state), extra);
    } else {
        snprintf(output, output_size,
            "{\"ok\":true,\"state\":\"%s\"}",
            state_str(g_media.state));
    }

    return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/**
 * music_play — open + prepare (wait PREPARED) + (seek) + start (wait STARTED).
 */

int tool_music_play_execute(const char* input_json,
    char* output, size_t output_size)
{
    media_ctx_init_once();

    cJSON* root = cJSON_Parse(input_json);
    if (!root) {
        return respond_error(output, output_size,
            "EINVAL_ARG", "Invalid JSON input");
    }

    cJSON* j_url = cJSON_GetObjectItem(root, "url");
    if (!j_url || !cJSON_IsString(j_url) || !j_url->valuestring[0]) {
        cJSON_Delete(root);
        return respond_error(output, output_size,
            "EINVAL_ARG", "Missing or empty 'url'");
    }

    /* Build media options string for prepare().
     * For raw PCM files, format info is required because there is no
     * container header for avformat to auto-detect. */

    char options_buf[MEDIA_OPTIONS_MAX] = { 0 };
    const char* options = NULL;

    cJSON* j_opts = cJSON_GetObjectItem(root, "options");
    if (j_opts && cJSON_IsString(j_opts) && j_opts->valuestring[0]) {
        snprintf(options_buf, sizeof(options_buf), "%s",
            j_opts->valuestring);
        options = options_buf;
    } else {
        cJSON* j_fmt = cJSON_GetObjectItem(root, "format");
        cJSON* j_rate = cJSON_GetObjectItem(root, "sample_rate");
        cJSON* j_ch = cJSON_GetObjectItem(root, "channels");

        /* Treat empty strings and zero values as "not provided" so
         * that bogus LLM args like format="" sample_rate=0 channels=0
         * fall through to auto-detection instead of producing an
         * invalid options string. */

        bool has_fmt = j_fmt && cJSON_IsString(j_fmt)
            && j_fmt->valuestring[0];
        bool has_rate = j_rate && cJSON_IsNumber(j_rate)
            && (int)j_rate->valuedouble > 0;
        bool has_ch = j_ch && cJSON_IsNumber(j_ch)
            && (int)j_ch->valuedouble > 0;

        if (has_fmt || has_rate || has_ch) {
            const char* fmt = has_fmt ? j_fmt->valuestring : "s16le";
            int rate = has_rate ? (int)j_rate->valuedouble : 16000;
            int ch = has_ch ? (int)j_ch->valuedouble : 1;
            const char* layout = (ch == 2) ? "stereo" : "mono";

            snprintf(options_buf, sizeof(options_buf),
                "format=%s:sample_rate=%d:ch_layout=%s",
                fmt, rate, layout);
            options = options_buf;
        }
    }

    /* Auto-detect: if URL ends with .pcm and no options given,
     * provide a sensible default (s16le, 16kHz, mono). */

    if (!options) {
        const char* ext = strrchr(j_url->valuestring, '.');
        if (ext && strcasecmp(ext, ".pcm") == 0) {
            snprintf(options_buf, sizeof(options_buf),
                "format=s16le:sample_rate=16000:ch_layout=mono");
            options = options_buf;
            syslog(LOG_INFO,
                "[%s] Auto-detected .pcm, using default options: %s\n",
                TAG, options);
        }
    }

    bool autostart = true;
    cJSON* j_auto = cJSON_GetObjectItem(root, "autostart");
    if (j_auto && cJSON_IsBool(j_auto)) {
        autostart = cJSON_IsTrue(j_auto);
    }

    int start_pos = 0;
    cJSON* j_pos = cJSON_GetObjectItem(root, "start_position_ms");
    if (j_pos && cJSON_IsNumber(j_pos)) {
        start_pos = (int)j_pos->valuedouble;
        if (start_pos < 0) {
            start_pos = 0;
        }
    }

    const char* url = j_url->valuestring;

    pthread_mutex_lock(&g_media.lock);

    if (g_media.exiting) {
        pthread_mutex_unlock(&g_media.lock);
        cJSON_Delete(root);
        return respond_error(output, output_size,
            "EINTERNAL", "Module is shutting down");
    }

    /* Cut-song: force close old session if not idle */

    if (g_media.state != MEDIA_STATE_IDLE) {
        syslog(LOG_INFO, "[%s] Cut-song: closing old session %" PRIu32 "\n",
            TAG, g_media.session_id);
        force_close_locked();
    }

    /* New session — drain stale sem posts from previous session */

    while (sem_trywait(&g_media.sem) == 0)
        ;

    g_media.session_id++;
    uint32_t sid = g_media.session_id;

    char safe_url[128];
    log_url(url, safe_url, sizeof(safe_url));
    syslog(LOG_INFO, "[%s] Play session %" PRIu32 " url=%s options=%s\n",
        TAG, sid, safe_url, options ? options : "(auto)");

    /* Open */

    g_media.handle = media_player_open(MEDIA_STREAM_MUSIC);
    if (!g_media.handle) {
        syslog(LOG_ERR, "[%s] media_player_open failed\n", TAG);
        pthread_mutex_unlock(&g_media.lock);
        cJSON_Delete(root);
        return respond_error(output, output_size,
            "EIO_MEDIA", "media_player_open failed");
    }

    /* Set event callback with session_id as cookie */

    int cb_ret = media_player_set_event_callback(g_media.handle,
        (void*)(uintptr_t)sid,
        media_event_cb);
    if (cb_ret < 0) {
        syslog(LOG_ERR, "[%s] set_event_callback failed: %d\n", TAG, cb_ret);
        media_player_close(g_media.handle, 0);
        g_media.handle = NULL;
        pthread_mutex_unlock(&g_media.lock);
        cJSON_Delete(root);
        return respond_error(output, output_size,
            "EIO_MEDIA", "set_event_callback failed");
    }

    /* Prepare + wait for MEDIA_EVENT_PREPARED */

    int ret = media_player_prepare(g_media.handle, url, options);
    if (ret < 0) {
        syslog(LOG_ERR, "[%s] media_player_prepare send failed: %d\n",
            TAG, ret);
        media_player_close(g_media.handle, 0);
        g_media.handle = NULL;
        pthread_mutex_unlock(&g_media.lock);
        cJSON_Delete(root);
        return respond_error(output, output_size,
            "EIO_MEDIA", "media_player_prepare failed");
    }

    if (wait_event(MEDIA_EVENT_PREPARED, TIMEOUT_PREPARE_MS) != 0) {
        /* Async error from callback — stream is broken, fail hard */

        if (g_media.state == MEDIA_STATE_ERROR) {
            syslog(LOG_ERR,
                "[%s] Prepare failed (stream error)\n", TAG);
            force_close_locked();
            pthread_mutex_unlock(&g_media.lock);
            cJSON_Delete(root);
            return respond_error(output, output_size,
                "EIO_MEDIA",
                "Track unavailable (prepare failed). "
                "Try another song from the search results.");
        }

        /* Timeout without error — some platforms skip PREPARED event */

        syslog(LOG_WARNING,
            "[%s] PREPARED event not confirmed within %d ms, "
            "proceeding anyway (prepare ret was 0)\n",
            TAG, TIMEOUT_PREPARE_MS);
        g_media.state = MEDIA_STATE_PREPARED;
    }

    /* State is now PREPARED (set by event callback or forced above) */

    snprintf(g_media.url, sizeof(g_media.url), "%s", url);

    /* Detect live stream for duration_supported semantics */

    g_media.is_live_stream = (strncasecmp(url, "rtsp://", 7) == 0 || strncasecmp(url, "rtmp://", 7) == 0);

    /* Pre-seek if requested */

    if (start_pos > 0) {
        ret = media_player_seek(g_media.handle, (unsigned int)start_pos);
        if (ret >= 0) {
            if (wait_event(MEDIA_EVENT_SEEKED, TIMEOUT_SEEK_MS) != 0) {
                syslog(LOG_WARNING,
                    "[%s] Pre-seek wait failed, continuing\n", TAG);
            } else {
                syslog(LOG_INFO, "[%s] Pre-seek to %d ms OK\n",
                    TAG, start_pos);
            }
        } else {
            syslog(LOG_WARNING,
                "[%s] Pre-seek send failed (%d), will try after start\n",
                TAG, ret);
        }
    }

    /* Start if autostart + wait for MEDIA_EVENT_STARTED */

    if (autostart) {
        ret = media_player_start(g_media.handle);
        if (ret < 0) {
            syslog(LOG_ERR, "[%s] media_player_start send failed: %d\n",
                TAG, ret);
            force_close_locked();
            pthread_mutex_unlock(&g_media.lock);
            cJSON_Delete(root);
            return respond_error(output, output_size,
                "EIO_MEDIA", "media_player_start failed");
        }

        if (wait_event(MEDIA_EVENT_STARTED, TIMEOUT_START_MS) != 0) {
            /* Same as PREPARED — on some hardware the event callback
             * never fires.  If start() returned 0, assume it worked. */

            syslog(LOG_WARNING,
                "[%s] STARTED event not confirmed within %d ms, "
                "proceeding anyway (start ret was 0)\n",
                TAG, TIMEOUT_START_MS);
            g_media.state = MEDIA_STATE_PLAYING;
        }

        /* State is now PLAYING (set by event callback or forced above) */

        /* Fallback seek after start if pre-seek was not done */

        if (start_pos > 0) {
            ret = media_player_seek(g_media.handle,
                (unsigned int)start_pos);
            if (ret >= 0) {
                /* Best-effort, don't fail the whole play for this */

                wait_event(MEDIA_EVENT_SEEKED, TIMEOUT_SEEK_MS);
            }
        }
    }

    /* Build response */

    char extra[256];
    snprintf(extra, sizeof(extra),
        "\"session_id\":%" PRIu32 ",\"url\":\"%s\","
        "\"start_position_ms\":%d",
        sid, url, start_pos);

    int rc = respond_ok_state(output, output_size, extra);

    pthread_mutex_unlock(&g_media.lock);
    cJSON_Delete(root);
    return rc;
}

/**
 * music_pause — pause + wait MEDIA_EVENT_PAUSED.
 */

int tool_music_pause_execute(const char* input_json,
    char* output, size_t output_size)
{
    (void)input_json;
    media_ctx_init_once();

    pthread_mutex_lock(&g_media.lock);

    if (g_media.state == MEDIA_STATE_PAUSED) {
        int rc = respond_ok_state(output, output_size, "\"noop\":true");
        pthread_mutex_unlock(&g_media.lock);
        return rc;
    }

    if (g_media.state != MEDIA_STATE_PLAYING) {
        pthread_mutex_unlock(&g_media.lock);
        return respond_error(output, output_size,
            "EINVAL_STATE",
            "Cannot pause: not playing");
    }

    int ret = media_player_pause(g_media.handle);
    if (ret < 0) {
        syslog(LOG_ERR, "[%s] media_player_pause send failed: %d\n",
            TAG, ret);
        pthread_mutex_unlock(&g_media.lock);
        return respond_error(output, output_size,
            "EIO_MEDIA", "media_player_pause failed");
    }

    if (wait_event(MEDIA_EVENT_PAUSED, TIMEOUT_PAUSE_MS) != 0) {
        syslog(LOG_ERR, "[%s] Pause not confirmed\n", TAG);
        pthread_mutex_unlock(&g_media.lock);
        return respond_error(output, output_size,
            "EIO_MEDIA",
            "media_player_pause timed out");
    }

    syslog(LOG_INFO, "[%s] Paused session %" PRIu32 "\n",
        TAG, g_media.session_id);

    int rc = respond_ok_state(output, output_size, NULL);
    pthread_mutex_unlock(&g_media.lock);
    return rc;
}

/**
 * music_resume — start + wait MEDIA_EVENT_STARTED.
 */

int tool_music_resume_execute(const char* input_json,
    char* output, size_t output_size)
{
    (void)input_json;
    media_ctx_init_once();

    pthread_mutex_lock(&g_media.lock);

    if (g_media.state == MEDIA_STATE_PLAYING) {
        int rc = respond_ok_state(output, output_size, "\"noop\":true");
        pthread_mutex_unlock(&g_media.lock);
        return rc;
    }

    if (g_media.state != MEDIA_STATE_PAUSED && g_media.state != MEDIA_STATE_PREPARED) {
        pthread_mutex_unlock(&g_media.lock);
        return respond_error(output, output_size,
            "EINVAL_STATE",
            "Cannot resume: not paused or prepared");
    }

    int ret = media_player_start(g_media.handle);
    if (ret < 0) {
        syslog(LOG_ERR, "[%s] media_player_start (resume) send failed: %d\n",
            TAG, ret);
        pthread_mutex_unlock(&g_media.lock);
        return respond_error(output, output_size,
            "EIO_MEDIA", "media_player_start failed");
    }

    if (wait_event(MEDIA_EVENT_STARTED, TIMEOUT_START_MS) != 0) {
        syslog(LOG_ERR, "[%s] Resume/start not confirmed\n", TAG);
        pthread_mutex_unlock(&g_media.lock);
        return respond_error(output, output_size,
            "EIO_MEDIA",
            "media_player_start timed out");
    }

    syslog(LOG_INFO, "[%s] Resumed session %" PRIu32 "\n",
        TAG, g_media.session_id);

    int rc = respond_ok_state(output, output_size, NULL);
    pthread_mutex_unlock(&g_media.lock);
    return rc;
}

/**
 * music_stop — stop + wait MEDIA_EVENT_STOPPED, then close.
 */

int tool_music_stop_execute(const char* input_json,
    char* output, size_t output_size)
{
    (void)input_json;
    media_ctx_init_once();

    pthread_mutex_lock(&g_media.lock);

    if (g_media.state == MEDIA_STATE_IDLE) {
        int rc = respond_ok_state(output, output_size, "\"noop\":true");
        pthread_mutex_unlock(&g_media.lock);
        return rc;
    }

    syslog(LOG_INFO, "[%s] Stop session %" PRIu32 "\n",
        TAG, g_media.session_id);

    if (g_media.handle && g_media.state != MEDIA_STATE_ERROR) {
        int ret = media_player_stop(g_media.handle);
        if (ret >= 0) {
            /* Best-effort wait — even if timeout, we still close */

            wait_event(MEDIA_EVENT_STOPPED, TIMEOUT_STOP_MS);
        }
    }

    /* Always force close to ensure clean state */

    force_close_locked();

    int rc = respond_ok_state(output, output_size, NULL);
    pthread_mutex_unlock(&g_media.lock);
    return rc;
}

/**
 * music_seek — seek + wait MEDIA_EVENT_SEEKED.
 */

int tool_music_seek_execute(const char* input_json,
    char* output, size_t output_size)
{
    media_ctx_init_once();

    cJSON* root = cJSON_Parse(input_json);
    if (!root) {
        return respond_error(output, output_size,
            "EINVAL_ARG", "Invalid JSON input");
    }

    cJSON* j_pos = cJSON_GetObjectItem(root, "position_ms");
    if (!j_pos || !cJSON_IsNumber(j_pos)) {
        cJSON_Delete(root);
        return respond_error(output, output_size,
            "EINVAL_ARG", "Missing 'position_ms'");
    }

    int pos = (int)j_pos->valuedouble;
    if (pos < 0) {
        pos = 0;
    }

    cJSON_Delete(root);

    pthread_mutex_lock(&g_media.lock);

    if (g_media.state != MEDIA_STATE_PREPARED && g_media.state != MEDIA_STATE_PLAYING && g_media.state != MEDIA_STATE_PAUSED) {
        pthread_mutex_unlock(&g_media.lock);
        return respond_error(output, output_size,
            "EINVAL_STATE",
            "Cannot seek in current state");
    }

    int ret = media_player_seek(g_media.handle, (unsigned int)pos);
    if (ret < 0) {
        syslog(LOG_ERR, "[%s] media_player_seek send failed: %d\n",
            TAG, ret);
        pthread_mutex_unlock(&g_media.lock);
        return respond_error(output, output_size,
            "EIO_MEDIA", "media_player_seek failed");
    }

    if (wait_event(MEDIA_EVENT_SEEKED, TIMEOUT_SEEK_MS) != 0) {
        syslog(LOG_ERR, "[%s] Seek not confirmed\n", TAG);
        pthread_mutex_unlock(&g_media.lock);
        return respond_error(output, output_size,
            "EIO_MEDIA",
            "media_player_seek timed out");
    }

    syslog(LOG_INFO, "[%s] Seek to %d ms, state=%s\n",
        TAG, pos, state_str(g_media.state));

    char extra[64];
    snprintf(extra, sizeof(extra), "\"position_ms\":%d", pos);

    int rc = respond_ok_state(output, output_size, extra);
    pthread_mutex_unlock(&g_media.lock);
    return rc;
}

/**
 * music_set_volume — maps 0..100 to hardware index via policy API.
 * Policy API is truly synchronous, no event wait needed.
 */

int tool_music_set_volume_execute(const char* input_json,
    char* output, size_t output_size)
{
    media_ctx_init_once();

    cJSON* root = cJSON_Parse(input_json);
    if (!root) {
        return respond_error(output, output_size,
            "EINVAL_ARG", "Invalid JSON input");
    }

    cJSON* j_vol = cJSON_GetObjectItem(root, "volume");
    if (!j_vol || !cJSON_IsNumber(j_vol)) {
        cJSON_Delete(root);
        return respond_error(output, output_size,
            "EINVAL_ARG", "Missing 'volume'");
    }

    int vol = (int)j_vol->valuedouble;
    cJSON_Delete(root);

    if (vol < MEDIA_VOL_USER_MIN) {
        syslog(LOG_WARNING, "[%s] Volume %d clamped to %d\n",
            TAG, vol, MEDIA_VOL_USER_MIN);
        vol = MEDIA_VOL_USER_MIN;
    }

    if (vol > MEDIA_VOL_USER_MAX) {
        syslog(LOG_WARNING, "[%s] Volume %d clamped to %d\n",
            TAG, vol, MEDIA_VOL_USER_MAX);
        vol = MEDIA_VOL_USER_MAX;
    }

    int idx = volume_to_index(vol);

    int ret = media_policy_set_stream_volume(MEDIA_STREAM_MUSIC, idx);
    if (ret < 0) {
        syslog(LOG_ERR, "[%s] media_policy_set_stream_volume failed: %d\n",
            TAG, ret);
        return respond_error(output, output_size,
            "EIO_MEDIA",
            "media_policy_set_stream_volume failed");
    }

    pthread_mutex_lock(&g_media.lock);
    g_media.volume = vol;
    pthread_mutex_unlock(&g_media.lock);

    syslog(LOG_INFO, "[%s] Volume set to %d (index %d)\n", TAG, vol, idx);

    char extra[64];
    snprintf(extra, sizeof(extra),
        "\"volume\":%d,\"mapped_index\":%d", vol, idx);

    snprintf(output, output_size, "{\"ok\":true,%s}", extra);
    return OK;
}

/**
 * music_status — read-only query, no event wait needed.
 */

int tool_music_status_execute(const char* input_json,
    char* output, size_t output_size)
{
    (void)input_json;
    media_ctx_init_once();

    pthread_mutex_lock(&g_media.lock);

    media_state_e st = g_media.state;
    uint32_t sid = g_media.session_id;
    int vol = g_media.volume;

    unsigned int position_ms = 0;
    unsigned int duration_ms = 0;
    bool duration_supported = !g_media.is_live_stream;
    bool duration_ok = false;
    bool position_ok = false;

    if (g_media.handle && st != MEDIA_STATE_IDLE && st != MEDIA_STATE_ERROR) {
        if (media_player_get_position(g_media.handle, &position_ms) == 0) {
            position_ok = true;
        }

        if (media_player_get_duration(g_media.handle, &duration_ms) == 0) {
            duration_ok = true;
        }
    }

    cJSON* resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", true);
    cJSON_AddStringToObject(resp, "state", state_str(st));

    if (st != MEDIA_STATE_IDLE) {
        cJSON_AddStringToObject(resp, "url", g_media.url);
        cJSON_AddNumberToObject(resp, "session_id", sid);
    }

    if (position_ok) {
        cJSON_AddNumberToObject(resp, "position_ms", position_ms);
    }

    if (duration_ok) {
        cJSON_AddNumberToObject(resp, "duration_ms", duration_ms);
    } else {
        cJSON_AddNumberToObject(resp, "duration_ms", -1);
        cJSON_AddBoolToObject(resp, "duration_supported", duration_supported);
        if (st != MEDIA_STATE_IDLE) {
            cJSON_AddStringToObject(resp, "duration_error", "query_failed");
        }
    }

    cJSON_AddNumberToObject(resp, "volume", vol);

    pthread_mutex_unlock(&g_media.lock);

    char* json = cJSON_PrintUnformatted(resp);
    if (json) {
        snprintf(output, output_size, "%s", json);
        free(json);
    } else {
        snprintf(output, output_size, "{\"ok\":true,\"state\":\"%s\"}",
            state_str(st));
    }

    cJSON_Delete(resp);
    return OK;
}

/**
 * Module cleanup — called on program exit.
 */

void tool_media_cleanup(void)
{
    if (!g_media_initialized) {
        return;
    }

    syslog(LOG_INFO, "[%s] Cleanup\n", TAG);

    pthread_mutex_lock(&g_media.lock);
    g_media.exiting = true;
    force_close_locked();
    pthread_mutex_unlock(&g_media.lock);

    sem_destroy(&g_media.sem);
    pthread_mutex_destroy(&g_media.lock);
    g_media_initialized = false;
}

/****************************************************************************
 * Music Search (Netease Cloud Music API)
 ****************************************************************************/

#define SEARCH_RESP_SIZE 8192
#define SEARCH_MAX_RESULTS 5

static void url_encode(const char* src, char* dst, size_t dstsz)
{
    size_t pos = 0;

    while (*src && pos + 3 < dstsz) {
        unsigned char c = (unsigned char)*src;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.') {
            dst[pos++] = c;
        } else {
            pos += snprintf(dst + pos, dstsz - pos, "%%%02X", c);
        }

        src++;
    }

    dst[pos] = '\0';
}

int tool_music_search_execute(const char* input_json,
    char* output, size_t output_size)
{
    cJSON* root = cJSON_Parse(input_json);
    if (!root) {
        snprintf(output, output_size, "Error: invalid JSON");
        return -1;
    }

    const char* keyword = cJSON_GetStringValue(cJSON_GetObjectItem(root, "keyword"));
    if (!keyword || !keyword[0]) {
        keyword = cJSON_GetStringValue(cJSON_GetObjectItem(root, "query"));
    }
    if (!keyword || !keyword[0]) {
        cJSON_Delete(root);
        snprintf(output, output_size, "Error: missing 'keyword'");
        return -1;
    }

    char encoded[256];
    url_encode(keyword, encoded, sizeof(encoded));

    char path[512];
    snprintf(path, sizeof(path),
        "/api/search/get/web?s=%s&type=1&offset=0&total=true&limit=%d",
        encoded, SEARCH_MAX_RESULTS);

    char* resp = malloc(SEARCH_RESP_SIZE);
    if (!resp) {
        cJSON_Delete(root);
        snprintf(output, output_size, "Error: OOM");
        return -1;
    }

    int status = vela_https_get("music.163.com", "443", path,
        resp, SEARCH_RESP_SIZE);
    cJSON_Delete(root);

    if (status != 200) {
        snprintf(output, output_size, "Error: HTTP %d", status);
        free(resp);
        return -1;
    }

    cJSON* j = cJSON_Parse(resp);
    free(resp);
    if (!j) {
        snprintf(output, output_size, "Error: bad JSON from server");
        return -1;
    }

    cJSON* songs = cJSON_GetObjectItem(
        cJSON_GetObjectItem(j, "result"), "songs");
    if (!songs || !cJSON_IsArray(songs) || cJSON_GetArraySize(songs) == 0) {
        cJSON_Delete(j);
        snprintf(output, output_size, "No results for this keyword");
        return 0;
    }

    int count = cJSON_GetArraySize(songs);
    if (count > SEARCH_MAX_RESULTS) {
        count = SEARCH_MAX_RESULTS;
    }

    /* Use cJSON to build result so song names/artists are properly escaped */
    cJSON* result = cJSON_CreateObject();
    cJSON* songs_arr = cJSON_CreateArray();
    if (!result || !songs_arr) {
        cJSON_Delete(result);
        cJSON_Delete(songs_arr);
        cJSON_Delete(j);
        snprintf(output, output_size, "Error: OOM");
        return -ENOMEM;
    }

    cJSON_AddItemToObject(result, "songs", songs_arr);

    for (int i = 0; i < count; i++) {
        cJSON* song = cJSON_GetArrayItem(songs, i);
        const char* name = cJSON_GetStringValue(cJSON_GetObjectItem(song, "name"));
        int id = (int)cJSON_GetNumberValue(cJSON_GetObjectItem(song, "id"));

        cJSON* artists = cJSON_GetObjectItem(song, "artists");
        const char* artist = "Unknown";
        if (artists && cJSON_GetArraySize(artists) > 0) {
            const char* a = cJSON_GetStringValue(
                cJSON_GetObjectItem(cJSON_GetArrayItem(artists, 0), "name"));
            if (a) {
                artist = a;
            }
        }

        cJSON* song_obj = cJSON_CreateObject();
        if (!song_obj) {
            cJSON_Delete(result);
            cJSON_Delete(j);
            snprintf(output, output_size, "Error: OOM");
            return -ENOMEM;
        }

        char url[128];
        snprintf(url, sizeof(url),
            "https://music.163.com/song/media/outer/url?id=%d.mp3", id);
        if (!cJSON_AddStringToObject(song_obj, "name", name ? name : "") ||
            !cJSON_AddStringToObject(song_obj, "artist", artist) ||
            !cJSON_AddStringToObject(song_obj, "url", url)) {
            cJSON_Delete(song_obj);
            cJSON_Delete(result);
            cJSON_Delete(j);
            snprintf(output, output_size, "Error: OOM");
            return -ENOMEM;
        }

        cJSON_AddItemToArray(songs_arr, song_obj);
    }

    cJSON_AddStringToObject(result, "action",
        "call music_play with one of the URLs above");

    char* result_str = cJSON_PrintUnformatted(result);
    cJSON_Delete(result);
    cJSON_Delete(j);

    if (!result_str) {
        snprintf(output, output_size, "Error: OOM building result");
        return -ENOMEM;
    }

    strlcpy(output, result_str, output_size);
    free(result_str);

    syslog(LOG_INFO, "[%s] Found %d songs for search\n", TAG, count);
    return 0;
}
