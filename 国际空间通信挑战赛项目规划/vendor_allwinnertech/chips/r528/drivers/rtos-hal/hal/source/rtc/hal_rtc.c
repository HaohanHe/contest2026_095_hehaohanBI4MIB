/* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.

 * Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
 * the the People's Republic of China and other countries.
 * All Allwinner Technology Co.,Ltd. trademarks are used with permission.

 * DISCLAIMER
 * THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
 * IF YOU NEED TO INTEGRATE THIRD PARTY¡¯S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
 * IN ALLWINNERS¡¯SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
 * ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
 * ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
 * COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
 * YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY¡¯S TECHNOLOGY.


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
#include <debug.h>
#include <hal_thread.h>
#include <hal_timer.h>
#include <stdlib.h>
#include <rtc/rtc.h>
#include <sunxi_hal_rtc.h>
#include <hal_log.h>
#include <hal_interrupt.h>
#include <hal_clk.h>
#include <hal_mem.h>

//for debug
#define CONFIG_DRIVERS_RTC_DEBUG
#ifdef CONFIG_DRIVERS_RTC_DEBUG
#define RTC_INFO(fmt, arg...) hal_log_info(fmt, ##arg)
#else
#define RTC_INFO(fmt, arg...) do {}while(0)
#endif

#define RTC_ERR(fmt, arg...) hal_log_err(fmt, ##arg)

static struct hal_rtc_dev sunxi_hal_rtc;

static struct hal_rtc_data_year data_year_param =
{
    .min        = 1970,
    .max        = 2097,
    .mask       = 0x7f,
    .yshift     = 16,
    .leap_shift = 23,
};

static hal_rtc_status_t hal_rtc_clk_init(struct hal_rtc_dev *rtc)
{
#if defined(CONFIG_SOC_SUN20IW1)
    hal_clk_type_t clk_r_type = HAL_SUNXI_R_CCU;
    hal_clk_id_t rtc_clk_r_id = CLK_R_AHB_BUS_RTC;

    hal_clk_type_t clk_rtc1k_type = HAL_SUNXI_RTC_CCU;
    hal_clk_id_t rtc_clk_rtc1k_id = CLK_RTC_1K;

    hal_clk_type_t clk_rtcspi_type = HAL_SUNXI_RTC_CCU;
    hal_clk_id_t rtc_clk_rtcspi_id = CLK_RTC_SPI;

    hal_reset_type_t reset_type = HAL_SUNXI_RESET;
    hal_reset_id_t rtc_reset_id = RST_R_AHB_BUS_RTC;

    rtc->bus_clk = hal_clock_get(clk_r_type, rtc_clk_r_id);
    if(hal_clock_enable(rtc->bus_clk))
    {
	RTC_ERR("rtc bus clk enable failed!\n");
	return RTC_ERROR;
    }
    rtc->rtc1k_clk = hal_clock_get(clk_rtc1k_type, rtc_clk_rtc1k_id);
    if(hal_clock_enable(rtc->rtc1k_clk))
    {
	RTC_ERR("rtc 1k clk enable failed!\n");
	return RTC_ERROR;
    }
    rtc->rtcspi_clk = hal_clock_get(clk_rtcspi_type, rtc_clk_rtcspi_id);
    if(hal_clock_enable(rtc->rtcspi_clk))
    {
	RTC_ERR("rtc spi clk enable failed!\n");
	return RTC_ERROR;
    }

    rtc->reset = hal_reset_control_get(reset_type, rtc_reset_id);
    if(hal_reset_control_deassert(rtc->reset))
    {
	RTC_ERR("rtc reset deassert failed!\n");
	return RTC_ERROR;
    }
#endif

    return RTC_OK;
}

void hal_rtc_write_data(int index, u32 val)
{
    hal_writel(val, (unsigned long)SUNXI_RTC_DATA_BASE + index * 4);
}

u32 hal_rtc_read_data(int index)
{
    return hal_readl((unsigned long)SUNXI_RTC_DATA_BASE + index * 4);
}

void hal_rtc_set_fel_flag(void)
{
    do
    {
        hal_rtc_write_data(RTC_FEL_INDEX, EFEX_FLAG);
        isb();
    } while (hal_rtc_read_data(RTC_FEL_INDEX) != EFEX_FLAG);
}

u32 hal_rtc_probe_fel_flag(void)
{
    return hal_rtc_read_data(RTC_FEL_INDEX) == EFEX_FLAG ? 1 : 0;
}

void hal_rtc_clear_fel_flag(void)
{
    do
    {
        hal_rtc_write_data(RTC_FEL_INDEX, 0);
        isb();
    } while (hal_rtc_read_data(RTC_FEL_INDEX) != 0);
}

int hal_rtc_set_bootmode_flag(u8 flag)
{
    do
    {
        hal_rtc_write_data(RTC_BOOT_INDEX, flag);
        isb();
    } while (hal_rtc_read_data(RTC_BOOT_INDEX) != flag);

    return 0;
}

int hal_rtc_get_bootmode_flag(void)
{
    uint boot_flag;

    boot_flag = hal_rtc_read_data(RTC_BOOT_INDEX);

    return boot_flag;
}


int hal_rtc_register_callback(rtc_callback_t user_callback)
{
    struct hal_rtc_dev *rtc_dev = &sunxi_hal_rtc;

    if (user_callback == NULL)
    {
        return -1;
    }

    rtc_dev->user_callback = user_callback;

    return 0;
}

int hal_rtc_register_callback_with_data(sunxi_rtc_alarm_callback alarm_callback, void *alarm_data)
{
    struct hal_rtc_dev *rtc_dev = &sunxi_hal_rtc;

    if (alarm_callback == NULL)
    {
        return -1;
    }
    if (alarm_data)
        rtc_dev->alarm_data = alarm_data;

    rtc_dev->alarm_callback = alarm_callback;

    return 0;
}

static hal_irqreturn_t hal_rtc_alarmirq(void *dev)
{
    struct hal_rtc_dev *rtc_dev = (struct hal_rtc_dev *)dev;
    u32 val;

    val = hal_readl(rtc_dev->base + SUNXI_ALRM_IRQ_STA);

    if (val & SUNXI_ALRM_IRQ_STA_CNT_IRQ_PEND)
    {
        val |= SUNXI_ALRM_IRQ_STA_CNT_IRQ_PEND;
        hal_writel(val, rtc_dev->base + SUNXI_ALRM_IRQ_STA);

        if (rtc_dev->alarm_callback)
        {
            rtc_dev->alarm_callback(rtc_dev->alarm_data);
            return 0;
        }
        else if (rtc_dev->user_callback)
        {
            rtc_dev->user_callback();
            return 0;
        }
    }

    return 1;
}

static void hal_rtc_setaie(int to, struct hal_rtc_dev *rtc_dev)
{
    u32 alrm_val = 0;
    u32 alrm_irq_val = 0;

    if (to)
    {
        alrm_val = hal_readl(rtc_dev->base + SUNXI_ALRM_EN);
        alrm_val |= SUNXI_ALRM_EN_CNT_EN;

        alrm_irq_val = hal_readl(rtc_dev->base + SUNXI_ALRM_IRQ_EN);
        alrm_irq_val |= SUNXI_ALRM_IRQ_EN_CNT_IRQ_EN;
    }
    else
    {
        hal_writel(SUNXI_ALRM_IRQ_STA_CNT_IRQ_PEND,
               rtc_dev->base + SUNXI_ALRM_IRQ_STA);
    }

    hal_writel(alrm_val, rtc_dev->base + SUNXI_ALRM_EN);
    hal_writel(alrm_irq_val, rtc_dev->base + SUNXI_ALRM_IRQ_EN);
}

static int hal_rtc_wait(int offset, unsigned int mask, unsigned int ms_timeout)
{
    struct hal_rtc_dev *rtc_dev = &sunxi_hal_rtc;
    u32 reg;
    unsigned int elapsed = 0;
    const unsigned int poll_interval = 1; /* 1ms poll interval */

    /* Poll until condition is met or timeout */
    while (elapsed < ms_timeout)
    {
        /* Read register value */
        reg = hal_readl(rtc_dev->base + offset);
        reg &= mask;

        /* Check if condition is met */
        if (reg == 0)
        {
            return 0; /* Condition met */
        }

        /* Sleep for poll interval */
        hal_msleep(poll_interval);
        elapsed += poll_interval;
    }

    /* Timeout - condition not met */
    return -1;
}

