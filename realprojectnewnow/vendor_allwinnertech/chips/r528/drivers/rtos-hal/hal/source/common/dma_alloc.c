/*
* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
*
* Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
* the the people's Republic of China and other countries.
* All Allwinner Technology Co.,Ltd. trademarks are used with permission.
*
* DISCLAIMER
* THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
* IF YOU NEED TO INTEGRATE THIRD PARTY¡¯S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
* IN ALLWINNERS¡¯SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
* ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
* ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
* COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
* YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY¡¯S TECHNOLOGY.
*
*
* THIS SOFTWARE IS PROVIDED BY ALLWINNER"AS IS" AND TO THE MAXIMUM EXTENT
* PERMITTED BY LAW, ALLWINNER EXPRESSLY DISCLAIMS ALL WARRANTIES OF ANY KIND,
* WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING WITHOUT LIMITATION REGARDING
* THE TITLE, NON-INFRINGEMENT, ACCURACY, CONDITION, COMPLETENESS, PERFORMANCE
* OR MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
* IN NO EVENT SHALL ALLWINNER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
* NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS, OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
* OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <debug.h>
#include <hal_mem.h>
#include <hal_atomic.h>
#include <aw_list.h>
#include "sunxi_hal_common.h"

#ifdef CONFIG_OS_MELIS
#ifndef CONFIG_DMA_VMAREA_START_ADDRESS
#define CONFIG_DMA_VMAREA_START_ADDRESS (0x80000000)
#endif

#define PAGE_SIZE (4096)

struct vm_struct
{
    void *page;
    int size;
    unsigned long prot;
    struct list_head i_list;
};

void unmap_vmap_area(unsigned long vmaddr, unsigned long free_sz, void *page);
int map_vm_area(unsigned long size, unsigned long prot, void *pages[], unsigned long target_vmaddr);

static LIST_HEAD(g_vm_struct_list);
static hal_spinlock_t hal_malloc_coherent_lock;
#endif

void hal_free_coherent(void *addr)
{
    free(addr);
}

void *hal_malloc_coherent(size_t size)
{
    return memalign(CACHELINE_LEN, ALIGN_UP(size, CACHELINE_LEN));
}

void hal_free_coherent_align(void *addr)
{
    free(addr);
}

void *hal_malloc_coherent_align(size_t size, int align)
{
    align = ALIGN_UP(align, CACHELINE_LEN);
    size = ALIGN_UP(size, CACHELINE_LEN);
    return memalign(align, size);
}

#ifdef CONFIG_OS_MELIS

void hal_malloc_coherent_init(void)
{
    hal_spin_lock_init(&hal_malloc_coherent_lock);
}

void hal_free_coherent_prot(void *addr)
{
    struct vm_struct *tmp;
    struct list_head *pos;
    struct list_head *q;
    char *malloc_ptr;
    unsigned long cpsr;

    if (!addr)
    {
        return;
    }

    malloc_ptr = (char *)addr - CONFIG_DMA_VMAREA_START_ADDRESS;
    hal_free_align(malloc_ptr);

    cpsr = hal_spin_lock_irqsave(&hal_malloc_coherent_lock);
    list_for_each_safe(pos, q, &g_vm_struct_list)
    {
        tmp = list_entry(pos, struct vm_struct, i_list);
        if (addr == tmp->page)
        {
            list_del(pos);
            hal_spin_unlock_irqrestore(&hal_malloc_coherent_lock, cpsr);
            unmap_vmap_area((unsigned long)addr, tmp->size, tmp->page);
            hal_free(tmp);
            return;
        }
    }
    hal_spin_unlock_irqrestore(&hal_malloc_coherent_lock, cpsr);
}

void *hal_malloc_coherent_prot(size_t size, unsigned long prot)
{
    char *malloc_ptr = NULL;
    unsigned long *pages = NULL;
    struct vm_struct *area;
    int page_num = 0;
    uint32_t cpsr;
    int i;

    size = ALIGN_UP(size, PAGE_SIZE);
    page_num = size / PAGE_SIZE;

    area = hal_malloc(sizeof(struct vm_struct));
    if (!area)
    {
        return NULL;
    }

    pages = hal_malloc(sizeof(void *) * page_num);
    if (!pages)
    {
        hal_free(area);
        return NULL;
    }

    malloc_ptr = hal_malloc_align(size, PAGE_SIZE);
    if (!malloc_ptr)
    {
        hal_free(pages);
        hal_free(area);
        return NULL;
    }

    for (i = 0; i < page_num; i++)
    {
        pages[i] = __va_to_pa((unsigned long)malloc_ptr + i * PAGE_SIZE);
    }

    area->page = malloc_ptr + CONFIG_DMA_VMAREA_START_ADDRESS;
    area->size = size;
    area->prot = prot;

    cpsr = hal_spin_lock_irqsave(&hal_malloc_coherent_lock);
    INIT_LIST_HEAD(&area->i_list);
    list_add(&area->i_list, &g_vm_struct_list);
    hal_spin_unlock_irqrestore(&hal_malloc_coherent_lock, cpsr);

    if (map_vm_area(size, get_memory_prot(prot), (void *)pages, (unsigned long)malloc_ptr + CONFIG_DMA_VMAREA_START_ADDRESS))
    {
        hal_free_align(malloc_ptr);
        hal_free(pages);
        hal_free(area);
        return NULL;
    }

    hal_free(pages);
    return malloc_ptr + CONFIG_DMA_VMAREA_START_ADDRESS;
}

void *hal_malloc_coherent_non_cacheable(size_t size)
{
    return hal_malloc_coherent_prot(size, PAGE_MEM_ATTR_NON_CACHEABLE);
}

void hal_free_coherent_non_cacheable(void *addr)
{
    hal_free_coherent_prot(addr);
}
#endif
