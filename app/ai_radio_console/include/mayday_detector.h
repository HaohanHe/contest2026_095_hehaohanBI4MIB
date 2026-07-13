#ifndef __MAYDAY_DETECTOR_H
#define __MAYDAY_DETECTOR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "radio_config.h"

typedef void (*mayday_alert_callback_t)(const alert_event_t *event, void *user_data);

int mayday_detector_init(void);
int mayday_detector_deinit(void);

int mayday_detector_feed_audio(const int16_t *samples, size_t count);
int mayday_detector_notify_transcript(const char *text, float confidence);

bool mayday_detector_is_triggered(void);
const alert_event_t *mayday_detector_get_last_alert(void);
void mayday_detector_clear_alert(void);

void mayday_detector_set_alert_cb(mayday_alert_callback_t cb, void *user_data);

#endif
