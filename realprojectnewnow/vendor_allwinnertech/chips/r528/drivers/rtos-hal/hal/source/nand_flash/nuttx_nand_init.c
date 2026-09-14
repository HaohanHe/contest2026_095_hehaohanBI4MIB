/*
* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
*
* Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
* the the people's Republic of China and other countries.
* All Allwinner Technology Co.,Ltd. trademarks are used with permission.
*
* DISCLAIMER
* THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
* IF YOU NEED TO INTEGRATE THIRD PARTY'S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
* IN ALLWINNERS'SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
* ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
* ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <nuttx/sched.h>
#include <nuttx/semaphore.h>
#include <nuttx/mutex.h>
#include <nuttx/fs/fs.h>
#include <nuttx/fs/ioctl.h>
#include <sys/mount.h>
#include <hal_thread.h>

#include <aw_common.h>
#include "nand_log.h"
#include "nand_inc.h"
#include "nand_cfg.h"
#include "nand_nftl.h"
#include "nand_osal.h"
#include "nand_struct.h"
#include "build_nand_partition.h"
#include "nand_physic_interface.h"
#include "build_phy_partition.h"
#include "nand_info_init.h"

//#define NAND_INIT_DEBUG

/* silence when > LOG_DEBUG */
#define NAND_DEFAULT_ERR_LOG_LEVEL		(LOG_ERR)
#define NAND_DEFAULT_WARN_LOG_LEVEL		(LOG_WARNING)
#define NAND_DEFAULT_INFO_LOG_LEVEL		(LOG_INFO)
#define NAND_DEFAULT_DBG_LOG_LEVEL		(LOG_DEBUG + 1)

#define PHY_SPACE_MAP_TO_LOGIC_SPACE

#ifdef PHY_SPACE_MAP_TO_LOGIC_SPACE
#include <nuttx/mtd/mtd.h>
#ifndef NAND_MAX_SPARE_SIZE
#define NAND_MAX_SPARE_SIZE (256)
#endif
#ifndef ECC_LIMIT
#define ECC_LIMIT               10
#endif

#ifndef ERR_ECC
#define ERR_ECC               (-2)
#endif
#endif

#ifndef SECTOR_SHIFT
#define SECTOR_SHIFT 9
#endif

#ifndef SECTOR_SIZE
#define SECTOR_SIZE (1 << 9)
#endif

#define PAGE_SIZE (4096)
#define AUTO_MOUNT_DATA (0)
#define NAND_REQ_PROXY
#define NAND_REQ_PROXY_CHECK
#define NAND_PHY_PARA_CHECK
#define SECURE_STORAGE_REQ_PROXY

#define check_align(sz, align_sz) (!(sz & (align_sz - 1)))
#define byte_to_sector(sz) ((sz) >> SECTOR_SHIFT)
#define sector_to_byte(sz) ((sz) << SECTOR_SHIFT)
#define page_to_sector(sz) ((sz) * (PAGE_SIZE >> SECTOR_SHIFT))
#define sector_to_page(sz) ((sz) * SECTOR_SIZE / PAGE_SIZE)
#define dev_to_blk(dev) (dev->nftl_blk)
#define time_after(a,b) ((long)((b) - (a)) < 0)

struct nand_part {
	char path[30];
	char by_name[30];
	unsigned int index;
	size_t start_sect;
	size_t nsect;
};

#ifdef PHY_SPACE_MAP_TO_LOGIC_SPACE
struct nand_phy_part {
	char path[30];
	char by_name[30];
	unsigned int index;
	size_t start_page;
	size_t npage;
};
#endif

struct nand_thread {
	void *arg;
	int exit_code;
	void *pid;
	unsigned int run;
	sem_t notify;
	sem_t respone;
};

static struct nand_dev {
	char path[20];
	struct _nand_info *nand_info;
	struct _nftl_blk *nftl_blk;
	mutex_t lock;
	long gc_timer;
	long active_timer;
	long read_active_timer;
#ifndef CONFIG_ARCH_TRUSTZONE_SECURE
	struct nand_thread thread_nftld;
	struct nand_thread thread_rcd;
#endif
	struct nand_part *parts;
	unsigned int npart;
#ifdef PHY_SPACE_MAP_TO_LOGIC_SPACE
	long long logic_sector_size;
	long long phy_sector_offset;
	char phy_path[20];
	struct nand_phy_part *phy_parts;
	unsigned int phy_npart;
#ifdef NAND_PHY_PARA_CHECK
	size_t phy_write_block_start;
	size_t phy_write_block_end;
	size_t phy_read_block_start;
	size_t phy_read_block_end;
#endif
	struct mtd_dev_s mtd;
#endif
#ifdef NAND_REQ_PROXY
	void *proxy;
#endif
} *nand_dev;

/* ------------------------------ log wrapper ------------------------------ */
#undef pr_debug
#undef pr_info
#undef pr_warn
#undef pr_err

static unsigned long nand_err_log_level  = NAND_DEFAULT_ERR_LOG_LEVEL;
static unsigned long nand_warn_log_level = NAND_DEFAULT_WARN_LOG_LEVEL;
static unsigned long nand_info_log_level = NAND_DEFAULT_INFO_LOG_LEVEL;
static unsigned long nand_dbg_log_level  = NAND_DEFAULT_DBG_LOG_LEVEL;

#define _CONTACT(__STR_X__, __STR_Y__)	__STR_X__##__STR_Y__
#define CONTACT(__STR_X__, __STR_Y__)	_CONTACT(__STR_X__, __STR_Y__)

#define nand_log_level(_level)		CONTACT(CONTACT(nand_, _level), _log_level)

