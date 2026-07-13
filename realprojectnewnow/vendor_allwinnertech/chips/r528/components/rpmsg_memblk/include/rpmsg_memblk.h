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
#ifndef _AW_RPMSG_MEMBLK_H
#define _AW_RPMSG_MEMBLK_H

#include <stdio.h>
#include <stdlib.h>
#include <hal_atomic.h>
#include <hal_mutex.h>
#include <hal_mem.h>
#include <hal_sem.h>
#include <hal_thread.h>
#include <hal_time.h>
#include <nuttx/rpmsg/rpmsg.h>
#include <nuttx/rptun/rptun.h>
#include <openamp/rpmsg.h>
#include <debug.h>

#include "memblk.h"

struct rpmsg_memblk_dev;

#ifdef CONFIG_AW_RPMSG_MEMBLK_DEBUG
#define rpmsg_memblk_dbg(fmt, ...)		\
		syslog(LOG_DEBUG, "[rpmsg-memblk] %d" fmt, __LINE__, ##__VA_ARGS__)
#else
#define rpmsg_memblk_dbg(...)		do { } while(0)
#endif

#define rpmsg_memblk_err(fmt, ...)		\
		syslog(LOG_ERR, "[rpmsg-memblk] %d" fmt, __LINE__, ##__VA_ARGS__)

struct memblk_entry {
	uint32_t idx;
	uint32_t addr;
	uint32_t len;
	uint32_t pool;
};
typedef struct memblk_entry * memblk_entry_t;

typedef void (*rpmsg_memblk_dev_release_cb)(struct rpmsg_memblk_dev *dev, void *priv);

/* compatible with previous code */
#define rpmsg_memblk_dev_unbind_cb rpmsg_memblk_dev_release_cb

int rpmsg_memblk_init(void);
int rpmsg_memblk_deinit(void);

struct rpmsg_memblk_dev *rpmsg_memblk_dev_create(const char *name,
				rpmsg_memblk_dev_release_cb release_cb, void *priv);
void rpmsg_memblk_dev_delete(struct rpmsg_memblk_dev *);
int rpmsg_memblk_set_pool(struct rpmsg_memblk_dev *dev,
				struct memblk_pool *pool);
void rpmsg_memblk_clear_pool(struct rpmsg_memblk_dev *dev);

int rpmsg_memblk_get_memblk(struct rpmsg_memblk_dev *dev,
				memblk_entry_t entry, int wait_ms);
int rpmsg_memblk_push_memblk(struct rpmsg_memblk_dev *dev,
				memblk_entry_t entry, uint32_t len);
int rpmsg_memblk_pull_memblk(struct rpmsg_memblk_dev *dev,
				memblk_entry_t entry, int wait_ms);
void rpmsg_memblk_return_memblk(struct rpmsg_memblk_dev *dev,
				memblk_entry_t entry);
void rpmsg_memblk_put_memblk(struct rpmsg_memblk_dev *dev, int idx);

const char *rpmsg_memblk_get_name(struct rpmsg_memblk_dev *dev);
void *rpmsg_memblk_get_addr(memblk_entry_t entry);
uint32_t rpmsg_memblk_get_len(memblk_entry_t entry);

#endif
