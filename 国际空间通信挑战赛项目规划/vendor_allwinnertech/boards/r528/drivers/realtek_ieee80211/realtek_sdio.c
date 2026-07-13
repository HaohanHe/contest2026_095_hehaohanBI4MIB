#include <nuttx/arch.h>
#include <nuttx/sdio.h>
#include <nuttx/kmalloc.h>
#include "wifi_io.h"
#include <errno.h>
#include <debug.h>
#include "realtek_sdio.h"
#include <sched.h>
#include <stdio.h>
#include <nuttx/kthread.h>
#include <osdep_service.h>
#include <arch/chip/realtek_wlan.h>
struct sdio_func *wifi_sdio_func = NULL;

int realtek_oob_irq(FAR void *arg)
{
  FAR struct realtek_sdio_dev *sbus = (FAR struct realtek_sdio_dev *)arg;
  int semcount;
	nxsem_get_value(&sbus->thread_signal, &semcount);
	if (semcount < 1)
	{
		nxsem_post(&sbus->thread_signal);
	}

  return OK;
}

int realtek_sdio_thread(int argc, char **argv)
{
  FAR struct realtek_sdio_dev *priv = (FAR struct realtek_sdio_dev *)
                                ((uintptr_t)strtoul(argv[1], NULL, 16));
  int ret;
  DBG_INFO("sdio irq_thread enter\n");
  while (1)
  {
	ret = nxsem_wait(&priv->thread_signal);

	if (ret == !OK)
		break;
	if (priv->irq_thread_abort)
		break;
	//rtw_msleep_os(10);
	realtek_sdio_claim_host(&priv->func);
	if (priv->irq_handler) {
		priv->irq_handler(&priv->func);
	}
	realtek_sdio_release_host(&priv->func);


	realtek_sdio_irq_clear(priv->sdio_dev);
  }
  

  return 0;
}

int realtek_sdio_bus_probe(void)
{
    return 0;
}

int realtek_sdio_bus_remove(void)
{
    return 0;
}

/**
 *	sdio_enable_func - enables a SDIO function for usage
 *	@func: SDIO function to enable
 *
 *	Powers up and activates a SDIO function so that register
 *	access is possible.
 */
int realtek_sdio_enable_func(struct sdio_func *func)
{
	int ret;
	struct realtek_sdio_dev *priv = (struct realtek_sdio_dev *)func;
	struct sdio_dev_s *dev = priv->sdio_dev;
	int function =func->num;

	ret = sdio_enable_function(dev, function);
	return ret;
}

/**
 *	sdio_disable_func - disable a SDIO function
 *	@func: SDIO function to disable
 *
 *	Powers down and deactivates a SDIO function. Register access
 *	to this function will fail until the function is reenabled.
 */
int realtek_sdio_disable_func(struct sdio_func *func)
{
  	int ret;
	struct realtek_sdio_dev *priv = (struct realtek_sdio_dev *)func;
	struct sdio_dev_s *dev = priv->sdio_dev;
  	uint8_t value;

  /* Read current I/O Enable register */

  ret = sdio_io_rw_direct(dev, false, 0, SDIO_CCCR_IOEN, 0, &value);
  if (ret != OK)
    {
      return ret;
    }
	value &= ~(1 << func->num);
  ret = sdio_io_rw_direct(dev, true, 0,
                          SDIO_CCCR_IOEN, value, NULL);

  if (ret != OK)
    {
      return ret;
    }

  return 0;
}


/**
 *	sdio_claim_irq - claim the IRQ for a SDIO function
 *	@func: SDIO function
 *	@handler: IRQ handler callback
 *
 *	Claim and activate the IRQ for the given SDIO function. The provided
 *	handler will be called when that IRQ is asserted.  The host is always
 *	claimed already when the handler is called so the handler must not
 *	call sdio_claim_host() nor sdio_release_host().
 */
