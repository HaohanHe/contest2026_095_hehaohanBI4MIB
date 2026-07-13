#ifndef SUNXI_HAL_MAILBOX_H
#define SUNXI_HAL_MAILBOX_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>
#include <stdint.h>

#ifdef CONFIG_KERNEL_FREERTOS
#include <FreeRTOS.h>
#include <queue.h>
typedef QueueHandle_t hal_mailbox_t;
typedef QueueHandle_t hal_queue_t;
#elif defined(CONFIG_RTTKERNEL)

#include <rtthread.h>
#include <workqueue.h>
#include <waitqueue.h>
typedef rt_mailbox_t hal_mailbox_t;
typedef rt_mq_t hal_queue_t;
typedef struct rt_workqueue hal_workqueue;
typedef struct rt_work hal_work;
typedef rt_wqueue_t hal_wqueue_t;

#define hal_work_init rt_work_init
hal_workqueue *hal_workqueue_create(const char *name, unsigned short stack_size, unsigned char priority);
int hal_workqueue_dowork(hal_workqueue *queue, hal_work *work);

void hal_wqueue_init(hal_wqueue_t *queue);
int  hal_wqueue_wait(hal_wqueue_t *queue, int condition, int timeout);
void hal_wqueue_wakeup(hal_wqueue_t *queue, void *key);

#elif defined(CONFIG_OS_NUTTX)

#include <mqueue.h>

#define MSG_QUEUE_NAME_SIZE    16

struct hal_queue
{
  struct file mq;                        /* Message queue handle */
  uint32_t    msgsize;                   /* Message size */
  char        name[MSG_QUEUE_NAME_SIZE]; /* Message queue name */
};

typedef struct hal_queue* hal_queue_t;
typedef struct hal_queue* hal_mailbox_t;
#endif

enum IPC_MAILBOX_CMD
{
    IPC_MAILBOX_CMD_GET_STATE,
    IPC_MAILBOX_CMD_NUMS,
};


hal_mailbox_t hal_mailbox_create(const char *name, unsigned int size);
int hal_mailbox_delete(hal_mailbox_t mailbox);
int hal_mailbox_send(hal_mailbox_t mailbox, unsigned int value);
int hal_mailbox_send_wait(hal_mailbox_t mailbox, unsigned int value, int timeout);
int hal_mailbox_recv(hal_mailbox_t mailbox, unsigned int *value, int timeout);
int hal_is_mailbox_empty(hal_mailbox_t mailbox);

hal_queue_t hal_queue_create(const char *name, unsigned int item_size, unsigned int queue_size);
int hal_queue_delete(hal_queue_t queue);
int hal_queue_send(hal_queue_t queue, void *buffer);
int hal_queue_send_wait(hal_queue_t queue, void *buffer, int timeout);
int hal_queue_recv(hal_queue_t queue, void *buffer, int timeout);
int hal_queue_is_empty(hal_queue_t queue);
int hal_queue_len(hal_queue_t queue);

#ifdef __cplusplus
}
#endif
#endif
