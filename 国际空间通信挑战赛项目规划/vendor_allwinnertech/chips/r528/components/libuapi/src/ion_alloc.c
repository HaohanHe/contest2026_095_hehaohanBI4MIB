#include <aw_list/list.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "typedef.h"
#include "ionAllocEntry.h"
#include <ion_mem_alloc.h>
#include <hal_atomic.h>
#include <debug.h>
#include <nuttx/irq.h>
#ifdef MEM_DEBUG
#define DBG(fmt, ...)           do{ eLIBs_printf("%s line %d, "fmt, __func__, __LINE__, ##__VA_ARGS__); } while(0)
#define BUG()                   do{ DBG("BUG happend!\n"); }while(0)
#define ENTER_FUNCTION(...)     do{ DBG("enter function!\n"); }while(0)
#define LEAVE_FUNCTION(...)     do{ DBG("leave function!\n"); }while(0)
#else
#define BUG()                   do{ DBG("BUG happend!\n"); }while(0)
#define DBG(fmt, ...)           do{ ; }while(0)
#define ENTER_FUNCTION(...)     do{ ; }while(0)
#define LEAVE_FUNCTION(...)     do{ ; }while(0)
#endif

#define loge(fmt, arg...) _info(fmt "\n", ##arg)
#define logw(fmt, arg...)
#define logd(fmt, arg...)
#define logv(fmt, arg...)
typedef void       *__hdle;
extern void *hal_malloc_align(uint32_t size, int align);

extern void hal_free_align(void *p);

extern unsigned long hal_virt_to_phys(unsigned long virtaddr);
extern void hal_dcache_clean_invalidate(unsigned long vaddr_start, unsigned long size);
typedef struct melis_alloc_context
{
    struct list_head    list;
    __hdle              lock;
    int                 open_flag;
    int                 ref_cnt;    // reference count
} melis_alloc_context_t;

typedef struct melis_buffer_node
{
    struct list_head    i_list;
    unsigned long       phy;
    void                *vir;
    unsigned int        size;
} melis_buffer_node_t;

static melis_alloc_context_t g_physical_list =
{
    .list       = LIST_HEAD_INIT(g_physical_list.list),
    .lock       = NULL,
    .open_flag  = 0,
    .ref_cnt    = 0,
};


/*funciton begin*/
int sunxi_ion_alloc_open(void)
{
    ENTER_FUNCTION();
    irqstate_t flags;
    flags = enter_critical_section();

    if (g_physical_list.open_flag)
    {
        g_physical_list.ref_cnt++;
        leave_critical_section(flags);
        LEAVE_FUNCTION();
        return 0;
    }

    init_list_head(&g_physical_list.list);
    g_physical_list.lock        = NULL;
    g_physical_list.open_flag   = 1;
    g_physical_list.ref_cnt++;
    leave_critical_section(flags);

#if 0
    if (g_physical_list.lock == NULL)
    {
        LEAVE_FUNCTION();
        software_break();
        return -1;
    }
#endif
    LEAVE_FUNCTION();
    return 0;
}

void sunxi_ion_alloc_close(void)
{
    struct list_head       *pos;
    struct list_head       *q;
    melis_buffer_node_t    *tmp;
    unsigned char           err;
    irqstate_t flags;
    ENTER_FUNCTION();
    flags = enter_critical_section();
    if(--g_physical_list.ref_cnt <= 0)
    {
        list_for_each_safe(pos, q, &g_physical_list.list)
        {
            tmp = list_entry(pos, melis_buffer_node_t, i_list);
            list_del(pos);
            //esMEMS_Pfree(tmp->vir, (tmp->size + 1023) / 1024);
            hal_free_align(tmp->vir);
            free(tmp);
        }
        init_list_head(&g_physical_list.list);
        g_physical_list.lock      = NULL;
        g_physical_list.open_flag = 0;
    }
    else
    {
        logv("ref cnt: %d > 0, do not free \n", g_physical_list.ref_cnt);
    }
    leave_critical_section(flags);
    LEAVE_FUNCTION();
    return;
}

