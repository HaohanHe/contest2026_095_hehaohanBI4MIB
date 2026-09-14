#include <hal_mem.h>

void *hal_malloc(uint32_t size)
{
	return memalign(64, size);
}

void hal_free(void *p)
{
	return free(p);
}

void *hal_malloc_align(uint32_t size, int align)
{
    return memalign(align, size);
}

void hal_free_align(void *p)
{
    return free(p);
}

unsigned long hal_virt_to_phys(unsigned long virtaddr)
{
    return virtaddr;
}

unsigned long hal_phys_to_virt(unsigned long phyaddr)
{
    return phyaddr;
}
