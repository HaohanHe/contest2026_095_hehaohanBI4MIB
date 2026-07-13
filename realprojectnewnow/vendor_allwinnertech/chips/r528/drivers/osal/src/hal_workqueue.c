#include <stdlib.h>
#include <hal_workqueue.h>
#include <hal_thread.h>
#include <hal_time.h>
#include <hal_status.h>
#include <sunxi_hal_common.h>
#include <nuttx/wqueue.h>

static void work_entry(void *args)
{
    hal_work *work = (hal_work *)args;
    work->work_func(work, work->work_data);
}

void hal_work_init(hal_work *work, void (*work_func)(hal_work *work, void *work_data), void *work_data)
{
    memset(work, 0, sizeof(hal_work));
    work->work_func = work_func;
    work->work_data = work_data;
    work->workqueue = NULL;
}

hal_workqueue *hal_workqueue_create(const char *name, uint16_t stack_size, uint8_t priority)
{
    hal_workqueue *queue = malloc(sizeof(hal_workqueue));
    if (queue) {
        queue->pid = HPWORK;
    }
    return queue;
}

int hal_workqueue_destroy(hal_workqueue *queue)
{
    free(queue);
    return HAL_OK;
}

int hal_workqueue_dowork(hal_workqueue *queue, hal_work *work)
{
    work_queue(queue->pid, &work->wq_s, (worker_t)work_entry, work, 0);
    return HAL_OK;
}

int hal_workqueue_submit_work(hal_workqueue *queue, hal_work *work, hal_tick_t time)
{
    work_queue(queue->pid, &work->wq_s, (worker_t)work_entry, work, 0);
    return HAL_OK;
}

int hal_workqueue_cancel_work(hal_workqueue *queue, hal_work *work)
{
    work_cancel(queue->pid, &work->wq_s);
    return HAL_OK;
}

int hal_workqueue_cancel_work_sync(hal_workqueue *queue, hal_work *work)
{
    work_cancel(queue->pid, &work->wq_s);
    return HAL_OK;
}

int hal_workqueue_cancel_all_work(hal_workqueue *queue)
{
    return HAL_OK;
}

void hal_delayed_work_init(hal_delayed_work *work, void (*work_func)(hal_work *work, void *work_data), void *work_data)
{
    hal_work_init(&work->work, work_func, work_data);
}
