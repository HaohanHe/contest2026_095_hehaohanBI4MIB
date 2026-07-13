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
#ifndef _KERNEL_OS_NUTTX_OS_QUEUE_H_
#define _KERNEL_OS_NUTTX_OS_QUEUE_H_
#include "_os_common.h"
#include <nuttx/mqueue.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct OS_Queue {
	char mqname[16];
	mqd_t queue;
	uint32_t itemsize;
} OS_Queue_t;

int OS_QueueIsValid(OS_Queue_t *queue_hd);

void OS_QueueSetInvalid(OS_Queue_t *queue_hd);

OS_Status OS_QueueCreate(OS_Queue_t *queue_hd, uint32_t queueLen, uint32_t itemSize);

OS_Status OS_QueueDelete(OS_Queue_t *queue_hd);

OS_Status OS_QueueSend(OS_Queue_t *queue_hd, const void *msg,OS_Time_t waitMS);

OS_Status OS_QueueReceive(OS_Queue_t *queue_hd, void *msg,OS_Time_t waitMS);

OS_Status OS_MsgQueueCreate(OS_Queue_t *queue, uint32_t queueLen);

OS_Status OS_MsgQueueDelete(OS_Queue_t *queue);

OS_Status OS_MsgQueueSend(OS_Queue_t *queue, void *msg, OS_Time_t waitMS);

OS_Status OS_MsgQueueReceive(OS_Queue_t *queue, void **msg, OS_Time_t waitMS);

#ifdef __cplusplus
}
#endif

#endif /* _KERNEL_OS_NUTTX_OS_QUEUE_H_ */
