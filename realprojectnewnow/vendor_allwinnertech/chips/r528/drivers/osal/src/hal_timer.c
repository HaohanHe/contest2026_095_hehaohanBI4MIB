#include <string.h>
#include <stdlib.h>
#include <nuttx/wdog.h>
#include <nuttx/wqueue.h>

#include <hal_timer.h>

static void os_timer_common_callback(wdparm_t arg)
{
    osal_timer_t timer;

    timer = (osal_timer_t)arg;

    if(timer->flag & OSAL_TIMER_FLAG_PERIODIC)
	    wd_start(&timer->dog, timer->delta_ticks, os_timer_common_callback, (wdparm_t)timer);

    work_queue(HPWORK, &timer->wq_s, timer->priv.callback, timer->priv.arg, 0);
}

osal_timer_t osal_timer_create(const char *name,
                               timeout_func timeout,
                               void *parameter,
                               unsigned int tick,
                               unsigned char flag)
{
    struct _osal_timer_t *timer;
    timer = malloc(sizeof(struct _osal_timer_t));
    if (timer == NULL)
    {
        return NULL;
    }
    memset(timer, 0, sizeof(struct _osal_timer_t));
    timer->priv.callback = timeout;
    timer->priv.arg = parameter;
	timer->delta_ticks = tick;
	timer->flag = flag;
    return (osal_timer_t)timer;
}

hal_status_t osal_timer_delete(osal_timer_t timer)
{
    work_cancel(LPWORK, &timer->wq_s);
    wd_cancel(&timer->dog);
    free(timer);
    return HAL_OK;
}

hal_status_t osal_timer_start(osal_timer_t timer)
{
    wd_start(&timer->dog, timer->delta_ticks, os_timer_common_callback, (wdparm_t)timer);
    return HAL_OK;
}

hal_status_t osal_timer_stop(osal_timer_t timer)
{
    wd_cancel(&timer->dog);
    work_cancel(LPWORK, &timer->wq_s);
    return HAL_OK;
}

hal_status_t osal_timer_control(osal_timer_t timer, int cmd, void *arg)
{
    switch (cmd)
    {
        case OSAL_TIMER_CTRL_SET_TIME:
            timer->delta_ticks = *(unsigned int *)arg;
            wd_start(&timer->dog, timer->delta_ticks, os_timer_common_callback, (wdparm_t)timer);
            break;
        default:
            break;
    }

    return HAL_OK;
}

int osal_timer_is_vaild(osal_timer_t timer)
{
    if (timer)
        return 1;
    else
        return 0;
}

int osal_timer_is_active(osal_timer_t timer)
{
    return WDOG_ISACTIVE(&timer->dog);
}
