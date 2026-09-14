/****************************************************************************
 * apps/vendor/allwinnertech/apps/ai_radio_console/ptt_control.h
 *
 * PTT GPIO control for AI Radio HAT
 ****************************************************************************/

#ifndef __AI_RADIO_PTT_H
#define __AI_RADIO_PTT_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize PTT GPIO output */
int ptt_init(const char *gpio_dev, int pin);

/* Key PTT (start radio transmission) */
int ptt_key(void);

/* Unkey PTT (stop radio transmission) */
int ptt_unkey(void);

/* Return the current PTT output key state (true = keyed). */
bool ptt_is_pressed(void);

/* Release PTT GPIO */
void ptt_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* __AI_RADIO_PTT_H */
