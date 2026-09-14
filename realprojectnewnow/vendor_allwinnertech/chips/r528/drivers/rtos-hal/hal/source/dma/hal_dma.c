/*
* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
*
* Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
* the the people's Republic of China and other countries.
* All Allwinner Technology Co.,Ltd. trademarks are used with permission.
*
* DISCLAIMER
* THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
* IF YOU NEED TO INTEGRATE THIRD PARTY��S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
* IN ALLWINNERS��SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
* ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
* ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
* COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
* YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY��S TECHNOLOGY.
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

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <debug.h>
#include <hal_reset.h>
#include <hal_cache.h>
#include <hal_mem.h>
#include <hal_atomic.h>
#include <hal_clk.h>
#include <hal_interrupt.h>
#include <interrupt.h>
#include <sunxi_hal_common.h>
#include <hal_dma.h>
#include <assert.h>
#ifdef CONFIG_COMPONENTS_PM
#include <pm_devops.h>
#include <pm_syscore.h>
#include <pm_debug.h>
#endif
#include <nuttx/list.h>

#define DMA_ERR(fmt, arg...) dmaerr("%s()%d " fmt, __func__, __LINE__, ##arg)

#ifdef DMA_ADDR_OFFSET
#define SUNXI_DMA_PHYADDR_OFFSET DMA_ADDR_OFFSET
#else
#define SUNXI_DMA_PHYADDR_OFFSET 0x0
#endif

#ifdef DMA_START_CHAN
#define SUNXI_DMA_CHAN_START DMA_START_CHAN
#else
#define SUNXI_DMA_CHAN_START 0
#endif
#define SET_DESC_HIGH_ADDR(x) ((((u64)x >> 32) & 0x3UL) | (x & 0xFFFFFFFC))

//#define DMA_DEBUG

static struct sunxi_dma_chan    dma_chan_source[NR_MAX_CHAN];
static hal_spinlock_t dma_lock;

#define DMA_LLI_POLL_ITEMS				(256)
#define BITMAP_WORDS					((DMA_LLI_POLL_ITEMS >> 5))

#define BITMAP_SET(p, idx)		((p)->bitmap[(idx) >> 5] |=  BIT((idx) & 0x1f))
#define BITMAP_CLR(p, idx)		((p)->bitmap[(idx) >> 5] &= ~BIT((idx) & 0x1f))
#define TEST_IDX(p, idx)		((p)->bitmap[(idx) >> 5] & BIT((idx) & 0x1f))

struct dma_lli_pool;

struct dma_lli_wrap {
	struct sunxi_dma_lli lli;
	uint32_t idx;
};

struct dma_lli_pool {
	struct dma_lli_wrap items[DMA_LLI_POLL_ITEMS];
	uint32_t bitmap[BITMAP_WORDS];
	uint32_t last;
	uint32_t used;
	uint32_t missing;
};

__attribute__((aligned(64))) static struct dma_lli_pool g_lli_pool = { 0 };

static inline int __ffz(uint32_t val)
{
	return __builtin_ffs(~val) - 1;;
}

static struct sunxi_dma_lli *_dma_get_lli(void)
{
	struct dma_lli_pool *pool = &g_lli_pool;
	struct dma_lli_wrap *lli_wrap = NULL;
	int i, idx;
	unsigned long flags;

	flags = hal_spin_lock_irqsave(&dma_lock);

	if (pool->used != DMA_LLI_POLL_ITEMS) {
		for (i = 0; i < BITMAP_WORDS; i++) {
			if (pool->last == BITMAP_WORDS)
				pool->last = 0;
			idx = __ffz(pool->bitmap[pool->last]);
			if (idx < 0) {
				pool->last++;
				continue;
			}

			idx += (i << 5);
			BITMAP_SET(pool, idx);
			pool->used++;
			lli_wrap = &pool->items[idx];
			lli_wrap->idx = idx;
			goto out;
		}
	}

out:
	hal_spin_unlock_irqrestore(&dma_lock, flags);

	if (!lli_wrap && ++pool->missing > 10) {
		dmawarn("dma lli pool maybe too small\n");
		pool->missing = 0;
	}

	return (struct sunxi_dma_lli *)lli_wrap;
}

static void _dma_put_lli(struct sunxi_dma_lli *lli)
{
	struct dma_lli_wrap *lli_wrap = (struct dma_lli_wrap *)lli;
	struct dma_lli_pool *pool = &g_lli_pool;
	unsigned long flags;

	flags = hal_spin_lock_irqsave(&dma_lock);
	BITMAP_CLR(pool, lli_wrap->idx);
	pool->used--;
	hal_spin_unlock_irqrestore(&dma_lock, flags);
}

/*
 * Fix sconfig's bus width according to at_dmac.
 * 1 byte -> 0, 2 bytes -> 1, 4 bytes -> 2, 8bytes -> 3
 */
static inline uint32_t convert_buswidth(enum dma_slave_buswidth addr_width)
{
    if (addr_width > DMA_SLAVE_BUSWIDTH_8_BYTES)
    {
        return 0;
    }

    switch (addr_width)
    {
        case DMA_SLAVE_BUSWIDTH_2_BYTES:
            return 1;
        case DMA_SLAVE_BUSWIDTH_4_BYTES:
            return 2;
        case DMA_SLAVE_BUSWIDTH_8_BYTES:
            return 3;
        default:
            /* For 1 byte width or fallback */
            return 0;
    }
}

static inline void convert_burst(uint32_t *maxburst)
{
    switch (*maxburst)
    {
        case 1:
            *maxburst = 0;
            break;
        case 4:
            *maxburst = 1;
            break;
        case 8:
            *maxburst = 2;
            break;
        case 16:
            *maxburst = 3;
            break;
        default:
            dmaerr("unknown maxburst\n");
            *maxburst = 0;
            break;
    }
}

static inline void sunxi_cfg_lli(struct sunxi_dma_lli *lli, uint32_t src_addr,
                            uint32_t dst_addr, uint32_t len,
                            struct dma_slave_config *config)
{
    uint32_t src_width = 0, dst_width = 0;

    if (NULL == lli && NULL == config)
    {
        return;
    }

    src_width = convert_buswidth(config->src_addr_width);
    dst_width = convert_buswidth(config->dst_addr_width);
    lli->cfg = SRC_BURST(config->src_maxburst) | \
               SRC_WIDTH(src_width) | \
               DST_BURST(config->dst_maxburst) | \
               DST_WIDTH(dst_width);

