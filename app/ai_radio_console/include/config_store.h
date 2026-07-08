#ifndef __CONFIG_STORE_H
#define __CONFIG_STORE_H

#include "radio_config.h"

int config_store_init(void);
int config_store_load(ai_config_t *config);
int config_store_save(const ai_config_t *config);
void config_set_defaults(ai_config_t *config);

#endif
