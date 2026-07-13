/****************************************************************************
 * boards/arm/r528/st7789_spi/src/st7789_spi.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <debug.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>

#include <nuttx/arch.h>
#include <nuttx/irq.h>
#include <nuttx/spi/spi.h>

#include "hal_gpio.h"
#include "hal_timer.h"

#include <nuttx/lcd/st7789.h>
#include <nuttx/lcd/lcd.h>
#include <sunxi_hal_spi.h>

/* 限制单次SPI传输大小，防止超时 (例如 4KB - 内部 buffer 小) */
#define ST7789_SPI_MAX_BLOCK_BYTES 4096

#ifdef CONFIG_LCD_SUPPORT_ST7789

extern spi_master_status_t hal_spi_write(hal_spi_master_port_t port, void *buf,
                                         uint32_t size);
extern spi_master_status_t hal_spi_read(hal_spi_master_port_t port, void *buf, 
                                        uint32_t size);

/* LCD SPI 端口定义 */
#define LCD_SPI_PORT HAL_SPI_MASTER_1

/* GPIO引脚定义 */
#define LCD_PANEL_ID 0

/* 控制引脚 - 使用GPIOD组 */
#define LCD_RST_PIN GPIOD(19) /* 复位 */
#define LCD_CS_PIN GPIOD(10)  /* 片选 */
#define LCD_SDI_PIN GPIOD(12) /* MOSI */
#define LCD_SCL_PIN GPIOD(11) /* SCLK */
#define LCD_DC_PIN GPIOD(14)  /* C/D */
#define LCD_SDO_PIN GPIOD(13) /* MISO */

#define LCD_BACKLIGHT_PIN GPIOD(20) /* 背光 */

/* GPIO数据值定义 */
#define GPIO_DATA_HIGH 1
#define GPIO_DATA_LOW 0

/* GPIO方向定义 */
#define GPIO_DIR_OUTPUT 1
#define GPIO_DIR_INPUT 0

/* 延时宏 */
#define lcd_delay_ms(ms) hal_msleep(ms)
#define lcd_delay_us(us) hal_usleep(us)

/* LCD控制宏 */
#define LCD_CS_CLR hal_gpio_set_data(LCD_CS_PIN, GPIO_DATA_LOW)
#define LCD_CS_SET hal_gpio_set_data(LCD_CS_PIN, GPIO_DATA_HIGH)

#define LCD_RST_CLR hal_gpio_set_data(LCD_RST_PIN, GPIO_DATA_LOW)
#define LCD_RST_SET hal_gpio_set_data(LCD_RST_PIN, GPIO_DATA_HIGH)

#define LCD_DC_CMD hal_gpio_set_data(LCD_DC_PIN, GPIO_DATA_LOW)
#define LCD_DC_DATA hal_gpio_set_data(LCD_DC_PIN, GPIO_DATA_HIGH)

/****************************************************************************
 * Private Function Protototypes
 ****************************************************************************/

static int st7789_spi_lock(FAR struct spi_dev_s *dev, bool lock);
static void st7789_spi_select(FAR struct spi_dev_s *dev, uint32_t devid,
                              bool selected);
static uint32_t st7789_spi_setfrequency(FAR struct spi_dev_s *dev,
                                        uint32_t frequency);
static void st7789_spi_setmode(FAR struct spi_dev_s *dev, enum spi_mode_e mode);
static void st7789_spi_setbits(FAR struct spi_dev_s *dev, int nbits);
#ifdef CONFIG_SPI_HWFEATURES
static int st7789_spi_hwfeatures(FAR struct spi_dev_s *dev,
                                 spi_hwfeatures_t  features);
#endif
static uint8_t st7789_spi_status(FAR struct spi_dev_s *dev, uint32_t devid);
#ifdef CONFIG_SPI_CMDDATA
static int st7789_spi_cmddata(FAR struct spi_dev_s *dev, uint32_t devid,
                              bool cmd);
#endif
static uint32_t st7789_spi_send(FAR struct spi_dev_s *dev, uint32_t wd);
#ifdef CONFIG_SPI_EXCHANGE
static void st7789_spi_exchange(FAR struct spi_dev_s *dev,
                                FAR const void *txbuffer, FAR void *rxbuffer,
                                size_t nwords);
