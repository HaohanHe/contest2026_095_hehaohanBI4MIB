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
#ifndef _KERNEL_OS_NUTTX_OS_TIMER_H_
#define _KERNEL_OS_NUTTX_OS_TIMER_H_

#include "_os_common.h"
#include <nuttx/irq.h>
#include <nuttx/wqueue.h>
#include <nuttx/wdog.h>
#ifdef __cplusplus
extern "C" {
#endif
#define NX_TIMER_LOGE sinfo
#define MAX_TIME_DELAY  0xffffffffU

typedef enum {
	OS_TIMER_ONCE		= 0,
	OS_TIMER_PERIODIC	= 1
} OS_TimerType;

typedef void (*OS_TimerCallback_t)(void *arg);
typedef FAR struct wdog_s *WDOG_ID;
typedef struct nuttx_timer {
	struct wdog_s handle;
	OS_TimerType type;
	OS_Time_t time_ms;
	bool cb_enable;
	OS_TimerCallback_t cb;
	void *cb_para;
	struct work_s wq_s;  /* For deferring poll work to the work queue */
}OS_Timer_t;

OS_Status OS_TimerCreate(OS_Timer_t *timer, OS_TimerType type,
                         OS_TimerCallback_t cb, void *ctx, OS_Time_t periodMS);

OS_Status OS_TimerDelete(OS_Timer_t *timer);

OS_Status OS_TimerStart(OS_Timer_t *timer);

OS_Status OS_TimerChangePeriod(OS_Timer_t *timer, OS_Time_t periodMS);

OS_Status OS_TimerStop(OS_Timer_t *timer);

int OS_TimerIsValid(OS_Timer_t *timer);

void OS_TimerSetInvalid(OS_Timer_t *timer);

int OS_TimerIsActive(OS_Timer_t *timer);

void *OS_TimerGetContext(void *arg);

#ifdef __cplusplus
}
#endif

#endif /* _KERNEL_OS_NUTTX_OS_TIMER_H_ */