    lli->src = src_addr - SUNXI_DMA_PHYADDR_OFFSET;
    lli->dst = dst_addr - SUNXI_DMA_PHYADDR_OFFSET;
    lli->len = len;
    lli->para = NORMAL_WAIT;
}

static void sunxi_dump_lli(struct sunxi_dma_chan *chan, struct sunxi_dma_lli *lli)
{
#ifdef DMA_DEBUG
    dmainfo("channum:%x\n"
           "\t\tdesc:desc - 0x%08x desc p - 0x%08x desc v - 0x%08x\n"
           "\t\tlli: v- 0x%08x v_lln - 0x%08x s - 0x%08x d - 0x%08x c - 0x%08x\n"
           "\t\tdist - 0x%08x len - 0x%08x para - 0x%08x p_lln - 0x%08x\n",
           chan->chan_count,
	   (uint32_t)chan->desc, (uint32_t)chan->desc->p_lln, (uint32_t)chan->desc->vlln,
	   (uint32_t)lli, (uint32_t)lli->vlln, (uint32_t)lli->src, (uint32_t)lli->dst, (uint32_t)lli->cfg,
           (uint32_t)lli->dst, (uint32_t)lli->len, (uint32_t)lli->para, (uint32_t)lli->p_lln);
#endif
}


static void sunxi_dump_com_regs(void)
{
#ifdef DMA_DEBUG
    dmainfo("Common register:\n"
           "\tmask0: 0x%08x\n"
           "\tmask1: 0x%08x\n"
           "\tpend0: 0x%08x\n"
           "\tpend1: 0x%08x\n"
#ifdef DMA_SECURE
           "\tsecur: 0x%08x\n"
#endif
#ifdef DMA_GATE
           "\t_gate: 0x%08x\n"
#endif
           "\tstats: 0x%08x\n",
           (uint32_t)hal_readl(DMA_IRQ_EN(0)),
           (uint32_t)hal_readl(DMA_IRQ_EN(1)),
           (uint32_t)hal_readl(DMA_IRQ_STAT(0)),
           (uint32_t)hal_readl(DMA_IRQ_STAT(1)),
#ifdef DMA_SECURE
           (uint32_t)hal_readl(DMA_SECURE),
#endif
#ifdef DMA_GATE
           (uint32_t)hal_readl(DMA_GATE),
#endif
           (uint32_t)hal_readl(DMA_STAT));
#endif
}

static inline void sunxi_dump_chan_regs(struct sunxi_dma_chan *ch)
{
#ifdef DMA_DEBUG
    u32 chan_num = ch->chan_count;
    dmainfo("Chan %d reg:\n"
           "\t___en: \t0x%08x\n"
           "\tpause: \t0x%08x\n"
           "\tstart: \t0x%08x\n"
           "\t__cfg: \t0x%08x\n"
           "\t__src: \t0x%08x\n"
           "\t__dst: \t0x%08x\n"
           "\tcount: \t0x%08x\n"
           "\t_para: \t0x%08x\n\n",
           chan_num,
           (uint32_t)hal_readl(DMA_ENABLE(chan_num)),
           (uint32_t)hal_readl(DMA_PAUSE(chan_num)),
           (uint32_t)hal_readl(DMA_LLI_ADDR(chan_num)),
           (uint32_t)hal_readl(DMA_CFG(chan_num)),
           (uint32_t)hal_readl(DMA_CUR_SRC(chan_num)),
           (uint32_t)hal_readl(DMA_CUR_DST(chan_num)),
           (uint32_t)hal_readl(DMA_CNT(chan_num)),
           (uint32_t)hal_readl(DMA_PARA(chan_num)));
#endif
}

static void *sunxi_lli_list(struct sunxi_dma_lli *prev, struct sunxi_dma_lli *next,
                        struct sunxi_dma_chan *chan)
{
    uint32_t temp_desc;

    if ((!prev && !chan) || !next)
    {
        return NULL;
    }

    temp_desc = __va_to_pa((unsigned long)next) - SUNXI_DMA_PHYADDR_OFFSET;

    if (!prev)
    {
        chan->desc = next;
        chan->desc->p_lln = temp_desc;
        chan->desc->vlln = next;
    }
    else
    {
        prev->p_lln = temp_desc;
        prev->vlln = next;
    }

    next->p_lln = LINK_END;
    next->vlln = NULL;

    return next;
}

static hal_irqreturn_t sunxi_dma_irq_handle(void *ptr)
{

    uint32_t status_l = 0, status_h = 0;
    int i = 0;

#if START_CHAN_OFFSET < HIGH_CHAN
    status_l = hal_readl(DMA_IRQ_STAT(0));
    status_l &= hal_readl(DMA_IRQ_EN(0));
#endif
#if NR_MAX_CHAN + START_CHAN_OFFSET > HIGH_CHAN
    status_h = hal_readl(DMA_IRQ_STAT(1));
    status_h &= hal_readl(DMA_IRQ_EN(1));
#endif
#if START_CHAN_OFFSET < HIGH_CHAN
    hal_writel(status_l, DMA_IRQ_STAT(0));
#endif
#if NR_MAX_CHAN + START_CHAN_OFFSET > HIGH_CHAN
    hal_writel(status_h, DMA_IRQ_STAT(1));
#endif

    for (i = SUNXI_DMA_CHAN_START; i < NR_MAX_CHAN; i++)
    {
        uint32_t __cpsr;
        struct sunxi_dma_chan *chan = &dma_chan_source[i];
        uint32_t chan_num = chan->chan_count;
        uint32_t status = 0;

        if (chan->used == 0)
        {
            continue;
        }

        __cpsr = hal_spin_lock_irqsave(&chan->lock);

        status = (chan_num + START_CHAN_OFFSET >= HIGH_CHAN) \
                 ? (status_h >> ((chan_num + START_CHAN_OFFSET - HIGH_CHAN) << 2)) \
                 : (status_l >> ((chan_num + START_CHAN_OFFSET) << 2));

        if (!(chan->irq_type & status))
        {
            goto unlock;
        }

        if (chan->cyclic)
        {
            dma_callback cb = NULL;
            void *cb_data = NULL;

            chan->periods_pos ++;
            if (chan->periods_pos * chan->desc->len >= chan->buf_len)
            {
                chan->periods_pos = 0;
            }
            cb = chan->callback;
            cb_data = chan->callback_param;

            hal_spin_unlock_irqrestore(&chan->lock, __cpsr);
            if (cb)
            {
                cb(cb_data);
            }
            __cpsr = hal_spin_lock_irqsave(&chan->lock);
        }
        else
        {
            dma_callback cb = NULL;
            void *cb_data = NULL;

            cb = chan->callback;
            cb_data = chan->callback_param;

            hal_spin_unlock_irqrestore(&chan->lock, __cpsr);
            if (cb)
            {
                cb(cb_data);
            }
            __cpsr = hal_spin_lock_irqsave(&chan->lock);
        }
unlock:
        hal_spin_unlock_irqrestore(&chan->lock, __cpsr);
    }
    return 0;
}

