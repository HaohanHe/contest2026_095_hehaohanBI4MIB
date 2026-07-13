#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <hal_queue.h>
#include <hal_status.h>
#include <hal_time.h>

#include <nuttx/mqueue.h>
#include <debug.h>

hal_queue_t hal_queue_create(const char *name, unsigned int item_size, unsigned int queue_size)
{
	struct mq_attr attr;
	struct hal_queue *mq_adpt;
	int ret;

	mq_adpt = malloc(sizeof(struct hal_queue));
	if (!mq_adpt)
		return NULL;

	snprintf(mq_adpt->name, sizeof(mq_adpt->name), "/tmp/%p", mq_adpt);

	attr.mq_maxmsg  = queue_size;
	attr.mq_msgsize = item_size;
	attr.mq_curmsgs = 0;
	attr.mq_flags   = 0;

	ret = file_mq_open(&mq_adpt->mq, mq_adpt->name,
                          O_RDWR | O_CREAT, 0644, &attr);

	if (ret < 0)
	{
		free(mq_adpt);
		return NULL;
	}

	mq_adpt->msgsize = item_size;
	return (void *)mq_adpt;
}

int hal_queue_delete(hal_queue_t queue)
{
	struct hal_queue *mq_adpt = (struct hal_queue *)queue;

	file_mq_close(&mq_adpt->mq);
	file_mq_unlink(mq_adpt->name);
	free(mq_adpt);
	return HAL_OK;
}

int hal_queue_send_wait(hal_queue_t queue, void *item, int timeout)
{
	int ret;
	struct timespec clock_timeout;
	struct hal_queue *mq_adpt = (struct hal_queue *)queue;

	if (timeout == HAL_WAIT_FOREVER)
	{
		ret = file_mq_send(&mq_adpt->mq, (const char *)item, mq_adpt->msgsize, 0);
		if (ret < 0)
		{
			wlerr("Failed to send message to mqueue error=%d\n", ret);
		}
    }
	else
    {
		ret = clock_gettime(CLOCK_REALTIME, &clock_timeout);
		if (ret < 0)
		{
			wlerr("Failed to get time %d\n", ret);
			return HAL_ERROR;
		}

		if (timeout)
		{
			hal_time_add_ticks(&clock_timeout, timeout);
		}

		ret = file_mq_timedsend(&mq_adpt->mq, (const char *)item,
				mq_adpt->msgsize, 0, &clock_timeout);
		if (ret < 0)
		{
			wlerr("Failed to timedsend message to mqueue error=%d\n", ret);
		}
    }

	if (ret < 0) {
		return HAL_ERROR;
	}
	return HAL_OK;
}

int hal_queue_send(hal_queue_t queue, void *buffer)
{
	return hal_queue_send_wait(queue, buffer, 0);
}

int hal_queue_recv(hal_queue_t queue, void *item, int timeout)
{
	ssize_t ret;
	struct timespec clock_timeout;
	unsigned int prio = 0;
	struct hal_queue *mq_adpt = (struct hal_queue *)queue;

	if (timeout == HAL_WAIT_FOREVER)
	{
		ret = file_mq_receive(&mq_adpt->mq, (char *)item,
			mq_adpt->msgsize, &prio);

		if (ret < 0)
		{
			wlerr("Failed to receive from mqueue error=%d\n", ret);
		}
	}
	else
	{
		ret = clock_gettime(CLOCK_REALTIME, &clock_timeout);

		if (ret < 0)
		{
			wlerr("Failed to get time %d\n", ret);
			return HAL_ERROR;
		}

		if (timeout)
		{
			hal_time_add_ticks(&clock_timeout, MSEC2TICK(timeout));
		}

		ret = file_mq_timedreceive(&mq_adpt->mq, (char *)item,
				mq_adpt->msgsize, &prio, &clock_timeout);

		if (ret < 0)
		{
			syslog(LOG_DEBUG, "Failed to timedreceive from mqueue error=%d\n", ret);
		}
	}

	return ret > 0 ? HAL_OK : HAL_ERROR;
}

int hal_queue_is_empty(hal_queue_t queue)
{
	int ret;
	struct mq_attr mq_stat;
	ret = file_mq_getattr(&queue->mq, &mq_stat);
	if (ret == 0) {
		if (mq_stat.mq_curmsgs == 0)
			return 1;
		return 0;
	}
	return -1;
}

hal_mailbox_t hal_mailbox_create(const char *name, unsigned int size)
{
	return (hal_mailbox_t)hal_queue_create(name, sizeof(unsigned int), size);
}

int hal_mailbox_delete(hal_mailbox_t mailbox)
{
	return hal_queue_delete((hal_queue_t)mailbox);
}

int hal_mailbox_send_wait(hal_mailbox_t mailbox, unsigned int value, int timeout)
{
	return hal_queue_send_wait((hal_queue_t)mailbox, &value, timeout);
}

int hal_mailbox_send(hal_mailbox_t mailbox, unsigned int value)
{
	return hal_mailbox_send_wait(mailbox, value, 0);
}

int hal_mailbox_recv(hal_mailbox_t mailbox, unsigned int *value, int timeout)
{
	return hal_queue_recv((hal_queue_t)mailbox, value, timeout);
}

int hal_mailbox_is_empty(hal_mailbox_t mailbox)
{
	return hal_queue_is_empty((hal_queue_t)mailbox);
}

