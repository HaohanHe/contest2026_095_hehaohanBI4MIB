#include "wav_encoder.h"
#include <string.h>
#include <stdio.h>

static void write_u32_le(uint8_t *p, uint32_t v)
{
    p[0] = v & 0xff;
    p[1] = (v >> 8) & 0xff;
    p[2] = (v >> 16) & 0xff;
    p[3] = (v >> 24) & 0xff;
}

static void write_u16_le(uint8_t *p, uint16_t v)
{
    p[0] = v & 0xff;
    p[1] = (v >> 8) & 0xff;
}

int wav_encoder_init(wav_encoder_t *enc, uint32_t sample_rate, uint16_t channels,
                     uint16_t bits_per_sample, uint8_t *buf, size_t buf_capacity)
{
    if (!enc || !buf || buf_capacity < 44) return -1;
    memset(enc, 0, sizeof(*enc));
    enc->sample_rate = sample_rate;
    enc->channels = channels;
    enc->bits_per_sample = bits_per_sample;
    enc->buffer = buf;
    enc->buffer_capacity = buf_capacity;
    enc->write_pos = 44;
    return 0;
}

int wav_encoder_write_samples(wav_encoder_t *enc, const int16_t *samples, size_t count)
{
    if (!enc || !samples) return -1;
    size_t bytes_per_sample = enc->bits_per_sample / 8;
    size_t needed = count * bytes_per_sample * enc->channels;
    if (enc->write_pos + needed > enc->buffer_capacity) {
        needed = enc->buffer_capacity - enc->write_pos;
        count = needed / (bytes_per_sample * enc->channels);
        needed = count * bytes_per_sample * enc->channels;
    }
    memcpy(enc->buffer + enc->write_pos, samples, needed);
    enc->write_pos += needed;
    enc->data_size += needed;
    return (int)count;
}

int wav_encoder_finalize(wav_encoder_t *enc)
{
    if (!enc) return -1;
    uint8_t *hdr = enc->buffer;
    uint32_t file_size = 36 + enc->data_size;
    uint32_t byte_rate = enc->sample_rate * enc->channels * enc->bits_per_sample / 8;
    uint16_t block_align = enc->channels * enc->bits_per_sample / 8;

    memcpy(hdr, "RIFF", 4);
    write_u32_le(hdr + 4, file_size);
    memcpy(hdr + 8, "WAVE", 4);
    memcpy(hdr + 12, "fmt ", 4);
    write_u32_le(hdr + 16, 16);
    write_u16_le(hdr + 20, 1);
    write_u16_le(hdr + 22, enc->channels);
    write_u32_le(hdr + 24, enc->sample_rate);
    write_u32_le(hdr + 28, byte_rate);
    write_u16_le(hdr + 32, block_align);
    write_u16_le(hdr + 34, enc->bits_per_sample);
    memcpy(hdr + 36, "data", 4);
    write_u32_le(hdr + 40, enc->data_size);
    return 0;
}

int wav_encoder_reset(wav_encoder_t *enc)
{
    if (!enc) return -1;
    enc->write_pos = 44;
    enc->data_size = 0;
    return 0;
}

size_t wav_encoder_get_total_size(const wav_encoder_t *enc)
{
    return enc ? 44 + enc->data_size : 0;
}

size_t wav_encoder_get_data_size(const wav_encoder_t *enc)
{
    return enc ? enc->data_size : 0;
}
