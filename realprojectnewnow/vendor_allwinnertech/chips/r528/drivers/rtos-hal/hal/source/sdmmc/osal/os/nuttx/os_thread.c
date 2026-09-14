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


#include <nuttx/kthread.h>
#include <nuttx/irq.h>
#include <nuttx/spinlock.h>
#include <os_thread.h>
#include <stdlib.h>
#include <stdio.h>
#include <debug.h>
//#define OS_TH_DBG printf
#define OS_TH_DBG(fmt,arg...)
#define OS_TH_ERR _info

#define PRIORIY_BASE  200

#if USE_POSIX_PTHREAD
OS_Status OS_ThreadCreate(OS_Thread_t *thread, const char *name,
										OS_ThreadEntry_t entry, void *arg,
										OS_Priority priority, uint32_t stackSize)
{
	pthread_attr_t thread_attr;
	pthread_attr_init(&thread_attr);

	if(pthread_attr_setstacksize(&thread_attr,stackSize) != 0) {
		OS_TH_ERR("set task stack attr failed.\n");
		return OS_FAIL;
	}

	if(pthread_create(&thread->thread_hd,&thread_attr,entry,arg) != 0) {
		OS_TH_ERR("create task failed.\n");
		return OS_FAIL;
	}
	if(pthread_attr_destroy(&thread_attr) != 0) {
		OS_TH_ERR("task attr destroy failed.\n");
	}
	thread->invaild = 0;
	return OS_OK;
}
OS_Status OS_ThreadDelete(OS_Thread_t *thread)
{
	if(pthread_join(thread->thread_hd,NULL) != 0) {
		OS_TH_ERR("pthread join failed.\n");
		return OS_FAIL;
	}
	thread->invaild = 1;
}
int OS_ThreadIsValid(OS_Thread_t *thread)
{
	return thread->invaild;
}
void OS_ThreadSetInvalid(OS_Thread_t *thread)
{
	thread->invaild = 1;
}
#else
//extern uint8_t g_sched_lock[RHINO_CONFIG_CPU_NUM];
static int kthread_entry_wrapper(int argc,FAR char *argv[])
{
	OS_Thread_t *handle;
	handle = (OS_Thread_t *)atoi(argv[1]);
	handle->entry(handle->data);
	return 0;
}

OS_Status OS_ThreadCreate(OS_Thread_t *thread, const char *name,
										OS_ThreadEntry_t entry, void *arg,
										OS_Priority priority, uint32_t stackSize)
{
	irqstate_t flags;
	char str[8];
	FAR char *argv[2];
	int p;
	OS_TH_DBG("[%s,%d] -+-+%p-+-+:%d\n",__func__,__LINE__,thread,thread->handle);
	flags = enter_critical_section();

	thread->data = arg;
	thread->entry = entry;

	OS_TH_DBG("[%s,%d] -+-+%p-+-+:%d\n",__func__,__LINE__,thread,thread->handle);
	p = (int)thread;

	(void)itoa(p, str, 10);

	OS_TH_DBG("[%s,%d] -+-+%p-+-+:%d\n",__func__,__LINE__,thread,thread->handle);
	argv[0] = str;
	argv[1] = NULL;
	thread->handle = kthread_create(name,priority+PRIORIY_BASE,stackSize,kthread_entry_wrapper,argv);
	OS_TH_DBG("[%s,%d] -+-+%p-+-+:%d\n",__func__,__LINE__,thread,thread->handle);
	if(thread->handle < 0) {
		leave_critical_section(flags);
		return OS_FAIL;
	} else {
		leave_critical_section(flags);
		return OS_OK;
	}
}

OS_Status OS_ThreadDelete(OS_Thread_t *thread)
{
	OS_TH_DBG("[%s,%d] -+-=\n",__func__,__LINE__);
	thread->handle = -1;
	return OS_OK;
}
int OS_ThreadIsValid(OS_Thread_t *thread)
{
	OS_TH_DBG("[%s,%d] -+-=\n",__func__,__LINE__);
	if(thread->handle < 0)
		return 0;
	else
		return 1;
}

void OS_ThreadSetInvalid(OS_Thread_t *thread)
{
	OS_TH_DBG("[%s,%d] -+-=\n",__func__,__LINE__);
	thread->handle = -1;
}

#endif
void OS_ThreadSuspendScheduler(void)
{
	OS_TH_DBG("[%s,%d] -+-=\n",__func__,__LINE__);
	sched_lock();
}

void OS_ThreadResumeScheduler(void)
{
	OS_TH_DBG("[%s,%d] -+-=\n",__func__,__LINE__);
	sched_unlock();
}


int OS_ThreadIsSchedulerRunning(void)
{
	OS_TH_DBG("[%s,%d] -+-=\n",__func__,__LINE__);
	//irqstate_t flags;
	//flags = enter_critical_section();
	if(sched_lockcount() == 0) {
		//leave_critical_section(flags);
		return 1;
	}else {
		//leave_critical_section(flags);
		return 0;
	}
}


void OS_ThreadSleep(OS_Time_t msec)
{
	//OS_TH_DBG("[%s,%d] -+-=\n",__func__,__LINE__);
	usleep(1000*msec);
}

void OS_ThreadYield(void)
{
	//OS_TH_DBG("[%s,%d] -+-=\n",__func__,__LINE__);
	sched_yield();
}

OS_ThreadHandle_t OS_ThreadGetCurrentHandle(void)
{
	//printf("[%s,%d] ======%d\n",__func__,__LINE__,getpid());
	return getpid();
}
