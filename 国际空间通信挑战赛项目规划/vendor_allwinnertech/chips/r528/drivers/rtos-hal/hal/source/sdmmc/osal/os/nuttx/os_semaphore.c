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


#include <time.h>
#include <nuttx/semaphore.h>
#include <stdio.h>
#include <os_semaphore.h>
//#define OS_SEM_DBG printf
#define OS_SEM_DBG(fmt,arg...)

OS_Status OS_SemaphoreCreate(OS_Semaphore_t *sem, uint32_t initCount, uint32_t maxCount)
{
	OS_SEM_DBG("[%s,%d] -+-+-+-+\n",__func__,__LINE__);
	if(nxsem_init(&sem->handle,0,initCount) == OK) {
		sem->invalid = 0;
		return OS_OK;
	}else
		return OS_FAIL;
}

OS_Status OS_SemaphoreCreateBinary(OS_Semaphore_t *sem)
{
	OS_SEM_DBG("[%s,%d] -+-+-+-+\n",__func__,__LINE__);
	if(nxsem_init(&sem->handle,0,0) == OK) {
		sem->invalid = 0;
		return OS_OK;
	} else
		return OS_FAIL;
}

OS_Status OS_SemaphoreDelete(OS_Semaphore_t *sem)
{
	OS_SEM_DBG("[%s,%d] -+-+-+-+\n",__func__,__LINE__);
	if(nxsem_destroy(&sem->handle) == OK) {
		sem->invalid = 1;
		return OS_OK;
	} else
		return OS_FAIL;
}

OS_Status OS_SemaphoreWait(OS_Semaphore_t *sem, OS_Time_t waitMS)//xradio irq call,can not in xip
{
	//OS_SEM_DBG("[%s,%d] -+-+-+-+\n",__func__,__LINE__);
#if 1
	struct timespec abstime;
	unsigned int timeout_sec;

	if(waitMS == OS_WAIT_FOREVER) {
		waitMS = 0x0fffffffU;
	}

  /* Get the current time */

	(void)clock_gettime(CLOCK_MONOTONIC, &abstime);

	timeout_sec      = waitMS / 1000;
	abstime.tv_sec  += timeout_sec;
	abstime.tv_nsec += 1000 * 1000 * (waitMS % 1000);

	if (abstime.tv_nsec >= 1000 * 1000 * 1000) {
	    abstime.tv_sec++;
	    abstime.tv_nsec -= 1000 * 1000 * 1000;
	}

	if(nxsem_clockwait(&sem->handle,CLOCK_MONOTONIC, &abstime) == OK)
		return OS_OK;
#else
	if(nxsem_wait(&sem->handle) == OK)
		return OS_OK;
#endif
	else
		return OS_FAIL;
}

OS_Status OS_SemaphoreRelease(OS_Semaphore_t *sem)//xradio irq, can not xin xip
{
	//OS_SEM_DBG("[%s,%d] -+-+-+-+\n",__func__,__LINE__);
	if(nxsem_post(&sem->handle) == OK) {
		return OS_OK;
	} else {
		return OS_FAIL;
	}
}

/**
 * @brief Release the semaphore object
 * @param[in] sem Pointer to the semaphore object
 * @retval OS_Status, OS_OK on success
 */
OS_Status OS_SemaphoreReset(OS_Semaphore_t *sem)
{
	nxsem_reset(&sem->handle, 0);
	return OS_OK;
}

int OS_SemaphoreIsValid(OS_Semaphore_t *sem)
{
	//OS_SEM_DBG("[%s,%d] -+-+-+-+\n",__func__,__LINE__);
	return sem->invalid;
}

void OS_SemaphoreSetInvalid(OS_Semaphore_t *sem)
{
	//OS_SEM_DBG("[%s,%d] -+-+-+-+\n",__func__,__LINE__);
	sem->invalid = 1;
}
