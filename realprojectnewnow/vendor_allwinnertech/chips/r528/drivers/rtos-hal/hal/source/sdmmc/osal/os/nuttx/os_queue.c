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


#include <fcntl.h>
#include <time.h>
#include <stdio.h>
#include <os_queue.h>
//#define OS_QUE_DBG printf
#define OS_QUE_DBG(fmt,arg...)

#define QUE_PRI 1

int OS_QueueIsValid(OS_Queue_t *queue_hd)
{
	OS_QUE_DBG("[%s,%d] -+-+-+\n",__func__,__LINE__);
	if(queue_hd->queue == 0)
		return 0;
	else
		return 1;
	//TODO check
}

void OS_QueueSetInvalid(OS_Queue_t *queue_hd)
{
	OS_QUE_DBG("[%s,%d] -+-+-%p-%s\n",__func__,__LINE__,queue_hd->queue,queue_hd->mqname);
	queue_hd->queue = 0;
}

//int aos_queue_new(aos_queue_t *queue, void *buf, unsigned int size, int max_msg);
OS_Status OS_QueueCreate(OS_Queue_t *queue_hd, uint32_t queueLen, uint32_t itemSize)
{
	struct mq_attr attr;
	char mqname[16];
	/* Create a message queue for the worker thread */
	snprintf(queue_hd->mqname, sizeof(mqname), "/tmp/%0lx", (unsigned long)queue_hd);
	attr.mq_maxmsg  = queueLen;
	attr.mq_msgsize = itemSize;
	attr.mq_curmsgs = 0;
	attr.mq_flags   = 0;
	queue_hd->queue = mq_open(queue_hd->mqname, O_RDWR | O_CREAT, 0644, &attr);

	queue_hd->itemsize = itemSize;

	OS_QUE_DBG("[%s,%d] -+-+-%p-%s\n",__func__,__LINE__,queue_hd->queue,queue_hd->mqname);
	if(queue_hd->queue == 0){
		return OS_FAIL;
    }
	return OS_OK;
}

OS_Status OS_QueueDelete(OS_Queue_t *queue_hd)
{
	OS_QUE_DBG("[%s,%d] -+-+-%p-%s\n",__func__,__LINE__,queue_hd->queue,queue_hd->mqname);
//	mq_close(queue_hd->queue);
	mq_unlink(queue_hd->mqname);
	queue_hd->queue = 0;
	return OS_OK;
}

OS_Status OS_QueueSend(OS_Queue_t *queue_hd, const void *msg,OS_Time_t waitMS)
{
	OS_QUE_DBG("[%s,%d] -+-+-%d-%s timeout:%d\n",__func__,__LINE__,queue_hd->queue,queue_hd->mqname,waitMS);
#if 0
	if(nxmq_send(queue_hd->queue,&msg,sizeof(void*),QUE_PRI) == OK)
#else
	struct timespec abstime;
	unsigned int timeout_sec;

	/* Get the current time */

	(void)clock_gettime(CLOCK_REALTIME, &abstime);

	timeout_sec      = waitMS / 1000;
	abstime.tv_sec  += timeout_sec;
	abstime.tv_nsec += 1000 * 1000 * (waitMS % 1000);

	if (abstime.tv_nsec >= 1000 * 1000 * 1000) {
		abstime.tv_sec++;
		abstime.tv_nsec -= 1000 * 1000 * 1000;
	}

	if(nxmq_timedsend(queue_hd->queue,(const char *)msg,queue_hd->itemsize,QUE_PRI,
				&abstime) == OK)
#endif
		return OS_OK;
	else
		return OS_FAIL;
}

OS_Status OS_QueueReceive(OS_Queue_t *queue_hd, void *msg,OS_Time_t waitMS)
{
	ssize_t msgsize;
	unsigned int priority;
	//TODO:
	if(waitMS == 0xffffffffU)
		waitMS = 0x0fffffffU;
	OS_QUE_DBG("[%s,%d] -+-+-%p-%s timeout:%d\n",__func__,__LINE__,queue_hd->queue,queue_hd->mqname,waitMS);
#if 0
	 msgsize = nxmq_receive(queue_hd->queue,msg,queue_hd->itemsize,&priority);
#else
	struct timespec abstime;
	unsigned int timeout_sec;

	/* Get the current time */

	(void)clock_gettime(CLOCK_REALTIME, &abstime);

	timeout_sec      = waitMS / 1000;
	abstime.tv_sec  += timeout_sec;
	abstime.tv_nsec += 1000 * 1000 * (waitMS % 1000);

	if (abstime.tv_nsec >= 1000 * 1000 * 1000) {
		abstime.tv_sec++;
		abstime.tv_nsec -= 1000 * 1000 * 1000;
	}
	msgsize = nxmq_timedreceive(queue_hd->queue,msg,queue_hd->itemsize,
			&priority, &abstime);
#endif
	if(msgsize < 0) {
		return OS_FAIL;
	}
	return OS_OK;

}

OS_Status OS_MsgQueueCreate(OS_Queue_t *queue, uint32_t queueLen)
{
	return OS_QueueCreate(queue, queueLen, sizeof(void*));
}

OS_Status OS_MsgQueueDelete(OS_Queue_t *queue)
{
	return OS_QueueDelete(queue);
}

OS_Status OS_MsgQueueSend(OS_Queue_t *queue, void *msg, OS_Time_t waitMS)
{
	return OS_QueueSend(queue, &msg, waitMS);
}

OS_Status OS_MsgQueueReceive(OS_Queue_t *queue, void **msg, OS_Time_t waitMS)
{
	return OS_QueueReceive(queue, msg, waitMS);
}
