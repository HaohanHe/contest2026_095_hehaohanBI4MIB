/****************************************************************************
 * apps/vendor/allwinnertech/apps/ai_radio_console/audio_recorder.c
 *
 * Audio capture from LINEIN for AI Radio Console.
 *
 * Two backends are supported, selected by Kconfig:
 *   CONFIG_AI_RADIO_CONSOLE_USE_VOICE_CHANNEL=y (default)
 *     Use ai_agent's voice_channel API. ai_agent handles capture,
 *     ASR (Volc/SiliconFlow/...) and HTTPS internally.
 *   CONFIG_AI_RADIO_CONSOLE_USE_VOICE_CHANNEL=n
 *     Fall back to raw nxrecorder capture + the custom HTTP ASR client.
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "audio_recorder.h"

#ifdef CONFIG_AI_RADIO_CONSOLE_USE_VOICE_CHANNEL

#include <voice/voice_channel.h>

#define TEXT_BUF_SIZE 1024

struct radio_recorder_s {
    bool initialized;
    bool running;
    char text[TEXT_BUF_SIZE];
};

static struct radio_recorder_s g_recorder;

int audio_recorder_init(const char *device, uint32_t sample_rate,
                        uint8_t channels, uint8_t bps)
{
    int ret;

    (void)device;
    (void)sample_rate;
    (void)channels;
    (void)bps;

    if (g_recorder.initialized) {
        return -EBUSY;
    }

    memset(&g_recorder, 0, sizeof(g_recorder));

    ret = voice_channel_init();
    if (ret < 0) {
        printf("[radio_rec] voice_channel_init failed: %d\n", ret);
        return ret;
    }

    g_recorder.initialized = true;
    printf("[radio_rec] voice_channel backend initialized\n");
    return 0;
}

int audio_recorder_start(void)
{
    int ret;

    if (!g_recorder.initialized) {
        printf("[radio_rec] not initialized\n");
        return -EINVAL;
    }

    if (g_recorder.running) {
        return 0;
    }

    g_recorder.text[0] = '\0';

    ret = voice_channel_start();
    if (ret < 0) {
        printf("[radio_rec] voice_channel_start failed: %d\n", ret);
        return ret;
    }

    g_recorder.running = true;
    printf("[radio_rec] voice_channel recording started\n");
    return 0;
}

int audio_recorder_stop(void)
{
    int ret;

    if (!g_recorder.initialized || !g_recorder.running) {
        return 0;
    }

    ret = voice_channel_stop_with_text(g_recorder.text, sizeof(g_recorder.text));
    if (ret < 0) {
        printf("[radio_rec] voice_channel_stop_with_text failed: %d\n", ret);
        g_recorder.text[0] = '\0';
    } else {
        printf("[radio_rec] voice_channel ASR: %s\n",
               g_recorder.text[0] ? g_recorder.text : "(empty)");
    }

    g_recorder.running = false;
    return ret;
}

int audio_recorder_get_text(char *buf, size_t cap)
{
    if (!buf || cap == 0) {
        return -EINVAL;
    }

    if (!g_recorder.initialized) {
        return -EINVAL;
    }

    strncpy(buf, g_recorder.text, cap - 1);
    buf[cap - 1] = '\0';
    return 0;
}

int audio_recorder_get_data(const uint8_t **pcm, size_t *len)
{
    (void)pcm;
    (void)len;
    return -ENOTSUP;
}

int audio_recorder_save(const char *path)
{
    (void)path;
    return -ENOTSUP;
}

void audio_recorder_deinit(void)
{
    if (!g_recorder.initialized) {
        return;
    }

    audio_recorder_stop();
    g_recorder.initialized = false;
    printf("[radio_rec] voice_channel backend deinit\n");
}

bool audio_recorder_is_running(void)
{
    return g_recorder.running;
}

#else /* !CONFIG_AI_RADIO_CONSOLE_USE_VOICE_CHANNEL */

#include <nuttx/audio/audio.h>
#include <audioutils/nxaudio.h>
#include <system/nxrecorder.h>

#define RECORD_FILE "/tmp/radio_recording.pcm"
#define MAX_PCM_SIZE (2 * 1024 * 1024) /* 2MB max ~ 1 min at 16kHz 16bit mono */

struct radio_recorder_s {
    FAR struct nxrecorder_s *recorder;
    char device[64];
    uint32_t sample_rate;
    uint8_t channels;
    uint8_t bps;
    int fd;
    bool running;
    bool initialized;
};

static struct radio_recorder_s g_recorder;

static int record_write_callback(int fd, FAR struct ap_buffer_s *apb)
{
    int ret;

    if (g_recorder.fd < 0) {
        return -EIO;
    }

    ret = write(g_recorder.fd, apb->samp, apb->nbytes);
    if (ret < 0) {
        printf("[radio_rec] write failed: %d\n", errno);
        return ret;
    }

    return ret;
}

static struct nxrecorder_enc_ops_s g_record_ops = {
    AUDIO_FMT_PCM,
    NULL,
    record_write_callback,
};

