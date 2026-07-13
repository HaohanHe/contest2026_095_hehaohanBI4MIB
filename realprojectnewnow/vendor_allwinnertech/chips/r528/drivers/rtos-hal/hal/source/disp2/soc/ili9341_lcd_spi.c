/****************************************************************************
 * boards/arm/r528/ili9341_spi/src/ili9341_spi.c
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

#include <nuttx/arch.h>
#include <nuttx/irq.h>

#include "hal_gpio.h"
#include "hal_timer.h"

#include <nuttx/lcd/ili9341.h>
#include <nuttx/lcd/lcd.h>
#include <nuttx/video/rgbcolors.h>
#include <sunxi_hal_spi.h>
#ifdef CONFIG_LCD_SUPPORT_ILI9341

extern spi_master_status_t hal_spi_write(hal_spi_master_port_t port, void *buf,
                                         uint32_t size);

/* LCD SPI 端口定义 */
#define LCD_SPI_PORT HAL_SPI_MASTER_1

/* GPIO引脚定义 */
#define LCD_PANEL_ID 0

/* 控制引脚 - 使用GPIOD组 */
#define LCD_RST_PIN GPIOD(19) /* 复位 */
#define LCD_CS_PIN GPIOD(10)  /* 片选 */
#define LCD_SDI_PIN GPIOD(12) /* MOSI (主出从入) */
#define LCD_SCL_PIN GPIOD(11) /* 时钟SCLK */
#define LCD_DC_PIN GPIOD(14)  /* 命令/数据选择 */
#define LCD_SDO_PIN GPIOD(13) /* MISO (主入从出) - 用于读取 */

#define LCD_BACKLIGHT_PIN GPIOD(20) /* 背光控制 */

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

/* ILI9341接口模式控制参数 */
#define ILI9341_IFMODE_PARAM                                            \
    (ILI9341_INTERFACE_CONTROL_DPL | ILI9341_INTERFACE_CONTROL_RCM(2) | \
     ILI9341_INTERFACE_CONTROL_BPASS)

#define ILI9341_IFCTL_PARAM1 (ILI9341_INTERFACE_CONTROL_WEMODE)

#define ILI9341_IFCTL_PARAM2 \
    (ILI9341_INTERFACE_CONTROL_MDT(0) | ILI9341_INTERFACE_CONTROL_EPF(0))

#define ILI9341_IFCTL_PARAM3 \
    (ILI9341_INTERFACE_CONTROL_RM | ILI9341_INTERFACE_CONTROL_DM(1))

/****************************************************************************
 * Private Function Protototypes
 ****************************************************************************/

static int ili9341_recvblock(struct ili9341_lcd_s *lcd, uint16_t *wd,
                             uint16_t nwords);
static void ili9341_deselect(struct ili9341_lcd_s *lcd);
static void ili9341_select(struct ili9341_lcd_s *lcd);
static int ili9341_sendcmd(struct ili9341_lcd_s *lcd, const uint8_t cmd);
static int ili9341_sendparam(struct ili9341_lcd_s *lcd, const uint8_t param);
static int ili9341_recvparam(struct ili9341_lcd_s *lcd, uint8_t *param);
static int ili9341_backlight(struct ili9341_lcd_s *lcd, int level);
static int ili9341_sendgram(struct ili9341_lcd_s *lcd, const uint16_t *wd,
                            uint32_t nwords);
static int ili9341_recvgram(struct ili9341_lcd_s *lcd, uint16_t *wd,
                            uint32_t nwords);

/* SPI读写函数 */
static void ili9341_spi_write_byte(uint8_t data);
static uint8_t ili9341_spi_read_byte(void);
static void ili9341_spi_gpio_init(void);
static void ili9341_lcd_send_cmd(uint8_t cmd);
static void ili9341_lcd_send_data(uint8_t data);
static void ili9341_lcd_send_data16(uint16_t data);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct ili9341_lcd_s g_ili9341_lcddev = {
    /* Initialize structure */
    .select = ili9341_select,
    .deselect = ili9341_deselect,
    .sendcmd = ili9341_sendcmd,
    .sendparam = ili9341_sendparam,
    .recvparam = ili9341_recvparam,
    .sendgram = ili9341_sendgram,
    .recvgram = ili9341_recvgram,
    .backlight = ili9341_backlight,
};