#ifdef SUNXI_SIMPLIFIED_TIMER
static short month_days[2][13] =
{
    {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
    {0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
};

static int hal_rtc_day_to_ymd(struct sunxi_rtc_time *rtc_tm, u32 min_year,
                              u32 udate)
{
    static u32 last_date;
    static int last_year, last_mon, last_mday;
    int year = 0, leap, i;
    int date = (int)udate;

    if (date == last_date)
    {
        rtc_tm->tm_mday = last_mday;
        rtc_tm->tm_mon = last_mon;
        rtc_tm->tm_year = last_year;
        return 0;
    }

    year = min_year;
    while (1)
    {
        if (is_leap_year(year))
        {
            if (date > 366)
            {
                year++;
                date -= 366;
            }
            else
            {
                break;
            }
        }
        else
        {
            if (date > 365)
            {
                year++;
                date -= 365;
            }
            else
            {
                break;
            }
        }
    }
    rtc_tm->tm_year = year - 1900;
    last_year = rtc_tm->tm_year;

    leap = is_leap_year(rtc_tm->tm_year);
    for (i = 1; date > month_days[leap][i]; i++)
    {
        date -= month_days[leap][i];
    }
    rtc_tm->tm_mon = i;
    last_mon = rtc_tm->tm_mon;
    rtc_tm->tm_mday = date;
    last_mday = rtc_tm->tm_mday;

    return 0;
}
#endif

int hal_rtc_gettime(struct sunxi_rtc_time *rtc_tm)
{
    struct hal_rtc_dev *rtc_dev = &sunxi_hal_rtc;
    u32 date, time;

    /*
     * read again in case it changes
     */
    do
    {
        date = hal_readl(rtc_dev->base + SUNXI_RTC_YMD);
        time = hal_readl(rtc_dev->base + SUNXI_RTC_HMS);
    } while ((date != hal_readl(rtc_dev->base + SUNXI_RTC_YMD)) ||
             (time != hal_readl(rtc_dev->base + SUNXI_RTC_HMS)));

    rtc_tm->tm_sec  = SUNXI_TIME_GET_SEC_VALUE(time);
    rtc_tm->tm_min  = SUNXI_TIME_GET_MIN_VALUE(time);
    rtc_tm->tm_hour = SUNXI_TIME_GET_HOUR_VALUE(time);

#ifndef SUNXI_SIMPLIFIED_TIMER
    rtc_tm->tm_mday = SUNXI_DATE_GET_DAY_VALUE(date);
    rtc_tm->tm_mon  = SUNXI_DATE_GET_MON_VALUE(date);
    rtc_tm->tm_year = SUNXI_DATE_GET_YEAR_VALUE(date, rtc_dev->data_year);

    /*
     * switch from (data_year->min)-relative offset to
     * a (1900)-relative one
     */
    rtc_tm->tm_year += SUNXI_YEAR_OFF(rtc_dev->data_year);
#else
    hal_rtc_day_to_ymd(rtc_tm, rtc_dev->data_year->min, date);
#endif
    rtc_tm->tm_mon  -= 1;

    RTC_INFO("Read hardware RTC time %04d-%02d-%02d %02d:%02d:%02d",
             rtc_tm->tm_year + 1900, rtc_tm->tm_mon + 1, rtc_tm->tm_mday,
             rtc_tm->tm_hour, rtc_tm->tm_min, rtc_tm->tm_sec);

    return 0;
}

int hal_rtc_convert_wday(struct sunxi_rtc_time *rtc_tm)
{
    time64_t time_sec;
    uint32_t days;

	time_sec = rtc_tm_to_time64(rtc_tm);
	days = (time_sec / SEC_IN_DAY) % 7;

   if (BASE_WDAY + days == 7)
        return 0;
   if (BASE_WDAY + days > 7)
        return BASE_WDAY + days - 7;

    return BASE_WDAY + days;
}

int hal_rtc_convert_yday(struct sunxi_rtc_time *rtc_tm)
{
    uint32_t day = rtc_tm->tm_mday;
    uint32_t month = rtc_tm->tm_mon;
    uint32_t year = rtc_tm->tm_year;

    return rtc_year_days(day, month, year);
}

int hal_rtc_settime(struct sunxi_rtc_time *rtc_tm)
{
    struct hal_rtc_dev *rtc_dev = &sunxi_hal_rtc;
    u32 date = 0;
    u32 time = 0;
    int year;
#ifdef SUNXI_SIMPLIFIED_TIMER
    int i, leap;
#endif
    /*
     * the input rtc_tm->tm_year is the offset relative to 1900. We use
     * the SUNXI_YEAR_OFF macro to rebase it with respect to the min year
     * allowed by the hardware
     */

    year = rtc_tm->tm_year + 1900;
    if (year < rtc_dev->data_year->min
        || year > rtc_dev->data_year->max)
    {
        RTC_ERR("rtc only supports year in range %d - %d",
                rtc_dev->data_year->min, rtc_dev->data_year->max);
        return -1;
    }

#ifndef SUNXI_SIMPLIFIED_TIMER
    rtc_tm->tm_year -= SUNXI_YEAR_OFF(rtc_dev->data_year);
    rtc_tm->tm_mon += 1;

    RTC_INFO("Will set hardware RTC time %04d-%02d-%02d %02d:%02d:%02d",
             rtc_tm->tm_year, rtc_tm->tm_mon, rtc_tm->tm_mday,
             rtc_tm->tm_hour, rtc_tm->tm_min, rtc_tm->tm_sec);

    date = SUNXI_DATE_SET_DAY_VALUE(rtc_tm->tm_mday) |
           SUNXI_DATE_SET_MON_VALUE(rtc_tm->tm_mon)  |
           SUNXI_DATE_SET_YEAR_VALUE(rtc_tm->tm_year, rtc_dev->data_year);

    if (is_leap_year(year))
    {
        date |= SUNXI_LEAP_SET_VALUE(1, rtc_dev->data_year->leap_shift);
    }
#else
    date = rtc_tm->tm_mday;
    rtc_tm->tm_mon += 1;
    leap = is_leap_year(year);
    for (i = 1; i < rtc_tm->tm_mon; i++)
    {
        date += month_days[leap][i];
    }

    for (i = year - 1; i >= rtc_dev->data_year->min; i--)
    {
        if (is_leap_year(i))
        {
            date += 366;
        }
        else
        {
            date += 365;
        }
    }
#endif

    time = SUNXI_TIME_SET_SEC_VALUE(rtc_tm->tm_sec)  |
           SUNXI_TIME_SET_MIN_VALUE(rtc_tm->tm_min)  |
           SUNXI_TIME_SET_HOUR_VALUE(rtc_tm->tm_hour);


    /*
     * before we write the RTC HH-MM-SS register,we
     * should check the SUNXI_LOSC_CTRL_RTC_HMS_ACC bit
     */
    if (hal_rtc_wait(SUNXI_LOSC_CTRL,
                     SUNXI_LOSC_CTRL_RTC_HMS_ACC, 50))
    {
        RTC_ERR("Failed to set rtc time.");
        return -1;
    }

    hal_writel(0, rtc_dev->base + SUNXI_RTC_HMS);
    /*
     * After writing the RTC HH-MM-SS register, the
     * SUNXI_LOSC_CTRL_RTC_HMS_ACC bit is set and it will not
     * be cleared until the real writing operation is finished
     */

    if (hal_rtc_wait(SUNXI_LOSC_CTRL, SUNXI_LOSC_CTRL_RTC_HMS_ACC, 50))
    {
        RTC_ERR("Failed to set rtc time.");
        return -1;
    }

    hal_writel(time, rtc_dev->base + SUNXI_RTC_HMS);

    /*
     * After writing the RTC HH-MM-SS register, the
     * SUNXI_LOSC_CTRL_RTC_HMS_ACC bit is set and it will not
     * be cleared until the real writing operation is finished
     */

    if (hal_rtc_wait(SUNXI_LOSC_CTRL, SUNXI_LOSC_CTRL_RTC_HMS_ACC, 50))
    {
        RTC_ERR("Failed to set rtc time.");
        return -1;
    }

    /*
     * After writing the RTC YY-MM-DD register, the
     * SUNXI_LOSC_CTRL_RTC_YMD_ACC bit is set and it will not
     * be cleared until the real writing operation is finished
     */

    if (hal_rtc_wait(SUNXI_LOSC_CTRL, SUNXI_LOSC_CTRL_RTC_YMD_ACC, 50))
    {
        RTC_ERR("Failed to set rtc time.");
        return -1;
    }
    hal_msleep(2);

    hal_writel(date, rtc_dev->base + SUNXI_RTC_YMD);

    /*
     * After writing the RTC YY-MM-DD register, the
     * SUNXI_LOSC_CTRL_RTC_YMD_ACC bit is set and it will not
     * be cleared until the real writing operation is finished
     */

    if (hal_rtc_wait(SUNXI_LOSC_CTRL, SUNXI_LOSC_CTRL_RTC_YMD_ACC, 50))
    {
        RTC_ERR("Failed to set rtc time.");
        return -1;
    }
    hal_msleep(2);

    return 0;
}

int hal_rtc_getalarm(struct rtc_wkalrm *wkalrm)
{
    struct hal_rtc_dev *rtc_dev = &sunxi_hal_rtc;
    struct sunxi_rtc_time *alrm_tm = &wkalrm->time;
    u32 alrm_en;
    unsigned int  alarm_cur = 0, alarm_cnt = 0;
    unsigned long alarm_seconds = 0;
    int ret;

#ifdef SUNXI_ALARM1_USED
    u32 alrm, date;
    alrm = hal_readl(rtc_dev->base + SUNXI_ALRM_DHMS);
    date = hal_readl(rtc_dev->base + SUNXI_RTC_YMD);

    alrm_tm->tm_sec = SUNXI_ALRM_GET_SEC_VALUE(alrm);
    alrm_tm->tm_min = SUNXI_ALRM_GET_MIN_VALUE(alrm);
    alrm_tm->tm_hour = SUNXI_ALRM_GET_HOUR_VALUE(alrm);

    alrm_tm->tm_mday = SUNXI_DATE_GET_DAY_VALUE(date);
    alrm_tm->tm_mon = SUNXI_DATE_GET_MON_VALUE(date);
    alrm_tm->tm_year = SUNXI_DATE_GET_YEAR_VALUE(date, rtc_dev->data_year);

    alrm_tm->tm_mon -= 1;

    /*
     * switch from (data_year->min)-relative offset to
     * a (1900)-relative one
     */
    alrm_tm->tm_year += SUNXI_YEAR_OFF(rtc_dev->data_year);
#else
    u32 alarm_hms, alarm_diff;
    u32 alarm_hour, alarm_min, alarm_sec;

    alarm_cur = hal_readl(rtc_dev->base + SUNXI_RTC_YMD);
    alarm_cnt = hal_readl(rtc_dev->base + SUNXI_ALRM_DAY);
    alarm_hms = hal_readl(rtc_dev->base + SUNXI_ALRM_HMS);
    alarm_hour = SUNXI_ALRM_GET_HOUR_VALUE(alarm_hms);
    alarm_min = SUNXI_ALRM_GET_MIN_VALUE(alarm_hms);
    alarm_sec = SUNXI_ALRM_GET_SEC_VALUE(alarm_hms);

    RTC_INFO("alarm_cnt: %d, alarm_cur: %d", alarm_cnt, alarm_cur);
    if (alarm_cur > alarm_cnt)
    {
        /* alarm is disabled. */
        wkalrm->enabled = 0;
        alrm_tm->tm_mon = -1;
        alrm_tm->tm_mday = -1;
        alrm_tm->tm_year = -1;
        alrm_tm->tm_hour = -1;
        alrm_tm->tm_min = -1;
        alrm_tm->tm_sec = -1;
        return 0;
    }

    ret = hal_rtc_gettime(alrm_tm);
    if (ret)
    {
        return -1;
    }

    rtc_tm_to_time(alrm_tm, &alarm_seconds);
    alarm_diff = (alarm_cnt - alarm_cur) * 3600 * 24 + alarm_hour * 3600 + alarm_min * 60 + alarm_sec;
    alarm_seconds += alarm_diff;
    rtc_time_to_tm(alarm_seconds, alrm_tm);
    RTC_INFO("alarm_seconds: %ld", alarm_seconds);

#endif

    alrm_en = hal_readl(rtc_dev->base + SUNXI_ALRM_IRQ_EN);
    if (alrm_en & SUNXI_ALRM_EN_CNT_EN)
    {
        wkalrm->enabled = 1;
    }

    return 0;
}


int hal_rtc_setalarm(struct rtc_wkalrm *wkalrm)
{

    struct hal_rtc_dev *rtc_dev = &sunxi_hal_rtc;
    struct sunxi_rtc_time *alrm_tm = &wkalrm->time;
    struct sunxi_rtc_time tm_now;
    u32 alrm;
    time64_t diff;
#ifndef SUNXI_SIMPLIFIED_TIMER
    unsigned long time_gap;
#endif
    unsigned long time_gap_day;
#if defined(SUNXI_ALARM1_USED)
    unsigned long time_gap_hour = 0;
    unsigned long time_gap_min = 0;
#endif
    int ret;
    ret = hal_rtc_gettime(&tm_now);
    if (ret < 0)
    {
        RTC_ERR("Error in getting time");
        return -1;
    }

    diff = rtc_tm_sub(alrm_tm, &tm_now);

    if (diff <= 0)
    {
        RTC_ERR("Date to set in the past");
        return -1;
    }

    if (diff > 255 * SEC_IN_DAY)
    {
        RTC_ERR("Day must be in the range 0 - 255");
        return -1;
    }
#ifndef SUNXI_SIMPLIFIED_TIMER
    time_gap = diff;
#endif
    time_gap_day = alrm_tm->tm_mday - tm_now.tm_mday;
#ifdef SUNXI_SIMPLIFIED_TIMER
    hal_rtc_setaie(0, rtc_dev);

    hal_writel(0, rtc_dev->base + SUNXI_ALRM_DAY);

    hal_writel(0, rtc_dev->base + SUNXI_ALRM_HMS);

    hal_writel(time_gap_day + hal_readl(rtc_dev->base + SUNXI_RTC_YMD),
           rtc_dev->base + SUNXI_ALRM_DAY);

    alrm = SUNXI_ALRM_SET_SEC_VALUE(alrm_tm->tm_sec) |
           SUNXI_ALRM_SET_MIN_VALUE(alrm_tm->tm_min) |
           SUNXI_ALRM_SET_HOUR_VALUE(alrm_tm->tm_hour);

    hal_writel(alrm, rtc_dev->base + SUNXI_ALRM_HMS);

#else
#ifdef SUNXI_ALARM1_USED
    time_gap -= time_gap_day * SEC_IN_DAY;
    time_gap_hour = time_gap / SEC_IN_HOUR;
    time_gap -= time_gap_hour * SEC_IN_HOUR;
    time_gap_min = time_gap / SEC_IN_MIN;
    time_gap -= time_gap_min * SEC_IN_MIN;
#endif

    hal_rtc_setaie(0, rtc_dev);
#ifdef SUNXI_ALARM1_USED
    if (hal_rtc_wait(SUNXI_LOSC_CTRL,
                     SUNXI_LOSC_CTRL_RTC_ALARM_ACC, 50))
    {
        RTC_ERR("Failed to set rtc alarm1.");
        return -1;
    }
    hal_msleep(2);

    hal_writel(0, rtc_dev->base + SUNXI_ALRM_DHMS);
    if (hal_rtc_wait(SUNXI_LOSC_CTRL,
                     SUNXI_LOSC_CTRL_RTC_ALARM_ACC, 50))
    {
        RTC_ERR("Failed to set rtc alarm1.");
        return -1;
    }
    hal_msleep(2);

    alrm = SUNXI_ALRM_SET_SEC_VALUE(time_gap) |
           SUNXI_ALRM_SET_MIN_VALUE(time_gap_min) |
           SUNXI_ALRM_SET_HOUR_VALUE(time_gap_hour) |
           SUNXI_ALRM_SET_DAY_VALUE(time_gap_day);
    hal_writel(alrm, rtc_dev->base + SUNXI_ALRM_DHMS);
    if (hal_rtc_wait(SUNXI_LOSC_CTRL,
                     SUNXI_LOSC_CTRL_RTC_ALARM_ACC, 50))
    {
        RTC_ERR("Failed to set rtc alarm1.");
        return -1;
    }
    hal_msleep(2);
#else
    hal_writel(0, rtc_dev->base + SUNXI_ALRM_COUNTER);
    alrm = time_gap;

    RTC_INFO("set alarm seconds:%d enable:%d", alrm, wkalrm->enabled);
    hal_writel(alrm, rtc_dev->base + SUNXI_ALRM_COUNTER);
#endif

#endif

    hal_writel(0, rtc_dev->base + SUNXI_ALRM_IRQ_EN);
    hal_writel(SUNXI_ALRM_IRQ_EN_CNT_IRQ_EN, rtc_dev->base + SUNXI_ALRM_IRQ_EN);

    hal_rtc_setaie(wkalrm->enabled, rtc_dev);

    return 0;
}

int hal_rtc_set_relative_alarm(uint32_t reltime)
{
    time64_t time_sec;
    //struct hal_rtc_dev *rtc_dev = &sunxi_hal_rtc;
    struct rtc_wkalrm *wkalrm = (struct rtc_wkalrm *)hal_malloc(sizeof(struct rtc_wkalrm));
    struct sunxi_rtc_time *tm_now = (struct sunxi_rtc_time *)hal_malloc(sizeof(struct sunxi_rtc_time));

    memset(wkalrm, 0, sizeof(struct rtc_wkalrm));
    memset(tm_now, 0, sizeof(struct sunxi_rtc_time));
    wkalrm->enabled = 1;

    if (reltime > MAX_RELTIME) {
        rtcerr("don't support the reltime that is greater than MAX_RELTIME(365 day)!\n");
        goto error;
    }

    if (hal_rtc_gettime(tm_now))
        goto error;

    time_sec = rtc_tm_to_time64(tm_now);
    time_sec += reltime;

    rtc_time64_to_tm(time_sec, &wkalrm->time);

    if (hal_rtc_setalarm(wkalrm))
        goto error;

    hal_free(wkalrm);
    hal_free(tm_now);
    return 0;

error:
    hal_free(wkalrm);
    hal_free(tm_now);
    return -1;
}

int hal_rtc_cancel_alarm(void)
{
    struct hal_rtc_dev *rtc_dev = &sunxi_hal_rtc;
    hal_rtc_setaie(0, rtc_dev);

    return 0;
}

int hal_rtc_alarm_irq_enable(unsigned int enabled)
{
    struct hal_rtc_dev *rtc_dev = &sunxi_hal_rtc;

    hal_rtc_setaie(enabled, rtc_dev);

    return 0;
}

void hal_rtc_min_year_show(unsigned int *min)
{
    struct hal_rtc_dev *rtc_dev = &sunxi_hal_rtc;

    *min = rtc_dev->data_year->min;

    RTC_INFO("sunxi rtc max year:%d", *min);
}

void hal_rtc_max_year_show(unsigned int *max)
{
    struct hal_rtc_dev *rtc_dev = &sunxi_hal_rtc;

    *max = rtc_dev->data_year->max;

    RTC_INFO("sunxi rtc max year:%d", *max);
}


#ifdef CONFIG_ARCH_SUN20IW2
static int sunxi_rtc_clk_init(void)
{
	hal_clk_status_t ret;
	hal_clk_type_t clk_type = HAL_SUNXI_R_CCU;
	hal_clk_id_t clk_id = CLK_SYSRTC32K;
	hal_clk_t clk;

	clk = hal_clock_get(clk_type, clk_id);
	ret = hal_clock_enable(clk);
	if (ret != HAL_CLK_STATUS_OK) {
		RTC_ERR("RTC clock enable failed.\n");
		return -1;
	}

	return 0;
}
#endif

int hal_rtc_init(void)
{
    struct hal_rtc_dev *rtc_dev = &sunxi_hal_rtc;
    int ret;
    unsigned int tmp_data;

    rtc_dev->base = SUNXI_RTC_BASE;
    rtc_dev->data_year = (struct hal_rtc_data_year *) &data_year_param;

    hal_writel(0, rtc_dev->base + SUNXI_ALRM_COUNTER);

    /* disable alarm, not generate irq pending */
    hal_writel(0, rtc_dev->base + SUNXI_ALRM_EN);

    /* disable alarm week/cnt irq, unset to cpu */
    hal_writel(0, rtc_dev->base + SUNXI_ALRM_IRQ_EN);

    /* clear alarm week/cnt irq pending */
    hal_writel(SUNXI_ALRM_IRQ_STA_CNT_IRQ_PEND, rtc_dev->base +
           SUNXI_ALRM_IRQ_STA);

    /* clear alarm wakeup output */
    hal_writel(SUNXI_ALRM_WAKEUP_OUTPUT_EN, rtc_dev->base +
           SUNXI_ALARM_CONFIG);

    if(hal_rtc_clk_init(rtc_dev))
    {
		RTC_ERR("rtc init clk error!\n");
		return RTC_CLK_ERROR;
    }

#ifndef CONFIG_ARCH_SUN20IW2
    /*
    * Step1: select RTC clock source
    */
    tmp_data = hal_readl(rtc_dev->base + SUNXI_LOSC_CTRL);
    tmp_data |= (REG_CLK32K_AUTO_SWT_EN);

    /* Enable auto switch function */
    tmp_data &= (~REG_CLK32K_AUTO_SWT_BYPASS);
    hal_writel(tmp_data, rtc_dev->base + SUNXI_LOSC_CTRL);

    tmp_data = hal_readl(rtc_dev->base + SUNXI_LOSC_CTRL);
    tmp_data |= (RTC_SOURCE_EXTERNAL | REG_LOSCCTRL_MAGIC);
    hal_writel(tmp_data, rtc_dev->base + SUNXI_LOSC_CTRL);

    /* We need to set GSM after change clock source */
    tmp_data = hal_readl(rtc_dev->base + SUNXI_LOSC_CTRL);
    tmp_data |= (EXT_LOSC_GSM | REG_LOSCCTRL_MAGIC);
    hal_writel(tmp_data, rtc_dev->base + SUNXI_LOSC_CTRL);
#else
    /* get clock source from */
    sunxi_rtc_clk_init();
#endif
    rtc_dev->irq = SUXNI_IRQ_RTC;

    ret = hal_request_irq(rtc_dev->irq, hal_rtc_alarmirq, "rtc", rtc_dev);
    if (ret < 0)/* Modify error detection methods for RTC to C906 & M33 */
    {
        RTC_ERR("Could not request IRQ");
        return -1;
    }
    hal_enable_irq(rtc_dev->irq);
    /* RTC_INFO("RTC enabled"); */

    return 0;
}

int hal_rtc_deinit(void)
{
    struct hal_rtc_dev *rtc_dev = &sunxi_hal_rtc;

#if defined(CONFIG_SOC_SUN20IW1)
    hal_clock_disable(rtc_dev->bus_clk);
    hal_clock_put(rtc_dev->bus_clk);
    hal_clock_disable(rtc_dev->rtc1k_clk);
    hal_clock_put(rtc_dev->rtc1k_clk);
    hal_clock_disable(rtc_dev->rtcspi_clk);
    hal_clock_put(rtc_dev->rtcspi_clk);
    hal_reset_control_assert(rtc_dev->reset);
    hal_reset_control_put(rtc_dev->reset);
#endif
    hal_disable_irq(rtc_dev->irq);
    hal_free_irq(rtc_dev->irq);
    return 0;
}

