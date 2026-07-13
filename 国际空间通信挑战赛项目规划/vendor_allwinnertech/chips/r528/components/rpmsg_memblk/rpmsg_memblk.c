/*
 * Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
 *
 * Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
 * the the people's Republic of China and other countries.
 * All Allwinner Technology Co.,Ltd. trademarks are used with permission.
 *
 * DISCLAIMER
 * THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
 * IF YOU NEED TO INTEGRATE THIRD PARTY'S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR
 * MPEGLA, ETC.) IN ALLWINNERS'SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE
 * TO OBTAIN ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES. ALLWINNER SHALL
 * HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
 * COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
 * YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY'S TECHNOLOGY.
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
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <aw_list.h>
#include <hal_status.h>
#include <hal_atomic.h>
#include <hal_mutex.h>
#include <hal_mem.h>
#include <hal_queue.h>
#include <hal_cache.h>
#include <hal_thread.h>
#include <hal_time.h>
#include <nuttx/rpmsg/rpmsg.h>
#include <nuttx/rptun/rptun.h>
#include <nuttx/mutex.h>
#include <openamp/rpmsg.h>
#include <semaphore.h>
#include <debug.h>

#include "rpmsg_memblk.h"
#include "memblk.h"

#define RPMSG_MEMBLK_DRV_PREFIX				"memblk-"

static sem_t bind_sem = SEM_INITIALIZER(0);
static mutex_t g_list_lock = NXMUTEX_INITIALIZER;
static LIST_HEAD(g_epts_list);
static LIST_HEAD(g_dev_list);

struct rpmsg_memblk_dev {
	struct rpmsg_endpoint ept;
	struct memblk_pool *pool;
	struct list_head list;
	hal_queue_t rx_queue;
	bool avail;
	bool bind;
	rpmsg_memblk_dev_release_cb release_cb;
	void *priv;
};

enum rpmsg_mem_cmd {
	RPMEM_CMD_OK = 0,
	RPMEM_CMD_UPDATE,
	RPMEM_CMD_RETURN,
};

struct rpmsg_memblk_head {
	uint32_t cmd;
	uint32_t idx;
	uint32_t pool;
	uint32_t pa;
	uint32_t len;
	uint32_t max;
} __attribute__((packed));

struct rpmsg_memblk_dev *
rpmsg_memblk_dev_create(const char *name,
				rpmsg_memblk_dev_release_cb release_cb, void *priv)
{
	struct rpmsg_memblk_dev *dev;
	struct rpmsg_memblk_dev *pos, *tmp;
	struct rpmsg_memblk_head data;

	dev = NULL;
	while (1) {
		nxsem_wait(&bind_sem);
		nxmutex_lock(&g_list_lock);
		list_for_each_entry_safe(pos, tmp, &g_epts_list, list) {
			if (strcmp(name, &pos->ept.name[sizeof(RPMSG_MEMBLK_DRV_PREFIX) - 1]))
				continue;
			dev = pos;
			break;
		}
		nxmutex_unlock(&g_list_lock);
		if (dev)
			break;
		/*
		 * sem val indicates how many endpoints have beed created by remote.
		 * we have not found the endpoint we need, so we need to release this sem.
		 */
		nxsem_post(&bind_sem);
	}

	rpmsg_memblk_dbg("rpmsg: %s create\r\n", dev->ept.name);

	list_del_init(&dev->list);
	dev->release_cb = release_cb;
	dev->priv = priv;

	nxmutex_lock(&g_list_lock);
	list_add(&dev->list, &g_dev_list);
	nxmutex_unlock(&g_list_lock);

	/* tell remote we are ready */
	data.cmd = RPMEM_CMD_OK;
	rpmsg_send(&dev->ept, &data, sizeof(data));

	dev->avail = true;

	return dev;
}

static void rpmsg_memblk_ept_release(struct rpmsg_endpoint *ept)
{
	struct rpmsg_memblk_dev *dev = ept->priv;

	rpmsg_memblk_dbg("endpoint: %s release\r\n", ept->name);

	if (dev->release_cb)
		dev->release_cb(dev, dev->priv);

	rpmsg_memblk_dev_delete(dev);
	hal_queue_delete(dev->rx_queue);
	kmm_free(dev);
}

