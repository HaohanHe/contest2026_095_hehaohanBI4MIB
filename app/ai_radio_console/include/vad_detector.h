#ifndef VAD_DETECTOR_H
#define VAD_DETECTOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vad_detector_s vad_detector_t;

typedef enum {
    VAD_STATE_SILENCE,
    VAD_STATE_SPEECH,
    VAD_STATE_END
} vad_state_t;

vad_detector_t *vad_create(int sample_rate);
void vad_destroy(vad_detector_t *vad);
void vad_reset(vad_detector_t *vad);
vad_state_t vad_process(vad_detector_t *vad, const int16_t *samples, int n);
int vad_get_frame_samples(vad_detector_t *vad);

#endif