#else
static void st7789_spi_sndblock(FAR struct spi_dev_s *dev,
                                FAR const void *txbuffer, size_t nwords);
static void st7789_spi_recvblock(FAR struct spi_dev_s *dev, FAR void *rxbuffer,
                                 size_t nwords);
#endif
static int st7789_spi_registercallback(FAR struct spi_dev_s *dev,
                                       spi_mediachange_t callback,
                                       void *arg);

static void st7789_spi_write_byte(uint8_t data);
static uint8_t st7789_spi_read_byte(void);
static void st7789_spi_gpio_init(void);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct spi_ops_s g_st7789_spi_ops = {
    .lock = st7789_spi_lock,
    .select = st7789_spi_select,
    .setfrequency = st7789_spi_setfrequency,
    .setmode = st7789_spi_setmode,
    .setbits = st7789_spi_setbits,
#ifdef CONFIG_SPI_HWFEATURES
    .hwfeatures = st7789_spi_hwfeatures,
#endif
    .status = st7789_spi_status,
#ifdef CONFIG_SPI_CMDDATA
    .cmddata = st7789_spi_cmddata,
#endif
    .send = st7789_spi_send,
#ifdef CONFIG_SPI_EXCHANGE
    .exchange = st7789_spi_exchange,
#else
    .sndblock = st7789_spi_sndblock,
    .recvblock = st7789_spi_recvblock,
#endif
    .registercallback = st7789_spi_registercallback,
};

static struct spi_dev_s g_st7789_spi_dev = {
    .ops = &g_st7789_spi_ops,
};

static struct lcd_dev_s *g_lcddev = NULL;
static uint16_t *g_st7789_lcd_buffer = NULL;
/* 保存用于释放的原始指针 (虽然目前不释放) */
static void *g_st7789_lcd_buffer_alloc = NULL;
static int g_st7789_spi_nbits = 8;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void st7789_spi_gpio_init(void)
{
#if !CONFIG_LCD_ST7789_HARDWARE_SPI
    hal_gpio_set_direction(LCD_CS_PIN, GPIO_DIR_OUTPUT);
    hal_gpio_set_direction(LCD_SCL_PIN, GPIO_DIR_OUTPUT);
    hal_gpio_set_direction(LCD_SDI_PIN, GPIO_DIR_OUTPUT);
#endif

    hal_gpio_set_direction(LCD_DC_PIN, GPIO_DIR_OUTPUT);
    hal_gpio_set_direction(LCD_RST_PIN, GPIO_DIR_OUTPUT);

#if !CONFIG_LCD_ST7789_HARDWARE_SPI
    hal_gpio_set_direction(LCD_SDO_PIN, GPIO_DIR_INPUT);

    LCD_CS_SET;
    hal_gpio_set_data(LCD_SCL_PIN, GPIO_DATA_LOW);
#endif

    LCD_DC_DATA;
    LCD_RST_SET;
}

static void st7789_spi_write_byte(uint8_t data)
{
#if CONFIG_LCD_ST7789_HARDWARE_SPI
    /* 注意: HAL_SPI_WRITE 在这里可能不够，因为我们是用byte-by-byte发送 */
    /* 如果 HAL 支持单字节发送最好，否则会有性能问题 */
    hal_spi_write(LCD_SPI_PORT, &data, 1);
#else
    int i;
    gpio_data_t bit_value;
    for (i = 0; i < 8; i++)
    {
        bit_value = (data & 0x80) ? GPIO_DATA_HIGH : GPIO_DATA_LOW;
        hal_gpio_set_data(LCD_SDI_PIN, bit_value);
        lcd_delay_us(1);
        hal_gpio_set_data(LCD_SCL_PIN, GPIO_DATA_HIGH);
        lcd_delay_us(1);
        hal_gpio_set_data(LCD_SCL_PIN, GPIO_DATA_LOW);
        lcd_delay_us(1);
        data <<= 1;
    }
#endif
}

