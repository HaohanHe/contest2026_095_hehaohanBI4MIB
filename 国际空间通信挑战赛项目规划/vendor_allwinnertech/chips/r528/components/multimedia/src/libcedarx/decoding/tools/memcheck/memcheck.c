#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "memcheck.h"
#include "alib_log.h"

#ifdef  LOG_TAG
#undef  LOG_TAG
#define LOG_TAG "Allwinner Audio Middle Layer"
#endif

#ifdef  ALIB_DEBUG
#undef  ALIB_DEBUG
#define ALIB_DEBUG 1
#endif


static mymeminfo mi[RECSIZE] = {0};
static int idx = 0;
static int smalloc = 0;
static int sfree = 0;

static void miprint(mymeminfo* smi)
{
    alib_logd("%s, line : %d, ptr : %u, size : %d  has malloc but not free!",smi->name,smi->line,smi->ptr,smi->size);
}

void* ckmalloc(int size,const char* name, int line)
{
    void* ptr = NULL;
    ptr = malloc(size);
    if(ptr == NULL)
        return NULL;
    memset(&mi[idx], 0x00, sizeof(mymeminfo));
    mi[idx].size = size;
    mi[idx].ptr  = (unsigned int)ptr;
    mi[idx].name = name;
    mi[idx].line  = line;
    mi[idx].malloc = 1;
    idx++;
    smalloc += size;
    return ptr;
}

void ckfree(void* ptr)
{
    int i = 0, freesize = 0;
    if(ptr == NULL)
        return;
    for(i = 0; i<idx; i++)
    {
        if((unsigned int)ptr == mi[i].ptr)
        {
            freesize = mi[i].size;
            memset(&mi[i], 0x00, sizeof(mymeminfo));
            mi[i].free = 1;
            break;
        }
    }
    sfree += freesize;
    free(ptr);
        
}

int getmallocsz()
{
    return smalloc;
}

int getfreesz()
{
    return sfree;
}

void showmeinfo()
{
   int i = 0;
   alib_logd("===========  Show you begin ================");
   for(i=0;i<idx;i++)
   {
       if(mi[i].malloc == 1)
       {
           if(mi[i].free != 1)
           {
               miprint(&mi[i]);
           }
       }
   }
   alib_logd("===========  Show you over, any mem leak??? ================");
}

#ifdef __cplusplus
}
#endif /* __cplusplus */


