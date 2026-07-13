#include <string.h>
#include <hal_atomic.h>
#include <hal_status.h>

unsigned long hal_spin_lock_irqsave(hal_spinlock_t *lock)
{
	unsigned long cpu_sr = spin_lock_irqsave(lock);

	return cpu_sr;
}

int hal_spin_lock_init(hal_spinlock_t *lock)
{
    memset(lock, 0, sizeof(hal_spinlock_t));
    return HAL_OK;
}

int hal_spin_lock_deinit(hal_spinlock_t *lock)
{
    return HAL_OK;
}

void hal_spin_unlock_irqrestore(hal_spinlock_t *lock, unsigned long __cpsr)
{
	spin_unlock_irqrestore(lock, __cpsr);
}

void hal_spin_lock(hal_spinlock_t *lock)
{
#ifdef CONFIG_SMP
	spin_lock(lock);
#endif
}

void hal_spin_unlock(hal_spinlock_t *lock)
{
#ifdef CONFIG_SMP
	spin_unlock(lock);
#endif
}
