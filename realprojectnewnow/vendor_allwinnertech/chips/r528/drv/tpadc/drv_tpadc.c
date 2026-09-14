/* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.

 * Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
 * the the People's Republic of China and other countries.
 * All Allwinner Technology Co.,Ltd. trademarks are used with permission.

 * DISCLAIMER
 * THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
 * IF YOU NEED TO INTEGRATE THIRD PARTY’S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
 * IN ALLWINNERS’SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
 * ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
 * ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
 * COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
 * YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY’S TECHNOLOGY.


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

#include <nuttx/config.h>
#include <nuttx/arch.h>

#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <debug.h>

#include <string.h>

#include <nuttx/irq.h>
#include <nuttx/clock.h>
#include <nuttx/analog/adc.h>
#include <nuttx/analog/ioctl.h>
#include <arch/board/board.h>
#include <hal_workqueue.h>
#include "sunxi_hal_tpadc.h"
#include <nuttx/input/touchscreen.h>
#include <../../nuttx/drivers/input/touchscreen_upper.c>
#include <nuttx/clock.h>
#include <debug.h>

#if defined(CONFIG_DRIVERS_TPADC)

#include <stdint.h>
#include <stdbool.h>

#define TOUCH_BUFFER_SIZE 64  /* 缓冲区大小，增大以应对高频中断 */
#define TP_BUFFER_RESERVED 8  /* 为重要事件(UP/DOWN)保留的缓冲区空间 */
#define TP_JITTER_THRESHOLD 5 /* 防抖阈值(像素) */

/* 触摸事件结构 */
typedef struct
{
    data_flag_t type;
    uint16_t screen_x;
    uint16_t screen_y;
} tp_event_t;

/* 环形缓冲区 */
typedef struct
{
    tp_event_t buffer[TOUCH_BUFFER_SIZE];
    uint32_t head;  /* 写入位置 */
    uint32_t tail;  /* 读取位置 */
    uint32_t count; /* 当前事件数量 */
} touch_buffer_t;

/* 缓冲区管理函数 */
void touch_buffer_init(touch_buffer_t *buf);
bool touch_buffer_push(touch_buffer_t *buf, data_flag_t type, uint16_t x, uint16_t y);
bool touch_buffer_pop(touch_buffer_t *buf, tp_event_t *event);
bool touch_buffer_is_empty(touch_buffer_t *buf);
bool touch_buffer_is_full(touch_buffer_t *buf);

/* 初始化缓冲区 */
void touch_buffer_init(touch_buffer_t *buf)
{
    if (!buf)
        return;

    buf->head = 0;
    buf->tail = 0;
    buf->count = 0;

    for (int i = 0; i < TOUCH_BUFFER_SIZE; i++)
    {
        buf->buffer[i].type = DATA_MOVE;
        buf->buffer[i].screen_x = 0;
        buf->buffer[i].screen_y = 0;
    }

    TPADC_INFO("Touch buffer initialized, size=%d\n", TOUCH_BUFFER_SIZE);
}

/* 压入事件到缓冲区（中断中调用） */
bool touch_buffer_push(touch_buffer_t *buf, data_flag_t type, uint16_t x, uint16_t y)
{
    irqstate_t flags;

    if (!buf)
        return false;

    flags = enter_critical_section();

    /* 改进：缓冲区保留策略，防止 UP 事件被 MOVE 事件淹没 */
    if (buf->count >= (TOUCH_BUFFER_SIZE - TP_BUFFER_RESERVED))
    {
        /* 如果不是重要事件(UP)，且缓冲区快满，则丢弃 */
        if (type == DATA_MOVE)
        {
            leave_critical_section(flags);
            return false;
        }
    }

    if (buf->count >= TOUCH_BUFFER_SIZE)
    {
        leave_critical_section(flags);
        TPADC_INFO("Touch buffer overflow!\n");
        return false;
    }

    uint32_t next_head = (buf->head + 1) % TOUCH_BUFFER_SIZE;

    buf->buffer[buf->head].type = type;
    buf->buffer[buf->head].screen_x = x;
    buf->buffer[buf->head].screen_y = y;

    buf->head = next_head;
    buf->count++;

    leave_critical_section(flags);

    return true;
}

