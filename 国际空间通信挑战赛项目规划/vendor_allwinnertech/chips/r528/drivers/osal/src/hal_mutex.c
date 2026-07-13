#include <stdio.h>
#include <hal_mutex.h>
#include <hal_time.h>
#include <hal_status.h>

int hal_mutex_init(hal_mutex *mutex)
{
	int ret;
	ret = pthread_mutex_init(mutex, NULL);
	if (ret < 0) {
		return HAL_ERROR;
	}
	return HAL_OK;
}

int hal_mutex_detach(hal_mutex *mutex)
{
	pthread_mutex_destroy(mutex);
	return HAL_OK;
}

hal_mutex_t hal_mutex_create(void)
{
	int ret;
	pthread_mutex_t *mutex = malloc(sizeof(pthread_mutex_t));
	if (!mutex)
		return NULL;
	ret = pthread_mutex_init(mutex, NULL);
	if (ret < 0) {
		free(mutex);
		return NULL;
	}
	return mutex;
}

int hal_mutex_delete(hal_mutex_t mutex)
{
	if (mutex == NULL) {
		return HAL_ERROR;
	}
	pthread_mutex_destroy(mutex);
	free(mutex);
	return HAL_OK;
}

int hal_mutex_unlock(hal_mutex_t mutex)
{
	int ret;
	ret = pthread_mutex_unlock(mutex);
	if (ret < 0)
		return HAL_ERROR;
	return HAL_OK;
}

int hal_mutex_timedwait(hal_mutex_t mutex, int ticks)
{
	int ret;
	struct timespec clock_timeout;

	ret = clock_gettime(CLOCK_REALTIME, &clock_timeout);
	if (ret < 0)
	{
		return HAL_ERROR;
	}
	if (ticks)
	{
		hal_time_add_ticks(&clock_timeout, ticks);
	}

	pthread_mutex_timedlock(mutex, &clock_timeout);
	return HAL_OK;
}

int hal_mutex_lock(hal_mutex_t mutex)
{
	int ret;
	ret = pthread_mutex_lock(mutex);
	if (ret < 0)
		return HAL_ERROR;
	return HAL_OK;
}

int hal_mutex_trylock(hal_mutex_t mutex)
{
	int ret;
	ret = pthread_mutex_trylock(mutex);
	if (ret < 0)
		return HAL_ERROR;
	return HAL_OK;
}

