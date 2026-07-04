#ifndef __RADIO_LOG_H
#define __RADIO_LOG_H

#include <stdint.h>
#include <stdbool.h>
#include "radio_config.h"

int radio_log_init(const char *db_path);
int radio_log_deinit(void);

int radio_log_add_qso(const qso_entry_t *entry);
int radio_log_get_qso(qso_entry_t *entries, size_t max_count, size_t *out_count);
int radio_log_get_qso_count(size_t *count);
int radio_log_delete_qso(uint32_t timestamp);
int radio_log_clear_all(void);

int radio_log_export_adif(const char *path);
int radio_log_import_adif(const char *path);

int radio_log_start_net(const char *net_name);
int radio_log_end_net(void);
int radio_log_add_net_participant(const char *callsign);
int radio_log_get_net_summary(char *summary, size_t max_len);

int radio_log_add_event(const alert_event_t *event);
int radio_log_get_events(alert_event_t *events, size_t max_count, size_t *out_count);

#endif