/* 从缓冲区弹出事件（工作线程中调用） */
bool touch_buffer_pop(touch_buffer_t *buf, tp_event_t *event)
{
    irqstate_t flags;

    if (!buf || !event)
        return false;

    flags = enter_critical_section();

    if (buf->count == 0)
    {
        leave_critical_section(flags);
        return false;
    }

    *event = buf->buffer[buf->tail];

    buf->tail = (buf->tail + 1) % TOUCH_BUFFER_SIZE;
    buf->count--;

    leave_critical_section(flags);

    return true;
}

/* 查看缓冲区下一个事件但不弹出 */
bool touch_buffer_peek(touch_buffer_t *buf, tp_event_t *event)
{
    irqstate_t flags;

    if (!buf || !event)
        return false;

    flags = enter_critical_section();

    if (buf->count == 0)
    {
        leave_critical_section(flags);
        return false;
    }

    *event = buf->buffer[buf->tail];

    leave_critical_section(flags);

    return true;
}

/* 检查缓冲区是否为空 */
bool touch_buffer_is_empty(touch_buffer_t *buf)
{
    return buf ? (buf->count == 0) : true;
}

/* 检查缓冲区是否已满 */
bool touch_buffer_is_full(touch_buffer_t *buf)
{
    return buf ? (buf->count >= TOUCH_BUFFER_SIZE) : true;
}

/* 驱动私有数据 */
typedef struct tpadc_driver
{
    hal_tpadc_t tpadc;

    touch_buffer_t touch_buf;
    tp_event_t last_event;
    bool touch_active;
    bool down_pending;     /* 标记收到了DOWN中断，等待MOVE坐标数据 */
    uint32_t last_up_tick; /* 记录上次抬起时间，用于抑制反弹的一一“连击” */

    /* 屏幕参数 */
    uint16_t screen_width;
    uint16_t screen_height;
    uint16_t x_min, x_max;
    uint16_t y_min, y_max;
    uint16_t pressure_threshold;

    /* 工作队列 */
    void *workqueue;
    hal_work work;

    /* OpenVela触摸设备 */
#ifdef CONFIG_INPUT_TOUCHSCREEN
    struct touch_lowerhalf_s lower;
#endif
    /* 设备节点名称 */
    char dev_name[32];
} tpadc_driver_t;

static tpadc_driver_t *tpadc_driver = NULL;
static void tpadc_cleanup(bool hw_init_done);
/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int r528_tp_control(FAR const struct touch_lowerhalf_s *lower,
                           int cmd, unsigned long arg);

static ssize_t r528_tp_write(FAR struct touch_lowerhalf_s *lower, FAR const char *buffer, size_t buflen);
static void tpadc_irq_callback(uint16_t x, uint16_t y, uint32_t directly);

/****************************************************************************
 * Private Data
 ****************************************************************************/
static int r528_tp_control(FAR const struct touch_lowerhalf_s *lower,
                           int cmd, unsigned long arg) { return 0; }

static ssize_t input_touch_notify(FAR void *uinput_lower,
                                  FAR const char *buffer, size_t buflen)
{
    return buflen;
}

static ssize_t r528_tp_write(FAR struct touch_lowerhalf_s *lower, FAR const char *buffer, size_t buflen)
{
    return 0;
}

static const struct touch_lowerhalf_s g_touchlower = {
    .control = r528_tp_control,
    .write = r528_tp_write,
    .maxpoint = 1,
};

static void tpadc_irq_callback(uint16_t x, uint16_t y, uint32_t directly)
{
    tpadc_driver_t *driver = tpadc_driver;

    if (driver == NULL)
        return;

    if ((driver->x_max <= driver->x_min) || (driver->y_max <= driver->y_min) ||
        driver->screen_width == 0 || driver->screen_height == 0)
        return;

    uint16_t screen_x, screen_y;
    screen_x = (y - driver->x_min) * driver->screen_width / (driver->x_max - driver->x_min);
    screen_y = (x - driver->y_min) * driver->screen_height / (driver->y_max - driver->y_min);
    screen_y = driver->screen_height - screen_y;

    if (!touch_buffer_push(&driver->touch_buf, directly,
                      screen_x,
                          screen_y))
        return;

    if (driver->workqueue)
        hal_workqueue_dowork(driver->workqueue, &driver->work);
}

