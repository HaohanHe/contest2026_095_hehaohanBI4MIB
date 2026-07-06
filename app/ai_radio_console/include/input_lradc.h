#ifndef __INPUT_LRADC_H
#define __INPUT_LRADC_H

#include <stdint.h>
#include <stdbool.h>
#include <nuttx/input/buttons.h>

#ifdef __cplusplus
extern "C" {
#endif

/* R528 LRADC button bitmask mapping (btn_buttonset_t) */
#define LRADC_BTN_HOME  ((btn_buttonset_t)1 << 0)  /* BUTTON_1: Home      */
#define LRADC_BTN_VOLDN ((btn_buttonset_t)1 << 1)  /* BUTTON_2: Vol-      */
#define LRADC_BTN_VOLUP ((btn_buttonset_t)1 << 2)  /* BUTTON_3: Vol+      */
#define LRADC_BTN_MENU  ((btn_buttonset_t)1 << 3)  /* BUTTON_4: Menu      */
#define LRADC_BTN_ENTER ((btn_buttonset_t)1 << 4)  /* BUTTON_5: Enter/PTT */

#define LRADC_POLL_MS        50   /* ~20 Hz */
#define LRADC_LONGPRESS_MS   200

int input_lradc_init(void);
int input_lradc_start(void);
int input_lradc_stop(void);

#ifdef __cplusplus
}
#endif

#endif
