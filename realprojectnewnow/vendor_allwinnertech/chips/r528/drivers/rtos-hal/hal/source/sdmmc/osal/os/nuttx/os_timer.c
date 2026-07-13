/*
* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
*
* Author: laumy <liumingyuan@allwinnertech.com>
* Date  : 2020-01-09
*
* Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
* the the people's Republic of China and other countries.
* All Allwinner Technology Co.,Ltd. trademarks are used with permission.
*
* DISCLAIMER
* THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
* IF YOU NEED TO INTEGRATE THIRD PARTY’S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
* IN ALLWINNERS’SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
* ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
* ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
* COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
* YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY’S TECHNOLOGY.
*
*
* THIS SOFTWARE IS PROVIDED BY ALLWINNER"AS IS" AND TO THE MAXIMUM EXTENT
* PERMITTED BY LAW, ALLWINNER EXPRESSLY DISCLAIMS ALL WARRANTIES OF ANY KIND,
* WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING WITHOUT LIMITATION REGARDING
* THE TITLE, NON-INFRINGEMENT, ACCURACY, CONDITION, COMPLETENESS, PERFORMANCE
* OR MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
* IN NO EVENT SHALL ALLWINNER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
* NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS, OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
* OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include <nuttx/irq.h>
#include <nuttx/spinlock.h>
#include <os_timer.h>
#include <stdbool.h>
#include <stdio.h>

//#define NX_TIMER_LOGE printf

static void nx_timer_callback(wdparm_t arg1)
{
	irqstate_t flags;
	OS_Timer_t *timer;

	//NX_TIMER_LOGE("--+++\n");

	flags = enter_critical_section();

	timer = (OS_Timer_t *)arg1;

	if(timer->cb_enable == true)
		work_queue(HPWORK, &timer->wq_s, timer->cb, timer->cb_para, 0);
	/*
	else
		NX_TIMER_LOGE("forbiden create work queue.\n");
	*/
	if(timer->type == OS_TIMER_PERIODIC)
		wd_start(&timer->handle,timer->time_ms*1000/CONFIG_USEC_PER_TICK,
			nx_timer_callback,1);

	leave_critical_section(flags);
}

OS_Status OS_TimerCreate(OS_Timer_t *timer, OS_TimerType type,
                         OS_TimerCallback_t cb, void *ctx, OS_Time_t periodMS)
{
	irqstate_t flags;
	flags = enter_critical_section();

	timer->type = type;
	timer->time_ms = periodMS;
	timer->cb = cb;
	timer->cb_para = ctx;

	leave_critical_section(flags);

	return OS_OK;
}

OS_Status OS_TimerDelete(OS_Timer_t *timer)
{
	irqstate_t flags;
	flags = enter_critical_section();

	timer->cb_enable = false;

	work_cancel(LPWORK,&timer->wq_s);

	work_cancel(LPWORK, &timer->wq_s);
	wd_cancel(&timer->handle);
	free(timer);
	//wd_delete(timer->handle);

	leave_critical_section(flags);

	return OS_OK;
}

OS_Status OS_TimerStart(OS_Timer_t *timer)
{
	irqstate_t flags;
	flags = enter_critical_section();

	timer->cb_enable = true;

	wd_start(&timer->handle,timer->time_ms*1000/CONFIG_USEC_PER_TICK,
			nx_timer_callback,1);

	leave_critical_section(flags);

	return OS_OK;
}

OS_Status OS_TimerChangePeriod(OS_Timer_t *timer, OS_Time_t periodMS)
{
	irqstate_t flags;
	flags = enter_critical_section();

	timer->time_ms = periodMS;

	timer->cb_enable = true;

	wd_start(&timer->handle,timer->time_ms*1000/CONFIG_USEC_PER_TICK,
			nx_timer_callback,1);

	leave_critical_section(flags);

	return OS_OK;
}

OS_Status OS_TimerStop(OS_Timer_t *timer)
{
	irqstate_t flags;

	flags = enter_critical_section();

	timer->cb_enable = false;

	//wd_cancel(timer->handle);
	timer->time_ms = MAX_TIME_DELAY;

	wd_start(&timer->handle,timer->time_ms,nx_timer_callback,1);
	work_cancel(LPWORK,&timer->wq_s);

	leave_critical_section(flags);

	return OS_OK;
}

int OS_TimerIsValid(OS_Timer_t *timer)
{
	return 1;
}

int OS_TimerIsActive(OS_Timer_t *timer)
{
	return 1;
}

void *OS_TimerGetContext(void *arg)
{
	return arg;
}

