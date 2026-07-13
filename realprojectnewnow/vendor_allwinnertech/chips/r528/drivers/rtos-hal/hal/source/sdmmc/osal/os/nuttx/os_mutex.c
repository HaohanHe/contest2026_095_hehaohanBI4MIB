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


#include <os_mutex.h>
#include <stdio.h>
#include <time.h>
#include <debug.h>

//#define OS_MUTEX_DBG(fmt,#arg) printf
#define OS_MUTEX_DBG(fmt,arg...)
#define OS_MUTEX_ERROR _info

OS_Status OS_MutexCreate(OS_Mutex_t *mutex)
{
#if USE_PHTREAD_MUTEX
	pthread_mutexattr_init(&mutex->attr);
	pthread_mutexattr_settype(&mutex->attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&mutex->mutex_hd, &mutex->attr);
	mutex->invalid = 0;
	return OS_OK;
#else
	if(nxsem_init(&mutex->handle,0,1) == OK) {
		mutex->invalid = 0;
		mutex->thread_ower = -1;
		mutex->count = 0;
		//mutex->thread_ower = getpid();
		return OS_OK;
	} else
		return OS_FAIL;
#endif
}

OS_Status OS_MutexDelete(OS_Mutex_t *mutex)
{
#if USE_PHTREAD_MUTEX
	pthread_mutex_destroy(&mutex->mutex_hd);
	pthread_mutexattr_destroy(&mutex->attr);
	mutex->invalid = 1;
	return OS_OK;
#else
	if(nxsem_destroy(&mutex->handle) == OK) {
		mutex->invalid = 1;
		return OS_OK;
	} else
		return OS_FAIL;
#endif
}

OS_Status OS_MutexLock(OS_Mutex_t *mutex, OS_Time_t waitMS)
{

	struct timespec abstime;
	unsigned int timeout_sec;

	if(waitMS == OS_WAIT_FOREVER) {
		waitMS = 0x0fffffffU;
	}

  /* Get the current time */

	(void)clock_gettime(CLOCK_REALTIME, &abstime);

	timeout_sec      = waitMS / 1000;
	abstime.tv_sec  += timeout_sec;
	abstime.tv_nsec += 1000 * 1000 * (waitMS % 1000);

	if (abstime.tv_nsec >= 1000 * 1000 * 1000) {
	    abstime.tv_sec++;
	    abstime.tv_nsec -= 1000 * 1000 * 1000;
	}

#if USE_PHTREAD_MUTEX
	pthread_mutex_timedlock(&mutex->mutex_hd, &abstime);
	mutex->thread_ower = getpid();
	return OS_OK;
#else
	pid_t me = getpid();
	if(mutex->thread_ower == me) {
		mutex->count ++;
		return OS_OK;
	} else {
		if(nxsem_timedwait(&mutex->handle, &abstime) == OK) {
			mutex->thread_ower = me;
			mutex->count ++;
			return OS_OK;
		}
		else
			return OS_FAIL;
	}
#endif
}

OS_Status OS_MutexUnlock(OS_Mutex_t *mutex)
{
#if USE_PHTREAD_MUTEX
	pthread_mutex_unlock(&mutex->mutex_hd);
	return OS_OK;
#else
	if(mutex->count == 1) {
		mutex->thread_ower = -1;
		mutex->count = 0;
		if(nxsem_post(&mutex->handle) == OK) {
			return OS_OK;
		} else {
			return OS_FAIL;
		}
	}else {
		mutex->count--;
		return OS_OK;
	}
#endif
}

OS_Status OS_RecursiveMutexCreate(OS_Mutex_t *mutex)
{
	return OS_MutexCreate(mutex);
}

OS_Status OS_RecursiveMutexDelete(OS_Mutex_t *mutex)
{
	return OS_MutexDelete(mutex);
}

OS_Status OS_RecursiveMutexLock(OS_Mutex_t *mutex, OS_Time_t waitMS)
{
	return OS_MutexLock(mutex, waitMS);
}

OS_Status OS_RecursiveMutexUnlock(OS_Mutex_t *mutex)
{
	return OS_MutexUnlock(mutex);
}

int OS_MutexIsValid(OS_Mutex_t *mutex)
{
	return mutex->invalid;
}

void OS_MutexSetInvalid(OS_Mutex_t *mutex)
{
	mutex->invalid = 1;
}

/**
 * @brief Get the mutex object's owner
 * @note A mutex object's owner is a thread that locks the mutex
 * @param[in] mutex Pointer to the mutex object
 * @return The handle of the thread that locks the mutex object.
 *         NULL when the mutex is not locked by any thread.
 */
pid_t OS_MutexGetOwner(OS_Mutex_t *mutex)
{
	if(mutex != NULL) {
		return mutex->thread_ower;
	}
	OS_MUTEX_ERROR("get muttex ower invalid\n");
	return -1;
}
