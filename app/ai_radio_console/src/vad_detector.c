/****************************************************************************
 * vad_detector.c - Voice Activity Detection (energy-based)
 *
 * Simple VAD using short-time energy (RMS) with adaptive threshold.
 ****************************************************************************/

#include "vad_detector.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define VAD_FRAME_MS      20
#define VAD_FRAME_SAMPLES (16000 * VAD_FRAME_MS / 1000)
#define VAD_SILENCE_MS    2000
#define VAD_SPEECH_MS     500
#define VAD_NOISE_INIT_MS 1000

struct vad_detector_s {
    float noise_level;
    float speech_level;
    float threshold;
    int   silence_frames;
    int   speech_frames;
    int   initialized;
    int   frame_samples;
};

static float compute_rms(const int16_t *samples, int n)
{
    if (n <= 0) return 0.0f;
    float sum = 0.0f;
    for (int i = 0; i < n; i++) {
        sum += (float)samples[i] * (float)samples[i];
    }
    return sqrtf(sum / n);
}

vad_detector_t *vad_create(int sample_rate)
{
    vad_detector_t *vad = calloc(1, sizeof(*vad));
    if (!vad) return NULL;
    vad->frame_samples = sample_rate * VAD_FRAME_MS / 1000;
    vad->noise_level = 500.0f;
    vad->speech_level = 5000.0f;
    vad->threshold = 3000.0f;
    vad->initialized = 0;
    return vad;
}

void vad_destroy(vad_detector_t *vad)
{
    free(vad);
}

void vad_reset(vad_detector_t *vad)
{
    if (!vad) return;
    vad->silence_frames = 0;
    vad->speech_frames = 0;
    vad->initialized = 0;
    vad->noise_level = 500.0f;
    vad->threshold = 3000.0f;
}

vad_state_t vad_process(vad_detector_t *vad, const int16_t *samples, int n)
{
    if (!vad || !samples || n <= 0) return VAD_STATE_SILENCE;

    float energy = compute_rms(samples, n);

    if (!vad->initialized) {
        vad->noise_level = energy;
        vad->speech_level = energy;
        vad->threshold = energy * 3.0f;
        if (vad->threshold < 500.0f) vad->threshold = 500.0f;
        vad->initialized = 1;
        return VAD_STATE_SILENCE;
    }

    if (energy > vad->threshold) {
        vad->speech_level = vad->speech_level * 0.95f + energy * 0.05f;
        vad->silence_frames = 0;
        vad->speech_frames++;
        return VAD_STATE_SPEECH;
    } else {
        vad->noise_level = vad->noise_level * 0.98f + energy * 0.02f;
        vad->threshold = (vad->noise_level + vad->speech_level) / 2.0f;
        if (vad->threshold < vad->noise_level * 1.5f) {
            vad->threshold = vad->noise_level * 1.5f;
        }
        vad->speech_frames = 0;
        vad->silence_frames++;

        int silence_frames_needed = VAD_SILENCE_MS / VAD_FRAME_MS;
        int speech_frames_needed = VAD_SPEECH_MS / VAD_FRAME_MS;

        if (vad->silence_frames >= silence_frames_needed && vad->speech_frames > 0) {
            return VAD_STATE_END;
        }
        return VAD_STATE_SILENCE;
    }
}

int vad_get_frame_samples(vad_detector_t *vad)
{
    return vad ? vad->frame_samples : 0;
}