void rpmsg_memblk_dev_delete(struct rpmsg_memblk_dev *_dev)
{
	struct rpmsg_memblk_dev *tmp, *dev = NULL;

	nxmutex_lock(&g_list_lock);
	list_for_each_entry_safe(dev, tmp, &g_epts_list, list) {
		if (dev != _dev)
			continue;
		list_del(&dev->list);
		goto out;
	}
	list_for_each_entry_safe(dev, tmp, &g_dev_list, list) {
		if (dev != _dev)
			continue;
		list_del(&dev->list);
		goto out;
	}
	dev = NULL;
out:
	nxmutex_unlock(&g_list_lock);
	if (!dev) {
		rpmsg_memblk_err("rpmsg_memblk_dev %p not found\r\n", _dev);
		return;
	}

	rpmsg_memblk_dbg("rpmsg: %s delete\r\n", dev->ept.name);

	if (dev->avail != false) {
		// do something if user called us
		dev->avail = false;
	}

	if (dev->bind)
		rpmsg_destroy_ept(&dev->ept);
	// it will trigger to call rpmsg_memblk_ept_release
	else
		rpmsg_memblk_ept_release(&dev->ept);
	// ept not create without bind, call it immediately
}

int rpmsg_memblk_set_pool(struct rpmsg_memblk_dev *dev,
				struct memblk_pool *pool)
{
	dev->pool = pool;
	return 0;
}

void rpmsg_memblk_clear_pool(struct rpmsg_memblk_dev *dev)
{
	dev->pool = NULL;
}

int rpmsg_memblk_get_memblk(struct rpmsg_memblk_dev *dev,
				memblk_entry_t entry, int wait_ms)
{
	int idx;

	if (!dev->pool)
		return -EBUSY;

	idx = memblk_get_idx(dev->pool, wait_ms);
	if (idx < 0)
		return -ENOMEM;

	rpmsg_memblk_dbg("pool %p: get idx %d\r\n", dev->pool, idx);
	entry->idx = idx;
	entry->addr = (uint32_t)memblk_get_addr(dev->pool, idx);
	entry->len = memblk_get_len(dev->pool, idx);

	return 0;
}

const char *rpmsg_memblk_get_name(struct rpmsg_memblk_dev *dev)
{
	return dev->ept.name;
}

void *rpmsg_memblk_get_addr(memblk_entry_t entry)
{
	return (void *)entry->addr;
}

uint32_t rpmsg_memblk_get_len(memblk_entry_t entry)
{
	return entry->len;
}

int rpmsg_memblk_push_memblk(struct rpmsg_memblk_dev *dev,
				memblk_entry_t entry, uint32_t len)
{
	struct rpmsg_memblk_head data;
	void *va;

	if (dev->avail != true) {
		rpmsg_memblk_err("dev not ready\r\n");
		return -EBUSY;
	}

	if (!dev->pool) {
		rpmsg_memblk_err("Please call rpmsg_memblk_set_pool to set pool\r\n");
		return -EBUSY;
	}

	if (len > memblk_get_len(dev->pool, entry->idx))
		len = memblk_get_len(dev->pool, entry->idx);

	data.cmd = RPMEM_CMD_UPDATE;
	data.idx = entry->idx;
	data.pool = (uint32_t)dev->pool;
	va = memblk_get_addr(dev->pool, entry->idx);
	data.pa = up_addrenv_va_to_pa(va);
	data.len = len;
	data.max = memblk_get_len(dev->pool, entry->idx);

	rpmsg_memblk_dbg("%s: [Push] cmd=%d,idx=%d,pool=0x%x,pa=0x%x,len=%d/%d\r\n",
					dev->ept.name, (int)data.cmd, (int)data.idx, (int)data.pool,
					(unsigned int)data.pa, (int)data.len, (int)data.max);
	hal_dcache_clean((unsigned long)va, data.len);
	rpmsg_send(&dev->ept, &data, sizeof(data));

	return 0;
}

int rpmsg_memblk_pull_memblk(struct rpmsg_memblk_dev *dev,
				memblk_entry_t buf, int wait_ms)
{
	int ret;

	if (dev->avail != true) {
		rpmsg_memblk_err("dev not ready\r\n");
		return -EBUSY;
	}

	ret = hal_queue_recv(dev->rx_queue, buf, wait_ms);
	if (ret != HAL_OK)
		return -ETIMEDOUT;

	hal_dcache_invalidate(buf->addr, buf->len);

	rpmsg_memblk_dbg("%s: [Pull] idx=%d,pool=0x%x,pa=0x%x,len=%d\r\n",
					dev->ept.name, buf->idx, buf->pool,
					buf->addr, buf->len);

	return 0;
}