int audio_recorder_init(const char *device, uint32_t sample_rate,
                        uint8_t channels, uint8_t bps)
{
    if (g_recorder.initialized) {
        return -EBUSY;
    }

    memset(&g_recorder, 0, sizeof(g_recorder));
    strncpy(g_recorder.device, device ? device : "/dev/audio/pcm0c",
            sizeof(g_recorder.device) - 1);
    g_recorder.sample_rate = sample_rate ? sample_rate : 16000;
    g_recorder.channels = channels ? channels : 1;
    g_recorder.bps = bps ? bps : 16;
    g_recorder.fd = -1;
    g_recorder.running = false;

    g_recorder.recorder = nxrecorder_create();
    if (!g_recorder.recorder) {
        printf("[radio_rec] nxrecorder_create failed\n");
        return -ENOMEM;
    }

    g_recorder.recorder->ops = &g_record_ops;
    g_recorder.initialized = true;
    printf("[radio_rec] nxrecorder backend init ok: %s %luHz %ubit %uch\n",
           g_recorder.device, (unsigned long)g_recorder.sample_rate,
           g_recorder.bps, g_recorder.channels);
    return 0;
}

int audio_recorder_start(void)
{
    int ret;

    if (!g_recorder.initialized) {
        printf("[radio_rec] not initialized\n");
        return -EINVAL;
    }

    if (g_recorder.running) {
        return 0;
    }

    unlink(RECORD_FILE);
    g_recorder.fd = open(RECORD_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (g_recorder.fd < 0) {
        printf("[radio_rec] open %s failed: %d\n", RECORD_FILE, errno);
        return -errno;
    }

    nxrecorder_setdevice(g_recorder.recorder, g_recorder.device);
    ret = nxrecorder_recordinternal(g_recorder.recorder, RECORD_FILE,
                                    AUDIO_FMT_PCM,
                                    g_recorder.channels,
                                    g_recorder.bps,
                                    g_recorder.sample_rate,
                                    1); /* chmap = left */
    if (ret != OK) {
        printf("[radio_rec] start failed: %d\n", ret);
        close(g_recorder.fd);
        g_recorder.fd = -1;
        return ret;
    }

    g_recorder.running = true;
    printf("[radio_rec] nxrecorder recording started\n");
    return 0;
}

int audio_recorder_stop(void)
{
    if (!g_recorder.initialized || !g_recorder.running) {
        return 0;
    }

    nxrecorder_stop(g_recorder.recorder);

    if (g_recorder.fd >= 0) {
        close(g_recorder.fd);
        g_recorder.fd = -1;
    }

    g_recorder.running = false;
    printf("[radio_rec] nxrecorder recording stopped\n");
    return 0;
}

int audio_recorder_get_text(char *buf, size_t cap)
{
    (void)buf;
    (void)cap;
    return -ENOTSUP;
}

int audio_recorder_get_data(const uint8_t **pcm, size_t *len)
{
    struct stat st;
    int fd;
    static uint8_t *s_buf = NULL;
    static size_t s_size = 0;

    if (!pcm || !len) {
        return -EINVAL;
    }

    if (stat(RECORD_FILE, &st) < 0) {
        *pcm = NULL;
        *len = 0;
        return -errno;
    }

    if ((size_t)st.st_size > MAX_PCM_SIZE) {
        printf("[radio_rec] recording too large: %ld\n", (long)st.st_size);
        return -EFBIG;
    }

    if ((size_t)st.st_size > s_size) {
        uint8_t *tmp = realloc(s_buf, st.st_size);
        if (!tmp) {
            return -ENOMEM;
        }
        s_buf = tmp;
        s_size = st.st_size;
    }

    fd = open(RECORD_FILE, O_RDONLY);
    if (fd < 0) {
        return -errno;
    }

    if (read(fd, s_buf, st.st_size) != st.st_size) {
        close(fd);
        return -EIO;
    }
    close(fd);

    *pcm = s_buf;
    *len = st.st_size;
    return 0;
}

int audio_recorder_save(const char *path)
{
    const uint8_t *pcm;
    size_t len;
    int fd;
    int ret;

    ret = audio_recorder_get_data(&pcm, &len);
    if (ret < 0) {
        return ret;
    }

    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        return -errno;
    }

    if (write(fd, pcm, len) != (ssize_t)len) {
        close(fd);
        return -EIO;
    }

    close(fd);
    printf("[radio_rec] saved %zu bytes to %s\n", len, path);
    return 0;
}

void audio_recorder_deinit(void)
{
    if (!g_recorder.initialized) {
        return;
    }

    audio_recorder_stop();

    if (g_recorder.recorder) {
        nxrecorder_release(g_recorder.recorder);
        g_recorder.recorder = NULL;
    }

    g_recorder.initialized = false;
    printf("[radio_rec] nxrecorder backend deinit\n");
}

bool audio_recorder_is_running(void)
{
    return g_recorder.running;
}

#endif /* CONFIG_AI_RADIO_CONSOLE_USE_VOICE_CHANNEL */
