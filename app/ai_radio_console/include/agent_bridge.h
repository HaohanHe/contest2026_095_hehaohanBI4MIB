#ifndef __AGENT_BRIDGE_H
#define __AGENT_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "radio_config.h"

typedef void (*agent_msg_callback_t)(const char *json_msg, void *user_data);
typedef void (*agent_alert_callback_t)(const alert_event_t *alert, void *user_data);
typedef void (*agent_transcript_cb_t)(const char *text, int is_partial, void *user_data);
typedef void (*agent_translate_cb_t)(const char *original, const char *translated, void *user_data);
typedef void (*agent_cw_text_cb_t)(const char *text, void *user_data);

int agent_bridge_init(void);
int agent_bridge_deinit(void);

int agent_bridge_send_audio(const int16_t *samples, size_t count);
int agent_bridge_send_text(const char *text);
int agent_bridge_send_event(const char *event_type, const char *json_payload);
int agent_bridge_request_translate(const char *text, translate_lang_t from, translate_lang_t to);
int agent_bridge_request_summary(const char *context);
int agent_bridge_send_cw_text(const char *text);
int agent_bridge_send_signal_report(float freq, float rssi, float snr, const char *interference_json);
int agent_bridge_request_freq_recommend(radio_band_t band, float lat, float lon);

int agent_bridge_install_skills(void);

bool agent_bridge_is_connected(void);
int agent_bridge_get_status(char *status, size_t max_len);

void agent_bridge_set_msg_cb(agent_msg_callback_t cb, void *user_data);
void agent_bridge_set_alert_cb(agent_alert_callback_t cb, void *user_data);
void agent_bridge_set_transcript_cb(agent_transcript_cb_t cb, void *user_data);
void agent_bridge_set_translate_cb(agent_translate_cb_t cb, void *user_data);
void agent_bridge_set_cw_cb(agent_cw_text_cb_t cb, void *user_data);

#endif
