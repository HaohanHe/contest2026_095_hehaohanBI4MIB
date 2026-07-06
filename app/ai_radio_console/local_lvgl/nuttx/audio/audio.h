#ifndef __NUTTX_AUDIO_AUDIO_H_LOCAL_STUB
#define __NUTTX_AUDIO_AUDIO_H_LOCAL_STUB

#include <stdint.h>
#include <stddef.h>

#define AUDIO_TYPE_INPUT 1
#define AUDIO_FMT_PCM    1

#define AUDIOIOC_RESERVE        1
#define AUDIOIOC_RELEASE        2
#define AUDIOIOC_CONFIGURE      3
#define AUDIOIOC_START          4
#define AUDIOIOC_STOP           5
#define AUDIOIOC_GETBUFFERINFO  6
#define AUDIOIOC_ALLOCBUFFER    7
#define AUDIOIOC_FREEBUFFER     8
#define AUDIOIOC_ENQUEUEBUFFER  9
#define AUDIOIOC_REGISTERMQ    10
#define AUDIOIOC_UNREGISTERMQ  11

struct audio_caps_s {
    uint8_t  ac_len;
    uint8_t  ac_type;
    uint8_t  ac_channels;
    uint8_t  ac_chmap;
    union {
        uint8_t  b[8];
        uint32_t hw[2];
    } ac_controls;
    uint8_t  ac_subtype;
};

struct audio_caps_desc_s {
    struct audio_caps_s caps;
};

struct ap_buffer_s {
    uint8_t  *samp;
    uint32_t  nbytes;
    uint32_t  nmaxbytes;
    uint32_t  curbyte;
    uint32_t  flags;
    uint32_t  stride;
};

struct ap_buffer_info_s {
    uint32_t  nbuffers;
    uint32_t  buffer_size;
};

struct audio_buf_desc_s {
    uint32_t numbytes;
    union {
        void *buffer;
        void *pbuffer;
        uint32_t u;
    } u;
};

struct audio_msg_s {
    uint16_t msg_id;
    union {
        uint32_t u32;
        void *ptr;
    } u;
};

#define AUDIO_MSG_DEQUEUE  1
#define AUDIO_MSG_STOP     2
#define AUDIO_MSG_COMPLETE 3
#define AUDIO_MSG_IOERR    4

#endif
