#ifndef __WAV_ENCODER_H
#define __WAV_ENCODER_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bits_per_sample;
    uint32_t data_size;
    uint8_t *buffer;
    size_t buffer_capacity;
    size_t write_pos;
} wav_encoder_t;

int wav_encoder_init(wav_encoder_t *enc, uint32_t sample_rate, uint16_t channels,
                     uint16_t bits_per_sample, uint8_t *buf, size_t buf_capacity);
int wav_encoder_write_samples(wav_encoder_t *enc, const int16_t *samples, size_t count);
int wav_encoder_reset(wav_encoder_t *enc);
size_t wav_encoder_get_total_size(const wav_encoder_t *enc);
size_t wav_encoder_get_data_size(const wav_encoder_t *enc);
int wav_encoder_finalize(wav_encoder_t *enc);

#endif