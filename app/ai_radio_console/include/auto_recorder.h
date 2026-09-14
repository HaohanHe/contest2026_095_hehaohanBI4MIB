#ifndef AUTO_RECORDER_H
#define AUTO_RECORDER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct auto_recorder_s auto_recorder_t;

int  auto_recorder_init(int audio_fd, int sample_rate);
void auto_recorder_deinit(void);
int  auto_recorder_get_segment_count(void);

#ifdef __cplusplus
}
#endif

#endif