int realtek_sdio_claim_irq(struct sdio_func *func, void(*handler)(struct sdio_func *))
{
	int ret;
	FAR char *argv[2];
  	char arg1[32];
	struct realtek_sdio_dev *priv = (struct realtek_sdio_dev *)func;
	struct sdio_dev_s *dev = priv->sdio_dev;
	int function = func->num;

	ret = sdio_enable_interrupt(dev, 0);
	if(ret != OK)
	{
		DBG_INFO(" enbale isr 0 fail");
		goto exit;
	}

	ret = sdio_enable_interrupt(dev, function);
	if(ret != OK)
	{
		goto exit;
	}
	priv->irq_handler= handler;
	priv->irq_thread_abort = 0;

	snprintf(arg1, sizeof(arg1), "%p", priv);
  	argv[0] = arg1;
  	argv[1] = NULL;
	ret = kthread_create("realtek_sdio_thread",
                       6+100,
                       20480,
                       realtek_sdio_thread, argv);

	if (ret <= 0)
    {
	  priv->irq_handler= NULL;
      ret = -EBADE;
    }
	/*cpu_set_t cpuset;
  	CPU_ZERO(&cpuset);
  	CPU_SET(1, &cpuset);
  	sched_setaffinity(ret, sizeof(cpu_set_t), &cpuset);*/

	realtek_setup_oob_irq(dev, realtek_oob_irq, priv);
	ret = OK;
exit:
	return ret;
}

/**
 *	sdio_release_irq - release the IRQ for a SDIO function
 *	@func: SDIO function
 *
 *	Disable and release the IRQ for the given SDIO function.
 */
int realtek_sdio_release_irq(struct sdio_func *func)
{
	int ret;
	unsigned char reg;
	struct realtek_sdio_dev *priv = (struct realtek_sdio_dev *)func;
	struct sdio_dev_s *dev = priv->sdio_dev;
	int function =func->num;
	if (priv->irq_handler) {
		priv->irq_handler = NULL;
		priv->irq_thread_abort = 1;
		nxsem_post(&priv->thread_signal);
	}

	ret = sdio_io_rw_direct(dev, false, 0, SDIO_CCCR_INTEN, 0, &reg);
	if (ret)
		goto exit;

	reg &= ~(1 << function);

	/* Disable master interrupt with the last function interrupt */
	if (!(reg & 0xFE))
		reg = 0;

	ret = sdio_io_rw_direct(dev, true, 0, SDIO_CCCR_INTEN, reg, NULL);
	if (ret)
		goto exit;

exit:
	return 0;
}

void realtek_sdio_claim_host(struct sdio_func *func)
{
	struct realtek_sdio_dev *priv = (struct realtek_sdio_dev *)func;
	nxmutex_lock(&priv->lock);
}

/**
 *	sdio_release_host - release a bus for a certain SDIO function
 *	@func: SDIO function that was accessed
 *
 *	Release a bus, allowing others to claim the bus for their
 *	operations.
 */
void realtek_sdio_release_host(struct sdio_func *func)
{
	struct realtek_sdio_dev *priv = (struct realtek_sdio_dev *)func;
	nxmutex_unlock(&priv->lock);
}


/* Split an arbitrarily sized data transfer into several
 * IO_RW_EXTENDED commands. */
static int sdio_io_rw_ext_helper(struct sdio_func *func, int write,
	unsigned addr, int incr_addr, unsigned char *buf, unsigned size)
{
	unsigned remainder = size;
	int ret;
	struct realtek_sdio_dev *priv = (struct realtek_sdio_dev *)func;
	struct sdio_dev_s *dev = priv->sdio_dev;
	while (remainder > func->cur_blksize) {
		unsigned blocks;

		blocks = remainder / func->cur_blksize;					
		size = blocks * func->cur_blksize;

		ret = sdio_io_rw_extended(dev, write,
			func->num, addr, incr_addr, buf, func->cur_blksize,
			blocks);
		if (ret)
			return ret;

		remainder -= size;
		buf += size;
		if (incr_addr)
			addr += size;
	}

	/* Write the remainder using byte mode. */
	while (remainder > 0) {
		ret = sdio_io_rw_extended(dev, write, func->num, addr,
			 incr_addr, buf, size, 0);
		if (ret)
			return ret;

		remainder -= size;
		buf += size;
		if (incr_addr)
			addr += size;
	}
	return 0;
}