/* return virtual address: 0 failed */
void* sunxi_ion_alloc_palloc(int size)
{
    ENTER_FUNCTION();
    melis_buffer_node_t    *new;
    unsigned char           err;
    melis_buffer_node_t    *tmp;
    new = malloc(sizeof(melis_buffer_node_t));

    if (new == NULL)
    {
        loge("malloc node error.");
        LEAVE_FUNCTION();
        return NULL;
    }

    memset(new, 0x00, sizeof(melis_buffer_node_t));


    init_list_head(&new->i_list);
    new->size = size;
    //new->vir = esMEMS_Palloc((size + 1023) / 1024, 0);
    new->vir = hal_malloc_align(((size + 1023) / 1024) * 1024, 4*1024);

    if (new->vir == NULL)
    {
        free(new);
        loge("palloc failure.");
        LEAVE_FUNCTION();
        return NULL;
    }

    //new->phy = esMEMS_VA2PA((unsigned long)new->vir);
    new->phy = hal_virt_to_phys((unsigned long)new->vir);
    irqstate_t flags = enter_critical_section();
    list_add(&new->i_list, &g_physical_list.list);
    leave_critical_section(flags);

    LEAVE_FUNCTION();
    return new->vir;
}

void sunxi_ion_alloc_pfree(void * pbuf)
{
    unsigned char           err;
    struct list_head       *pos;
    struct list_head       *q;
    melis_buffer_node_t    *tmp;
    unsigned char           found = 0;
    int                     freesize = 0;
    ENTER_FUNCTION();

    if (pbuf == NULL)
    {
        loge("invalid ptr to free.");
        LEAVE_FUNCTION();
        return;
    }

    irqstate_t flags = enter_critical_section();
    list_for_each_safe(pos, q, &g_physical_list.list)
    {
        tmp = list_entry(pos, melis_buffer_node_t, i_list);

        if (tmp->vir == pbuf)
        {
            found = 1;
            freesize = tmp->size;
            //esMEMS_Pfree(pbuf, (freesize + 1023) / 1024);
            hal_free_align(pbuf);
            list_del(pos);
            free(tmp);
            break;
        }
    }
    leave_critical_section(flags);

    if (found == 0)
    {
        loge("cant found the error memory node need to free!");
        //software_break();
    }

    LEAVE_FUNCTION();
    return;
}

static void *__sunxi_ion_alloc_get_phyaddr(void *pvirtaddr)
{
    unsigned char           err;
    struct list_head       *pos;
    struct list_head       *q;
    melis_buffer_node_t    *tmp;
    unsigned char           found = 0;
    unsigned long           addr_phy = 0;
    ENTER_FUNCTION();
    irqstate_t flags = enter_critical_section();
    list_for_each_safe(pos, q, &g_physical_list.list)
    {
        tmp = list_entry(pos, melis_buffer_node_t, i_list);

        if ((pvirtaddr >= tmp->vir) && ((unsigned long)pvirtaddr < (unsigned long)((unsigned long)tmp->vir + tmp->size)))
        {
            found = 1;
            addr_phy = (unsigned long)tmp->phy + (unsigned long)pvirtaddr - (unsigned long)tmp->vir;
            break;
        }
    }
    leave_critical_section(flags);

    if (found == 0)
    {
        loge("cant found the error memory node need to free!");
        //software_break();
    }

    LEAVE_FUNCTION();
    return (void *)addr_phy;
}

static void *__sunxi_ion_alloc_get_virtaddr(void *pphyaddr)
{
    unsigned char           err;
    struct list_head       *pos;
    struct list_head       *q;
    melis_buffer_node_t    *tmp;
    unsigned char           found = 0;
    unsigned long           addr_virt = 0;
    ENTER_FUNCTION();
    irqstate_t flags = enter_critical_section();
    list_for_each_safe(pos, q, &g_physical_list.list)
    {
        tmp = list_entry(pos, melis_buffer_node_t, i_list);

        if (((unsigned long)pphyaddr >= tmp->phy) && ((unsigned long)pphyaddr < (tmp->phy + tmp->size)))
        {
            found = 1;
            addr_virt = (unsigned long)tmp->vir + (unsigned long)pphyaddr - (unsigned long)tmp->phy;
            break;
        }
    }
    leave_critical_section(flags);

    if (found == 0)
    {
        loge("cant found the error memory node need to free!");
        //software_break();
    }

    LEAVE_FUNCTION();
    return (void *)addr_virt;
}