static void tpadc_work_handler(hal_work *work, void *data)
{
#ifdef CONFIG_INPUT_TOUCHSCREEN
    tpadc_driver_t *p_tpadc_driver = (tpadc_driver_t *)data;
    if (!p_tpadc_driver->lower.priv)
        return;

    while (true)
    {
        tp_event_t tp_event;
        struct touch_sample_s sample;

        memset(&tp_event, 0, sizeof(tp_event));
        if (!touch_buffer_pop(&p_tpadc_driver->touch_buf, &tp_event))
            break;

        memset(&sample, 0, sizeof(sample));
        sample.point[0].x = tp_event.screen_x;
        sample.point[0].y = tp_event.screen_y;
        sample.point[0].pressure = 100;
        sample.point[0].id = 0;
        sample.point[0].timestamp = clock_systime_ticks();
        sample.npoints = 1;

        if (tp_event.type == DATA_DOWN)
        {
            if (!p_tpadc_driver->touch_active)
            {
                /* 仅标记 DOWN 待处理，需要等待 MOVE 提供坐标 */
                p_tpadc_driver->down_pending = true;
                ainfo("Touch Down irq received, waiting for coords...\n");
            }
        }
        else if (tp_event.type == DATA_MOVE)
        {
            /* 场景 1: 正常流程，DOWN 之后紧接着收到 MOVE，上报 DOWN */
            if (p_tpadc_driver->down_pending)
            {
                ainfo("Touch Down at X=%d, Y=%d (with pending)\n", tp_event.screen_x, tp_event.screen_y);
                sample.point[0].flags = TOUCH_DOWN;
                touch_event(p_tpadc_driver->lower.priv, &sample);

                memcpy(&p_tpadc_driver->last_event, &tp_event, sizeof(tp_event_t));
                p_tpadc_driver->touch_active = true;
                p_tpadc_driver->down_pending = false;
            }
            /* 场景 2: 之前漏发了 DOWN, 或者通过 Auto-Down 恢复 */
            else if (!p_tpadc_driver->touch_active)
            {
                /* 只有距离上次UP超过一定时间(比如5个tick)才认为是新点击，过滤UP后的噪声 */
                uint32_t now = clock_systime_ticks();
                if (now - p_tpadc_driver->last_up_tick > 5)
                {
                    ainfo("Touch Auto-Down (from Move) at X=%d, Y=%d\n", tp_event.screen_x, tp_event.screen_y);
                    sample.point[0].flags = TOUCH_DOWN;
                    touch_event(p_tpadc_driver->lower.priv, &sample);

                    memcpy(&p_tpadc_driver->last_event, &tp_event, sizeof(tp_event_t));
                    p_tpadc_driver->touch_active = true;
                }
            }
            /* 场景 3: 正常的 MOVE 更新 */
            else
            {
                /* 防抖处理 */
                if (abs((int)tp_event.screen_x - (int)p_tpadc_driver->last_event.screen_x) > TP_JITTER_THRESHOLD ||
                    abs((int)tp_event.screen_y - (int)p_tpadc_driver->last_event.screen_y) > TP_JITTER_THRESHOLD)
                {
                    sample.point[0].flags = TOUCH_MOVE;
                    touch_event(p_tpadc_driver->lower.priv, &sample);
                    memcpy(&p_tpadc_driver->last_event, &tp_event, sizeof(tp_event_t));
                }
            }
        }
        else if (tp_event.type == DATA_UP)
        {
            /* 如果还在等待 DOWN 的坐标，却收到了 UP，说明是一次无效点击或噪声 */
            if (p_tpadc_driver->down_pending)
            {
                 /* 关键修正：保持 down_pending = true
                  * 理由：如果在 DOWN 后极短时间收到 UP（无 MOVE），可能是按下的瞬间产生了抖动噪声。
                  * 此时不应放弃等待，因为稍后可能紧接着会有真正的 DATA_MOVE 或 DATA_DOWN 到来。
                  * 如果真的松手了，后续不再有中断，down_pending 保持 true 也无害（下次触摸会重置）。
                  */
                 ainfo("Touch Up received while pending Down (ignored - noise filtering)\n");
            }
            else if (p_tpadc_driver->touch_active)
            {
                /* 预读下一个事件，如果紧接着又有 DOWN/MOVE，说明这是抖动，忽略本次 UP */
                tp_event_t next_event;
                bool has_next = touch_buffer_peek(&p_tpadc_driver->touch_buf, &next_event);

                if (has_next && (next_event.type == DATA_MOVE || next_event.type == DATA_DOWN))
                {
                    ainfo("Touch Up ignored (Glitch filter, next=%d)\n", next_event.type);
                    /* 忽略本次 UP，保持 Active 状态，让后续事件延续触摸 */
                }
                else
                {
                    ainfo("Touch Up at X=%d, Y=%d\n", p_tpadc_driver->last_event.screen_x, p_tpadc_driver->last_event.screen_y);
                    sample.point[0].x = p_tpadc_driver->last_event.screen_x;
                    sample.point[0].y = p_tpadc_driver->last_event.screen_y;
                    sample.point[0].flags = TOUCH_UP;
                    touch_event(p_tpadc_driver->lower.priv, &sample);

                    memcpy(&p_tpadc_driver->last_event, &tp_event, sizeof(tp_event_t));
                    p_tpadc_driver->touch_active = false;

                    /* 记录时间，用于抑制后续噪声 */
                    p_tpadc_driver->last_up_tick = clock_systime_ticks();
                }
            }
        }
    }
#endif
}