static int sunxi_dma_clk_init(bool enable)
{
    hal_clk_status_t ret;
    u32  reset_id;
    hal_clk_id_t clk_id, mbus_clk_id, dsp_clk_id;
    hal_clk_t clk;
    struct reset_control *reset;

    clk_id = SUNXI_CLK_DMA;
    reset_id = SUNXI_RST_DMA;
    mbus_clk_id = SUNXI_CLK_MBUS_DMA;
#ifdef CONFIG_ARCH_SUN55IW3
    hal_reset_type_t reset_type = HAL_SUNXI_DSP_RESET;
    hal_clk_type_t clk_type = HAL_SUNXI_DSP;
    dsp_clk_id = SUNXI_DSP_CLK;
#else
    hal_reset_type_t reset_type = HAL_SUNXI_RESET;
    hal_clk_type_t clk_type = HAL_SUNXI_CCU;
    dsp_clk_id = (hal_clk_id_t)NULL;
#endif

    if (enable)
    {
	if (reset_id) {
		reset = hal_reset_control_get(reset_type, reset_id);
		hal_reset_control_deassert(reset);
		hal_reset_control_put(reset);
	}
	if (mbus_clk_id)
		hal_clock_enable(hal_clock_get(clk_type, SUNXI_CLK_MBUS_DMA));

	clk = hal_clock_get(clk_type, clk_id);
	ret = hal_clock_enable(clk);
	if (ret != HAL_CLK_STATUS_OK)
	    DMA_ERR("DMA clock enable failed.\n");
	if (dsp_clk_id)
	    hal_clock_enable(hal_clock_get(clk_type, dsp_clk_id));
    }
    else
    {
	clk = hal_clock_get(clk_type, clk_id);
	ret = hal_clock_disable(clk);
	if (ret != HAL_CLK_STATUS_OK)
	    DMA_ERR("DMA clock disable failed.\n");
	if (mbus_clk_id)
		hal_clock_disable(hal_clock_get(clk_type, SUNXI_CLK_MBUS_DMA));
	hal_clock_put(clk);
    }

    return ret;
}

static void sunxi_dma_free_ill(struct sunxi_dma_chan *chan)
{
    struct sunxi_dma_lli *li_adr = NULL, *next = NULL;
    unsigned long __cpsr;

    if (NULL == chan)
    {
        DMA_ERR("[dma] chan is NULL\n");
        return ;
    }

    __cpsr = hal_spin_lock_irqsave(&chan->lock);

    li_adr = chan->desc;
    chan->desc = NULL;
    chan->callback = NULL;
    chan->callback_param = NULL;

    hal_spin_unlock_irqrestore(&chan->lock, __cpsr);

    while (li_adr)
    {
        next = li_adr->vlln;
        _dma_put_lli(li_adr);
        li_adr = next;
    }

}

hal_dma_chan_status_t hal_dma_chan_request(struct sunxi_dma_chan **dma_chan)
{
    int i = 0;
    struct sunxi_dma_chan *chan;
    uint32_t __cpsr;

    for (i = SUNXI_DMA_CHAN_START; i < NR_MAX_CHAN; i++)
    {
        chan = &dma_chan_source[i];
        if (chan->used == 0)
        {
            __cpsr = hal_spin_lock_irqsave(&chan->lock);
            chan->used = 1;
            chan->chan_count = i;
            hal_spin_unlock_irqrestore(&chan->lock, __cpsr);
            *dma_chan = &dma_chan_source[i];
            return HAL_DMA_CHAN_STATUS_FREE;
        }
    }

    return HAL_DMA_CHAN_STATUS_BUSY;
}

hal_dma_status_t hal_dma_prep_memcpy(struct sunxi_dma_chan *chan,
				       uint32_t dest, uint32_t src, uint32_t len)
{
    unsigned long __cpsr;
    struct sunxi_dma_lli *l_item = NULL;
    struct dma_slave_config *config = NULL;

    if ((NULL == chan) || (dest == 0 || src == 0))
    {
        DMA_ERR("[dma] chan is NULL\n");
        return HAL_DMA_STATUS_INVALID_PARAMETER;
    }

    ASSERT(!in_interrupt());

    l_item = _dma_get_lli();
    if (!l_item)
    {
        return HAL_DMA_STATUS_NO_MEM;
    }
	if  ( ( dest <= 0x040210b8 && (dest + len) > 0x040210b8 ) 
		|| ( src <= 0x040210b8 && (src + len) > 0x040210b8 ) ) {
		syslog(LOG_ERR,"dma_test_memcpy: dest:%lx,src:%lx,len:%ld\n",
				dest,src,len);
		sched_dumpstack(gettid());
	}

    memset(l_item, 0, sizeof(struct sunxi_dma_lli));

    __cpsr = hal_spin_lock_irqsave(&chan->lock);

    config = &chan->cfg;
    dest = __va_to_pa(dest);
    src = __va_to_pa(src);
    sunxi_cfg_lli(l_item, src, dest, len, config);

    l_item->cfg |= SRC_DRQ(DRQSRC_SDRAM) \
                   | DST_DRQ(DRQDST_SDRAM) \
                   | DST_LINEAR_MODE \
                   | SRC_LINEAR_MODE;
    sunxi_lli_list(NULL, l_item, chan);
    hal_spin_unlock_irqrestore(&chan->lock, __cpsr);

    sunxi_dump_lli(chan, l_item);

    return HAL_DMA_STATUS_OK;
}