static uint16_t *g_ili9341_lcd_buffer = NULL;

static struct lcd_dev_s *g_lcddev = NULL;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: ili9341_spi_gpio_init
 *
 * Description: SPI GPIO初始化
 ****************************************************************************/

static void ili9341_spi_gpio_init(void)
{
#if !CONFIG_LCD_ILI9341_HARDWARE_SPI
    /* 配置所有SPI控制引脚为输出 */
    hal_gpio_set_direction(LCD_CS_PIN, GPIO_DIR_OUTPUT);
    hal_gpio_set_direction(LCD_SCL_PIN, GPIO_DIR_OUTPUT);
    hal_gpio_set_direction(LCD_SDI_PIN, GPIO_DIR_OUTPUT);
#endif

    hal_gpio_set_direction(LCD_DC_PIN, GPIO_DIR_OUTPUT);
    hal_gpio_set_direction(LCD_RST_PIN, GPIO_DIR_OUTPUT);

#if !CONFIG_LCD_ILI9341_HARDWARE_SPI
    /* MISO引脚为输入（用于读取） */
    hal_gpio_set_direction(LCD_SDO_PIN, GPIO_DIR_INPUT);

    /* 初始状态 */
    LCD_CS_SET;                                    /* 片选高电平（不选中） */
    hal_gpio_set_data(LCD_SCL_PIN, GPIO_DATA_LOW); /* 时钟低电平 */
#endif

    LCD_DC_DATA; /* 默认数据模式 */
    LCD_RST_SET; /* 不复位 */
}

/****************************************************************************
 * Name: ili9341_spi_write_byte
 *
 * Description: 软件SPI发送一个字节
 ****************************************************************************/

static void ili9341_spi_write_byte(uint8_t data)
{
#if CONFIG_LCD_ILI9341_HARDWARE_SPI
    hal_spi_write(LCD_SPI_PORT, &data, 1);
#else
    int i;
    gpio_data_t bit_value;

    /* SPI模式0：CPOL=0, CPHA=0
     * 时钟空闲为低电平，数据在时钟上升沿采样
     * MSB优先
     */

    for (i = 0; i < 8; i++)
    {
        /* 在时钟上升沿之前设置数据 */
        bit_value = (data & 0x80) ? GPIO_DATA_HIGH : GPIO_DATA_LOW;
        hal_gpio_set_data(LCD_SDI_PIN, bit_value);

        /* 短暂延时确保数据稳定 */
        lcd_delay_us(1);

        /* 时钟上升沿（数据被采样） */
        hal_gpio_set_data(LCD_SCL_PIN, GPIO_DATA_HIGH);
        lcd_delay_us(1);

        /* 时钟下降沿 */
        hal_gpio_set_data(LCD_SCL_PIN, GPIO_DATA_LOW);
        lcd_delay_us(1);

        data <<= 1; /* 处理下一位 */
    }
#endif
}

/****************************************************************************
 * Name: ili9341_spi_read_byte
 *
 * Description: 软件SPI读取一个字节
 ****************************************************************************/

static uint8_t ili9341_spi_read_byte(void)
{
#if CONFIG_LCD_ILI9341_HARDWARE_SPI
    uint8_t data = 0;
    hal_spi_read(LCD_SPI_PORT, &data, 1);
    return data;
#else
    uint8_t data = 0;
    int i;
    gpio_data_t read_value;

    /* SPI模式0：CPOL=0, CPHA=0
     * 在时钟上升沿采样，在下降沿读取
     */

    for (i = 0; i < 8; i++)
    {
        data <<= 1; /* 先移位，MSB优先 */

        /* 时钟上升沿（LCD在上升沿输出数据） */
        hal_gpio_set_data(LCD_SCL_PIN, GPIO_DATA_HIGH);
        lcd_delay_us(1);

        /* 时钟下降沿之前读取数据 */
        hal_gpio_get_data(LCD_SDO_PIN, &read_value);
        if (read_value == GPIO_DATA_HIGH)
        {
            data |= 0x01; /* 设置最低位 */
        }

        /* 时钟下降沿 */
        hal_gpio_set_data(LCD_SCL_PIN, GPIO_DATA_LOW);
        lcd_delay_us(1);
    }

    return data;
#endif
}

