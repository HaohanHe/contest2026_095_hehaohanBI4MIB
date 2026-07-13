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

#include <rtc/rtc.h>

static const unsigned char rtc_days_in_month[] =
{
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

static const unsigned short rtc_ydays[2][13] =
{
    /* Normal years */
    { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
    /* Leap years */
    { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
};

#define LEAPS_THRU_END_OF(y) ((y)/4 - (y)/100 + (y)/400)

/*
 * The number of days in the month.
 */
int rtc_month_days(unsigned int month, unsigned int year)
{
    return rtc_days_in_month[month] + (is_leap_year(year) && month == 1);
}

/*
 * The number of days since January 1. (0 to 365)
 */
int rtc_year_days(unsigned int day, unsigned int month, unsigned int year)
{
    return rtc_ydays[is_leap_year(year)][month] + day - 1;
}

/*
 * Does the rtc_time represent a valid date/time?
 */
int rtc_valid_tm(struct sunxi_rtc_time *tm)
{
    if (tm->tm_year < 70
        || ((unsigned)tm->tm_mon) >= 12
        || tm->tm_mday < 1
        || tm->tm_mday > rtc_month_days(tm->tm_mon, tm->tm_year + 1900)
        || ((unsigned)tm->tm_hour) >= 24
        || ((unsigned)tm->tm_min) >= 60
        || ((unsigned)tm->tm_sec) >= 60)
    {
        return -1;
    }

    return 0;
}

/*
 * mktime64 - Converts date to seconds.
 * Converts Gregorian date to seconds since 1970-01-01 00:00:00.
 * Assumes input in normal date format, i.e. 1980-12-31 23:59:59
 * => year=1980, mon=12, day=31, hour=23, min=59, sec=59.
 *
 * [For the Julian calendar (which was used in Russia before 1917,
 * Britain & colonies before 1752, anywhere else before 1582,
 * and is still in use by some communities) leave out the
 * -year/100+year/400 terms, and add 10.]
 *
 * This algorithm was first published by Gauss (I think).
 *
 * A leap second can be indicated by calling this function with sec as
 * 60 (allowable under ISO 8601).  The leap second is treated the same
 * as the following second since they don't exist in UNIX time.
 *
 * An encoding of midnight at the end of the day as 24:00:00 - ie. midnight
 * tomorrow - (allowable under ISO 8601) is supported.
 */
time64_t mktime64(const unsigned int year0, const unsigned int mon0,
                  const unsigned int day, const unsigned int hour,
                  const unsigned int min, const unsigned int sec)
{
    unsigned int mon = mon0, year = year0;

    /* 1..12 -> 11,12,1..10 */
    if (0 >= (int)(mon -= 2))
    {
        mon += 12;  /* Puts Feb last since it has leap day */
        year -= 1;
    }

    return ((((time64_t)
              (year / 4 - year / 100 + year / 400 + 367 * mon / 12 + day) +
              year * 365 - 719499
             ) * 24 + hour /* now have hours - midnight tomorrow handled here */
            ) * 60 + min /* now have minutes */
           ) * 60 + sec; /* finally seconds */
}


/*
 * rtc_tm_to_time64 - Converts rtc_time to time64_t.
 * Convert Gregorian date to seconds since 01-01-1970 00:00:00.
 */
time64_t rtc_tm_to_time64(struct sunxi_rtc_time *tm)
{
    return mktime64(tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                    tm->tm_hour, tm->tm_min, tm->tm_sec);
}

static inline u64 div_u64_rem(u64 dividend, u32 divisor, u32 *remainder)
{
    *remainder = dividend % divisor;
    return dividend / divisor;
}

static s64 div_s64_rem(s64 dividend, s32 divisor, unsigned int *remainder)
{
    u64 quotient;

    if (dividend < 0)
    {
        quotient = div_u64_rem(-dividend, abs(divisor), (u32 *)remainder);
        *remainder = -*remainder;
        if (divisor > 0)
        {
            quotient = -quotient;
        }
    }
    else
    {
        quotient = div_u64_rem(dividend, abs(divisor), (u32 *)remainder);
        if (divisor < 0)
        {
            quotient = -quotient;
        }
    }
    return quotient;
}

/*
 * rtc_time_to_tm64 - Converts time64_t to rtc_time.
 * Convert seconds since 01-01-1970 00:00:00 to Gregorian date.
 */
void rtc_time64_to_tm(time64_t time, struct sunxi_rtc_time *tm)
{
    unsigned int month, year, secs;
    int days;

    /* time must be positive */
    days = div_s64_rem(time, 86400, &secs);

    /* day of the week, 1970-01-01 was a Thursday */
    tm->tm_wday = (days + 4) % 7;

    year = 1970 + days / 365;
    days -= (year - 1970) * 365
            + LEAPS_THRU_END_OF(year - 1)
            - LEAPS_THRU_END_OF(1970 - 1);
    if (days < 0)
    {
        year -= 1;
        days += 365 + is_leap_year(year);
    }
    tm->tm_year = year - 1900;
    tm->tm_yday = days + 1;

    for (month = 0; month < 11; month++)
    {
        int newdays;

        newdays = days - rtc_month_days(month, year);
        if (newdays < 0)
        {
            break;
        }
        days = newdays;
    }
    tm->tm_mon = month;
    tm->tm_mday = days + 1;

    tm->tm_hour = secs / 3600;
    secs -= tm->tm_hour * 3600;
    tm->tm_min = secs / 60;
    tm->tm_sec = secs - tm->tm_min * 60;

    tm->tm_isdst = 0;
}