hal_dma_status_t hal_dma_prep_device(struct sunxi_dma_chan *chan,
				       uint32_t dest, uint32_t src,
				       uint32_t len, enum dma_transfer_direction dir)
{
    struct sunxi_dma_lli *l_item = NULL;
    struct dma_slave_config *config = NULL;
    unsigned long __cpsr;

    if ((NULL == chan) || (dest == 0 || src == 0))
    {
        DMA_ERR("[dma] chan is NULL\n");
        return HAL_DMA_STATUS_INVALID_PARAMETER;
    }

    ASSERT(!in_interrupt());

    l_item = _dma_get_lli();
    if (!l_item)
    {
        return HAL_DMA_STATUS_NO_MEM;
    }
	if  ( ( dest <= 0x040210b8 && (dest + len) > 0x040210b8 ) 
		|| ( src <= 0x040210b8 && (src + len) > 0x040210b8 ) ) {
		syslog(LOG_ERR,"dma_test_device: dest:%lx,src:%lx,len:%ld\n",
				dest,src,len);
		sched_dumpstack(gettid());
	}

    memset(l_item, 0, sizeof(struct sunxi_dma_lli));

    __cpsr = hal_spin_lock_irqsave(&chan->lock);

    config = &chan->cfg;

    if (dir == DMA_MEM_TO_DEV)
    {
        src = __va_to_pa(src);
        sunxi_cfg_lli(l_item, src, dest, len, config);
        l_item->cfg |= GET_DST_DRQ(config->slave_id) \
                       | SRC_LINEAR_MODE \
                       | DST_IO_MODE \
                       | SRC_DRQ(DRQSRC_SDRAM);
    }
    else if (dir == DMA_DEV_TO_MEM)
    {
        dest = __va_to_pa(dest);
        sunxi_cfg_lli(l_item, src, dest, len, config);
        l_item ->cfg |= GET_SRC_DRQ(config->slave_id)  \
                        | DST_LINEAR_MODE \
                        | SRC_IO_MODE \
                        | DST_DRQ(DRQSRC_SDRAM);
    }
    else if (dir == DMA_DEV_TO_DEV)
    {
        sunxi_cfg_lli(l_item, src, dest, len, config);
        l_item->cfg |= GET_SRC_DRQ(config->slave_id) \
                       | DST_IO_MODE \
                       | SRC_IO_MODE \
                       | GET_DST_DRQ(config->slave_id);
    }

    sunxi_lli_list(NULL, l_item, chan);
    hal_spin_unlock_irqrestore(&chan->lock, __cpsr);

    sunxi_dump_lli(chan, l_item);

    return HAL_DMA_STATUS_OK;
}

hal_dma_status_t hal_dma_prep_cyclic(struct sunxi_dma_chan *chan,
				     uint32_t buf_addr, uint32_t buf_len,
				     uint32_t period_len, enum dma_transfer_direction dir)
{
    struct sunxi_dma_lli *prev = NULL, *li_old = NULL;
    uint32_t periods = buf_len / period_len;
    struct dma_slave_config *config = NULL;
    unsigned long __cpsr;
    struct sunxi_dma_lli *l_item[periods];
    uint32_t i = 0;

    if ((NULL == chan && chan->cyclic) || (0 == buf_addr))
    {
        DMA_ERR("[dma] chan or buf_addr is NULL\n");
        return HAL_DMA_STATUS_INVALID_PARAMETER;
    }

    ASSERT(!in_interrupt());

	if  ( buf_addr <= 0x040210b8 && (buf_addr + buf_len) > 0x040210b8 ) {
		syslog(LOG_ERR,"dma_test_cyclic: buf_addr:%lx,buf_len:%lx,period_len:%ld\n",
				buf_addr,buf_len,period_len);
		sched_dumpstack(gettid());
	}

    memset(l_item, 0, sizeof(l_item));
    for (i = 0; i < periods; i++) {
	    l_item[i] = _dma_get_lli();
	    if (!l_item[i])
		goto no_mem;
	    memset(l_item[i], 0, sizeof(struct sunxi_dma_lli));
    }

    __cpsr = hal_spin_lock_irqsave(&chan->lock);

    if (chan->desc) {
        li_old = chan->desc;
        chan->desc = NULL;
    }

    config = &chan->cfg;
    for (i = 0; i < periods; i++)
    {
        if (dir == DMA_MEM_TO_DEV)
        {
            sunxi_cfg_lli(l_item[i], __va_to_pa(buf_addr + period_len * i),
                          config->dst_addr, period_len, config);
            l_item[i]->cfg |= GET_DST_DRQ(config->slave_id) \
                           | SRC_LINEAR_MODE \
                           | DST_IO_MODE \
                           | SRC_DRQ(DRQSRC_SDRAM);
        }
        else if (dir == DMA_DEV_TO_MEM)
        {
            sunxi_cfg_lli(l_item[i], config->src_addr, \
                          __va_to_pa(buf_addr + period_len * i), \
                          period_len, config);
            l_item[i]->cfg |= GET_SRC_DRQ(config->slave_id)  \
                            | DST_LINEAR_MODE \
                            | SRC_IO_MODE \
                            | DST_DRQ(DRQSRC_SDRAM);
        }
        else if (dir == DMA_DEV_TO_DEV)
        {
            sunxi_cfg_lli(l_item[i], config->src_addr, \
                          config->dst_addr, period_len, config);
            l_item[i]->cfg |= GET_SRC_DRQ(config->slave_id) \
                           | DST_IO_MODE \
                           | SRC_IO_MODE \
                           | GET_DST_DRQ(config->slave_id);

        }
        prev = sunxi_lli_list(prev, l_item[i], chan);
    }

    prev->p_lln = __va_to_pa((unsigned long)chan->desc);
    chan->cyclic = true;
#ifdef DMA_DEBUG
    for (prev = chan->desc; prev != NULL; prev = prev->vlln)
    {
        sunxi_dump_lli(chan, prev);
    }
#endif
    hal_spin_unlock_irqrestore(&chan->lock, __cpsr);

    if (li_old) {
        struct sunxi_dma_lli *next = NULL;

        while (li_old)
        {
            next = li_old->vlln;
            _dma_put_lli(li_old);
            li_old = next;
        }
    }

    return HAL_DMA_STATUS_OK;
no_mem:
    for (i = 0; i < periods; i++) {
	if (!l_item[i])
		continue;
	_dma_put_lli(l_item[i]);
    }
    return HAL_DMA_STATUS_NO_MEM;
}

