/****************************************************************************
 * apps/vendor/allwinnertech/apps/ai_radio_console/audio_recorder.h
 *
 * Audio capture from LINEIN for AI Radio Console
 *
 * Supports two backends selected by Kconfig:
 *   - voice_channel (recommended): ai_agent handles PTT + ASR end-to-end.
 *   - nxrecorder (legacy): raw LINEIN PCM + custom HTTP ASR client.
 ****************************************************************************/

#ifndef __AI_RADIO_RECORDER_H
#define __AI_RADIO_RECORDER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize the recorder.
 * For voice_channel path the device/sample_rate/channels/bps arguments
 * are ignored; ai_agent reads its own config.
 * For nxrecorder path they select the capture device and format. */
int audio_recorder_init(const char *device, uint32_t sample_rate,
                        uint8_t channels, uint8_t bps);

/* Start recording / PTT. */
int audio_recorder_start(void);

/* Stop recording / PTT.
 * For voice_channel this also runs ASR internally. */
int audio_recorder_stop(void);

/* Get the transcribed text. Valid after stop().
 * Returns 0 on success, -ENOTSUP if the current backend cannot provide
 * text directly (nxrecorder path), or another negative errno on failure.
 * On success buf is NUL-terminated. */
int audio_recorder_get_text(char *buf, size_t cap);

/* Get the recorded PCM data and length. Valid after stop().
 * Only supported by the nxrecorder backend. */
int audio_recorder_get_data(const uint8_t **pcm, size_t *len);

/* Save recorded PCM data to file. Only supported by nxrecorder backend. */
int audio_recorder_save(const char *path);

/* Release recorder resources */
void audio_recorder_deinit(void);

/* Returns true if currently recording */
bool audio_recorder_is_running(void);

#ifdef __cplusplus
}
#endif

#endif /* __AI_RADIO_RECORDER_H */
