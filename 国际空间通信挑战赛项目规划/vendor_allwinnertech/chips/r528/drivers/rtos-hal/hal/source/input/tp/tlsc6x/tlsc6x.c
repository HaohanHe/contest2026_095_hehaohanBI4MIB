
#include <stdio.h>
#include <hal_timer.h>
#include <hal_interrupt.h>
#include <hal_gpio.h>
#include <sunxi_hal_twi.h>
#include "tlsc6x.h"

#define	TS_MAX_FINGER	1

#define TLSC6X_DEV_NAME	"tlsc6x"
#define INT_GPIO_MUX	0
//tp config init.
#if defined(CONFIG_ARCH_SUN8IW18P1)
struct tlsc6x_hw_cfg tlsc6x_cfg = {
	.twi_id = 1,
	.addr = 0x2e,
	.int_gpio = GPIOB(8),
	.reset_gpio =GPIOH(8),
	.screen_max_x = 240,
	.screen_max_y = 320,
	.revert_x_flag = 1,
	.revert_y_flag = 0,
	.exchange_x_y_flag = 1,
};
#endif

#if defined(CONFIG_ARCH_SUN20IW2)
struct tlsc6x_hw_cfg tlsc6x_cfg = {
	.twi_id = 0,
	.addr = 0x2e,
	.int_gpio = GPIOA(27),
	.reset_gpio = GPIOA(28),
	.screen_max_x = 240,
	.screen_max_y = 320,
	.revert_x_flag = 1,
	.revert_y_flag = 0,
	.exchange_x_y_flag = 1,
};
#endif

#if defined(CONFIG_ARCH_SUN8IW20)
struct tlsc6x_hw_cfg tlsc6x_cfg = {
#if defined(CONFIG_ARCH_BOARD_R528S3_EVB4) || defined(CONFIG_ARCH_BOARD_R528S3_GEMINI_S1)
	.twi_id = 2,
	.addr = 0x2e,
	.int_gpio = GPIOD(19),
	.reset_gpio = GPIOD(18),
	.screen_max_x = 800,
	.screen_max_y = 480,
	.revert_x_flag = 1,
	.revert_y_flag = 0,
	.exchange_x_y_flag = 1,
#elif CONFIG_ARCH_BOARD_R528S3_X4B
	.twi_id = 2,
	.addr = 0x2e,
	.int_gpio = GPIOB(6),
	.reset_gpio = GPIOB(4),
	.screen_max_x = 800,
	.screen_max_y = 480,
	.revert_x_flag = 1,
	.revert_y_flag = 0,
	.exchange_x_y_flag = 1,
#else
	.twi_id = 2,
	.addr = 0x2e,
	.int_gpio = GPIOB(6),
	.reset_gpio = GPIOB(4),
	.screen_max_x = 800,
	.screen_max_y = 480,
	.revert_x_flag = 0,
	.revert_y_flag = 0,
	.exchange_x_y_flag = 0,
#endif
};
#endif

struct tlsc6x_drv_data *tlsc6x_data = NULL;
int g_is_telink_comp;

int tlsc6x_i2c_read(struct tlsc6x_drv_data *data, unsigned char *writebuf, int writelen,
			unsigned char *readbuf, int readlen)
{
	int ret = 0;
	struct twi_msg msgs[2];
	u8 i2c_port = data->config->twi_id;
	uint16_t flags = 0;

	if (readlen > 0) {
		if (writelen > 0) {
			msgs[0].addr = data->config->addr;
			msgs[0].flags = flags & TWI_M_TEN;
			msgs[0].flags &= ~(TWI_M_RD);
			msgs[0].len = writelen;
			msgs[0].buf = writebuf;

			msgs[1].addr = data->config->addr;
			msgs[1].flags = flags & TWI_M_TEN;
			msgs[1].flags |= TWI_M_RD;
			msgs[1].len = readlen;
			msgs[1].buf = readbuf;

			ret = hal_twi_xfer(i2c_port, msgs, 1);
			if (ret < 0) {
				tlsc_err
				    ("[IIC]: i2c_transfer(1) error, addr= 0x%02x!!\n",
				     writebuf[0]);
				tlsc_err
				    ("[IIC]: i2c_transfer(1) error, ret=%d, rlen=%d, wlen=%d!!\n",
				     ret, readlen, writelen);
			} else {
				ret =
				    hal_twi_xfer(i2c_port, &msgs[1], 1);
				if (ret < 0) {
					tlsc_err
					    ("[IIC]: i2c_transfer(2) error, addr= 0x%x!!\n",
					     writebuf[0]);
					tlsc_err
					    ("[IIC]: i2c_transfer(2) error, ret=%d, rlen=%d, wlen=%d!!\n",
					     ret, readlen, writelen);
				}
			}
		} else {
			msgs[1].addr = data->config->addr;
			msgs[1].flags = TWI_M_RD;
			msgs[1].len = readlen;
			msgs[1].buf = readbuf;

			ret = hal_twi_xfer(i2c_port, &msgs[1], 1);
			if (ret < 0) {
				tlsc_err
				    ("[IIC]: i2c_transfer(read) error, ret=%d, rlen=%d, wlen=%d!!",
				     ret, readlen, writelen);
			}
		}
	}

	return ret;
}