/**
 *	sdio_memcpy_fromio - read a chunk of memory from a SDIO function
 *	@func: SDIO function to access
 *	@dst: buffer to store the data
 *	@addr: address to begin reading from
 *	@count: number of bytes to read
 *
 *	Reads from the address space of a given SDIO function. Return
 *	value indicates if the transfer succeeded or not.
 */
int realtek_memcpy_fromio(struct sdio_func *func, void *dst,
	unsigned int addr, int count)
{
	return sdio_io_rw_ext_helper(func, 0, addr, 1, dst, count);
}

/**
 *	sdio_memcpy_toio - write a chunk of memory to a SDIO function
 *	@func: SDIO function to access
 *	@addr: address to start writing to
 *	@src: buffer that contains the data to write
 *	@count: number of bytes to write
 *
 *	Writes to the address space of a given SDIO function. Return
 *	value indicates if the transfer succeeded or not.
 */
int realtek_memcpy_toio(struct sdio_func *func, unsigned int addr,
	void *src, int count)
{
	return sdio_io_rw_ext_helper(func, 1, addr, 1, src, count);
}

/**
 *	sdio_readb - read a single byte from a SDIO function
 *	@func: SDIO function to access
 *	@addr: address to read
 *	@err_ret: optional status value from transfer
 *
 *	Reads a single byte from the address space of a given SDIO
 *	function. If there is a problem reading the address, 0xff
 *	is returned and @err_ret will contain the error code.
 */
unsigned char realtek_sdio_readb(struct sdio_func *func, unsigned int addr, int *err_ret)
{
	int ret;
	unsigned char val;

	struct realtek_sdio_dev *priv = (struct realtek_sdio_dev *)func;

	if (err_ret)
		*err_ret = 0;

	ret = sdio_io_rw_direct(priv->sdio_dev, 0, func->num, addr, 0, &val);

	if (ret) {
		if (err_ret)
			*err_ret = ret;
		return 0xFF;
	}

	return val;
}

/**
 *	sdio_writeb - write a single byte to a SDIO function
 *	@func: SDIO function to access
 *	@b: byte to write
 *	@addr: address to write to
 *	@err_ret: optional status value from transfer
 *
 *	Writes a single byte to the address space of a given SDIO
 *	function. @err_ret will contain the status of the actual
 *	transfer.
 */
void realtek_sdio_writeb(struct sdio_func *func, unsigned char b, unsigned int addr, int *err_ret)
{
	int ret;

  	struct realtek_sdio_dev *priv = (struct realtek_sdio_dev *)func;
	ret = sdio_io_rw_direct(priv->sdio_dev, 1, func->num, addr, b, NULL);
	if (err_ret)
		*err_ret = ret;
}

/**
 *	sdio_readw - read a 16 bit integer from a SDIO function
 *	@func: SDIO function to access
 *	@addr: address to read
 *	@err_ret: optional status value from transfer
 *
 *	Reads a 16 bit integer from the address space of a given SDIO
 *	function. If there is a problem reading the address, 0xffff
 *	is returned and @err_ret will contain the error code.
 */
unsigned short realtek_sdio_readw(struct sdio_func *func, unsigned int addr, int *err_ret)
{
	int ret;

	if (err_ret)
		*err_ret = 0;

	rtw_memset((unsigned char*)func->tmpbuf, 0, 4);

	ret = realtek_memcpy_fromio(func, func->tmpbuf, addr, 2);
	if (ret) {
		if (err_ret)
			*err_ret = ret;
		return 0xFFFF;
	}

	return le16_to_cpup((unsigned short *)func->tmpbuf);
}

/**
 *	sdio_writew - write a 16 bit integer to a SDIO function
 *	@func: SDIO function to access
 *	@b: integer to write
 *	@addr: address to write to
 *	@err_ret: optional status value from transfer
 *
 *	Writes a 16 bit integer to the address space of a given SDIO
 *	function. @err_ret will contain the status of the actual
 *	transfer.
 */
void realtek_sdio_writew(struct sdio_func *func, unsigned short b, unsigned int addr, int *err_ret)
{
	int ret;
	*(unsigned short *)func->tmpbuf = cpu_to_le16(b);

	ret = realtek_memcpy_toio(func, addr, func->tmpbuf, 2);

	if (err_ret)
		*err_ret = ret;
}