static void tpadc_cleanup(bool hw_init_done)
{
    if (!tpadc_driver)
        return;

    if (tpadc_driver->workqueue)
    {
        hal_workqueue_cancel_work(tpadc_driver->workqueue, &tpadc_driver->work);
        hal_workqueue_destroy(tpadc_driver->workqueue);
        tpadc_driver->workqueue = NULL;
    }

    if (hw_init_done)
        hal_tpadc_exit();

    free(tpadc_driver);
    tpadc_driver = NULL;
}

int r528_touchscreen_initialize(FAR const char *devname)
{
    int ret = -1;

    TPADC_INFO("TPADC hardware initialize enter\n");
    if (tpadc_driver != NULL)
    {
        return TPADC_OK; /* 已经初始化 */
    }

    /* 分配内存 */
    tpadc_driver = malloc(sizeof(tpadc_driver_t));
    if (tpadc_driver == NULL)
    {
        TPADC_ERR("Failed to allocate memory\n");
        return TPADC_ERROR;
    }

    memset(tpadc_driver, 0, sizeof(tpadc_driver_t));
    tpadc_driver->last_event.type = DATA_UP;
    tpadc_driver->touch_active = false;

    touch_buffer_init(&tpadc_driver->touch_buf);

    /* 初始化硬件配置 */
    tpadc_driver->tpadc.reg_base = 0x02009c00; /* 从设备树获取 */
    tpadc_driver->tpadc.irq_num = R528_IRQ_TPADC;
    tpadc_driver->tpadc.rate = 1000000; /* 1MHz */

    /* 默认屏幕参数 */
    tpadc_driver->screen_width = 320;
    tpadc_driver->screen_height = 240;

    /* 默认校准参数 */
    tpadc_driver->x_min = 100;
    tpadc_driver->x_max = 3900;
    tpadc_driver->y_min = 100;
    tpadc_driver->y_max = 3900;
    tpadc_driver->pressure_threshold = 50;

    /* 设置设备节点名称 */
    strcpy(tpadc_driver->dev_name, devname);

    tpadc_driver->lower = g_touchlower;

    // g_hw_version = hw_version_get();
    // syslog(LOG_INFO,"hardware version %d\n", g_hw_version);
    ret = hal_tpadc_init();
    if (ret)
    {
        TPADC_INFO("tpadc init failed!\n");
        tpadc_cleanup(false);
        return -1;
    }

    tpadc_driver->workqueue = hal_workqueue_create("tpadc_workqueue", 1024, 224);
    if (tpadc_driver->workqueue == NULL)
    {
        TPADC_ERR("Failed to create workqueue\n");
        tpadc_cleanup(true);
        return TPADC_ERROR;
    }

    /* 初始化工作项 */
    hal_work_init(&tpadc_driver->work, tpadc_work_handler, tpadc_driver);

    ret = touch_register(&tpadc_driver->lower, tpadc_driver->dev_name, tpadc_driver->lower.maxpoint);
    if (ret)
    {
        TPADC_INFO("touch_register init fail\n");
        tpadc_cleanup(true);
        return -ENODEV;
    }
    TPADC_INFO("touch_register init success!\n");

    hal_tpadc_register_callback(tpadc_irq_callback);

    return OK;
}

#endif
