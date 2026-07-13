#ifndef __CONFIG_STORE_H
#define __CONFIG_STORE_H

#include "radio_config.h"

int radio_config_store_init(void);
int radio_config_store_load(ai_config_t *config);
int radio_config_store_save(const ai_config_t *config);
void radio_config_set_defaults(ai_config_t *config);

#endif