/**
 *	sdio_readl - read a 32 bit integer from a SDIO function
 *	@func: SDIO function to access
 *	@addr: address to read
 *	@err_ret: optional status value from transfer
 *
 *	Reads a 32 bit integer from the address space of a given SDIO
 *	function. If there is a problem reading the address,
 *	0xffffffff is returned and @err_ret will contain the error
 *	code.
 */
unsigned int realtek_sdio_readl(struct sdio_func *func, unsigned int addr, int *err_ret)
{
	int ret;

	if (err_ret)
		*err_ret = 0;

	rtw_memset((unsigned char*)func->tmpbuf, 0, 4);
	ret = realtek_memcpy_fromio(func, func->tmpbuf, addr, 4);
	if (ret) {
		if (err_ret)
			*err_ret = ret;
		return 0xFFFFFFFF;
	}
	return le32_to_cpup((unsigned int *)func->tmpbuf);
}

/**
 *	sdio_writel - write a 32 bit integer to a SDIO function
 *	@func: SDIO function to access
 *	@b: integer to write
 *	@addr: address to write to
 *	@err_ret: optional status value from transfer
 *
 *	Writes a 32 bit integer to the address space of a given SDIO
 *	function. @err_ret will contain the status of the actual
 *	transfer.
 */
void realtek_sdio_writel(struct sdio_func *func, unsigned int b, unsigned int addr, int *err_ret)
{
	int ret;
	*(unsigned int *)func->tmpbuf = cpu_to_le32(b);

	ret = realtek_memcpy_toio(func, addr, func->tmpbuf, 4);
	if (err_ret)
		*err_ret = ret;
}

int realtek_wl_sdio_init(struct sdio_dev_s * dev)
{
	int ret;
	FAR struct realtek_sdio_dev *sbus;

  	///* Allocate sdio bus structure */
	//struct sdio_dev_s * dev = sdio_initialize(sdcno);
	//mmcsd_slotinitialize(100,dev);

  	sbus = (FAR struct realtek_sdio_dev*)kmm_malloc(sizeof(*sbus));
	DBG_INFO("test  realtek_wl_sdio_init \n");
  	if (!sbus)
    {
		DBG_INFO("sdio sbus malloc fail \n");
      return -ENOMEM;
    }
  /* Initialize sdio bus device structure */

 	memset(sbus, 0, sizeof(*sbus));
  	sbus->sdio_dev           = dev;

  /* Attach and prepare SDIO interrupts */

  	SDIO_ATTACH(sbus->sdio_dev);
  /* Set ID mode clocking (<400KHz) */
  
	SDIO_CLOCK(dev, CLOCK_IDMODE);

	ret = sdio_probe(dev);
  	if (ret != OK)
    {
		DBG_INFO("sdio_probe fail ret:%d \n",ret);
      goto exit_error;
    }
	DBG_INFO("sdio_probe ok \n");
  	/* Init thread semaphore */

  	nxsem_init(&sbus->thread_signal, 0, 0);
	nxmutex_init(&sbus->lock);

  	ret = sdio_set_blocksize(dev, 1, 512);
  	if (ret != OK)
    {
		DBG_INFO("sdio_set_blocksize fail\n");
      goto exit_error;
    }

	sbus->func.cur_blksize = 512;

  /* Default device clock speed is up to 25 MHz
   * We could set EHS bit to operate at a clock rate up to 50 MHz.
   */

  	SDIO_CLOCK(dev, CLOCK_SD_TRANSFER_4BIT);
  	up_mdelay(500);

	ret = sdio_enable_function(dev, 1);
  	if (ret != OK)
    {
		DBG_INFO("sdio_enable_function fail,ret=%d\n",ret);
      goto exit_error;
    }
	sbus->func.num = 1;
	/* probe wifi driver */
	wifi_sdio_func = &sbus->func;

	//ret= wifi_fake_driver_probe_rtlwifi(wifi_sdio_func);

exit_error:
  if (ret<0)
	  DBG_INFO("ERROR: failed to probe device \n");

  return ret;
}