/****************************************************************************
 * Name: ili9341_lcd_send_cmd
 *
 * Description: 发送命令到LCD
 ****************************************************************************/

static void ili9341_lcd_send_cmd(uint8_t cmd)
{
    LCD_DC_CMD; /* DC=0 表示命令 */
#if CONFIG_LCD_ILI9341_HARDWARE_SPI
    hal_spi_write(LCD_SPI_PORT, &cmd, 1);
#else
    /* 注意：片选由上层(ili9341_select)控制，此处严禁操作CS，否则会中断命令序列 */
    ili9341_spi_write_byte(cmd);
#endif
    LCD_DC_DATA; /* 恢复为数据模式 */
}

/****************************************************************************
 * Name: ili9341_lcd_send_data
 *
 * Description: 发送数据（参数）到LCD
 ****************************************************************************/

static void ili9341_lcd_send_data(uint8_t data)
{
    LCD_DC_DATA; /* DC=1 表示数据 */
#if CONFIG_LCD_ILI9341_HARDWARE_SPI
    hal_spi_write(LCD_SPI_PORT, &data, 1);
#else
    /* 注意：片选由上层控制 */
    ili9341_spi_write_byte(data);
#endif
}

/****************************************************************************
 * Name: ili9341_lcd_send_data16
 *
 * Description: 发送16位数据到LCD（用于像素）
 ****************************************************************************/

static void ili9341_lcd_send_data16(uint16_t data)
{
    ili9341_lcd_send_data(data >> 8);   /* 高字节 */
    ili9341_lcd_send_data(data & 0xFF); /* 低字节 */
}

/****************************************************************************
 * Name: ili9341_recvblock
 *
 * Description: 接收数据块
 ****************************************************************************/

static int ili9341_recvblock(struct ili9341_lcd_s *lcd, uint16_t *wd,
                             uint16_t nwords)
{
    uint16_t *dest = wd;
#if CONFIG_LCD_ILI9341_HARDWARE_SPI
    /* 使用硬件SPI读取数据 */
    hal_spi_read(LCD_SPI_PORT, (void *)dest, nwords * 2);
    return OK;
#endif
    /* 对于SPI接口，需要先发送Dummy字节才能读取 */
    /* 注意：片选由上层控制 */
    LCD_DC_DATA;

    while (nwords--)
    {
        uint8_t r, g, b;

        /* 读取RGB数据（每个像素18位，分3次读取） */
        /* 先发送Dummy字节 */
        ili9341_spi_write_byte(0x00);

        /* 读取红色分量 */
        r = ili9341_spi_read_byte();
        r = r >> 3; /* 6位转5位 */

        /* 读取绿色分量 */
        g = ili9341_spi_read_byte();
        g = g >> 2; /* 6位转6位 */

        /* 读取蓝色分量 */
        b = ili9341_spi_read_byte();
        b = b >> 3; /* 6位转5位 */

        /* 组合成RGB565格式 */
        *dest++ = ((r << 11) | (g << 5) | b);
    }

    return OK;
}

/****************************************************************************
 * Name: ili9341_select
 *
 * Description: 选择LCD
 ****************************************************************************/

static void ili9341_select(struct ili9341_lcd_s *lcd) { LCD_CS_CLR; }

/****************************************************************************
 * Name: ili9341_deselect
 *
 * Description: 取消选择LCD
 ****************************************************************************/

static void ili9341_deselect(struct ili9341_lcd_s *lcd) { LCD_CS_SET; }

/****************************************************************************
 * Name: ili9341_sendcmd
 *
 * Description: 发送命令到LCD
 ****************************************************************************/

static int ili9341_sendcmd(struct ili9341_lcd_s *lcd, const uint8_t cmd)
{
    ili9341_lcd_send_cmd(cmd);
    return OK;
}

/****************************************************************************
 * Name: ili9341_sendparam
 *
 * Description: 发送参数到LCD
 ****************************************************************************/

static int ili9341_sendparam(struct ili9341_lcd_s *lcd, const uint8_t param)
{
    ili9341_lcd_send_data(param);
    return OK;
}

/****************************************************************************
 * Name: ili9341_recvparam
 *
 * Description: 从LCD接收参数
 ****************************************************************************/