void* sunxi_ion_alloc_vir2phy_cpu(void * pbuf)
{
    ENTER_FUNCTION();
    void *p = __sunxi_ion_alloc_get_phyaddr(pbuf);
    LEAVE_FUNCTION();
    return p;
}

void* sunxi_ion_alloc_phy2vir_cpu(void * pbuf)
{
    ENTER_FUNCTION();
    void *p = __sunxi_ion_alloc_get_virtaddr(pbuf);
    LEAVE_FUNCTION();
    return p;
}

int sunxi_ion_alloc_get_bufferFd(void * pbuf)
{
    return 0;
}

void* sunxi_ion_alloc_get_viraddr_byFd(int nShareFd)
{
    return NULL;
}

void sunxi_ion_alloc_flush_cache(void* startAddr, int size)
{
    ENTER_FUNCTION();
    //esMEMS_CleanFlushDCacheRegion(startAddr, size);
    hal_dcache_clean_invalidate((unsigned long)startAddr, size);
    LEAVE_FUNCTION();
    return;
}

void sunxi_ion_flush_cache_all(void)
{
    return;
}

void* sunxi_ion_alloc_alloc_drm(int size)
{
    return NULL;
}

/*return total meminfo with MB */
int sunxi_ion_alloc_get_total_size(void)
{
    ENTER_FUNCTION();
    LEAVE_FUNCTION();
    return TOTAL_MEM_SIZE;
}

int sunxi_ion_alloc_memset(void* buf, int value, size_t n)
{
    memset(buf, value, n);
    return -1;
}

int sunxi_ion_alloc_copy(void* dst, void* src, size_t n)
{
    memcpy(dst, src, n);
    return -1;
}

int sunxi_ion_alloc_read(void* dst, void* src, size_t n)
{
    memcpy(dst, src, n);
    return -1;
}

int sunxi_ion_alloc_write(void* dst, void* src, size_t n)
{
    memcpy(dst, src, n);
    return -1;
}

int sunxi_ion_alloc_setup(void)
{
    return -1;
}

int sunxi_ion_alloc_shutdown(void)
{
    return -1;
}

struct SunxiMemOpsS _allocionMemOpsS =
{
    open:				sunxi_ion_alloc_open,
    close:				sunxi_ion_alloc_close,
    total_size:			sunxi_ion_alloc_get_total_size,
    palloc:			sunxi_ion_alloc_palloc,
    pfree:				sunxi_ion_alloc_pfree,
    flush_cache:		sunxi_ion_alloc_flush_cache,
    cpu_get_phyaddr:	sunxi_ion_alloc_vir2phy_cpu,
    cpu_get_viraddr:	sunxi_ion_alloc_phy2vir_cpu,
    mem_set:			sunxi_ion_alloc_memset,
    mem_cpy:			sunxi_ion_alloc_copy,
    mem_read:			sunxi_ion_alloc_read,
    mem_write:			sunxi_ion_alloc_write,
    setup:				sunxi_ion_alloc_setup,
    shutdown:			sunxi_ion_alloc_shutdown,
    palloc_secure:		sunxi_ion_alloc_alloc_drm,
    get_bufferFd:       sunxi_ion_alloc_get_bufferFd,
    get_viraddr_byFd:   sunxi_ion_alloc_get_viraddr_byFd

};

struct SunxiMemOpsS* GetMemAdapterOpsS()
{
	logd("*** get __GetIonMemOpsS ***");

	return &_allocionMemOpsS;
}