static uint8_t st7789_spi_read_byte(void)
{
#if CONFIG_LCD_ST7789_HARDWARE_SPI
    uint8_t data = 0;
    hal_spi_read(LCD_SPI_PORT, &data, 1);
    return data;
#else
    uint8_t data = 0;
    int i;
    gpio_data_t read_value;
    for (i = 0; i < 8; i++)
    {
        data <<= 1;
        hal_gpio_set_data(LCD_SCL_PIN, GPIO_DATA_HIGH);
        lcd_delay_us(1);
        hal_gpio_get_data(LCD_SDO_PIN, &read_value);
        if (read_value == GPIO_DATA_HIGH)
        {
            data |= 0x01;
        }
        hal_gpio_set_data(LCD_SCL_PIN, GPIO_DATA_LOW);
        lcd_delay_us(1);
    }
    return data;
#endif
}

/****************************************************************************
 * SPI Method Implementations
 ****************************************************************************/

static int st7789_spi_lock(FAR struct spi_dev_s *dev, bool lock)
{
    return OK;
}

static void st7789_spi_select(FAR struct spi_dev_s *dev, uint32_t devid,
                              bool selected)
{
    if (selected)
    {
        LCD_CS_CLR;
    }
    else
    {
        LCD_CS_SET;
    }
}

static uint32_t st7789_spi_setfrequency(FAR struct spi_dev_s *dev,
                                        uint32_t frequency)
{
    return frequency;
}

static void st7789_spi_setmode(FAR struct spi_dev_s *dev, enum spi_mode_e mode)
{
}

static void st7789_spi_setbits(FAR struct spi_dev_s *dev, int nbits)
{
    g_st7789_spi_nbits = nbits;
}

#ifdef CONFIG_SPI_HWFEATURES
static int st7789_spi_hwfeatures(FAR struct spi_dev_s *dev,
                                 spi_hwfeatures_t features)
{
    return 0;
}
#endif

static uint8_t st7789_spi_status(FAR struct spi_dev_s *dev, uint32_t devid)
{
    return 0;
}

#ifdef CONFIG_SPI_CMDDATA
static int st7789_spi_cmddata(FAR struct spi_dev_s *dev, uint32_t devid,
                              bool cmd)
{
    if (cmd)
    {
        LCD_DC_CMD;
    }
    else
    {
        LCD_DC_DATA;
    }
    return OK;
}
#endif

static uint32_t st7789_spi_send(FAR struct spi_dev_s *dev, uint32_t wd)
{
    st7789_spi_write_byte((uint8_t)wd);
    return wd;
}