static int ili9341_recvparam(struct ili9341_lcd_s *lcd, uint8_t *param)
{
    LCD_DC_DATA;
    /* 注意：片选由上层控制 */

    /* 发送Dummy字节 */
    ili9341_spi_write_byte(0x00);
    *param = ili9341_spi_read_byte();

    lcdinfo("param=%02x\n", *param);
    return OK;
}

/****************************************************************************
 * Name: ili9341_sendgram
 *
 * Description: 发送GRAM数据（像素数据）
 ****************************************************************************/

static int ili9341_sendgram(struct ili9341_lcd_s *lcd, const uint16_t *wd,
                            uint32_t nwords)
{
    const uint16_t *src = wd;

    LCD_DC_DATA;
#if CONFIG_LCD_ILI9341_HARDWARE_SPI
    /* 优化建议：使用32位访问加速大小端转换 (SIMD-like logic) */
    uint32_t i;
    uint32_t n32 = nwords >> 1; /* 每次处理2个像素 */

    /* 检查内存对齐，确保 src 可以被转换为 uint32_t* 访问 */
    if (((uintptr_t)src & 0x3) == 0)
    {
        uint32_t *src32 = (uint32_t *)src;
        uint32_t *dst32 = (uint32_t *)g_ili9341_lcd_buffer;
        uint32_t val;

        for (i = 0; i < n32; i++)
        {
            val = *src32++;
            /* 
             * 将 0xAABBCCDD (P1:AABB, P0:CCDD) 转换为 0xBBAADDCC 
             * 这相当于 ARM 的 REV16 指令，同时交换两个像素内部的字节序
             */
            val = ((val & 0x00FF00FF) << 8) | ((val & 0xFF00FF00) >> 8);
            *dst32++ = val;
        }
    }
    else
    {
        /* 未对齐的回退路径：保持逐个处理 */
        uint32_t *dst32 = (uint32_t *)g_ili9341_lcd_buffer;
        for (i = 0; i < n32; i++)
        {
             uint32_t px1 = __builtin_bswap16(*src++);
             uint32_t px2 = __builtin_bswap16(*src++);
             *dst32++ = (px2 << 16) | px1; /* 这里的顺序取决于 Host 端序 */
        }
    }

    /* 处理剩余的那个像素（如果是奇数个） */
    if (nwords & 0x1)
    {
        g_ili9341_lcd_buffer[nwords - 1] = __builtin_bswap16(wd[nwords - 1]);
    }

    /* 启动 DMA 传输 */
    hal_spi_write(LCD_SPI_PORT, (void *)g_ili9341_lcd_buffer, nwords * 2);
#else
    /* 软件 SPI 优化路径 */
    /* 注意：片选由上层控制 */
    while (nwords--)
    {
        uint16_t word = *src++;
        ili9341_spi_write_byte(word >> 8);
        ili9341_spi_write_byte(word & 0xFF);
    }
#endif

    return OK;
}

/****************************************************************************
 * Name: ili9341_recvgram
 *
 * Description: 接收GRAM数据
 ****************************************************************************/

static int ili9341_recvgram(struct ili9341_lcd_s *lcd, uint16_t *wd,
                            uint32_t nwords)
{
    return ili9341_recvblock(lcd, wd, nwords);
}

/****************************************************************************
 * Name: ili9341_backlight
 *
 * Description: 背光控制
 ****************************************************************************/

static int ili9341_backlight(struct ili9341_lcd_s *lcd, int level)
{
    /* 如果有背光引脚，在这里实现控制逻辑 */
    return OK;
}

/****************************************************************************
 * Name: write_byte
 *
 * Description: 发送一个字节（兼容原接口）
 ****************************************************************************/

static inline void write_byte(uint8_t data) { ili9341_spi_write_byte(data); }

/****************************************************************************
 * Name: read_byte
 *
 * Description: 读取一个字节（兼容原接口）
 ****************************************************************************/

static inline uint8_t read_byte(void) { return ili9341_spi_read_byte(); }

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: r528_ili93418b_initialize
 *
 * Description: 初始化ILI9341 LCD驱动
 ****************************************************************************/

