/*
 * Copyright (C) 2017 XRADIO TECHNOLOGY CO., LTD. All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the
 *       distribution.
 *    3. Neither the name of XRADIO TECHNOLOGY CO., LTD. nor the names of
 *       its contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _KERNEL_OS_NUTTX_OS_TIME_H_
#define _KERNEL_OS_NUTTX_OS_TIME_H_
#include "_os_common.h"
#include <debug.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef clock_t xr_os_time_t;

/* Parameters of time */
#ifndef OS_MSEC_PER_SEC
#define OS_MSEC_PER_SEC		1000UL
#endif
#ifndef OS_USEC_PER_MSEC
#define OS_USEC_PER_MSEC	1000UL
#endif
#ifndef OS_USEC_PER_SEC
#define OS_USEC_PER_SEC		1000000UL
#endif

#ifndef OS_HZ
/* system clock's frequency, ticks per second */
#define OS_HZ				(OS_USEC_PER_SEC / CONFIG_USEC_PER_TICK)
#endif

#ifndef OS_TICK
/* usec per tick (1000000 / OS_HZ) */
#define OS_TICK				(CONFIG_USEC_PER_TICK)
#endif

#ifndef OS_GetTicks
/* The number of ticks since boot. */
//#define OS_GetTicks()		((uint32_t)(krhino_sys_tick_get() / OS_MSEC_PER_SEC))
#define OS_GetTicks()		(clock())
#endif

	/* Due to portTICK_TYPE_IS_ATOMIC is 1, call xTaskGetTickCount() in ISR is safe also */

#ifndef OS_GetTime //(clock_systimer() / OS_HZ))
/* The number of seconds since boot. */
#if 0
#define OS_GetTime() \
({ \
	uint32_t ret; \
	ret = (clock_systimer() / OS_HZ); \
	_info("[%s,%d] get time:%d\n",__func__,__LINE__, ret); \
	ret; \
 })
#else
#define OS_GetTime() (clock() / OS_HZ)
#endif
#endif

#ifndef OS_SecsToTicks
#define OS_SecsToTicks(sec)     ((xr_os_time_t)(sec) * OS_HZ)
#endif

#ifndef OS_MSecsToTicks // ((xr_os_time_t)(msec * (CONFIG_USEC_PER_TICK/1000))))
#if 0
#define OS_MSecsToTicks(msec) \
({ \
	xr_os_time_t ret; \
	ret = ((xr_os_time_t)(msec * (CONFIG_USEC_PER_TICK/1000)));\
	_info("[%s,%d] ms to tick:%d\n",__func__,__LINE__,ret); \
	ret; \
 })
#else
#define OS_MSecsToTicks(msec) ((xr_os_time_t)(msec * OS_HZ / 1000))
#endif
#endif

#ifndef OS_TicksToMSecs
#if 0
#define OS_TicksToMSecs(t) \
({ \
	uint32_t ret; \
	ret = ((uint32_t)(t) / (OS_USEC_PER_MSEC / OS_TICK)); \
	_info("[%s,%d] tick to ms:%d\n",__func__,__LINE__, ret); \
	ret; \
 })
#else
#define OS_TicksToMSecs(t) ((clock_t)(t) / (OS_USEC_PER_MSEC / OS_TICK))
#endif
#endif

#ifndef OS_TicksToSecs //((uint32_t)(t) / (OS_USEC_PER_SEC / OS_TICK))
#if 0
#define OS_TicksToSecs(t)   \
({ \
	uint32_t ret; \
	ret = ((uint32_t)(t) / (OS_USEC_PER_SEC / OS_TICK)); \
	_info("[%s,%d] tick to s:%d\n",__func__,__LINE__, ret); \
	ret; \
 })
#else
#define OS_TicksToSecs(t)   ((clock_t)(t) / (OS_USEC_PER_SEC / OS_TICK))
#endif
#endif

#ifndef OS_GetJiffies
#define OS_GetJiffies()			OS_GetTicks()
#endif

#ifndef OS_SecsToJiffies
#define OS_SecsToJiffies(sec)	OS_SecsToTicks(sec)
#endif

#ifndef OS_MSecsToJiffies
#define	OS_MSecsToJiffies(msec)	OS_MSecsToTicks(msec)
#endif

#ifndef OS_JiffiesToMSecs
#define	OS_JiffiesToMSecs(j)	OS_TicksToMSecs(j)
#endif

#ifndef OS_JiffiesToSecs
#define	OS_JiffiesToSecs(j)		OS_TicksToSecs(j)
#endif

#ifndef OS_MSleep
/* sleep */
#define OS_MSleep(msec)			nxsig_usleep(1000*msec)
#endif

#ifndef OS_Sleep
#define OS_Sleep(sec)			nxsig_sleep(sec)
#endif
#ifndef OS_SSleep
#define OS_SSleep(sec)			OS_Sleep(sec)
#endif

#ifndef OS_TimeAfter
#define OS_TimeAfter(a, b)              ((sclock_t)(b) - (sclock_t)(a) < 0)
#endif
#ifndef OS_TimeBefore
#define OS_TimeBefore(a, b)             OS_TimeAfter(b, a)
#endif
#ifndef OS_TimeAfterEqual
#define OS_TimeAfterEqual(a, b)         ((sclock_t)(a) - (sclock_t)(b) >= 0)
#endif
#ifndef OS_TimeBeforeEqual
#define OS_TimeBeforeEqual(a, b)        OS_TimeAfterEqual(b, a)
#endif

/* rand, read value from Cortex-M SYST_CVR register */
int rand(void);
#ifndef OS_Rand32
#define OS_Rand32() ((uint32_t)((rand() & 0xffffff) | (OS_GetTicks() << 24)))//((uint32_t)(rand() * OS_GetTicks() + 19))
#endif
#ifdef __cplusplus
}
#endif

#endif /* _KERNEL_OS_NUTTX_OS_TIME_H_ */