#ifdef CONFIG_SPI_EXCHANGE
static void st7789_spi_exchange(FAR struct spi_dev_s *dev,
                                FAR const void *txbuffer, FAR void *rxbuffer,
                                size_t nwords)
{
    if (txbuffer)
    {
        if (g_st7789_spi_nbits > 8)
        {
            /* 16-bit 模式：像素数据传输 */
            const uint16_t *src = (const uint16_t *)txbuffer;
            size_t remaining = nwords; /* nwords 是 16位 单元的数量 */

#if CONFIG_LCD_ST7789_HARDWARE_SPI
            if (g_st7789_lcd_buffer != NULL)
            {
                /* 使用对齐的中间 Buffer 进行流式处理 */
                while (remaining > 0)
                {
                    size_t chunk_words = remaining;
                    /* 限制单次传输不超过 Buffer 大小 */
                    if (chunk_words * 2 > ST7789_SPI_MAX_BLOCK_BYTES)
                    {
                        chunk_words = ST7789_SPI_MAX_BLOCK_BYTES / 2;
                    }

                    /* 转换数据到中间 Buffer 并进行 R/B 交换和大小端交换 */
                    /* 说明：
                     * 1. 之前测试 Byte Swap 后 Red -> Blue，说明 Panel 可能是 BGR 格式。
                     *    因此需要进行 RGB565 -> BGR565 转换 (R/B Swap)。
                     * 2. 同时保持 Byte Swap (LE -> BE) 以适应 SPI 传输。
                     */
                    uint32_t i;
                    uint32_t n32 = chunk_words >> 1;

                    /* 检查源数据是否对齐 */
                    if (((uintptr_t)src & 0x3) == 0)
                    {
                        uint32_t *src32 = (uint32_t *)src;
                        uint32_t *dst32 = (uint32_t *)g_st7789_lcd_buffer;
                        uint32_t val;

                        /* 
                         * Back to Byte Swap only.
                         * Inversion issues should be handled by ST7789_INVON/OFF commands.
                         */
                        for (i = 0; i < chunk_words; i++)
                        {
                            uint16_t pixel = ((uint16_t *)src)[i];
                            g_st7789_lcd_buffer[i] = __builtin_bswap16(pixel);
                        }
                    }
                    else
                    {
                        /* 未对齐源数据 */
                        const uint16_t *p = src;
                        for (i = 0; i < chunk_words; i++)
                        {
                            uint16_t pixel = *p++;
                            g_st7789_lcd_buffer[i] = __builtin_bswap16(pixel);
                        }
                    }

                    /*
                     * 注意：上面的循环已经处理了所有像素 (chunk_words)
                     * 不需要单独处理 n32/remainder，因为为了 R/B Swap 逻辑简化，这里使用了 uint16 循环
                     */

                    /* 发送数据块 */
                    hal_spi_write(LCD_SPI_PORT, (void *)g_st7789_lcd_buffer, chunk_words * 2);

                    /* 移动指针 */
                    src += chunk_words;
                    remaining -= chunk_words;
                }
            }
            else
            {
                 /* 缓冲区未分配回退：直接发送 (可能面临 DMA 对齐报错或颜色错误) */
                 lcderr("ERROR: buffer not alloc, direct send\n");
                 hal_spi_write(LCD_SPI_PORT, (void *)txbuffer, nwords * 2);
            }
#else
            /* 软件 SPI 16bit 发送 */
            while (nwords--)
            {
                uint16_t word = *src++;
                st7789_spi_write_byte(word >> 8);
                st7789_spi_write_byte(word & 0xFF);
            }
#endif
        }
        else
        {
            /* 8-bit 模式：命令传输 (通常很短，直接发) */
            /* 如果遇到长数据，最好也加上分块逻辑，但命令一般就在几十字节以内 */
#if CONFIG_LCD_ST7789_HARDWARE_SPI
            hal_spi_write(LCD_SPI_PORT, (void *)txbuffer, nwords);
#else
            const uint8_t *src = (const uint8_t *)txbuffer;
            while (nwords--)
            {
                 st7789_spi_write_byte(*src++);
            }
#endif
        }
    }
    else if (rxbuffer)
    {
#if CONFIG_LCD_ST7789_HARDWARE_SPI
        hal_spi_read(LCD_SPI_PORT, rxbuffer, nwords);
#else
        uint8_t *dst = (uint8_t *)rxbuffer;
        while (nwords--)
        {
            *dst++ = st7789_spi_read_byte();
        }
#endif
    }
}
#endif

#ifndef CONFIG_SPI_EXCHANGE
static void st7789_spi_sndblock(FAR struct spi_dev_s *dev,
                                FAR const void *txbuffer, size_t nwords)
{
    const uint16_t *src = (const uint16_t *)txbuffer;

#if CONFIG_LCD_ST7789_HARDWARE_SPI

    uint32_t i;
    uint32_t n32 = nwords >> 1;

    if (((uintptr_t)src & 0x3) == 0)
    {
        uint32_t *src32 = (uint32_t *)src;
        uint32_t *dst32 = (uint32_t *)g_st7789_lcd_buffer;
        uint32_t val;

        for (i = 0; i < n32; i++)
        {
            val = *src32++;
            val = ((val & 0x00FF00FF) << 8) | ((val & 0xFF00FF00) >> 8);
            *dst32++ = val;
        }
    }
    else
    {
        uint32_t *dst32 = (uint32_t *)g_st7789_lcd_buffer;
        const uint16_t *p = src;
        for (i = 0; i < n32; i++)
        {
             uint32_t px1 = __builtin_bswap16(*p++);
             uint32_t px2 = __builtin_bswap16(*p++);
             *dst32++ = (px2 << 16) | px1;
        }
    }

    if (nwords & 0x1)
    {
        g_st7789_lcd_buffer[nwords - 1] = __builtin_bswap16(src[nwords - 1]);
    }

    hal_spi_write(LCD_SPI_PORT, (void *)g_st7789_lcd_buffer, nwords * 2);
#else
    while (nwords--)
    {
        uint16_t word = *src++;
        st7789_spi_write_byte(word >> 8);
        st7789_spi_write_byte(word & 0xFF);
    }
#endif
}