hal_dma_status_t hal_dma_callback_install(struct sunxi_dma_chan *chan,
					  dma_callback callback,
					  void *callback_param)
{
    unsigned long __cpsr;

    if (NULL == chan)
    {
        DMA_ERR("[dma] chan is NULL\n");
        return HAL_DMA_STATUS_INVALID_PARAMETER;
    }

    if (NULL == callback)
    {
        DMA_ERR("[dma] callback is NULL\n");
        return HAL_DMA_STATUS_INVALID_PARAMETER;
    }

    if (NULL == callback_param)
    {
        DMA_ERR("[dma] callback_param is NULL\n");
        return HAL_DMA_STATUS_INVALID_PARAMETER;
    }

    __cpsr = hal_spin_lock_irqsave(&chan->lock);
    chan->callback = callback;
    chan->callback_param = callback_param;
    hal_spin_unlock_irqrestore(&chan->lock, __cpsr);

    return HAL_DMA_STATUS_OK;
}

hal_dma_status_t hal_dma_slave_config(struct sunxi_dma_chan *chan,
				      struct dma_slave_config *config)
{
    unsigned long __cpsr;

    if (NULL == config || NULL == chan)
    {
        DMA_ERR("[dma] dma config is NULL\n");
        return HAL_DMA_STATUS_INVALID_PARAMETER;
    }

    __cpsr = hal_spin_lock_irqsave(&chan->lock);
    convert_burst(&config->src_maxburst);
    convert_burst(&config->dst_maxburst);
    memcpy((void *) & (chan->cfg), (void *)config, sizeof(struct dma_slave_config));
    hal_spin_unlock_irqrestore(&chan->lock, __cpsr);

    return HAL_DMA_STATUS_OK;
}

enum dma_status hal_dma_tx_status(struct sunxi_dma_chan *chan, uint32_t *left_size)
{
    uint32_t i = 0;
    struct sunxi_dma_lli *l_item = NULL;
    enum dma_status status = DMA_INVALID_PARAMETER;
    unsigned long __cpsr;

    if (NULL == chan || NULL == left_size)
    {
        DMA_ERR("[dma] chan or left_size is NULL\n");
        return HAL_DMA_STATUS_INVALID_PARAMETER;
    }

    __cpsr = hal_spin_lock_irqsave(&chan->lock);
    if (chan->cyclic)
    {
        for (i = 0, l_item = chan->desc; i <= chan->periods_pos; i++, l_item = l_item->vlln)
        {
            if (NULL == l_item)
            {
                *left_size = 0;
                status = DMA_COMPLETE;
                goto out;
            }
        }
        if (NULL == l_item)
        {
            *left_size = 0;
            status = DMA_COMPLETE;
        }
        else
        {
            uint32_t pos = 0;
            bool count = false;

            pos = hal_readl(DMA_LLI_ADDR(chan->chan_count));
            *left_size = hal_readl(DMA_CNT(chan->chan_count));
            if (pos == LINK_END)
            {
                status = DMA_COMPLETE;
                goto out;
            }
            for (l_item = chan->desc; l_item != NULL; l_item = l_item->vlln)
            {
                if (l_item->p_lln == pos)
                {
                    count = true;
                    continue;
                }
                if (count)
                {
                    *left_size += l_item->len;
                }
            }
            status = DMA_IN_PROGRESS;
        }
    }
    else
    {
        *left_size = hal_readl(DMA_CNT(chan->chan_count));

        if (*left_size == 0)
        {
            status = DMA_COMPLETE;
        }
        else
        {
            status = DMA_IN_PROGRESS;
        }
    }

out:
    hal_spin_unlock_irqrestore(&chan->lock, __cpsr);

    return status;
}

static void hexdump(FAR const void *addr, size_t size)
{
	char buf[96];
	unsigned int i = 0, off = 0;
	unsigned int *ptr = (unsigned int *)addr;

	for (i = 0; i < size; i += sizeof(*ptr)) {
		if ((i % 32) == 0) {
			if (off) {
				buf[off] = 0;
				syslog(LOG_ERR, "%s", buf);
				off = 0;
			}
			off += sprintf(buf + off, "%08x: ", i);
		}
		off += sprintf(buf + off, "%08x ", ptr[i / sizeof(*ptr)]);
	}

	if (off) {
		buf[off] = 0;
		syslog(LOG_ERR, "%s", buf);
		off = 0;
	}
}


static void dma_err_dump_com_regs(void)
{
    syslog(LOG_ERR, "Common register:\n"
           "\tmask0: 0x%08x\n"
           "\tmask1: 0x%08x\n"
           "\tpend0: 0x%08x\n"
           "\tpend1: 0x%08x\n"
#ifdef DMA_SECURE
           "\tsecur: 0x%08x\n"
#endif
#ifdef DMA_GATE
           "\t_gate: 0x%08x\n"
#endif
           "\tstats: 0x%08x\n",
           (unsigned int)hal_readl(DMA_IRQ_EN(0)),
           (unsigned int)hal_readl(DMA_IRQ_EN(1)),
           (unsigned int)hal_readl(DMA_IRQ_STAT(0)),
           (unsigned int)hal_readl(DMA_IRQ_STAT(1)),
#ifdef DMA_SECURE
           (unsigned int)hal_readl(DMA_SECURE),
#endif
#ifdef DMA_GATE
           (unsigned int)hal_readl(DMA_GATE),
#endif
           (unsigned int)hal_readl(DMA_STAT));
}

static inline void dma_err_dump_chan_regs(struct sunxi_dma_chan *ch)
{
    u32 chan_num = ch->chan_count;
    syslog(LOG_ERR, "Chan %lu reg:\n"
           "\t___en: \t0x%08x\n"
           "\tpause: \t0x%08x\n"
           "\tstart: \t0x%08x\n"
           "\t__cfg: \t0x%08x\n"
           "\t__src: \t0x%08x\n"
           "\t__dst: \t0x%08x\n"
           "\tcount: \t0x%08x\n"
           "\t_para: \t0x%08x\n\n",
           chan_num,
           (unsigned int)hal_readl(DMA_ENABLE(chan_num)),
           (unsigned int)hal_readl(DMA_PAUSE(chan_num)),
           (unsigned int)hal_readl(DMA_LLI_ADDR(chan_num)),
           (unsigned int)hal_readl(DMA_CFG(chan_num)),
           (unsigned int)hal_readl(DMA_CUR_SRC(chan_num)),
           (unsigned int)hal_readl(DMA_CUR_DST(chan_num)),
           (unsigned int)hal_readl(DMA_CNT(chan_num)),
           (unsigned int)hal_readl(DMA_PARA(chan_num)));
}

