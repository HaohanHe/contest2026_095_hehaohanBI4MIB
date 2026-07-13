#include <hal_sem.h>
#include <hal_status.h>

hal_sem_t hal_sem_create(unsigned int cnt)
{
	int ret;
	sem_t *sem = malloc(sizeof(sem_t));
	if (!sem)
		return NULL;
	ret = sem_init(sem, 0, cnt);
	if (ret < 0) {
		free(sem);
		sem = NULL;
	}
	return sem;
}

int hal_sem_delete(hal_sem_t sem)
{
	int ret = sem_destroy(sem);
	if (ret < 0) {
		return HAL_ERROR;
	} else {
		free(sem);
		return HAL_OK;
	}
}

int hal_sem_getvalue(hal_sem_t sem, int *val)
{
	int ret = HAL_OK;
	if (val == NULL) {
		return HAL_INVALID;
	}
	ret = sem_getvalue(sem, val);
	if (ret < 0) {
		return HAL_ERROR;
	}
	return HAL_OK;
}

int hal_sem_post(hal_sem_t sem)
{
	int ret = HAL_OK;
	ret = sem_post(sem);
	if (ret < 0) {
		ret = HAL_ERROR;
	}
	return ret;
}

int hal_sem_timedwait(hal_sem_t sem, unsigned long ticks)
{
	int ret = HAL_OK;
	ret = nxsem_tickwait(sem, ticks);
	if (ret < 0)
		ret = HAL_ERROR;
	return ret;
}

int hal_sem_timedwait_uninterruptible(hal_sem_t sem, unsigned long ticks)
{
	int ret = HAL_OK;
	ret = nxsem_tickwait_uninterruptible(sem, ticks);
	if (ret < 0)
		ret = HAL_ERROR;
	return ret;
}

int hal_sem_trywait(hal_sem_t sem)
{
	int ret = HAL_OK;
	ret = nxsem_tickwait(sem, 0);
	if (ret < 0)
		ret = HAL_ERROR;
	return ret;
}


int hal_sem_wait(hal_sem_t sem)
{
	return sem_wait(sem);
}

int hal_sem_clear(hal_sem_t sem)
{
	nxsem_reset(sem, 0);
	return HAL_OK;
}

