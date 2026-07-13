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
#include <hal_time.h>

#include "memblk.h"

#define BITMAP_WORDS			((CONFIG_MEMBLK_MAX_ITEM >> 5) + 1)

#ifndef BIT
#define BIT(x)			(1 << (x))
#endif

#define BITMAP_SET(p, idx)		((p)->bitmap[(idx) >> 5] |=  BIT((idx) & 0x1f))
#define BITMAP_CLR(p, idx)		((p)->bitmap[(idx) >> 5] &= ~BIT((idx) & 0x1f))
#define TEST_IDX(p, idx)		((p)->bitmap[(idx) >> 5] & BIT((idx) & 0x1f))

static int __ffz(uint32_t val)
{
	int num = 0;

	if ((val & 0xffff) == 0xffff) {
		num += 16;
		val >>= 16;
	}

	if ((val & 0xff) == 0xff) {
		num += 8;
		val >>= 8;
	}

	if ((val & 0xf) == 0xf) {
		num += 4;
		val >>= 4;
	}

	if ((val & 0x3) == 0x3) {
		num += 2;
		val >>= 2;
	}

	if ((val & 0x1) == 0x1)
		num += 1;

	return num;
}

static int alloc_idx(struct memblk_pool *pool)
{
	int idx, i;
	unsigned long flags;

	flags = hal_spin_lock_irqsave(&pool->lock);
	for (i = 0; i < BITMAP_WORDS; i++) {
		idx = __ffz(pool->bitmap[i]) + (i << 5);

		if (idx >= pool->item_num) {
			idx = -ENOMEM;
			break;
		}
		/* index already allocated? maybe no free index */
		if (TEST_IDX(pool, idx)) {
			idx = -ENOMEM;
			continue;
		}

		BITMAP_SET(pool, idx);
		break;
	}
	hal_spin_unlock_irqrestore(&pool->lock, flags);

	if (idx >= 0)
		memblk_dbg("pool %p: alloc %d idx\r\n", pool, idx);
	else
		memblk_dbg("pool %p: no free idx\r\n", pool);

	return idx;
}

static void free_idx(struct memblk_pool *pool, int idx)
{
	unsigned long flags;

	memblk_dbg("pool %p: free %d idx\r\n", pool, idx);
	flags = hal_spin_lock_irqsave(&pool->lock);
	BITMAP_CLR(pool, idx);
	hal_spin_unlock_irqrestore(&pool->lock, flags);
}

int memblk_pool_init(struct memblk_pool *pool, void *base,
				uint32_t item_size, uint32_t item_num)
{
	memset(pool->bitmap, 0, sizeof(*pool));
	pool->base = base;
	pool->item_size = item_size;
	pool->item_num = item_num;
	hal_spin_lock_init(&pool->lock);
	sem_init(&pool->avail, 0, item_num);
	return 0;
}

void memblk_pool_deinit(struct memblk_pool *pool)
{
	hal_spin_lock_deinit(&pool->lock);
	sem_destroy(&pool->avail);
	memset(pool->bitmap, 0, sizeof(*pool));
}

void *memblk_get_addr(struct memblk_pool *pool, int idx)
{
	if (idx < 0)
		return NULL;

	return ((char *)(pool->base) + idx * pool->item_size);
}

int memblk_get_len(struct memblk_pool *pool, int idx)
{
	if (idx < 0)
		return -EINVAL;

	return pool->item_size;
}

int memblk_get_idx(struct memblk_pool *pool, int wait_ms)
{
	int idx;
	int ret;

	ret = nxsem_tickwait(&pool->avail, MS_TO_OSTICK(wait_ms));
	if (ret != OK)
		return ret;

	idx = alloc_idx(pool);
	if (idx < 0)
		return idx;

	memblk_dbg("pool %p: alloc entry: %p+0x%x\r\n", pool,
					((char *)(pool->base) + idx * pool->item_size),
					pool->item_size);
	return idx;
}

void memblk_put_idx(struct memblk_pool *pool, int idx)
{
	memblk_dbg("pool %p: free entry: %p\r\n", pool,
					((char *)(pool->base) + idx * pool->item_size));
	free_idx(pool, idx);
	sem_post(&pool->avail);
}