void hal_dma_dump(struct sunxi_dma_chan *chan)
{
	unsigned long __cpsr;
	struct sunxi_dma_lli desc;
#if 1
#else
	size_t i = 0;
#endif

	dma_err_dump_com_regs();

	if (!chan)
		return;

	dma_err_dump_chan_regs(chan);

	syslog(LOG_ERR, "chan mem: %lx", (unsigned long)chan);
	hexdump(chan, sizeof(*chan));

	__cpsr = hal_spin_lock_irqsave(&chan->lock);

#if 1
	memcpy(&desc, chan->desc, sizeof(desc));
#else
	for (prev = chan->desc; prev != NULL; prev = prev->vlln) {
		syslog(LOG_ERR, "chan desc[%lu]: %lx", (unsigned long)i, (unsigned long)prev);
		hexdump(prev, sizeof(*prev));
		i++;
	}
#endif

	hal_spin_unlock_irqrestore(&chan->lock, __cpsr);

#if 1
	syslog(LOG_ERR, "chan desc: %lx", (unsigned long)chan->desc);
	hexdump(&desc, sizeof(desc));
#endif
}

hal_dma_status_t hal_dma_start(struct sunxi_dma_chan *chan)
{
    uint32_t high = 0;
    uint32_t irq_val = 0;
    struct sunxi_dma_lli *prev = NULL;
    uint32_t desc_addr;
    unsigned long __cpsr, __ccpsr;

    if (NULL == chan)
    {
        DMA_ERR("[dma] chan is NULL\n");
        return HAL_DMA_STATUS_INVALID_PARAMETER;
    }

    if (chan->desc == NULL)
    {
        DMA_ERR("[dma] chan->desc is NULL\n");
        return HAL_DMA_STATUS_INVALID_PARAMETER;
    }

    __cpsr = hal_spin_lock_irqsave(&chan->lock);

    if (chan->cyclic)
        chan->irq_type = IRQ_PKG;
    else
        chan->irq_type = IRQ_QUEUE;

    high = (chan->chan_count + START_CHAN_OFFSET >= HIGH_CHAN) ? 1 : 0;

    __ccpsr = hal_spin_lock_irqsave(&dma_lock);
    irq_val = hal_readl(DMA_IRQ_EN(high));
    irq_val |= SHIFT_IRQ_MASK(chan->irq_type, chan->chan_count);
    hal_writel(irq_val, DMA_IRQ_EN(high));
    hal_spin_unlock_irqrestore(&dma_lock, __ccpsr);

    /* FlashCtrl cannot support handshake mode  */
    /* SET_OP_MODE(chan->chan_count, SRC_HS_MASK | DST_HS_MASK); */

    for (prev = chan->desc; prev != NULL; prev = prev->vlln)
    {
        hal_dcache_clean((unsigned long)prev, sizeof(*prev));
        /* k_dcache_clean(prev, sizeof(*prev)); */
        //k_dcache_clean(prev->src, prev->len);
        //k_dcache_clean_invalidate(prev->dst, prev->len);
    }
    desc_addr = __va_to_pa((unsigned long)chan->desc) - SUNXI_DMA_PHYADDR_OFFSET;
    hal_writel(SET_DESC_HIGH_ADDR(desc_addr), DMA_LLI_ADDR(chan->chan_count));
    hal_writel(CHAN_START, DMA_ENABLE(chan->chan_count));
    sunxi_dump_com_regs();
    sunxi_dump_chan_regs(chan);
    hal_spin_unlock_irqrestore(&chan->lock, __cpsr);

    return HAL_DMA_STATUS_OK;
}

hal_dma_status_t hal_dma_stop(struct sunxi_dma_chan *chan)
{
    unsigned long __cpsr;

    if (NULL == chan)
    {
        DMA_ERR("[dma] chan is NULL\n");
        return HAL_DMA_STATUS_INVALID_PARAMETER;
    }

    __cpsr = hal_spin_lock_irqsave(&chan->lock);
    /*We should entry PAUSE state first to avoid missing data
    * count witch transferring on bus.
    */
    hal_writel(CHAN_PAUSE, DMA_PAUSE(chan->chan_count));
    hal_writel(CHAN_STOP, DMA_ENABLE(chan->chan_count));
    hal_writel(CHAN_RESUME, DMA_PAUSE(chan->chan_count));

    if (chan->cyclic)
    {
        chan->cyclic = false;
    }
    hal_spin_unlock_irqrestore(&chan->lock, __cpsr);

    return HAL_DMA_STATUS_OK;
}