void rpmsg_memblk_return_memblk(struct rpmsg_memblk_dev *dev,
				memblk_entry_t buf)
{
	struct rpmsg_memblk_head data;

	if (dev->avail != true) {
		rpmsg_memblk_err("dev not ready\r\n");
		return;
	}

	data.cmd = RPMEM_CMD_RETURN;
	data.idx = buf->idx;
	data.pool = buf->pool;

	rpmsg_send(&dev->ept, &data, sizeof(data));
}

static void _rpmsg_memblk_put_memblk(struct memblk_pool *pool, int idx)
{
	rpmsg_memblk_dbg("pool %p: put idx %d\r\n", pool, idx);
	memblk_put_idx(pool, idx);
}

void rpmsg_memblk_put_memblk(struct rpmsg_memblk_dev *dev, int idx)
{
	if (dev->pool)
		_rpmsg_memblk_put_memblk(dev->pool, idx);
}

/************************rpmsg dev function ************************** */
static bool rpmsg_memblk_ns_match(struct rpmsg_device *rdev,
                             void *priv_, const char *name,
                             uint32_t dest)
{
	return !strncmp(name, RPMSG_MEMBLK_DRV_PREFIX, strlen(RPMSG_MEMBLK_DRV_PREFIX));
}

static void rpmsg_memblk_unbind_cb(struct rpmsg_endpoint *ept)
{
	struct rpmsg_memblk_dev *eptdev = ept->priv;

	rpmsg_memblk_dbg("endpoint: %s unbinding\r\n", ept->name);

	eptdev->avail = false;
	rpmsg_memblk_dev_delete(eptdev);
}

static int rpmsg_memblk_callback(struct rpmsg_endpoint *ept, void *data,
		size_t len, uint32_t src, void *priv)
{
	struct rpmsg_memblk_head *info = data;
	struct rpmsg_memblk_dev *eptdev = ept->priv;

	if (len != sizeof(*info))
		return -EINVAL;

	if (info->cmd == RPMEM_CMD_UPDATE) {
		struct memblk_entry entry;
		entry.idx = info->idx;
		entry.addr = (uint32_t)up_addrenv_pa_to_va(info->pa);
		entry.pool = info->pool;
		entry.len = info->len;
		hal_queue_send_wait(eptdev->rx_queue, &entry, HAL_WAIT_FOREVER);
	} else if (info->cmd == RPMEM_CMD_RETURN) {
		_rpmsg_memblk_put_memblk((struct memblk_pool *)info->pool, info->idx);
	} else if (info->cmd == RPMEM_CMD_OK) {
		/* do nothing */
	}

	return 0;
}

static void rpmsg_memblk_ns_bind(struct rpmsg_device *rdev,
								 void *priv_, const char *name,
								 uint32_t dest)
{
	int ret;
	struct rpmsg_memblk_dev *eptdev;

	rpmsg_memblk_dbg("endpoint: %s bind\r\n", name);

	eptdev = kmm_malloc(sizeof(*eptdev));
	if (!eptdev) {
		rpmsg_memblk_err("failed to alloc client entry\r\n");
		return;
	}

	memset(eptdev, 0, sizeof(*eptdev));
	eptdev->ept.priv = eptdev;

	eptdev->rx_queue = hal_queue_create(name, sizeof(struct memblk_entry),
					CONFIG_MEMBLK_MAX_ITEM);
	if (!eptdev->rx_queue) {
		rpmsg_memblk_err("failed to hal_queue_create\r\n");
		goto free_eptdev;
	}

	eptdev->ept.release_cb = rpmsg_memblk_ept_release;
	ret = rpmsg_create_ept(&eptdev->ept, rdev, name,
							 RPMSG_ADDR_ANY, dest,
							 rpmsg_memblk_callback, rpmsg_memblk_unbind_cb);
	if (ret) {
		rpmsg_memblk_err("failed to rpmsg_create_ept\r\n");
		goto free_queue;
	}

	eptdev->bind = true;
	nxmutex_lock(&g_list_lock);
	list_add(&eptdev->list, &g_epts_list);
	nxmutex_unlock(&g_list_lock);
	nxsem_post(&bind_sem);

	return;
free_queue:
	hal_queue_delete(eptdev->rx_queue);
free_eptdev:
	kmm_free(eptdev);
}

int rpmsg_memblk_init(void)
{
	rpmsg_register_callback(NULL, NULL, NULL,
				rpmsg_memblk_ns_match, rpmsg_memblk_ns_bind);

	return 0;
}

int rpmsg_memblk_deinit(void)
{
	rpmsg_unregister_callback(NULL, NULL, NULL,
				rpmsg_memblk_ns_match, rpmsg_memblk_ns_bind);

	return 0;
}
