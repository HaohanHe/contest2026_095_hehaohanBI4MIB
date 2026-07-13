#ifndef TLSC6X_H
#define TLSC6X_H

#include <stdio.h>
#include <hal_timer.h>
#include <hal_sem.h>
#include <hal_workqueue.h>
#include <stdint.h>
#ifdef CONFIG_INPUT_TOUCHSCREEN
#include <nuttx/input/touchscreen.h>
#endif
#include <syslog.h>

#define DEBUG

#ifdef DEBUG
#define tlsc_info(x...) syslog(LOG_INFO, "[tlsc][info] " x)
#define TLSC_FUNC_ENTER() syslog(LOG_INFO, "[tlsc]%s: Enter\n", __func__)
#define TLSC_FUNC_EXIT() syslog(LOG_INFO, "[tlsc]%s: Exit\n", __func__)
#else
#define tlsc_info(x...)
#define TLSC_FUNC_ENTER()
#define TLSC_FUNC_EXIT()
#endif


//#define tlsc_err(x...) printf("[tlsc][error] " x)
#define tlsc_err(x...) syslog(LOG_ERR, "[tlsc][error] " x)

#define swap(a, b) \
	do { typeof(a) __tmp = (a); (a) = (b); (b) = __tmp; } while (0)

#define u8 unsigned char
#define u16 unsigned short
#define u32 unsigned int

#define TLSC_MUL_VENDOR  /*是否兼容多家屏厂与多款TP*/
#define TLSC_AUTO_UPGRADE 		//firmware auto update
//#define TLSC_3535

struct tlsc6x_updfile_header {
	u32 sig;
	u32 resv;
	u32 n_cfg;
	u32 n_match;
	u32 len_cfg;
	u32 len_boot;
};


struct tlsc6x_hw_cfg {
	int twi_id;
	int addr;
	int int_gpio;
	int reset_gpio;
	int screen_max_x;
	int screen_max_y;
	int revert_x_flag;
	int revert_y_flag;
	int exchange_x_y_flag;
};

struct tlsc6x_drv_data {
	struct tlsc6x_hw_cfg	*config;
	uint32_t irq_num;
	osal_timer_t tp_timer;

	hal_workqueue *workqueue;
	hal_work work;

	struct sunxi_input_dev *input_dev;
	//struct ts_event		event;
#ifdef CONFIG_INPUT_TOUCHSCREEN
	struct touch_lowerhalf_s lower;
#endif
};

int tlsc6x_i2c_read(struct tlsc6x_drv_data *data, unsigned char *writebuf, int writelen,
			unsigned char *readbuf, int readlen);

int tlsc6x_i2c_write(struct tlsc6x_drv_data *data, unsigned char *writebuf, int writelen);

int tlsc6x_tp_dect(struct tlsc6x_drv_data *data);
#endif  /*TLSC6X_H*/