hal_dma_status_t hal_dma_quick_start(struct sunxi_dma_chan *chan, struct dma_slave_config *config,
				     uint32_t len1, uint32_t len2)
{
	uint32_t high = 0;
	uint32_t val = 0;
	uint32_t reuse = 0;
	struct sunxi_dma_lli lli;
	struct sunxi_dma_lli *l_item1 = NULL, *l_item2 = NULL;
	uint32_t desc_addr;
	uint32_t vdst, dst, vsrc, src, chan_num, irq_mask, slave_id;
	enum dma_transfer_direction dir;
	unsigned long __cpsr, __ccpsr;
#if 1
	volatile uint32_t *reg;
	uint32_t reg_data;

	reg = (volatile uint32_t *)(0x2000000 + 0xd0); // PE_DAT
#endif

	ASSERT(!in_interrupt());

	if (NULL == chan) {
		DMA_ERR("[dma] chan is NULL\n");
		return HAL_DMA_STATUS_INVALID_PARAMETER;
	}

	if (chan->desc != NULL) {
		DMA_ERR("[dma] chan->desc exist!\n");
		return HAL_DMA_STATUS_ERR_PERM;
	}

	if (NULL == config) {
		DMA_ERR("[dma] dma config is NULL\n");
		return HAL_DMA_STATUS_INVALID_PARAMETER;
	}

	if (len1 & 63) {
		DMA_ERR("[dma] len1 is invalid\n");
		return HAL_DMA_STATUS_INVALID_PARAMETER;
	}

	dst = config->dst_addr;
	src = config->src_addr;
	dir = config->direction;
	slave_id = config->slave_id;
	vdst = dst;
	vsrc = src;

	if (!dst || !src || dir == DMA_DEV_TO_DEV) {
		DMA_ERR("[dma] dma config is invalid\n");
		return HAL_DMA_STATUS_INVALID_PARAMETER;
	}

	if (dir == DMA_MEM_TO_DEV && src & 63) {
		DMA_ERR("[dma] dma src_addr is not align to cache\n");
		return HAL_DMA_STATUS_INVALID_PARAMETER;
	}

	if (dir == DMA_DEV_TO_MEM && dst & 63) {
		DMA_ERR("[dma] dma dst_addr is not align to cache\n");
		return HAL_DMA_STATUS_INVALID_PARAMETER;
	}

	l_item1 = _dma_get_lli();
	if (!l_item1)
		return HAL_DMA_STATUS_NO_MEM;

	memset(l_item1, 0, sizeof(struct sunxi_dma_lli));

	__cpsr = hal_spin_lock_irqsave(&chan->lock);

	// prepare first desc
	convert_burst(&config->src_maxburst);
	convert_burst(&config->dst_maxburst);
	memcpy((void *) & (chan->cfg), (void *)config, sizeof(struct dma_slave_config));
	config = &(chan->cfg);

	chan_num = chan->chan_count;

	reg_data = *reg & (~(1<<4)); *reg = reg_data; // PE4 low
	if (dir == DMA_MEM_TO_DEV) {
		src = __va_to_pa(src);
		sunxi_cfg_lli(l_item1, src, dst, len1, config);
		l_item1->cfg |= GET_DST_DRQ(slave_id) \
			     | SRC_LINEAR_MODE \
			     | DST_IO_MODE \
			     | SRC_DRQ(DRQSRC_SDRAM);
		hal_dcache_clean((unsigned long)vsrc, len1);
	} else if (dir == DMA_DEV_TO_MEM) {
		dst = __va_to_pa(dst);
		sunxi_cfg_lli(l_item1, src, dst, len1, config);
		l_item1->cfg |= GET_SRC_DRQ(slave_id)  \
			     | DST_LINEAR_MODE \
			     | SRC_IO_MODE \
			     | DST_DRQ(DRQSRC_SDRAM);
		hal_dcache_invalidate((unsigned long)vdst, len1);
	}
	reg_data = *reg & (~(1<<4)); *reg = reg_data | (1<<4); // PE4 high
	l_item1->p_lln = LINK_END;
	l_item1->vlln = NULL;
	chan->desc = l_item1;
	hal_dcache_clean((unsigned long)l_item1, sizeof(*l_item1));

	reg_data = *reg & (~(1<<4)); *reg = reg_data; // PE4 low

	chan->cyclic = false;
	chan->irq_type = IRQ_QUEUE;

	irq_mask = SHIFT_IRQ_MASK(IRQ_QUEUE, chan_num);
	high = (chan_num + START_CHAN_OFFSET >= HIGH_CHAN) ? 1 : 0;

	desc_addr = __va_to_pa((unsigned long)chan->desc) - SUNXI_DMA_PHYADDR_OFFSET;
	hal_writel(SET_DESC_HIGH_ADDR(desc_addr), DMA_LLI_ADDR(chan_num));
	hal_writel(CHAN_START, DMA_ENABLE(chan_num));

	//disable dma chan irq for quick process
	__ccpsr = hal_spin_lock_irqsave(&dma_lock);
	val = hal_readl(DMA_IRQ_EN(high));
	val &= ~irq_mask;
	hal_writel(val, DMA_IRQ_EN(high));
	hal_spin_unlock_irqrestore(&dma_lock, __ccpsr);

	reg_data = *reg & (~(1<<4)); *reg = reg_data | (1<<4); // PE4 high

	l_item2 = _dma_get_lli();
	if (!l_item2) {
		reuse = 1;
		l_item2 = &lli;
	}
	memset(l_item2, 0, sizeof(*l_item2));

	//prepare second desc
	if (dir == DMA_MEM_TO_DEV) {
		sunxi_cfg_lli(l_item2, src + len1, dst, len2, config);
		l_item2->cfg |= GET_DST_DRQ(slave_id) \
			     | SRC_LINEAR_MODE \
			     | DST_IO_MODE \
			     | SRC_DRQ(DRQSRC_SDRAM);
		hal_dcache_clean((unsigned long)vsrc + len1, len2);
	} else if (dir == DMA_DEV_TO_MEM) {
		sunxi_cfg_lli(l_item2, src, dst + len1, len2, config);
		l_item2->cfg |= GET_SRC_DRQ(slave_id)  \
			     | DST_LINEAR_MODE \
			     | SRC_IO_MODE \
			     | DST_DRQ(DRQSRC_SDRAM);
		hal_dcache_invalidate((unsigned long)vdst + len1, len2);
	}
	l_item2->p_lln = LINK_END;
	l_item2->vlln = NULL;

	if (reuse == 0)
		hal_dcache_clean((unsigned long)l_item2, sizeof(*l_item2));

	reg_data = *reg & (~(1<<4)); *reg = reg_data; // PE4 low
	// wait first dma irq pending
	while (1) {
		val = hal_readl(DMA_IRQ_STAT(high));
		if (val & irq_mask) {
			// clear dma irq pending and reset
			hal_writel(irq_mask, DMA_IRQ_STAT(high));
			hal_writel(CHAN_PAUSE, DMA_PAUSE(chan_num));
			hal_writel(CHAN_STOP, DMA_ENABLE(chan_num));
			hal_writel(CHAN_RESUME, DMA_PAUSE(chan_num));
			break;
		}
	}
	reg_data = *reg & (~(1<<4)); *reg = reg_data | (1<<4); // PE4 high

	if (reuse == 0) {
		// change desc
		chan->desc = l_item2;
		desc_addr = __va_to_pa((unsigned long)chan->desc) - SUNXI_DMA_PHYADDR_OFFSET;
		hal_writel(SET_DESC_HIGH_ADDR(desc_addr), DMA_LLI_ADDR(chan_num));
	} else {
		// replace desc
		memcpy(l_item1, l_item2, sizeof(*l_item1));
		l_item2 = l_item1;
		hal_dcache_clean((unsigned long)l_item2, sizeof(*l_item2));
	}
	hal_writel(CHAN_START, DMA_ENABLE(chan_num));

	//enable dma chan irq for normal process
	__ccpsr = hal_spin_lock_irqsave(&dma_lock);
	val = hal_readl(DMA_IRQ_EN(high));
	val |= irq_mask;
	hal_writel(val, DMA_IRQ_EN(high));
	hal_spin_unlock_irqrestore(&dma_lock, __ccpsr);

	hal_spin_unlock_irqrestore(&chan->lock, __cpsr);

	if (reuse == 0)
		_dma_put_lli(l_item1);

	return HAL_DMA_STATUS_OK;
}