struct lcd_dev_s *r528_ili93418b_initialize(void)
{
    struct ili9341_lcd_s *lcd = &g_ili9341_lcddev;

    lcdinfo("initialize ili9341 SPI subdriver for R528\n");

    /* 优化建议：避免重复分配或内存泄漏 */
    if (g_ili9341_lcd_buffer == NULL)
    {
        g_ili9341_lcd_buffer = (uint16_t *)malloc(240 * 320 * 2);
        if (g_ili9341_lcd_buffer == NULL)
        {
            lcderr("ERROR: Failed to allocate lcd buffer\n");
            return NULL;
        }
    }

    ili9341_spi_gpio_init();

    /* 复位LCD */
    LCD_RST_CLR;
    lcd_delay_ms(100);
    LCD_RST_SET;
    lcd_delay_ms(50);

    /* 初始化LCD驱动 */
    g_lcddev = ili9341_initialize(lcd, 0);

    if (!g_lcddev)
    {
        lcderr("ERROR: Failed to initialize ili9341\n");
        return NULL;
    }

    lcd->sendcmd(lcd, ILI9341_DISPLAY_ON);
#if 0
    /* 选择LCD设备 */
    lcd->select(lcd);

    /* ILI9341初始化序列 */

    /* 软件复位 */
    lcdinfo("ili9341 LCD driver: Software Reset\n");
    lcd->sendcmd(lcd, ILI9341_SOFTWARE_RESET);
    lcd_delay_ms(5);

    /* RGB接口信号控制 */
    lcdinfo("ili9341 LCD driver: Set RGB Interface signal control: %02x\n",
            ILI9341_IFMODE_PARAM);
    lcd->sendcmd(lcd, ILI9341_RGB_SIGNAL_CONTROL);
    lcd->sendparam(lcd, ILI9341_IFMODE_PARAM);

    /* 接口控制 */
    lcdinfo("ili9341 LCD driver: Set Interface control: %d:%d:%d\n",
            ILI9341_IFCTL_PARAM1,
            ILI9341_IFCTL_PARAM2,
            ILI9341_IFCTL_PARAM3);

    lcd->sendcmd(lcd, ILI9341_INTERFACE_CONTROL);
    lcd->sendparam(lcd, ILI9341_IFCTL_PARAM1);
    lcd->sendparam(lcd, ILI9341_IFCTL_PARAM2);
    lcd->sendparam(lcd, ILI9341_IFCTL_PARAM3);

    /* 帧率控制 */
    lcdinfo("ili9341 set Frame control\n");
    lcd->sendcmd(lcd, ILI9341_FRAME_RATE_CONTROL_NORMAL);
    lcd->sendparam(lcd, 0x00);
    lcd->sendparam(lcd, 0x13); /* 100Hz */

    /* 退出睡眠模式 */
    lcdinfo("ili9341 LCD driver: Sleep Out\n");
    lcd->sendcmd(lcd, ILI9341_SLEEP_OUT);
    lcd_delay_ms(5);

    /* 打开显示 */
    lcdinfo("ili9341 LCD driver: Display On\n");
    lcd->sendcmd(lcd, ILI9341_DISPLAY_ON);

    /* 取消选择 */
    lcd->deselect(lcd);
#endif

    return g_lcddev;
}

/****************************************************************************
 * Name: board_lcd_initialize
 *
 * Description: 板级LCD初始化
 ****************************************************************************/

int board_lcd_initialize(void)
{
    g_lcddev = r528_ili93418b_initialize();
    if (g_lcddev == NULL)
    {
        lcdinfo("Initialize ili9341 lcd driver NULL\n");
        return ENODEV;
    }
    lcd_delay_ms(100);
    hal_gpio_sel_vol_mode(LCD_BACKLIGHT_PIN, POWER_MODE_330);
    hal_gpio_set_driving_level(LCD_BACKLIGHT_PIN, GPIO_DRIVING_LEVEL3);
    hal_gpio_set_direction(LCD_BACKLIGHT_PIN, GPIO_DIRECTION_OUTPUT);
    hal_gpio_set_pull(LCD_BACKLIGHT_PIN, GPIO_PULL_DOWN_DISABLED);
    hal_gpio_set_data(LCD_BACKLIGHT_PIN, 1);
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

#endif /* CONFIG_LCD_ILI9341 */