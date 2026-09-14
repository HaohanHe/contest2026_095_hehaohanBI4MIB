#include <debug.h>
#include <hal_thread.h>
#include <hal_interrupt.h>
#include <hal_status.h>
#include <nuttx/kthread.h>

typedef void (*hal_thread_fn_t)(void *data);

typedef struct kthread_arg {
	hal_thread_fn_t entry;
	void *data;
	char *argv[2];
} kthread_arg_t;

static int kthread_entry_wrapper(int argc, FAR char *argv[])
{
	kthread_arg_t *arg;

	if (!argv)
		return -1;
	arg = (kthread_arg_t *)atoi(argv[1]);
	if (!arg)
		return -1;
	arg->entry(arg->data);
	free(arg->argv[0]);
	free(arg);
	return 0;
}

void *hal_thread_create(void (*entry)(void *data), void *data, const char *name, int stack_size, int priority)
{
	pid_t pid;
	kthread_arg_t *arg;
	char *pointer_str;
	int len = 16;

	if (!entry)
		return NULL;

	arg = zalloc(sizeof(kthread_arg_t));
	if (!arg)
		return NULL;
	pointer_str = zalloc(len);
	if (!pointer_str) {
		free(arg);
		return NULL;
	}
	arg->entry = entry;
	arg->data = data;
	snprintf(pointer_str, len, "%u", (uintptr_t)arg);
	arg->argv[0] = pointer_str;
	arg->argv[1] = NULL;

	pid = kthread_create(name, priority, stack_size, kthread_entry_wrapper, arg->argv);

	if (pid < 0) {
		free(pointer_str);
		free(arg);
		return NULL;
	}
	return (void *)pid;
}

int hal_thread_stop(void *thread)
{
	pid_t pid;
	pid  = (pid_t)thread;
	if (thread != NULL) {
		kthread_delete(pid);
	} else {
		/*  kthread_delete(getpid()); */
	}
	return HAL_OK;
}

void *hal_thread_self(void)
{
    return (void *)getpid();
}

int hal_thread_start(void *thread)
{
    return HAL_OK;
}

int hal_thread_resume(void *task)
{
	_info("%s not support!\r\n", __func__);
    return HAL_OK;
}

int hal_thread_suspend(void *task)
{
	_info("%s not support!\r\n", __func__);
    return HAL_OK;
}

int hal_thread_scheduler_is_running(void)
{
	if (sched_lockcount() == 0)
		return 1;
	return 0;
}

int hal_thread_is_in_critical_context(void)
{
    if (hal_interrupt_get_nest() == 0
			&& hal_thread_scheduler_is_running()
			&& !hal_interrupt_is_disable())
    {
        return 0;
    }
    return 1;
}

char *hal_thread_get_name(void *thread)
{
	_info("%s not support!\r\n", __func__);
    return NULL;;
}

int hal_thread_scheduler_suspend(void)
{
    sched_lock();
    return HAL_OK;
}

int hal_thread_scheduler_resume(void)
{
	sched_unlock();
    return HAL_OK;
}
