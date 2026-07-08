#include "input_lradc.h"

/* LRADC button handling is now unified in main.c button_poll_timer to avoid
 * two readers competing for /dev/input/event1 events. This module remains as a
 * placeholder so existing callers do not need to be changed.
 */

volatile bool g_lradc_enter_consumed = false;

int input_lradc_init(void)
{
    return 0;
}

int input_lradc_start(void)
{
    return 0;
}

int input_lradc_stop(void)
{
    return 0;
}