#define nand_log(_level, fmt, ...)	\
	do { \
		unsigned long level = nand_log_level(_level); \
		if (level > LOG_DEBUG) \
			break; \
		syslog(level, fmt, ##__VA_ARGS__); \
	} while(0)

#define pr_err(fmt, ...)	do { nand_log(err, fmt, ##__VA_ARGS__); } while(0)
#define pr_warn(fmt, ...)	do { nand_log(warn, fmt, ##__VA_ARGS__); } while(0)
#define pr_info(fmt, ...)	do { nand_log(info, fmt, ##__VA_ARGS__); } while(0)
#define pr_dbg(fmt, ...) 	do { nand_log(dbg, fmt, ##__VA_ARGS__); } while(0)
#define pr_debug(fmt, ...) 	do { nand_log(dbg, "[%s:%lu]" fmt, __func__, (unsigned long)__LINE__, ##__VA_ARGS__); } while(0)
/* ------------------------------ log wrapper ------------------------------ */

/* ----------------------------- lock wrapper ------------------------------ */
static int inline nand_lock_init(void)
{
	return nxmutex_init(&nand_dev->lock);
}

static int inline nand_lock_exit(void)
{
	return nxmutex_destroy(&nand_dev->lock);
}

static int inline nand_lock(void)
{
	return nxmutex_lock(&nand_dev->lock);
}

static int inline nand_unlock(void)
{
	return nxmutex_unlock(&nand_dev->lock);
}
/* ----------------------------- lock wrapper ------------------------------ */

/* ----------------------------- task wrapper ------------------------------ */
#define NAND_EXIT_CODE_NONE		(-EBUSY)

static inline void nand_task_mem_clear(struct nand_thread *thread)
{
	nxsem_destroy(&thread->notify);
	nxsem_destroy(&thread->respone);
	memset(thread, 0, sizeof(*thread));
	thread->exit_code = NAND_EXIT_CODE_NONE;
}

static inline int nand_create_task(struct nand_thread *thread,
				   const char *name, void (*fn)(void *), void *arg)
{
	memset(thread, 0, sizeof(*thread));

	nxsem_init(&thread->notify, 0, 0);
	nxsem_init(&thread->respone, 0, 0);
	thread->arg = arg;
	thread->run = 1;
	thread->exit_code = NAND_EXIT_CODE_NONE;
	thread->pid = hal_thread_create(fn, thread, name, 4096, 120);
	if (!thread->pid) {
		nand_task_mem_clear(thread);
		return -ENOMEM;
	}
#ifdef CONFIG_SMP
	pid_t pid = (pid_t)thread->pid;
	if (pid >= 0) {
		cpu_set_t cpuset;
		CPU_ZERO(&cpuset);
		CPU_SET(0, &cpuset);
		nxsched_set_affinity(pid, sizeof(cpuset), &cpuset);
	}
#endif
	pr_info("thread %p create", thread->pid);
	return 0;
}

static inline int nand_delete_task(struct nand_thread *thread)
{
	int exit_code;

	thread->run = 0;
	nxsem_post(&thread->notify);
	nxsem_wait_uninterruptible(&thread->respone);

	pr_info("thread %p delete", thread->pid);
	hal_thread_stop(thread);
	exit_code = thread->exit_code;

	nand_task_mem_clear(thread);
	return exit_code;
}

#define nand_task_should_stop(_thread)		(((_thread)->run == 0) ? 1 : 0)
#define nand_task_msleep(_thread, _msec)	\
	nxsem_tickwait_uninterruptible(&(_thread)->notify, MSEC2TICK((_msec)));
#define nand_task_exit(_thread, _exit)		\
	do { \
		pr_info("thread %p exit", (_thread)->pid); \
		(_thread)->exit_code = (int)(_exit); \
		nxsem_post(&(_thread)->respone); \
		return; \
	} while (0)
/* ----------------------------- task wrapper ------------------------------ */

static int nand_static_wear_leveling(void)
{
	static int swl_done = 0, swl_time = 0, first_miss_swl = 0;
	struct _nftl_blk *blk = nand_dev->nftl_blk;
	int need_swl;

	if (swl_done) {
		if (time_after(nand_dev->gc_timer, swl_time + 64))
			need_swl = 1;	/* next static WL is over 64s */
	} else if (time_after(nand_dev->gc_timer,
				nand_dev->active_timer + 4)) {
		/* nftl is idle over 4s */
		need_swl = 1;
	} else if (first_miss_swl) {
		if (time_after(nand_dev->gc_timer, swl_time + 64 ))
			need_swl = 1;	/* next static WL is over 64s */
	} else {
		first_miss_swl = 1;
		swl_time = nand_dev->gc_timer;
	}

	if (need_swl) {
		first_miss_swl = 0;
		need_swl = 0;
		swl_done = blk->static_wear_leveling(blk);
		if (!swl_done) {
			swl_done = 1;
			swl_time = nand_dev->gc_timer;
		} else
			swl_done = 0;
	}
	return 0;
}

static int nand_dynamic_gc(void)
{
	struct _nftl_blk *blk = nand_dev->nftl_blk;

	/*
	 * deep gc for small spinand
	 * In order to recover small spinand speed with DISCARD NOT OK
	 * YET, we can do deep gc. It means gc more block dynamically
	 * according to how many free blocks. The fewer free blocks,
	 * the more recycled.
	 * As far as we know, the speed problem is only issued on small
	 * nand, for example 128MB. So, it works only when NAND has
	 * samll capacity (128M).
	 * Avoid to reduce speed for userspace, we do it when in idle.
	 */
	/* nand 10 min idle */
	if (time_after(nand_dev->gc_timer,
				nand_dev->active_timer + (10 * 60))) {
		int ret;
		/*
		 * gc with invalid_page_count = (page_per_block / 2)
		 *
		 * gc_all_enhance may take a few seconds. To avoid
		 * reduce read/write speed, do it only when nand is in
		 * idle for 10min
		 */
		ret = blk->dynamic_gc(blk, true);
		if (ret)
			pr_err("nftl_thread: enhance gc all error\n");
	/* read nand 8 sec idle */
	} else if (time_after(nand_dev->gc_timer,
				nand_dev->read_active_timer + 8)) {
		int ret;
		/*
		 * gc according to free block count
		 * invalid page cnt between 32 to 61
		 *
		 * gc_all_base_on_free_blks just needs to check
		 * read_active_time rather than active_time. Because
		 * if some apps, like syslogd, keep writing, it may
		 * never be idle if active_time.
		 * Write operation allways takes time to do gc, it's no
		 * matter to do more here.
		 */
		ret = blk->dynamic_gc(blk, false);
		if (ret)
			pr_err("nftl_thread: dynamic gc all error\n");
	}
	return 0;
}

static void __attribute__((unused)) nftl_gc_thread(void *arg)
{
	struct nand_thread *thread = (struct nand_thread *)arg;
	struct _nftl_blk *blk = nand_dev->nftl_blk;

	if (!blk->reclaim)
		nand_task_exit(thread, -EINVAL);

	nand_dev->gc_timer = 0;
	while (1) {
		if (nand_task_should_stop(thread))
			break;
		nand_task_msleep(thread, 1000);
		nand_dev->gc_timer++;

		nand_lock();

		nand_static_wear_leveling();
		blk->garbage_collect(blk);
		nand_dynamic_gc();

		nand_unlock();
	}

	nand_task_exit(thread, 0);
}

static void __attribute__((unused)) nand_rc_thread(void *arg)
{
	struct nand_thread *thread = (struct nand_thread *)arg;
	struct _nftl_blk *blk = nand_dev->nftl_blk;
	unsigned int start_time = 600;

	if (!blk->reclaim)
		nand_task_exit(thread, -EINVAL);

	while (1) {
		if (nand_task_should_stop(thread))
			break;
		nand_task_msleep(thread, 1000);

		if (start_time) {
			start_time--;
			continue;
		}

		nand_lock();
		blk->reclaim(blk);
		nand_unlock();
	}

	nand_task_exit(thread, 0);
}

#ifdef NAND_REQ_PROXY
#include "../req_proxy.h"

#define NAND_PROXY_READ			(1)
#define NAND_PROXY_WRITE		(2)
#define NAND_PROXY_SYNC			(3)
#define NAND_PROXY_GEOMETRY		(4)
#define NAND_PROXY_IOCTL		(5)

#ifdef PHY_SPACE_MAP_TO_LOGIC_SPACE
#define NAND_PROXY_PHY_ERASE		(6)
#define NAND_PROXY_PHY_BREAD		(7)
#define NAND_PROXY_PHY_BWRITE		(8)
#define NAND_PROXY_PHY_IOCTL		(9)
#define NAND_PROXY_PHY_ISBAD		(10)
#define NAND_PROXY_PHY_MARKBAD		(11)

struct nand_phy_ops_para_t {
	FAR uint8_t *buffer;
	off_t startblock;
	size_t nblocks;
};
#endif

#ifdef SECURE_STORAGE_REQ_PROXY
#define SECURE_STORAGE_READ			(12)
#define SECURE_STORAGE_WRITE		(13)

struct nand_secure_storage_ops_para_t {
	int item;
	FAR unsigned char *buf;
	unsigned int len;
};
#endif

struct nand_rw_para_t {
	FAR unsigned char *buffer;
	blkcnt_t start_page;
	unsigned int npages;
};

struct nand_geometry_para_t {
	FAR struct geometry *geometry;
};

struct nand_ioctl_para_t {
	int cmd;
	unsigned long arg;
};

union nand_para_t {
	struct nand_rw_para_t rw;
	struct nand_geometry_para_t geometry;
	struct nand_ioctl_para_t ioctl;
#ifdef PHY_SPACE_MAP_TO_LOGIC_SPACE
	struct nand_phy_ops_para_t phy_ops;
#endif
#ifdef SECURE_STORAGE_REQ_PROXY
	struct nand_secure_storage_ops_para_t ss_ops;
#endif
};

struct nand_proxy_req_t {
	int type;
	union nand_para_t para;
	ssize_t ret;
	unsigned long check_sum; // must last
};

#ifdef NAND_REQ_PROXY_CHECK
static inline int check_sum(void *_ptr, unsigned long len)
{
	unsigned long sum = 0;
	unsigned long *ptr = _ptr;

	while (len) {
		sum = *ptr;
		len -= sizeof(*ptr);
	}

	return sum;
}

static inline int check_req(FAR struct nand_proxy_req_t *req)
{
	unsigned long check = check_sum(req, offsetof(struct nand_proxy_req_t, check_sum));

	return check != req->check_sum ? -1 : 0;
}

static inline void gen_req_check_sum(FAR struct nand_proxy_req_t *req)
{
	req->check_sum = check_sum(req, offsetof(struct nand_proxy_req_t, check_sum));
}
#else
static inline int check_req(FAR struct nand_proxy_req_t *req)
{
	return 0;
}

static inline void gen_req_check_sum(FAR struct nand_proxy_req_t *req)
{
	req->check_sum = 0;
}
#endif

static inline void set_nand_rw_para_to_req(FAR struct nand_proxy_req_t *req, int type,
	FAR unsigned char *buffer, blkcnt_t start_page, unsigned int npages)
{
	req->type = type;
	req->para.rw.buffer = buffer;
	req->para.rw.start_page = start_page;
	req->para.rw.npages = npages;
	req->ret = -1;
	gen_req_check_sum(req);
}

static inline void set_nand_geometry_para_to_req(FAR struct nand_proxy_req_t *req, FAR struct geometry *geometry)
{
	req->type = NAND_PROXY_GEOMETRY;
	req->para.geometry.geometry = geometry;
	req->ret = -1;
	gen_req_check_sum(req);
}

static inline void set_nand_ioctl_para_to_req(FAR struct nand_proxy_req_t *req, int cmd, unsigned long arg)
{
	req->type = NAND_PROXY_IOCTL;
	req->para.ioctl.cmd = cmd;
	req->para.ioctl.arg = arg;
	req->ret = -1;
	gen_req_check_sum(req);
}

static inline FAR unsigned char *get_buffer_from_req(FAR struct nand_proxy_req_t *req)
{
	return req->para.rw.buffer;
}

static inline blkcnt_t get_start_page_from_req(FAR struct nand_proxy_req_t *req)
{
	return req->para.rw.start_page;
}

static inline unsigned int get_npages_from_req(FAR struct nand_proxy_req_t *req)
{
	return req->para.rw.npages;
}

static inline FAR struct geometry *get_geometry_from_req(FAR struct nand_proxy_req_t *req)
{
	return req->para.geometry.geometry;
}

static inline int get_cmd_from_req(FAR struct nand_proxy_req_t *req)
{
	return req->para.ioctl.cmd;
}

static inline unsigned long get_arg_req(FAR struct nand_proxy_req_t *req)
{
	return req->para.ioctl.arg;
}

#ifdef PHY_SPACE_MAP_TO_LOGIC_SPACE
static inline void set_nand_ioctl_para_to_phy_req(FAR struct nand_proxy_req_t *req, int cmd, unsigned long arg)
{
	req->type = NAND_PROXY_PHY_IOCTL;
	req->para.ioctl.cmd = cmd;
	req->para.ioctl.arg = arg;
	req->ret = -1;
	gen_req_check_sum(req);
}

static inline FAR uint8_t *get_buffer_from_phy_req(FAR struct nand_proxy_req_t *req)
{
	return req->para.phy_ops.buffer;
}

static inline off_t get_startblock_from_phy_req(FAR struct nand_proxy_req_t *req)
{
	return req->para.phy_ops.startblock;
}

static inline size_t get_nblocks_from_phy_req(FAR struct nand_proxy_req_t *req)
{
	return req->para.phy_ops.nblocks;
}

#ifdef NAND_PHY_PARA_CHECK
static inline size_t nand_phy_get_block_size(struct nand_dev *dev)
{
	struct _nand_info *info = dev->nand_info;

	return (size_t)(512 * info->SectorNumsPerPage);
}

static inline int nand_phy_block_read_range_check(struct nand_dev *dev, off_t startblock, size_t nblocks)
{
	if (nblocks == 0)
		return -1;

	if (startblock < dev->phy_read_block_start)
		return -1;

	if (startblock >= dev->phy_read_block_end)
		return -1;

	if ((startblock + nblocks) > dev->phy_read_block_end)
		return -1;

	return 0;
}

static inline int nand_phy_block_write_range_check(struct nand_dev *dev, off_t startblock, size_t nblocks)
{
	if (nblocks == 0)
		return -1;

	if (startblock < dev->phy_write_block_start)
		return -1;

	if (startblock >= dev->phy_write_block_end)
		return -1;

	if ((startblock + nblocks) > dev->phy_write_block_end)
		return -1;

	return 0;
}

static inline int buffer_read_test(FAR const void *buf, size_t size)
{
#if defined(CONFIG_MM_KASAN) && !defined(CONFIG_MM_KASAN_DISABLE_READS_CHECK)
	extern void __asan_loadN(FAR void *addr, size_t size);
	__asan_loadN((void *)buf, size);
#else
	FAR const uint8_t *buffer = (uint8_t *)buf;
	uint8_t dummy = 0;
	volatile uint8_t *ptr = (volatile uint8_t *)&dummy;

	// Simple testing 1
	// *ptr = buffer[0];
	// *ptr += buffer[size - 1];
	// Simple testing 2
	*ptr += buffer[size - 1];
	while ((buffer - (uint8_t *)buf) < size) {
		*ptr += *buffer;
		buffer += 1000 * 1000;
	}
#endif
	return 0;
}

static inline int buffer_write_test(FAR void *buf, size_t size)
{
#if defined(CONFIG_MM_KASAN) && !defined(MM_KASAN_DISABLE_WRITES_CHECK)
	extern void __asan_storeN(FAR void *addr, size_t size);
	__asan_storeN(buf, size);
#else
	FAR uint8_t *buffer = (uint8_t *)buf;

	// Simple testing 1
	// buffer[0] = 0;
	// buffer[size - 1] = 0;
	// Simple testing 2
	buffer[size - 1] = 0;
	while ((buffer - (uint8_t *)buf) < size) {
		*buffer = 0;
		buffer += 1000 * 1000;
	}
#endif
	return 0;
}

static inline int nand_phy_bread_check(FAR struct nand_dev *dev, off_t startblock, size_t nblocks, FAR uint8_t *buffer)
{
	if (nand_phy_block_read_range_check(dev, startblock, nblocks)) {
		pr_err("%s: nand_phy_block_read_range_check failed! startblock: %u nblocks: %u\n", __func__, (unsigned int)startblock, (unsigned int)nblocks);
		PANIC();
		return -1;
	}

	return buffer_write_test(buffer, nand_phy_get_block_size(dev));
}

static inline int nand_phy_bwrite_check(FAR struct nand_dev *dev, off_t startblock, size_t nblocks, FAR const uint8_t *buffer)
{
	if (nand_phy_block_write_range_check(dev, startblock, nblocks)) {
		pr_err("%s: nand_phy_block_write_range_check failed! startblock: %u nblocks: %u\n", __func__, (unsigned int)startblock, (unsigned int)nblocks);
		PANIC();
		return -1;
	}

	return buffer_read_test(buffer, nand_phy_get_block_size(dev));
}
#else
static inline int nand_phy_bread_check(FAR struct nand_dev *dev, off_t startblock, size_t nblocks, FAR uint8_t *buffer)
{
	return 0;
}

static inline int nand_phy_bwrite_check(FAR struct nand_dev *dev, off_t startblock, size_t nblocks, FAR const uint8_t *buffer)
{
	return 0;
}
#endif

static int nand_phy_isbad(FAR struct mtd_dev_s *mtd, off_t block);
static int nand_phy_markbad(FAR struct mtd_dev_s *mtd, off_t block);
static int nand_phy_erase(FAR struct mtd_dev_s *mtd, off_t startblock, size_t nblocks);
static ssize_t nand_phy_bread(FAR struct mtd_dev_s *mtd, off_t startblock, size_t nblocks, FAR uint8_t *buffer);
static ssize_t nand_phy_bwrite(FAR struct mtd_dev_s *mtd, off_t startblock, size_t nblocks, FAR const uint8_t *buffer);
static int nand_phy_ioctl(FAR struct mtd_dev_s *mtd, int cmd, unsigned long arg);
#endif

#ifdef SECURE_STORAGE_REQ_PROXY
static inline void set_secure_storage_para_to_req(FAR struct nand_proxy_req_t *req, int type,
	int item, FAR unsigned char *buf, unsigned int len)
{
	req->type = type;
	req->para.ss_ops.item = item;
	req->para.ss_ops.buf = buf;
	req->para.ss_ops.len = len;
	req->ret = -1;
	gen_req_check_sum(req);
}

static inline int get_secure_storage_item_from_phy_req(FAR struct nand_proxy_req_t *req)
{
	return req->para.ss_ops.item;
}

static inline FAR unsigned char *get_secure_storage_buf_from_phy_req(FAR struct nand_proxy_req_t *req)
{
	return req->para.ss_ops.buf;
}

static inline unsigned int get_secure_storage_len_from_phy_req(FAR struct nand_proxy_req_t *req)
{
	return req->para.ss_ops.len;
}

int nand_secure_storage_read(int item, FAR unsigned char *buf, unsigned int len)
{
	struct nand_proxy_req_t req;

	if (!buf || len == 0)
		return -EINVAL;

	if (!nand_dev || !nand_dev->proxy)
		return -ENODEV;

	set_secure_storage_para_to_req(&req, SECURE_STORAGE_READ, item, buf, len);

	if(req_proxy_request(nand_dev->proxy, &req)) {
		pr_err("%s: req_proxy_request failed!\n", __func__);
		return -1;
	}

	return req.ret;
}

int nand_secure_storage_write(int item, FAR unsigned char *buf, unsigned int len)
{
	struct nand_proxy_req_t req;

	if (!buf || len == 0)
		return -EINVAL;

	if (!nand_dev || !nand_dev->proxy)
		return -ENODEV;

	set_secure_storage_para_to_req(&req, SECURE_STORAGE_WRITE, item, buf, len);

	if(req_proxy_request(nand_dev->proxy, &req)) {
		pr_err("%s: req_proxy_request failed!\n", __func__);
		return -1;
	}

	return req.ret;
}

int do_nand_secure_storage_read(int item, FAR unsigned char *buf, unsigned int len);
int do_nand_secure_storage_write(int item, FAR unsigned char *buf, unsigned int len);
#endif

static inline ssize_t do_nand_read(FAR struct nand_dev *dev, FAR unsigned char *buffer, blkcnt_t start_page, unsigned int npages)
{
	ssize_t ret;
	struct _nftl_blk *blk;

	if (!dev)
		return -EIO;
	blk = dev_to_blk(dev);
	if (!blk)
		return -EIO;

	dev->active_timer = dev->gc_timer;
	dev->read_active_timer = dev->gc_timer;

	pr_debug("try to read addr start sect %u npages %u\n",
			(unsigned int)page_to_sector(start_page), (unsigned int)page_to_sector(npages));

	nand_lock();
	ret = blk->read_data(blk, page_to_sector(start_page),
			page_to_sector(npages), buffer);
	nand_unlock();
	if (ret)
		pr_err("read start sect %u num sect %u failed\n",
				(unsigned int)page_to_sector(start_page),
				(unsigned int)page_to_sector(npages));
	return ret == 0 ? npages : ret;
}

static inline ssize_t do_nand_write(FAR struct nand_dev *dev, FAR const unsigned char *buffer, blkcnt_t start_page, unsigned int npages)
{
	ssize_t ret;
	struct _nftl_blk *blk;

	if (!dev)
		return -EIO;
	blk = dev_to_blk(dev);
	if (!blk)
		return -EIO;

	dev->active_timer = dev->gc_timer;

	pr_debug("try to write start sect %u npages %u\n",
			(unsigned int)page_to_sector(start_page), (unsigned int)page_to_sector(npages));

	nand_lock();
	ret = blk->write_data(blk, page_to_sector(start_page),
			page_to_sector(npages), (unsigned char *)buffer);
	nand_unlock();
	if (ret)
		pr_err("write start sect %u num sect %u failed\n",
				(unsigned int)page_to_sector(start_page),
				(unsigned int)page_to_sector(npages));
	return ret == 0 ? npages : ret;
}

static inline int do_nand_geometry(FAR struct nand_dev *dev, FAR struct geometry *geometry)
{
	struct _nftl_blk *blk;

	if (!dev)
		return -EIO;
	blk = dev_to_blk(dev);
	if (!blk)
		return -EIO;

	geometry->geo_available = true;
	geometry->geo_mediachanged = true;
	geometry->geo_writeenabled = true;
	/*
	 * littlefs use sectorsize as read/write/erase size, set it to 4KB,
	 * which is equal to super page size.
	 */
	geometry->geo_sectorsize = PAGE_SIZE;
	/* littlefs use nsectors as neraseblocks */
	geometry->geo_nsectors = sector_to_page(blk->logic_sects);
	return 0;
}

static inline int do_nand_ioctl(FAR struct nand_dev *dev, int cmd, unsigned long arg)
{
	struct _nftl_blk *blk;

	if (!dev)
		return -EIO;
	blk = dev_to_blk(dev);
	if (!blk)
		return -EIO;

	switch (cmd) {
	case BIOC_FLUSH:
		nand_lock();
		blk->flush_write_cache(blk, 0xFFFFFFFF);
		nand_unlock();
		return 0;
	}
	return -ENOTTY;
}

static ssize_t nand_read_req(FAR struct inode *inode, FAR unsigned char *buffer, blkcnt_t start_page, unsigned int npages)
{
	struct nand_proxy_req_t req;
	FAR void *priv;

	if (!inode)
		return -1;

	priv = (FAR void *)inode->i_private;
	if (!priv)
		return -1;

	set_nand_rw_para_to_req(&req, NAND_PROXY_READ, buffer, start_page, npages);

	if(req_proxy_request(priv, &req)) {
		pr_err("%s: req_proxy_request failed!\n", __func__);
		return -1;
	}

	return req.ret;
}

static ssize_t nand_write_req(FAR struct inode *inode, FAR const unsigned char *buffer, blkcnt_t start_page, unsigned int npages)
{
	struct nand_proxy_req_t req;
	FAR void *priv;

	if (!inode)
		return -1;

	priv = (FAR void *)inode->i_private;
	if (!priv)
		return -1;

	set_nand_rw_para_to_req(&req, NAND_PROXY_WRITE, (FAR unsigned char *)buffer, start_page, npages);

	if(req_proxy_request(priv, &req)) {
		pr_err("%s: req_proxy_request failed!\n", __func__);
		return -1;
	}

	return req.ret;
}

static int nand_geometry_req(FAR struct inode *inode, FAR struct geometry *geometry)
{
	struct nand_proxy_req_t req;
	FAR void *priv;

	if (!inode)
		return -1;

	priv = (FAR void *)inode->i_private;
	if (!priv)
		return -1;

	set_nand_geometry_para_to_req(&req, geometry);

	if(req_proxy_request(priv, &req)) {
		pr_err("%s: req_proxy_request failed!\n", __func__);
		return -1;
	}

	return req.ret;
}

static int nand_ioctl_req(FAR struct inode *inode, int cmd, unsigned long arg)
{
	struct nand_proxy_req_t req;
	FAR void *priv;

	if (!inode)
		return -1;

	priv = (FAR void *)inode->i_private;
	if (!priv)
		return -1;

	set_nand_ioctl_para_to_req(&req, cmd, arg);

	if(req_proxy_request(priv, &req)) {
		pr_err("%s: req_proxy_request failed!\n", __func__);
		return -1;
	}

	return req.ret;
}

static unsigned long req_cnt = 0;
static int nand_proxy_req_handler(void *priv, void *data)
{
	FAR struct nand_dev *dev = (FAR struct nand_dev *)priv;
	FAR struct nand_proxy_req_t *req = (FAR struct nand_proxy_req_t *)data;

	req_cnt++;
	// check req

	if (check_req(req)) {
		pr_err("%s: nand req packet damage!\n", __func__);
		return -1;
	}

#ifdef CONFIG_ARCH_TRUSTZONE_SECURE
  up_irq_enable();
#endif

	switch (req->type) {
	case NAND_PROXY_READ: {
		FAR unsigned char *buffer = get_buffer_from_req(req);
		size_t start_page = get_start_page_from_req(req);
		unsigned int npages = get_npages_from_req(req);
		req->ret = do_nand_read(dev, buffer, start_page, npages);
		break;
	}
	case NAND_PROXY_WRITE: {
		FAR unsigned char *buffer = get_buffer_from_req(req);
		size_t start_page = get_start_page_from_req(req);
		unsigned int npages = get_npages_from_req(req);
		req->ret = do_nand_write(dev, buffer, start_page, npages);
		break;
	}
	case NAND_PROXY_SYNC:
		req->ret = -1;
		break;
	case NAND_PROXY_GEOMETRY: {
		FAR struct geometry *geometry = get_geometry_from_req(req);
		req->ret = do_nand_geometry(dev, geometry);
		break;
	}
	case NAND_PROXY_IOCTL: {
		int cmd = get_cmd_from_req(req);
		unsigned long arg = get_arg_req(req);
		req->ret = do_nand_ioctl(dev, cmd, arg);
		break;
	}
#ifdef PHY_SPACE_MAP_TO_LOGIC_SPACE
	case NAND_PROXY_PHY_ERASE: {
		off_t startblock = get_startblock_from_phy_req(req);
		size_t nblocks = get_nblocks_from_phy_req(req);

		req->ret = nand_phy_erase(&nand_dev->mtd, startblock, nblocks);
		break;
	}
	case NAND_PROXY_PHY_BREAD: {
		FAR uint8_t *buffer = get_buffer_from_phy_req(req);
		off_t startblock = get_startblock_from_phy_req(req);
		size_t nblocks = get_nblocks_from_phy_req(req);

		req->ret = nand_phy_bread(&nand_dev->mtd, startblock, nblocks, buffer);
		break;
	}
	case NAND_PROXY_PHY_BWRITE: {
		FAR uint8_t *buffer = get_buffer_from_phy_req(req);
		off_t startblock = get_startblock_from_phy_req(req);
		size_t nblocks = get_nblocks_from_phy_req(req);

		req->ret = nand_phy_bwrite(&nand_dev->mtd, startblock, nblocks, buffer);
		break;
	}
	case NAND_PROXY_PHY_IOCTL: {
		int cmd = get_cmd_from_req(req);
		unsigned long arg = get_arg_req(req);

		req->ret = nand_phy_ioctl(&nand_dev->mtd, cmd, arg);
		break;
	}
	case NAND_PROXY_PHY_ISBAD: {
		off_t block = get_startblock_from_phy_req(req);

		req->ret = nand_phy_isbad(&nand_dev->mtd, block);
		break;
	}
	case NAND_PROXY_PHY_MARKBAD: {
		off_t block = get_startblock_from_phy_req(req);

		req->ret = nand_phy_markbad(&nand_dev->mtd, block);
		break;
	}
#endif
#ifdef SECURE_STORAGE_REQ_PROXY
	case SECURE_STORAGE_READ: {
		int item = get_secure_storage_item_from_phy_req(req);
		FAR unsigned char *buf = get_secure_storage_buf_from_phy_req(req);
		unsigned int len = get_secure_storage_len_from_phy_req(req);

		req->ret = do_nand_secure_storage_read(item, buf, len);
		break;
	}
	case SECURE_STORAGE_WRITE: {
		int item = get_secure_storage_item_from_phy_req(req);
		FAR unsigned char *buf = get_secure_storage_buf_from_phy_req(req);
		unsigned int len = get_secure_storage_len_from_phy_req(req);

		req->ret = do_nand_secure_storage_write(item, buf, len);
		break;
	}
#endif
	default:
		pr_err("%s: unknwon type: %d\n", __func__, req->type);
		break;
	}

#ifdef CONFIG_ARCH_TRUSTZONE_SECURE
  up_irq_disable();
#endif

	return 0;
}

static const struct block_operations nand_ops =
{
	.open = NULL,
	.close = NULL,
	.read = nand_read_req,
	.write = nand_write_req,
	.geometry = nand_geometry_req,
	.ioctl = nand_ioctl_req,
	.unlink = NULL,
};
#else

static ssize_t nand_read(FAR struct inode *inode, FAR unsigned char *buffer,
		blkcnt_t start_page, unsigned int npages)
{
	ssize_t ret;
	struct nand_dev *dev;
	struct _nftl_blk *blk;

	dev = (FAR struct nand_dev *)inode->i_private;
	if (!dev)
		return -EIO;
	blk = dev_to_blk(nand_dev);
	if (!blk)
		return -EIO;

	dev->active_timer = dev->gc_timer;
	dev->read_active_timer = dev->gc_timer;

	pr_debug("try to read addr start sect %u npages %u\n",
			(unsigned int)page_to_sector(start_page), (unsigned int)page_to_sector(npages));

	nand_lock();
	ret = blk->read_data(blk, page_to_sector(start_page),
					page_to_sector(npages), buffer);
	nand_unlock();
	if (ret)
		pr_err("read start sect %llu num sect %u failed\n",
				page_to_sector(start_page),
				page_to_sector(npages));
	return ret == 0 ? npages : ret;
}

static ssize_t nand_write(FAR struct inode *inode,
		FAR const unsigned char *buffer,
		blkcnt_t start_page, unsigned int npages)
{
	ssize_t ret;
	struct nand_dev *dev;
	struct _nftl_blk *blk;

	dev = (FAR struct nand_dev *)inode->i_private;
	if (!dev)
		return -EIO;
	blk = dev_to_blk(nand_dev);
	if (!blk)
		return -EIO;

	dev->active_timer = dev->gc_timer;

	pr_debug("try to write start sect %u npages %u\n",
			(unsigned int)page_to_sector(start_page), (unsigned int)page_to_sector(npages));

	nand_lock();
	ret = blk->write_data(blk, page_to_sector(start_page),
			page_to_sector(npages), (unsigned char *)buffer);
	nand_unlock();
	if (ret)
		pr_err("write start sect %llu num sect %u failed\n",
				page_to_sector(start_page),
				page_to_sector(npages));
	return ret == 0 ? npages : ret;
}

static int nand_geometry(FAR struct inode *inode,
		FAR struct geometry *geometry)
{
	struct nand_dev *dev;
	struct _nftl_blk *blk;

	dev = (FAR struct nand_dev *)inode->i_private;
	if (!dev)
		return -EIO;
	blk = dev_to_blk(nand_dev);
	if (!blk)
		return -EIO;

	geometry->geo_available = true;
	geometry->geo_mediachanged = true;
	geometry->geo_writeenabled = true;
	/*
	 * littlefs use sectorsize as read/write/erase size, set it to 4KB,
	 * which is equal to super page size.
	 */
	geometry->geo_sectorsize = PAGE_SIZE;
	/* littlefs use nsectors as neraseblocks */
	geometry->geo_nsectors = sector_to_page(blk->logic_sects);
	return 0;
}

static int nand_ioctl(FAR struct inode *inode, int cmd, unsigned long arg)
{
	struct nand_dev *dev;
	struct _nftl_blk *blk;

	dev = (FAR struct nand_dev *)inode->i_private;
	if (!dev)
		return -EIO;
	blk = dev_to_blk(nand_dev);
	if (!blk)
		return -EIO;

	switch (cmd) {
	case BIOC_FLUSH:
		nand_lock();
		blk->flush_write_cache(blk, 0xFFFFFFFF);
		nand_unlock();
		return 0;
	}
	return -ENOTTY;
}

static const struct block_operations nand_ops =
{
	.open = NULL,
	.close = NULL,
	.read = nand_read,
	.write = nand_write,
	.geometry = nand_geometry,
	.ioctl = nand_ioctl,
	.unlink = NULL,
};
#endif

static int nand_register_partition(struct nand_dev *dev)
{
	struct _nftl_blk *blk = dev->nftl_blk;
	struct _nand_info *info = dev->nand_info;
	struct _nand_phy_partition *phy_part;
	struct _nand_disk *disk;
	struct nand_part *part;
	int index, ret;
	size_t used_sects;

	phy_part = get_head_phy_partition_from_nand_info(info);
	disk = get_disk_from_phy_partition(phy_part);

#ifdef PHY_SPACE_MAP_TO_LOGIC_SPACE
	for (index = 0; (disk[index].name[0] != 0xFF) && strncmp((const char *)disk[index].name, "dummy", 5); index++)
		dev->npart++;
#else
	for (index = 0; disk[index].name[0] != 0xFF; index++)
		dev->npart++;
#endif
	dev->parts = malloc(dev->npart * sizeof(struct nand_part));
	if (!dev->parts)
		return -ENOMEM;
	memset(dev->parts, 0, dev->npart * sizeof(struct nand_part));

	used_sects = 0;
	for (index = 0; index < dev->npart; index++) {
		part = &dev->parts[index];
		part->index = index + 1;
		part->start_sect = used_sects;
#ifdef PHY_SPACE_MAP_TO_LOGIC_SPACE
		if (index + 1 == dev->npart && !strncmp((const char *)disk[index].name, "UDISK", 6)) {
#else
		if (index + 1 == dev->npart) {
#endif
			/* fix up the last partition size */
			part->nsect = blk->logic_sects - used_sects;
		} else {
			part->nsect = disk[index].size;
			used_sects += part->nsect;
		}

		snprintf(part->path, sizeof(part->path), "%sp%d",
				dev->path, part->index);
		/*
		 * Since we assign PAGE_SIZE(4K) to geometry->sectorsize on
		 * nand_geometry(), all the operations base on PAGE_SIZE.
		 * The partition operations will alos base on PAGE_SIZE,
		 * so, we must register partitions on unit PAGE, otherwise
		 * nand dirver may get wrongly start page and page count.
		 */
		ret = register_blockpartition(part->path, 0666, dev->path,
				sector_to_page(part->start_sect),
				sector_to_page(part->nsect));
		if (ret) {
			pr_err("add part %s nsect %u start sect %u failed\n",
					part->path, part->nsect, part->start_sect);
			return ret;
		}

#if 0
		if(strcmp((const char *)disk[index].name, "user_data") == 0) {
			snprintf(part->by_name, sizeof(part->by_name),"/dev/%s", "data");
			ret = register_blockpartition(part->by_name, 0666, dev->path,
			sector_to_page(part->start_sect),
			sector_to_page(part->nsect));
		}
#endif
		snprintf(part->by_name, sizeof(part->by_name),
				"/dev/%s", disk[index].name);
		//printf("%s %s %u %u\n", part->by_name, dev->path,
		//	(unsigned int)part->start_sect, (unsigned int)part->nsect);
		ret = register_blockpartition(part->by_name, 0666, dev->path,
				sector_to_page(part->start_sect),
				sector_to_page(part->nsect));
		if (ret) {
			pr_err("add (by-name) part %s nsect %u start sect %u failed\n",
					part->by_name, part->nsect, part->start_sect);
			return ret;
		}

		pr_debug("add part %s nsect %u start sect %u\n", part->path,
				part->nsect, part->start_sect);
	}

	return 0;
}

#ifdef PHY_SPACE_MAP_TO_LOGIC_SPACE
unsigned char g_spare_data[NAND_MAX_SPARE_SIZE];

static inline int nand_ret_to_mtd_ret(int ret)
{
	switch (ret) {
	case 0: return 0;
	case ECC_LIMIT: return -EUCLEAN;
	case ERR_ECC: return -EBADMSG;
	default: return -EIO;
	}
}

static inline void mtd_block_to_nand_pa(off_t block, unsigned int page_per_blk,
	unsigned int *blk, unsigned int *page)
{
	*blk = block / page_per_blk;
	*page = block % page_per_blk;
}

static int nand_phy_isbad(FAR struct mtd_dev_s *mtd, off_t block)
{
	struct nand_dev *dev = container_of(mtd, struct nand_dev, mtd);
	int ret;
	unsigned int chip, blk;

	pr_debug("block %u\n", (unsigned int)block);
	chip = dev->nand_info->ChipNum;
	// the bad block is different from the read/write's startblock
	blk = block;
	nand_lock();

	ret = nand_physic_bad_block_check(chip, blk);

	nand_unlock();
	pr_debug("return %d\n", ret);
	return ret;
}

static int nand_phy_markbad(FAR struct mtd_dev_s *mtd, off_t block)
{
	struct nand_dev *dev = container_of(mtd, struct nand_dev, mtd);
	ssize_t ret;
	unsigned int chip, blk;

	pr_debug("block %u\n", (unsigned int)block);
	chip = dev->nand_info->ChipNum;
	// the bad block is different from the read/write's startblock
	blk = block;
	nand_lock();

	ret = nand_physic_bad_block_mark(chip, blk);

	nand_unlock();
	pr_debug("return %d\n", (int)ret);
	return ret;
}

static int nand_phy_erase(FAR struct mtd_dev_s *mtd, off_t startblock, size_t nblocks)
{
	struct nand_dev *dev = container_of(mtd, struct nand_dev, mtd);
	int ret, failed = 0;
	unsigned int chip, blk;
	off_t i;

	pr_debug("startblock: %u, nblocks: %u\n", (unsigned int)startblock, (unsigned int)nblocks);
	chip = dev->nand_info->ChipNum;
	nand_lock();

	// the erase's startblock is different from the read/write's startblock
	for (i = startblock; i < (startblock + nblocks); i++) {
		blk = i;
#if 0 // should be handled by upper level applications
		if (page) {
			pr_err("mtd block not align to phy blk! %d", (int)i);
			ret = -EINVAL;
			goto out;
		}
		ret = nand_physic_bad_block_check(chip, blk);
		if (ret) {
			pr_err("attempting to erase bad blk! %d", (int)i);
			ret = -EIO;
			break;
		}
#endif
		ret = nand_physic_erase_block(chip, blk);
		if (ret) {
			pr_err("erase exception! %d %d", (int)i, (int)ret);
			failed++;
		}
	}

#if 0
out:
#endif
	nand_unlock();
	pr_debug("return %d\n", (int)(nblocks - failed));
	return nblocks - failed;
}

static ssize_t nand_phy_bread(FAR struct mtd_dev_s *mtd, off_t startblock, size_t nblocks, FAR uint8_t *buffer)
{
	struct nand_dev *dev = container_of(mtd, struct nand_dev, mtd);
	ssize_t ret;
	unsigned int chip, blk, page, page_per_blk, page_size;
	off_t i;
	int correct_cnt = 0;
	int err_cnt = 0;

	pr_debug("startblock: %u, nblocks: %u\n", (unsigned int)startblock, (unsigned int)nblocks);
	nand_phy_bread_check(dev, startblock, nblocks, buffer);
	chip = dev->nand_info->ChipNum;
	page_per_blk = dev->nand_info->PageNumsPerBlk;
	page_size = 512 * dev->nand_info->SectorNumsPerPage;
	nand_lock();

	for (i = startblock; i < (startblock + nblocks); i++) {
		mtd_block_to_nand_pa(i, page_per_blk, &blk, &page);
#if 0 // should be handled by upper level applications
		ret = nand_physic_bad_block_check(chip, blk);
		if (ret) {
			ret = -EIO;
			goto out;
		}
#endif
		ret = nand_physic_read_page(chip, blk, page, page_size / 512, buffer, g_spare_data);
		ret = nand_ret_to_mtd_ret(ret);
		if (ret) {
			if (ret == -EUCLEAN)
				correct_cnt++;
			else if (ret == -EBADMSG)
				err_cnt++;
		}

		buffer += page_size;
	}

	ret = nblocks;
	nand_unlock();
	pr_debug("return %d\n", (int)ret);
	if (err_cnt > 0) {
		ret = -EBADMSG;
	} else if (correct_cnt > 0) {
		ret = -EUCLEAN;
	}

	return ret;
}

#if 0
int check_data(FAR const uint8_t *buffer, unsigned int size, const uint8_t fill)
{
	unsigned int i;

	for (i = 0; i < size; i++) {
		if (buffer[i] != fill)
			return -(i + 1);
	}

	return 0;
}

unsigned char g_main_data[4096];
int nand_physic_writeable_block_check(unsigned int chip, unsigned int blk, unsigned int page, unsigned int page_size)
{
	int ret;

	ret = nand_physic_read_page(chip, blk, page, page_size / 512, g_main_data, g_spare_data);
	if (ret)
		return ret;

	ret = check_data(g_main_data, page_size, 0xff);
	if (ret)
		return ret;

	ret = check_data(g_spare_data, 16, 0xff);
	if (ret)
		return ret - 2048;

	return 0;
}
#endif

static ssize_t nand_phy_bwrite(FAR struct mtd_dev_s *mtd, off_t startblock, size_t nblocks, FAR const uint8_t *buffer)
{
	struct nand_dev *dev = container_of(mtd, struct nand_dev, mtd);
	ssize_t ret;
	unsigned int chip, blk, page, page_per_blk, page_size;
	off_t i;

	pr_debug("startblock: %u, nblocks: %u\n", (unsigned int)startblock, (unsigned int)nblocks);
	nand_phy_bwrite_check(dev, startblock, nblocks, buffer);
	chip = dev->nand_info->ChipNum;
	page_per_blk = dev->nand_info->PageNumsPerBlk;
	page_size = 512 * dev->nand_info->SectorNumsPerPage;
	nand_lock();

	memset(g_spare_data, 0xff, NAND_MAX_SPARE_SIZE);
	for (i = startblock; i < (startblock + nblocks); i++) {
		mtd_block_to_nand_pa(i, page_per_blk, &blk, &page);
#if 0 // should be handled by upper level applications
		ret = nand_physic_bad_block_check(chip, blk);
		if (ret) {
			ret = -EIO;
			goto out;
		}
		ret = nand_physic_writeable_block_check(chip, blk, page, page_size);
		if (ret) {
			pr_err("write exception! can not write %d %d", (int)i, (int)ret);
			ret = nand_ret_to_mtd_ret(ret);
			goto out;
		}
#endif
		ret = nand_physic_write_page(chip, blk, page, page_size / 512, (FAR uint8_t *)buffer, g_spare_data);
		ret = nand_ret_to_mtd_ret(ret);
		if (ret) {
			pr_err("write exception! %d %d", (int)i, (int)ret);
			goto out;
		}
		buffer += page_size;
	}

	ret = nblocks;
out:
	nand_unlock();
	pr_debug("return %d\n", (int)ret);
	return ret;
}

static int nand_phy_ioctl(FAR struct mtd_dev_s *mtd, int cmd, unsigned long arg)
{
	struct nand_dev *dev = container_of(mtd, struct nand_dev, mtd);
	int ret = -ENOTTY;

	pr_debug("cmd: %x\n", cmd);
	nand_lock();

	switch (cmd) {
	case FIOC_FILEPATH:
		FAR char *path = (FAR char *)((uintptr_t)arg);

		if (!path)
			return -EINVAL;
		strcpy(path, dev->phy_path);
		ret = OK;
		break;
	case MTDIOC_GEOMETRY: {
		FAR struct mtd_geometry_s *geo = (FAR struct mtd_geometry_s *)((uintptr_t)arg);
		struct _nand_info *info = dev->nand_info;

		if (!geo)
			return -EINVAL;
		geo->blocksize = 512 * info->SectorNumsPerPage; /* Size of one read/write block. */
		geo->erasesize = geo->blocksize * info->PageNumsPerBlk; /* Size of one erase blocks(must be a multiple of blocksize) */
		geo->neraseblocks = info->BlkPerChip; /* Number of erase blocks*/
		ret = OK;
		pr_debug("blocksize: %d erasesize: %d neraseblocks: %d\n", (int)geo->blocksize,
			(int)geo->erasesize, (int)geo->neraseblocks);
		break;
	}
	case BIOC_FLUSH: {
		struct _nftl_blk *blk = dev_to_blk(dev);
		blk->flush_write_cache(blk, 0xFFFFFFFF);
		ret = OK;
		break;
	}
	default:
		break;
	}

	nand_unlock();
	pr_debug("return %d\n", ret);
	return ret;
}

#ifdef NAND_REQ_PROXY
static inline void set_nand_phy_ops_para_to_req(FAR struct nand_proxy_req_t *req, int type,
	FAR uint8_t *buffer, off_t startblock, size_t nblocks)
{
	req->type = type;
	req->para.phy_ops.buffer = buffer;
	req->para.phy_ops.startblock = startblock;
	req->para.phy_ops.nblocks = nblocks;
	req->ret = -1;
	gen_req_check_sum(req);
}

static int nand_phy_isbad_req(FAR struct mtd_dev_s *mtd, off_t block)
{
	struct nand_proxy_req_t req;
	struct nand_dev *dev;

	if (!mtd)
		return -1;

	dev = container_of(mtd, struct nand_dev, mtd);

	set_nand_phy_ops_para_to_req(&req, NAND_PROXY_PHY_ISBAD, NULL, block, 0);

	if(req_proxy_request(dev->proxy, &req)) {
		pr_err("%s: req_proxy_request failed!\n", __func__);
		return -1;
	}

	return req.ret;
}

static int nand_phy_markbad_req(FAR struct mtd_dev_s *mtd, off_t block)
{
	struct nand_proxy_req_t req;
	struct nand_dev *dev;

	if (!mtd)
		return -1;

	dev = container_of(mtd, struct nand_dev, mtd);

	set_nand_phy_ops_para_to_req(&req, NAND_PROXY_PHY_MARKBAD, NULL, block, 0);

	if(req_proxy_request(dev->proxy, &req)) {
		pr_err("%s: req_proxy_request failed!\n", __func__);
		return -1;
	}

	return req.ret;
}

static int nand_phy_erase_req(FAR struct mtd_dev_s *mtd, off_t startblock, size_t nblocks)
{
	struct nand_proxy_req_t req;
	struct nand_dev *dev;

	if (!mtd)
		return -1;

	dev = container_of(mtd, struct nand_dev, mtd);

	set_nand_phy_ops_para_to_req(&req, NAND_PROXY_PHY_ERASE, NULL, startblock, nblocks);

	if(req_proxy_request(dev->proxy, &req)) {
		pr_err("%s: req_proxy_request failed!\n", __func__);
		return -1;
	}

	return req.ret;
}

static ssize_t nand_phy_bread_req(FAR struct mtd_dev_s *mtd, off_t startblock, size_t nblocks, FAR uint8_t *buffer)
{
	struct nand_proxy_req_t req;
	struct nand_dev *dev;

	if (!mtd)
		return -1;

	dev = container_of(mtd, struct nand_dev, mtd);

	set_nand_phy_ops_para_to_req(&req, NAND_PROXY_PHY_BREAD, buffer, startblock, nblocks);

	nand_phy_bread_check(dev, startblock, nblocks, buffer);

	if(req_proxy_request(dev->proxy, &req)) {
		pr_err("%s: req_proxy_request failed!\n", __func__);
		return -1;
	}

	return req.ret;
}

static ssize_t nand_phy_bwrite_req(FAR struct mtd_dev_s *mtd, off_t startblock, size_t nblocks, FAR const uint8_t *buffer)
{
	struct nand_proxy_req_t req;
	struct nand_dev *dev;

	if (!mtd)
		return -1;

	dev = container_of(mtd, struct nand_dev, mtd);

	set_nand_phy_ops_para_to_req(&req, NAND_PROXY_PHY_BWRITE, (FAR uint8_t *)buffer, startblock, nblocks);
	nand_phy_bwrite_check(dev, startblock, nblocks, buffer);

	if(req_proxy_request(dev->proxy, &req)) {
		pr_err("%s: req_proxy_request failed!\n", __func__);
		return -1;
	}

	return req.ret;
}

static int nand_phy_ioctl_req(FAR struct mtd_dev_s *mtd, int cmd, unsigned long arg)
{
	struct nand_proxy_req_t req;
	struct nand_dev *dev;

	if (!mtd)
		return -1;

	dev = container_of(mtd, struct nand_dev, mtd);

	set_nand_ioctl_para_to_phy_req(&req, cmd, arg);

	if(req_proxy_request(dev->proxy, &req)) {
		pr_err("%s: req_proxy_request failed!\n", __func__);
		return -1;
	}

	return req.ret;
}



#endif

int nand_init_mtddriver(struct nand_dev *dev)
{
	FAR struct mtd_dev_s *mtd = &dev->mtd;

	//nxmutex_init(&mtd->lock);
#ifdef NAND_REQ_PROXY
	mtd->erase = nand_phy_erase_req;
	mtd->bread = nand_phy_bread_req;
	mtd->bwrite = nand_phy_bwrite_req;
	mtd->ioctl = nand_phy_ioctl_req;
	mtd->name = "nand_phy";
	mtd->isbad = nand_phy_isbad_req;
	mtd->markbad = nand_phy_markbad_req;
#else
	mtd->erase = nand_phy_erase;
	mtd->bread = nand_phy_bread;
	mtd->bwrite = nand_phy_bwrite;
	mtd->ioctl = nand_phy_ioctl;
	mtd->name = "nand_phy";
	mtd->isbad = nand_phy_isbad;
	mtd->markbad = nand_phy_markbad;
#endif

	return 0;
}

static int nand_register_mtdpartition(struct nand_dev *dev)
{
	struct _nand_info *info = dev->nand_info;
	struct _nand_phy_partition *phy_part = get_head_phy_partition_from_nand_info(info);
	struct _nand_disk *disk, *disks = get_disk_from_phy_partition(phy_part);
	size_t used_sects = 0, sector_per_page = info->SectorNumsPerPage;
	int index_start, index, ret;
	struct nand_phy_part *part;

	// skip logic partition
	index = 0;
	for (disk = &disks[index]; disk->name[0] != 0xFF; disk = &disks[++index]) {
		used_sects += disk->size;
		if (!strncmp((const char *)disk->name, "dummy", 5))
			break;
	}

	if (disks[index].name[0] == 0xFF) {
		pr_debug("%s: not found phy partition\n", __func__);
		dev->logic_sector_size = 0x7fffffffffffffff; // LONG_LONG_MAX
		dev->phy_sector_offset = dev->logic_sector_size;
		dev->phy_npart = 0;
		return 0;
	}

	// The phy partition is located behind the dummy partition
	dev->logic_sector_size = used_sects;
	dev->phy_sector_offset = dev->logic_sector_size;
	index++;
	index_start = index;

	for (disk = &disks[index]; disk->name[0] != 0xFF; disk = &disks[++index]) {
		if (!strncmp((const char *)disk->name, "UDISK", 6))
			break;
		dev->phy_npart++;
	}

	pr_err("detected %u phy partition, offset: %x\n",
		(unsigned int)dev->phy_npart, (unsigned int)dev->phy_sector_offset);

	dev->phy_parts = malloc(dev->phy_npart * sizeof(struct nand_phy_part));
	if (!dev->phy_parts)
		return -ENOMEM;
	memset(dev->phy_parts, 0, dev->phy_npart * sizeof(struct nand_phy_part));

	used_sects = 0;
	for (index = 0; index < dev->phy_npart; index++) {
		part = &dev->phy_parts[index];
		disk = &disks[index + index_start];
		if (disk->size % sector_per_page) {
			pr_err("partition size not align!\n");
			pr_err("%s sector range: [%u, %u) (%u)\n",
				(const char *)disk->name, (unsigned int)used_sects,
				(unsigned int)(used_sects + disk->size), (unsigned int)disk->size);
			goto err_out;
		}

		part->index = index + 1;
		part->start_page = used_sects / sector_per_page;
		part->npage = disk->size / sector_per_page;
		used_sects += disk->size;

		snprintf(part->path, sizeof(part->path), "%sp%d", dev->phy_path, part->index);
		snprintf(part->by_name, sizeof(part->by_name), "/dev/%s", disk->name);
		pr_debug("page range: [%u, %u) (%u), %s <- %s\n",
			(unsigned int)part->start_page,
			(unsigned int)(part->start_page + part->npage), (unsigned int)part->npage,
			(const char *)part->path, (const char *)part->by_name);

		ret = register_mtdpartition(part->path, 0, dev->phy_path, part->start_page, part->npage);
		pr_debug("register_mtdpartition(%s) return %d\n", part->path, ret);

		ret = register_mtdpartition(part->by_name, 0, dev->phy_path, part->start_page, part->npage);
		pr_debug("register_mtdpartition(%s) return %d\n", part->by_name, ret);

#ifdef NAND_PHY_PARA_CHECK
		if (!strcmp("sst", (const char *)disk->name)) {
			dev->phy_write_block_start = part->start_page;
		}
		if (!strcmp("usrdata", (const char *)disk->name)) {
			dev->phy_write_block_end = part->start_page + part->npage;
		}
		dev->phy_read_block_end = part->start_page + part->npage;
#endif
	}
#ifdef NAND_PHY_PARA_CHECK
	dev->phy_read_block_start = 0;
#endif

	return 0;
err_out:
	return -1;
}
#endif

#ifdef NAND_INIT_DEBUG
static volatile int nand_wait_dbg = 1;
#endif
static int nand_init(void)
{
	int ret = -EINVAL;
	struct _nand_info *nand_info;
	struct _nftl_blk *nftl_blk;
	unsigned short part_no;

	pr_err("nand log ctrl: %p %p %p %p\n",
		&(nand_log_level(err)),
		&(nand_log_level(warn)),
		&(nand_log_level(info)),
		&(nand_log_level(dbg)));

	if (nand_dev)
		return -EBUSY;
	nand_dev = malloc(sizeof(*nand_dev));
	if (!nand_dev)
		return -ENOMEM;
	memset(nand_dev, 0, sizeof(*nand_dev));

#ifdef NAND_INIT_DEBUG
	while (nand_wait_dbg);
#endif
	nand_info = NandHwInit();
	if (!nand_info)
		goto free_dev;

	/* TODO: if set cache level to 0, it will cause wrong data.  and it need to fix */
	set_cache_level(nand_info, 0);
	set_capacity_level(nand_info, NAND_CAPACITY_LEVEL);
	ret = nand_info_init(nand_info, 0, 8, NULL);
	if (ret)
		goto hw_exit;

	nftl_blk = malloc(sizeof(*nftl_blk));
	if (!nftl_blk) {
		ret = -ENOMEM;
		goto hw_exit;
	}
	memset(nftl_blk, 0, sizeof(*nftl_blk));

	nftl_blk->nand = build_nand_partition(nand_info->phy_partition_head);
	if (!nftl_blk->nand)
		goto free_blk;

	part_no = get_partitionNO(nand_info->phy_partition_head);
	ret = nftl_init(nftl_blk, part_no);
	if (ret)
		goto free_part;

	nand_dev->nand_info = nand_info;
	nand_dev->nftl_blk = nftl_blk;

	ret = nand_lock_init();
	if (ret)
		goto nftl_exit;


#ifndef CONFIG_ARCH_TRUSTZONE_SECURE
	ret = nand_create_task(&nand_dev->thread_nftld, "nftld", nftl_gc_thread, NULL);
	if (ret) {
		pr_err("create nftld failed!\n");
		ret = -ENOMEM;
		goto del_lock;
	}

	ret = nand_create_task(&nand_dev->thread_rcd, "nand_rcd", nand_rc_thread, NULL);
	if (ret) {
		pr_err("create nand_rcd failed!\n");
		ret = -ENOMEM;
		goto del_nftld_task;
	}
#endif

	ret = nand_dbg_init(nftl_blk);
	if (ret)
		goto del_nandrc_task;

#ifdef NAND_REQ_PROXY
	nand_dev->proxy = req_proxy_create("nand_proxy", nand_proxy_req_handler, nand_dev, sizeof(struct nand_proxy_req_t), 64);
	if (!nand_dev->proxy) {
		pr_err("req_proxy_create failed\n");
		goto dbg_exit;
	}
	snprintf(nand_dev->path, sizeof(nand_dev->path), "/dev/nand%d", 0);
	ret = register_blockdriver(nand_dev->path, &nand_ops, 0666, nand_dev->proxy);
	if (ret) {
		pr_err("register block driver failed\n");
		goto dbg_exit;
	}
#else
	snprintf(nand_dev->path, sizeof(nand_dev->path), "/dev/nand%d", 0);
	ret = register_blockdriver(nand_dev->path, &nand_ops, 0666, nand_dev);
	if (ret) {
		pr_err("register block driver failed\n");
		goto dbg_exit;
	}
#endif

	ret = nand_register_partition(nand_dev);
	if (ret)
		goto unregister_blkdrv;

#ifdef PHY_SPACE_MAP_TO_LOGIC_SPACE
	snprintf(nand_dev->phy_path, sizeof(nand_dev->phy_path), "/dev/nand%d", 1);

	ret = nand_init_mtddriver(nand_dev);
	if (ret) {
		pr_err("nand_init_mtddriver failed\n");
	}

	register_mtddriver(nand_dev->phy_path, &nand_dev->mtd, 0, &nand_dev->mtd);


	ret = nand_register_mtdpartition(nand_dev);
	if (ret) {
		pr_err("nand_register_mtdpartition failed\n");
	}
#endif

	pr_info("Nand Init OK\n");
	return 0;

unregister_blkdrv:
	unregister_blockdriver(nand_dev->path);
dbg_exit:
	nand_dbg_exit();
del_nandrc_task:
#ifndef CONFIG_ARCH_TRUSTZONE_SECURE
	nand_delete_task(&nand_dev->thread_rcd);
del_nftld_task:
	nand_delete_task(&nand_dev->thread_nftld);
del_lock:
	nand_lock_exit();
#endif
nftl_exit:
	nftl_exit(nftl_blk);
free_part:
	free_nand_partition(nftl_blk->nand);
free_blk:
	free(nftl_blk);
hw_exit:
	NandHwExit();
free_dev:
#ifdef NAND_REQ_PROXY
	req_proxy_destroy(nand_dev->proxy);
	nand_dev->proxy = NULL;
#endif
	free(nand_dev);
	nand_dev = NULL;
	pr_err("Nand Init FAILED\n");
	return ret;
}

static void nand_exit(void)
{
	struct _nftl_blk *blk;
	int ret;

	if (!nand_dev)
		return;
	blk = dev_to_blk(nand_dev);
	if (!blk)
		return;

	ret = unregister_mtddriver(nand_dev->phy_path);
	if (ret)
		pr_err("unregister_mtddriver failed! ret: %d\n", ret);
	ret = unregister_blockdriver(nand_dev->path);
	if (ret)
		pr_err("unregister_blockdriver failed! ret: %d\n", ret);
	pr_err("nand unregister driver\n");
#ifdef NAND_REQ_PROXY
	req_proxy_destroy(nand_dev->proxy);
	nand_dev->proxy = NULL;
#endif
	nand_lock();
	nand_dbg_exit();
	blk->flush_write_cache(blk, 0xFFFFFFFF);
	nand_unlock();
#ifndef CONFIG_ARCH_TRUSTZONE_SECURE
	nand_delete_task(&nand_dev->thread_rcd);
	nand_delete_task(&nand_dev->thread_nftld);
#endif
	nand_lock();
	nftl_exit(blk);
	free_nand_partition(blk->nand);
	free(blk);
	NandHwExit();
	nand_unlock();
	nand_lock_exit();
	free(nand_dev);
	nand_dev = NULL;
}

int hal_nand_init(void)
{
	int ret;

	ret = nand_init();
	if (ret)
		return ret;
#if AUTO_MOUNT_DATA
	ret = mount("/dev/user_data", "/data", "littlefs", 0, NULL);
#else
	ret = 0;
	pr_info("Do not need auto mount data partition!!!\n");
#endif
	if (ret) {
		pr_err("mount user_data to /data failed %d\n", ret);
		return ret;
	}

	return 0;
}

void hal_nand_exit(void)
{
	return nand_exit();
}
