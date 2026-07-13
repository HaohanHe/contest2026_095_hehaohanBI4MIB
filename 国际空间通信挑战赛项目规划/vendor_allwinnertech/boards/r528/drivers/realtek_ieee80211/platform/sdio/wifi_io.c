//#include "include/common.h"
//#include <nuttx/sdio.h>
#include "customer_rtos_service.h"
#include "wifi_io.h"
#include "realtek_sdio.h"
#include <stdio.h>
/* test wifi driver */
#define ADDR_MASK 0x10000
#define LOCAL_ADDR_MASK 0x00000
#ifndef NULL
#define	NULL	0
#endif

int wifi_read(struct sdio_func *func, unsigned int addr, unsigned int cnt, void *pdata)
{
	int err;

	realtek_sdio_claim_host(func);

	err = realtek_memcpy_fromio(func, pdata, addr, cnt);
	if (err) {
		DBG_INFO("%s: FAIL(%d)! ADDR=%#x Size=%d\n", __func__, err, addr, cnt);
	}

	realtek_sdio_release_host(func);

	return err;
}

int wifi_write(struct sdio_func *func, unsigned int addr, unsigned int cnt, void *pdata)
{
	int err;
	unsigned int size;

	realtek_sdio_claim_host(func);

	size = cnt;
	err = realtek_memcpy_toio(func, addr, pdata, size);
	if (err) {
		DBG_INFO("%s: FAIL(%d)! ADDR=%#x Size=%d(%d)\n", __func__, err, addr, cnt, size);
	}

	realtek_sdio_release_host(func);

	return err;
}

unsigned char wifi_readb(struct sdio_func *func, unsigned int addr)
{
	int err;
	unsigned char ret = 0;

	realtek_sdio_claim_host(func);
	ret = realtek_sdio_readb(func, ADDR_MASK | addr, &err);
	realtek_sdio_release_host(func);

	if (err)
		DBG_INFO("%s: FAIL!(%d) addr=0x%05x\n", __func__, err, addr);

	return ret;
}

unsigned short wifi_readw(struct sdio_func *func, unsigned int addr)
{
	int err;
	unsigned short v;

	realtek_sdio_claim_host(func);
	v = realtek_sdio_readw(func, ADDR_MASK | addr, &err);
	realtek_sdio_release_host(func);
	if (err)
		DBG_INFO("%s: FAIL!(%d) addr=0x%05x\n", __func__, err, addr);

	return  v;
}

unsigned int wifi_readl(struct sdio_func *func, unsigned int addr)
{
	int err;
	unsigned int v;

	realtek_sdio_claim_host(func);
	v = realtek_sdio_readl(func, ADDR_MASK | addr, &err);
	realtek_sdio_release_host(func);

	return  v;
}

void wifi_writeb(struct sdio_func *func, unsigned int addr, unsigned char val)
{
	int err;

	realtek_sdio_claim_host(func);
	realtek_sdio_writeb(func, val, ADDR_MASK | addr, &err);
	realtek_sdio_release_host(func);
	if (err)
		DBG_INFO("%s: FAIL!(%d) addr=0x%05x val=0x%02x\n", __func__, err, addr, val);
}

void wifi_writew(struct sdio_func *func, unsigned int addr, unsigned short v)
{
	int err;

	realtek_sdio_claim_host(func);
	realtek_sdio_writew(func, v, ADDR_MASK | addr, &err);
	realtek_sdio_release_host(func);
	if (err)
		DBG_INFO("%s: FAIL!(%d) addr=0x%05x val=0x%04x\n", __func__, err, addr, v);
}

void wifi_writel(struct sdio_func *func, unsigned int addr, unsigned int v)
{
	int err;

	realtek_sdio_claim_host(func);
	realtek_sdio_writel(func, v, ADDR_MASK | addr, &err);
	realtek_sdio_release_host(func);
}

unsigned char wifi_readb_local(struct sdio_func *func, unsigned int addr)
{
	int err;
	unsigned char ret = 0;

	ret = realtek_sdio_readb(func, LOCAL_ADDR_MASK | addr, &err);

	return ret;
}

void wifi_writeb_local(struct sdio_func *func, unsigned int addr, unsigned char val)
{
	int err;

	realtek_sdio_writeb(func, val, LOCAL_ADDR_MASK | addr, &err);
}
extern int rtw_fake_driver_probe(struct sdio_func *func);
void wifi_fake_driver_probe_rtlwifi(struct sdio_func *func)
{
	rtw_fake_driver_probe(func);//todo1
}

extern int sdio_bus_probe(void);
extern int sdio_bus_remove(void);
SDIO_BUS_OPS rtw_sdio_bus_ops = {
	realtek_sdio_bus_probe,
	realtek_sdio_bus_remove,
	realtek_sdio_enable_func,
	realtek_sdio_disable_func,
	NULL,
	NULL,
	realtek_sdio_claim_irq,
	realtek_sdio_release_irq,
	realtek_sdio_claim_host,
	realtek_sdio_release_host,
	realtek_sdio_readb,
	realtek_sdio_readw,
	realtek_sdio_readl,
	realtek_sdio_writeb,
	realtek_sdio_writew,
	realtek_sdio_writel,
	realtek_memcpy_fromio,
	realtek_memcpy_toio
};