hal_dma_status_t hal_dma_chan_free(struct sunxi_dma_chan *chan)
{
    uint32_t high = 0;
    unsigned long irq_val = 0, __cpsr, __ccpsr;

    if (NULL == chan)
    {
        DMA_ERR("[dma] chan is NULL\n");
        return HAL_DMA_STATUS_INVALID_PARAMETER;
    }

    if (!chan->used)
    {
        return HAL_DMA_STATUS_INVALID_PARAMETER;
    }

    ASSERT(!in_interrupt());

    __cpsr = hal_spin_lock_irqsave(&chan->lock);

    high = (chan->chan_count + START_CHAN_OFFSET >= HIGH_CHAN) ? 1 : 0;

    __ccpsr = hal_spin_lock_irqsave(&dma_lock);
    irq_val = hal_readl(DMA_IRQ_EN(high));
    irq_val &= ~(SHIFT_IRQ_MASK(chan->irq_type, chan->chan_count));
    hal_writel(irq_val, DMA_IRQ_EN(high));
    hal_spin_unlock_irqrestore(&dma_lock, __ccpsr);
    chan->used = 0;
    hal_spin_unlock_irqrestore(&chan->lock, __cpsr);

    sunxi_dma_free_ill(chan);

    return HAL_DMA_STATUS_OK;
}

hal_dma_status_t hal_dma_chan_desc_free(struct sunxi_dma_chan *chan)
{
    ASSERT(!in_interrupt());

    sunxi_dma_free_ill(chan);

    return HAL_DMA_STATUS_OK;
}

void hal_dma_resource_get(void)
{
    uint32_t i = 0, high = 0, secure = 0;

    memset((void *)dma_chan_source, 0, NR_MAX_CHAN * sizeof(struct sunxi_dma_chan));

    hal_spin_lock_init(&dma_lock);

    /* disable auto gating */
    hal_writel(DMA_MCLK_GATE | DMA_COMMON_GATE | DMA_CHAN_GATE, DMA_GATE);
    sunxi_dma_clk_init(true);

    for (i = 0; i < NR_MAX_CHAN; i++)
        hal_spin_lock_init(&dma_chan_source[i].lock);

    for (i = START_CHAN_OFFSET + SUNXI_DMA_CHAN_START; i < START_CHAN_OFFSET + NR_MAX_CHAN; i++)
    {
        high = (i >= HIGH_CHAN) ? 1 : 0;
        /*disable all dma irq*/
        hal_writel(0, DMA_IRQ_EN(high));
        /*clear all dma irq pending*/
        hal_writel(0xffffffff, DMA_IRQ_STAT(high));

#ifdef ENABLE_SECURE_DMA
        /*set secure*/
        secure = hal_readl(DMA_SECURE);
        if ((secure & (1 << i)) != 0)
            hal_writel(secure & (~(1 << i)), DMA_SECURE);
#else
        /*set non-secure*/
        secure = hal_readl(DMA_SECURE);
        if ((secure & (1 << i)) == 0)
            hal_writel(secure | (1 << i), DMA_SECURE);
#endif
    }

    /*request dma irq*/
#ifdef ENABLE_SECURE_DMA
    if (hal_request_irq(DMA_SECURE_IRQ_NUM, sunxi_dma_irq_handle, "dma-s", NULL) < 0)
    {
        DMA_ERR("[dma] request secure irq error\n");
    }
    hal_enable_irq(DMA_SECURE_IRQ_NUM);
#else
    if (hal_request_irq(DMA_IRQ_NUM, sunxi_dma_irq_handle, "dma", NULL) < 0)
    {
        DMA_ERR("[dma] request irq error\n");
    }
    hal_enable_irq(DMA_IRQ_NUM);
#endif
}

#ifdef CONFIG_COMPONENTS_PM
static int hal_dma_suspend(void *date, suspend_mode_t mode)
{
    uint32_t i = 0, high = 0;

#ifdef ENABLE_SECURE_DMA
    hal_disable_irq(DMA_SECURE_IRQ_NUM);
#else
    hal_disable_irq(DMA_IRQ_NUM);
#endif
    for (i = START_CHAN_OFFSET + SUNXI_DMA_CHAN_START; i < START_CHAN_OFFSET + NR_MAX_CHAN; i++)
    {
        high = (i >= HIGH_CHAN) ? 1 : 0;
        /*disable all dma irq*/
        hal_writel(0, DMA_IRQ_EN(high));
        hal_writel(CHAN_PAUSE, DMA_PAUSE(i));
        hal_writel(CHAN_STOP, DMA_ENABLE(i));
        /*clear all dma irq pending*/
        hal_writel(0xffffffff, DMA_IRQ_STAT(high));
    }

    sunxi_dma_clk_init(false);

    for (i = 0; i < NR_MAX_CHAN; i++)
        hal_spin_lock_deinit(&dma_chan_source[i].lock);

    hal_spin_lock_deinit(&dma_lock);

    pm_inf("support dma suspend\n");
    return 0;
}
static void hal_dma_resume(void *data, suspend_mode_t mode)
{
    hal_dma_resource_get();
    pm_inf("support dma resume\n");
}
struct syscore_ops pm_dma_ops = {
    .name = "sunxi_pm_dma",
    .suspend = hal_dma_suspend,
    .resume = hal_dma_resume,
};
#endif

/* only need to be executed once */
void hal_dma_init(void)
{
    hal_dma_resource_get();
#ifdef CONFIG_COMPONENTS_PM
    pm_syscore_register(&pm_dma_ops);
#endif
}

