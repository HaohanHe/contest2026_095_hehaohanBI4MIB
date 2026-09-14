#ifndef SUNXI_HAL_TIME_H
#define SUNXI_HAL_TIME_H

#ifdef __cplusplus
extern "C"
{
#endif

/* Parameters used to convert the timespec values: */
#ifndef MSEC_PER_SEC
#define MSEC_PER_SEC    1000L
#endif
#ifndef USEC_PER_MSEC
#define USEC_PER_MSEC   1000L
#endif
#ifndef NSEC_PER_USEC
#define NSEC_PER_USEC   1000L
#endif
#ifndef NSEC_PER_MSEC
#define NSEC_PER_MSEC   1000000L
#endif
#ifndef USEC_PER_SEC
#define USEC_PER_SEC    1000000L
#endif
#ifndef NSEC_PER_SEC
#define NSEC_PER_SEC    1000000000L
#endif
#ifndef FSEC_PER_SEC
#define FSEC_PER_SEC    1000000000000000LL
#endif

#ifdef CONFIG_KERNEL_FREERTOS
#include <FreeRTOS.h>
#include <task.h>
#include <portmacro.h>

#undef HAL_WAIT_FOREVER
#define HAL_WAIT_FOREVER portMAX_DELAY
#define HAL_WAIT_NO      (0)

#define OSTICK_TO_MS(x) (x * (MSEC_PER_SEC / CONFIG_HZ))
#define MS_TO_OSTICK(x) (x / (MSEC_PER_SEC / CONFIG_HZ))

#define hal_tick_get()  xTaskGetTickCount()

typedef TickType_t hal_tick_t;

#elif defined(CONFIG_RTTKERNEL)

#include <rtthread.h>

#undef HAL_WAIT_FOREVER
#define HAL_WAIT_FOREVER RT_WAITING_FOREVER
#define HAL_WAIT_NO      RT_WAITING_NO

#define OSTICK_TO_MS(x) (x * (MSEC_PER_SEC / CONFIG_HZ))
#define MS_TO_OSTICK(x) (x / (MSEC_PER_SEC / CONFIG_HZ))

#define hal_tick_get()  rt_tick_get()
typedef rt_tick_t hal_tick_t;

#elif defined(CONFIG_OS_NUTTX)

#include <time.h>

typedef clock_t hal_tick_t;

#define HAL_WAIT_FOREVER 0xFFFFFFFF
#define HAL_WAIT_NO      0

#define NS_PER_TICK     (41.66666666666667f)

struct timespec64
{
	int64_t  tv_sec;                 /*  seconds */
	int32_t  tv_nsec;                /*  nanoseconds */
};
extern void hal_time_add_ticks(struct timespec *timespec, unsigned long ticks);

#define OSTICK_TO_MS(x) (x * (MSEC_PER_SEC / (USEC_PER_SEC / CONFIG_USEC_PER_TICK)))
#define MS_TO_OSTICK(x) (x * ((USEC_PER_SEC / CONFIG_USEC_PER_TICK) / MSEC_PER_SEC))

#endif

int hal_sleep(unsigned int secs);
int hal_usleep(unsigned int usecs);
int hal_msleep(unsigned int msecs);
void hal_udelay(unsigned int us);
void hal_mdelay(unsigned int ms);
void hal_sdelay(unsigned int s);

uint64_t hal_gettime_ns(void);

#ifdef __cplusplus
}
#endif

#endif
