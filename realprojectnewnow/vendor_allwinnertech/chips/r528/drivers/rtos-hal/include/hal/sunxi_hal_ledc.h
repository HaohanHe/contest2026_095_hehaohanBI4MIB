#ifndef __HAL_LEDC_H
#define __HAL_LEDC_H
#include "sunxi_hal_common.h"


#include <hal_sem.h>
#include <hal_clk.h>
#include <hal_reset.h>

#ifdef __cplusplus
extern "C" {
#endif

/* LEDC register offset */
#define LEDC_CTRL_REG       		(0x00) 	/* LEDC Control Register */
#define LED_T01_TIMING_CTRL_REG     (0x04) 	/* LED T0 & 1 Timing Control Register */
#define LEDC_DATA_FINISH_CNT_REG   	(0x08) 	/* LEDC Data Finish Counter Register */
#define LED_RST_TIMING_CTRL_REG    	(0x0c) 	/* LED Reset Timing Control Register */
#define LEDC_WAIT_TIME0_CTRL_REG	(0x10)	/* LEDC Wait Time0 Control Register */
#define LEDC_DATA_REG    			(0x14) 	/* LEDC Data Register */
#define LEDC_DMA_CTRL_REG      		(0X18) 	/* LEDC Dma Control Register */
#define LEDC_INTC_REG   			(0x1c)	/* LEDC Interrupt Control Register */
#define LEDC_INTS_REG   			(0x20)	/* LEDC Interrupt Status Register */
#define LEDC_WAIT_TIME1_CTRL_REG   	(0x28) 	/* LEDC Wait Time1 Control Register */
#define LEDC_VER_NUM_REG   			(0x2C) 	/* LEDC Version Number Register */
#define LEDC_FIFO_DATA0_REG   		(0x30) 	/* LEDC Fifo Data0 Register */
#define LEDC_FIFO_DATA1_REG   		(0x34) 	/* LEDC Fifo Data1 Register */
#define LEDC_FIFO_DATA2_REG   		(0x38) 	/* LEDC Fifo Data2 Register */

#define LEDC_MAX_LED_COUNT 1024

#define LEDC_DEFAULT_LED_COUNT 1

#define LEDC_RESET_TIME_MIN_NS 84
#define LEDC_RESET_TIME_MAX_NS 327000

#define LEDC_T1H_MIN_NS 84
#define LEDC_T1H_MAX_NS 2560

#define LEDC_T1L_MIN_NS 84
#define LEDC_T1L_MAX_NS 1280

#define LEDC_T0H_MIN_NS 84
#define LEDC_T0H_MAX_NS 1280

#define LEDC_T0L_MIN_NS 84
#define LEDC_T0L_MAX_NS 2560

#define LEDC_WAIT_TIME0_MIN_NS 84
#define LEDC_WAIT_TIME0_MAX_NS 10000

#define LEDC_WAIT_TIME1_MIN_NS 84
#define LEDC_WAIT_TIME1_MAX_NS 85000000000

#define LEDC_WAIT_DATA_TIME_MIN_NS 84
#define LEDC_WAIT_DATA_TIME_MAX_NS_IC 655000
#define LEDC_WAIT_DATA_TIME_MAX_NS_FPGA 20000000


#define SUNXI_LEDC_FIFO_DEPTH 32
#define RESULT_COMPLETE 1
#define RESULT_ERR      2

enum ledc_output_mode_val {
	LEDC_OUTPUT_GRB = 0 << 6,
	LEDC_OUTPUT_GBR = 1 << 6,
	LEDC_OUTPUT_RGB = 2 << 6,
	LEDC_OUTPUT_RBG = 3 << 6,
	LEDC_OUTPUT_BGR = 4 << 6,
	LEDC_OUTPUT_BRG = 5 << 6
};

enum {
	DEBUG_INIT    = 1U << 0,
	DEBUG_SUSPEND = 1U << 1,
	DEBUG_INFO    = 1U << 2,
	DEBUG_INFO1   = 1U << 3,
	DEBUG_INFO2   = 1U << 4,
};

struct ledc_config {
	unsigned int led_count;
	unsigned int reset_ns;
	unsigned int t1h_ns;
	unsigned int t1l_ns;
	unsigned int t0h_ns;
	unsigned int t0l_ns;
	unsigned int wait_time0_ns;
	unsigned long long wait_time1_ns;
	unsigned int wait_data_time_ns;
	char *output_mode;
	unsigned int *align_dma_buf;
	unsigned int *data;
	unsigned int length;
};

enum ledc_irq_ctrl_reg {
	LEDC_TRANS_FINISH_INT_EN     = (1 << 0),
	LEDC_FIFO_CPUREQ_INT_EN      = (1 << 1),
	LEDC_WAITDATA_TIMEOUT_INT_EN = (1 << 3),
	LEDC_FIFO_OVERFLOW_INT_EN    = (1 << 4),
	LEDC_GLOBAL_INT_EN           = (1 << 5),
};

enum ledc_irq_status_reg {
	LEDC_TRANS_FINISH_INT     = (1 << 0),
	LEDC_FIFO_CPUREQ_INT      = (1 << 1),
	LEDC_WAITDATA_TIMEOUT_INT = (1 << 3),
	LEDC_FIFO_OVERFLOW_INT    = (1 << 4),
	LEDC_FIFO_FULL            = (1 << 16),
	LEDC_FIFO_EMPTY           = (1 << 17),
};

static u32 sunxi_ledc_regs_offset[] = {
	LEDC_CTRL_REG,
	LED_RST_TIMING_CTRL_REG,
	LED_T01_TIMING_CTRL_REG,
	LEDC_WAIT_TIME0_CTRL_REG,
	LEDC_WAIT_TIME1_CTRL_REG,
	LEDC_INTC_REG,
	LEDC_DATA_REG,
	LEDC_DMA_CTRL_REG,
};

struct sunxi_led {
	struct reset_control *reset;
	hal_clk_t mod_clk;
	hal_clk_t bus_clk;
	u8 result;
	struct ledc_config config;
	u32 regs_backup[ARRAY_SIZE(sunxi_ledc_regs_offset)];
};

#define LEDC_CLK_TYPE HAL_SUNXI_CCU
#define LEDC_CLK_ID CLK_BUS_LEDC

void hal_ledc_init(void);
void hal_ledc_deinit(void);
void hal_ledc_trans_data(struct ledc_config *ledc);
void hal_ledc_clear_all_irq(void);
unsigned int hal_ledc_get_irq_status(void);
void hal_ledc_dma_callback(void *para);
void hal_ledc_reset(void);
int sunxi_led_init(void);
int sunxi_set_led_brightness(int led_num, unsigned int brightness);
//int sunxi_set_all_led(int led_num, unsigned int brightness);
#ifdef __cplusplus
}
#endif
#endif