static void st7789_spi_recvblock(FAR struct spi_dev_s *dev, FAR void *rxbuffer,
                                 size_t nwords)
{
#if CONFIG_LCD_ST7789_HARDWARE_SPI
    hal_spi_read(LCD_SPI_PORT, rxbuffer, nwords);
#else
    uint8_t *dst = (uint8_t *)rxbuffer;
    while (nwords--)
    {
        *dst++ = st7789_spi_read_byte();
    }
#endif
}
#endif

static int st7789_spi_registercallback(FAR struct spi_dev_s *dev,
                                       spi_mediachange_t callback,
                                       void *arg)
{
    return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: r528_st7789_initialize
 *
 * Description: 初始化ST7789 LCD驱动
 ****************************************************************************/

struct lcd_dev_s *r528_st7789_initialize(void)
{
    lcdinfo("initialize st7789 SPI subdriver for R528\n");

    if (g_st7789_lcd_buffer == NULL)
    {
        /* 申请空间以进行 64 字节对齐，并只需满足分块传输大小 */
        g_st7789_lcd_buffer_alloc = malloc(ST7789_SPI_MAX_BLOCK_BYTES + 64);
        if (g_st7789_lcd_buffer_alloc == NULL)
        {
            lcderr("ERROR: Failed to allocate lcd buffer\n");
            return NULL;
        }
        /* 手动对齐到 64 字节边界 */
        uintptr_t addr = (uintptr_t)g_st7789_lcd_buffer_alloc;
        g_st7789_lcd_buffer = (uint16_t *)((addr + 63) & ~(uintptr_t)63);

        lcdinfo("Aligned buffer: alloc=%p used=%p size=%d\n", g_st7789_lcd_buffer_alloc, g_st7789_lcd_buffer, ST7789_SPI_MAX_BLOCK_BYTES);
    }

    st7789_spi_gpio_init();

    /* 硬件复位 LCD */
    LCD_RST_CLR;
    lcd_delay_ms(100);
    LCD_RST_SET;
    lcd_delay_ms(120);

    /* 背光初始化 */
    hal_gpio_sel_vol_mode(LCD_BACKLIGHT_PIN, POWER_MODE_330);
    hal_gpio_set_driving_level(LCD_BACKLIGHT_PIN, GPIO_DRIVING_LEVEL3);
    hal_gpio_set_direction(LCD_BACKLIGHT_PIN, GPIO_DIRECTION_OUTPUT);
    hal_gpio_set_pull(LCD_BACKLIGHT_PIN, GPIO_PULL_DOWN_DISABLED);
    hal_gpio_set_data(LCD_BACKLIGHT_PIN, 1);

    /* 调用NuttX st7789 驱动初始化 (传入SPI设备) */
    /* 注意：orientation, xoff, yoff 参数可能需要根据实际屏幕调整 */
#ifdef CONFIG_LCD_DYN_ORIENTATION
    g_lcddev = st7789_lcdinitialize(&g_st7789_spi_dev, LCD_PORTRAIT, 0, 0);
#else
    g_lcddev = st7789_lcdinitialize(&g_st7789_spi_dev);
#endif

    if (!g_lcddev)
    {
        lcderr("ERROR: Failed to initialize st7789\n");
        return NULL;
    }

    return g_lcddev;
}

/****************************************************************************
 * Name: board_lcd_initialize
 *
 * Description: 板级LCD初始化 (Common Interface)
 ****************************************************************************/

int board_lcd_initialize(void)
{
    g_lcddev = r528_st7789_initialize();
    if (g_lcddev == NULL)
    {
        lcdinfo("Initialize st7789 lcd driver NULL\n");
        return ENODEV;
    }
    return OK;
}

/****************************************************************************
 * Name: board_lcd_getdev
 *
 * Description: 获取LCD设备
 ****************************************************************************/

struct lcd_dev_s *board_lcd_getdev(int lcddev)
{
    if (lcddev == 0)
    {
        return g_lcddev;
    }

    return NULL;
}

#endif /* CONFIG_LCD_SUPPORT_ST7789 */
