#ifndef __AUDIO_HELPER_H__
#define __AUDIO_HELPER_H__

#include <stdint.h>
#include <stdbool.h>

/****************************************************************************
 * Public Types
 ****************************************************************************/

enum audio_mode_e
{
  AUDIO_MODE_RECORD,    /* Record then playback */
  AUDIO_MODE_LOOPBACK,  /* Real-time loopback */
};

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/* Initialize audio system with specified mode and parameters */
int audio_init(enum audio_mode_e mode, uint8_t channels, uint8_t bps,
               uint32_t sample_rate, uint8_t chmap);

/* Start audio operation (recording + playing for loopback, or recording for record mode) */
int audio_start(void);

/* Stop audio operation */
void audio_stop(void);

/* Playback recorded file (only for record mode) */
int audio_playback(void);

/* Cleanup resources */
void audio_cleanup(void);

/* Volume control */
void audio_set_volume(int vol);

#endif /* __AUDIO_HELPER_H__ */