int tlsc6x_i2c_write(struct tlsc6x_drv_data *data, unsigned char *writebuf, int writelen)
{
	int ret = 0;
	u8 i2c_port = data->config->twi_id;
	struct twi_msg msgs;

	if (writelen > 0) {
		msgs.addr = data->config->addr;
		msgs.flags = 0;
		msgs.len = writelen;
		msgs.buf = writebuf;

		ret = hal_twi_xfer(i2c_port, &msgs, 1);
		if (ret < 0) {
			tlsc_err("[IIC]: i2c_transfer(write) error, ret=%d!!\n",
				 ret);
		}
	}

	return ret;
}

int tlsc6x_read_reg(struct tlsc6x_drv_data *data, unsigned char regaddr, unsigned char *regvalue)
{
	return tlsc6x_i2c_read(data, &regaddr, 1, regvalue, 1);
}

static int tlsc6x_update_data(struct tlsc6x_drv_data *parm)
{
	struct tlsc6x_drv_data *data = parm;
	struct tlsc6x_hw_cfg *config = data->config;
#ifdef CONFIG_INPUT_TOUCHSCREEN
	struct touch_sample_s sample;
	memset(&sample, 0, sizeof(sample));
#endif
	u8 buf[20] = { 0 };
	int ret = -1;
	int i;
	u16 x, y;
	//u8 tlsc_pressure, tlsc_size, tlsc_event, touch_point, finger_id;
	u8 tlsc_pressure, tlsc_event, touch_point, finger_id;

	ret = tlsc6x_i2c_read(data, buf, 1, buf, 18);
	if (ret < 0) {
		tlsc_err("%s read_data i2c_rxdata failed: %d\n", __func__, ret);
		return ret;
	}

	touch_point = buf[2] & 0x07;

		for (i = 0; i < TS_MAX_FINGER; i++) {
		if ((buf[6 * i + 3] & 0xc0) == 0xc0) {
			continue;
		}
		x = (s16) (buf[6 * i + 3] & 0x0F) << 8 | (s16) buf[6 * i + 4];
		y = (s16) (buf[6 * i + 5] & 0x0F) << 8 | (s16) buf[6 * i + 6];
		if (config->exchange_x_y_flag)
			swap(x, y);
		if (config->revert_x_flag)
			x = config->screen_max_x - x;
		if (config->revert_y_flag)
			y = config->screen_max_y - y;

		tlsc_pressure = buf[6 * i + 7];
		if (tlsc_pressure > 127) {
			tlsc_pressure = 127;
		}
		//tlsc_size = (buf[6 * i + 8] >> 4) & 0x0F;
		tlsc_event = (buf[6 * i + 3] >> 4) & 0x0F;
		finger_id = (buf[6 * i + 5] >> 4) & 0x0F;
		/*if ((buf[6 * i + 3] & 0x40) == 0x0) {
			if (y == 1500) {
				if (x == 40) {
				}
				if (x == 80) {
				}
				if (x == 120) {
				}
			} else {
			}
		}*/
#ifdef CONFIG_INPUT_TOUCHSCREEN
		if (tlsc_event == 0)
			sample.point[0].flags = TOUCH_DOWN;
		else if (tlsc_event == 8)
			sample.point[0].flags = TOUCH_MOVE;
		else if (tlsc_event == 4)
			sample.point[0].flags = TOUCH_UP;
		sample.point[0].x = x;
		sample.point[0].y = y;
		sample.point[0].pressure = tlsc_pressure;
		sample.point[0].id = finger_id;
		sample.npoints = 1;
		touch_event(data->lower.priv, &sample);
#endif
	}
	if (touch_point == 0) {
		if (y == 1500) {
			if (x == 40) {
			}
			if (x == 80) {
			}
			if (x == 120) {
			}
		}
	}
	return 0;

}

void touch_event_handler(hal_work *work, void *work_data)
{
	struct tlsc6x_drv_data *data = (struct tlsc6x_drv_data *)work_data;
	tlsc6x_update_data(data);
}

static hal_irqreturn_t tlsc6x_irq_handler(void *parm)
{
	struct tlsc6x_drv_data *data = (struct tlsc6x_drv_data *)parm;
	int ret = hal_workqueue_dowork(data->workqueue, &data->work);
	if (ret == HAL_OK)
		return HAL_IRQ_OK;
	else
		return HAL_IRQ_ERR;
}

