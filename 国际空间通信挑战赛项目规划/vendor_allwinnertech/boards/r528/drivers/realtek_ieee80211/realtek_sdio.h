#include <semaphore.h>
#include <nuttx/mutex.h>

#define RTW_LITTLE_ENDIAN
#ifdef RTW_LITTLE_ENDIAN
#define le16_to_cpup(x)	(*(unsigned short*)(x))
#define le32_to_cpup(x)	(*(unsigned int*)(x))
#define cpu_to_le16(x)	((unsigned short)(x))
#define cpu_to_le32(x) 	((unsigned int)(x))
#else //PLATFORM_TODO
#define le16_to_cpup(x)	(*(unsigned short*)(x))
#define le32_to_cpup(x)	(*(unsigned int*)(x))
#define cpu_to_le16(x)	((unsigned short)(x))
#define cpu_to_le32(x) 	((unsigned int)(x))
#endif

struct realtek_sdio_dev
{
	struct sdio_func func;
	struct sdio_dev_s *sdio_dev;
	void	(*irq_handler)(struct sdio_func *); /* IRQ callback */
	int irq_thread_abort;
	sem_t thread_signal;             /* Semaphore for processing thread event */
	mutex_t lock;             /* mutex for bus */
};

int realtek_sdio_bus_probe(void);
int realtek_sdio_bus_remove(void);
int realtek_sdio_enable_func(struct sdio_func *func);
int realtek_sdio_disable_func(struct sdio_func *func);
int realtek_sdio_claim_irq(struct sdio_func *func, void(*handler)(struct sdio_func *));
int realtek_sdio_release_irq(struct sdio_func *func);
void realtek_sdio_claim_host(struct sdio_func *func);
void realtek_sdio_release_host(struct sdio_func *func);
int realtek_memcpy_fromio(struct sdio_func *func, void *dst,
	unsigned int addr, int count);
int realtek_memcpy_toio(struct sdio_func *func, unsigned int addr,
	void *src, int count);
unsigned char realtek_sdio_readb(struct sdio_func *func, unsigned int addr, int *err_ret);
void realtek_sdio_writeb(struct sdio_func *func, unsigned char b, unsigned int addr, int *err_ret);
unsigned short realtek_sdio_readw(struct sdio_func *func, unsigned int addr, int *err_ret);
void realtek_sdio_writew(struct sdio_func *func, unsigned short b, unsigned int addr, int *err_ret);
unsigned int realtek_sdio_readl(struct sdio_func *func, unsigned int addr, int *err_ret);
void realtek_sdio_writel(struct sdio_func *func, unsigned int b, unsigned int addr, int *err_ret);