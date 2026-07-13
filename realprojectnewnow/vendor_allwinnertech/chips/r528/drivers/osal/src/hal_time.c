#include <string.h>

#include <hal_time.h>
#include <hal_status.h>

#include <sunxi_hal_common.h>

#include <nuttx/arch.h>
//extern void sleep(int seconds);
//extern int usleep(int usecs);
//extern void udelay(unsigned int us);

#define GENERNIC_TIMRE_REQ	(24000000)

static inline uint32_t arch_timer_get_cntfrq(void)
{
    uint32_t val;
    asm volatile("mrc p15, 0, %0, c14, c0, 0" : "=r"(val));
    return val;
}

static inline uint64_t arch_counter_get_cntvct(void)
{
    uint64_t cval;

    isb();
    asm volatile("mrrc p15, 1, %Q0, %R0, c14" : "=r"(cval));
    return cval;
}

static inline uint64_t arch_counter_get_cntpct(void)
{
    uint64_t cval;

    isb();
    asm volatile("mrrc p15, 0, %Q0, %R0, c14" : "=r"(cval));
    return cval;
}

uint64_t read_cntpct(void)
{
    uint64_t cval;

    isb();
    asm volatile("mrrc p15, 0, %Q0, %R0, c14" : "=r"(cval));
    return cval;
}

static uint64_t (*arch_timer_read_counter)(void) = arch_counter_get_cntpct;

uint64_t ktime_get(void)
{
    //~41.6666667 nano seconds per tick.
    double ns_per_ticks = NS_PER_TICK;
    uint64_t arch_counter = arch_timer_read_counter();

    return arch_counter * ns_per_ticks;
}

int do_gettimeofday(struct timespec64 *ts)
{
    if (ts == NULL)
        return 0;

    int64_t nsecs = ktime_get();

    ts->tv_sec  = nsecs / NSEC_PER_SEC;
    ts->tv_nsec = nsecs % NSEC_PER_SEC;

    return 0;
}

int msleep(unsigned int msecs)
{
    usleep(msecs * 1000);

    return HAL_OK;
}

int hal_sleep(unsigned int secs)
{
    sleep(secs);
    return HAL_OK;
}

int hal_usleep(unsigned int usecs)
{
    usleep(usecs);
    return HAL_OK;
}

int hal_msleep(unsigned int msecs)
{
    msleep(msecs);
    return HAL_OK;
}

void hal_udelay(unsigned int us)
{
    up_udelay(us);
}

void hal_mdelay(unsigned int ms)
{
    hal_udelay(ms * 1000);
}

void hal_sdelay(unsigned int s)
{
    hal_mdelay(s * 1000);
}

uint64_t hal_gettime_ns(void)
{
    struct timespec64 timeval;
    memset(&timeval, 0, sizeof(struct timespec64));
    do_gettimeofday(&timeval);
    return timeval.tv_sec * MSEC_PER_SEC * USEC_PER_MSEC * 1000LL + timeval.tv_nsec;
}

void hal_time_add_ticks(struct timespec *timespec, unsigned long ticks)
{
	uint32_t tmp;

	tmp = TICK2SEC(ticks);
	timespec->tv_sec += tmp;

	ticks -= SEC2TICK(tmp);
	tmp = TICK2NSEC(ticks);

	timespec->tv_nsec += tmp;
}