static void tlsc6x_tpd_reset(int reset_gpio)
{
	hal_gpio_set_data(reset_gpio, 1);
	hal_msleep(1);
	hal_gpio_set_data(reset_gpio, 0);
	hal_msleep(20);
	hal_gpio_set_data(reset_gpio, 1);
	hal_msleep(30);
}

static int tlsc6x_hw_init(struct tlsc6x_drv_data *data)
{
	int ret;
	int reset_count = 0;
	struct tlsc6x_hw_cfg *config = data->config;

	ret = hal_gpio_pinmux_set_function(config->int_gpio, INT_GPIO_MUX);
	if (ret < 0) {
		tlsc_err("int gpio init err\n");
		return -1;
	}

	ret = hal_gpio_to_irq(config->int_gpio, &data->irq_num);
	if (ret < 0) {
		tlsc_err("get irq num err\n");
		return -1;
	}
	hal_gpio_set_pull(config->int_gpio, GPIO_PULL_UP);
	hal_gpio_set_driving_level(config->int_gpio, GPIO_DRIVING_LEVEL3);


	ret = hal_gpio_set_direction(config->reset_gpio, GPIO_DIRECTION_OUTPUT);
	if (ret < 0) {
		tlsc_err("reset gpio init err\n");
		return -1;
	}
	while(reset_count++ <= 3) {
		tlsc6x_tpd_reset(config->reset_gpio);
		g_is_telink_comp = tlsc6x_tp_dect(data);
		if (g_is_telink_comp)
			break;
	}
	if (g_is_telink_comp) {
		tlsc6x_tpd_reset(config->reset_gpio);
	} else {
		printf("tlsc6x_tp_dect err\n");
		return -1;
	}



	return 0;
}

static inline void get_tlsc6x_cfg(struct tlsc6x_drv_data *data)
{
	data->config = &tlsc6x_cfg;
}


int tlsc6x_init(void)
{
	int ret;

	tlsc6x_data = malloc(sizeof(struct tlsc6x_drv_data));
	if (NULL == tlsc6x_data) {
		tlsc_err("malloc tlsc6x_data err\n");
		return -1;
	}
	memset(tlsc6x_data, 0, sizeof(struct tlsc6x_drv_data));
	get_tlsc6x_cfg(tlsc6x_data);

	hal_twi_init(tlsc6x_data->config->twi_id);
	hal_twi_control(tlsc6x_data->config->twi_id, I2C_SLAVE,
			&tlsc6x_data->config->addr);
	ret = tlsc6x_hw_init(tlsc6x_data);
	if (ret < 0) {
		tlsc_err("tlsc6x_hw_init err\n");
		goto err_free_data;
	}

	ret = hal_gpio_irq_request(tlsc6x_data->irq_num, tlsc6x_irq_handler,
			IRQ_TYPE_EDGE_FALLING, tlsc6x_data);
	if (ret < 0) {
		tlsc_err("irq request err\n");
		goto err_free_data;
	}
	ret = hal_gpio_irq_enable(tlsc6x_data->irq_num);
	if (ret < 0) {
		tlsc_err("irq request err\n");
		goto err_free_irq;
	}

	tlsc6x_data->workqueue = hal_workqueue_create("tlsc6x_workqueue", 1024, 224);
	hal_work_init(&tlsc6x_data->work, touch_event_handler, tlsc6x_data);
	if (tlsc6x_data->workqueue == NULL) {
		tlsc_err("workqueue create error\n");
		goto err_disable_irq;
	}

#ifdef CONFIG_INPUT_TOUCHSCREEN
	tlsc6x_data->lower.maxpoint = 1;
	ret = touch_register(&tlsc6x_data->lower, "/dev/input0", 4);
	if (ret < 0) {
		tlsc_err("irq request err\n");
		goto err_destroy_workqueue;
	}
#endif
	return 0;
err_destroy_workqueue:
	hal_workqueue_destroy(tlsc6x_data->workqueue);
err_disable_irq:
	hal_gpio_irq_disable(tlsc6x_data->irq_num);
err_free_irq:
	hal_gpio_irq_free(tlsc6x_data->irq_num);
err_free_data:
	free(tlsc6x_data);
	tlsc6x_data = NULL;
	return -1;
}

int tlsc6x_deinit(void)
{
	if (NULL == tlsc6x_data) {
		tlsc_err("tlsc6x is not init\n");
		return -1;
	}

	hal_workqueue_destroy(tlsc6x_data->workqueue);
	hal_gpio_irq_disable(tlsc6x_data->irq_num);

	hal_gpio_irq_free(tlsc6x_data->irq_num);

	free(tlsc6x_data);

	tlsc6x_data = NULL;

	return 0;
}


