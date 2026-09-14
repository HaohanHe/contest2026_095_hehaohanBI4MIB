#include <hal_cache.h>

#ifdef CONFIG_CACHE_ALIGN_CHECK
#include <sunxi_hal_common.h>

#ifdef CONFIG_DEBUG_BACKTRACE
#include <backtrace.h>
#include <assert.h>
#define CACHELINE_CHECK(option) \
{ \
	if (!option) { \
		printf("[%s] cacheline check failed\n", __func__); \
		backtrace(NULL, NULL, 0, 0, printf); \
		assert(0); \
	} \
} while (0)

#else
#define CACHELINE_CHECK(option) \
{ \
	assert(option); \
} while (0)

#endif /* CONFIG_DEBUG_BACKTRACE */
#endif /* CONFIG_CACHE_ALIGN_CHECK */


void hal_dcache_clean(unsigned long vaddr_start, unsigned long size)
{
#ifdef CONFIG_ARCH_HAVE_DCACHE
#ifdef CONFIG_CACHE_ALIGN_CHECK
	CACHELINE_CHECK(!(vaddr_start & (CACHELINE_LEN - 1)));
#endif
	up_clean_dcache(vaddr_start, vaddr_start + size);
#endif
}

void hal_dcache_invalidate(unsigned long vaddr_start, unsigned long size)
{
#ifdef CONFIG_ARCH_HAVE_DCACHE
#ifdef CONFIG_CACHE_ALIGN_CHECK
	CACHELINE_CHECK(!(vaddr_start & (CACHELINE_LEN - 1)));
#endif
	up_invalidate_dcache(vaddr_start, vaddr_start + size);
#endif
}

void hal_dcache_clean_invalidate(unsigned long vaddr_start, unsigned long size)
{
#ifdef CONFIG_ARCH_HAVE_DCACHE
#ifdef CONFIG_CACHE_ALIGN_CHECK
	CACHELINE_CHECK(!(vaddr_start & (CACHELINE_LEN - 1)));
#endif
	up_flush_dcache(vaddr_start, vaddr_start + size);
#endif
}

void hal_icache_invalidate_all(void)
{
#ifdef CONFIG_ARCH_HAVE_ICACHE
    up_invalidate_icache_all();
#endif
}

void hal_dcache_invalidate_all(void)
{
#ifdef CONFIG_ARCH_HAVE_DCACHE
    up_invalidate_dcache_all();
#endif
}

void hal_dcache_clean_all(void)
{
#ifdef CONFIG_ARCH_HAVE_DCACHE
	up_clean_dcache_all();
#endif
}

void hal_icache_invalidate(unsigned long vaddr_start, unsigned long size)
{
#ifdef CONFIG_ARCH_HAVE_ICACHE
    up_invalidate_icache(vaddr_start, vaddr_start + size);
#endif
}
