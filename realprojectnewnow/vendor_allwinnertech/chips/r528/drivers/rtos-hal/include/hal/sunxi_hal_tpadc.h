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

#ifndef HAL_TPADC_H
#define HAL_TPADC_H

#include "hal_clk.h"
#include "hal_reset.h"
#include "sunxi_hal_common.h"
#include <hal_log.h>
#include <interrupt.h>
#include <tpadc/platform_tpadc.h>
#include <tpadc/common_tpadc.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TPADC_DEBUG
#ifdef TPADC_DEBUG
#define TPADC_INFO(fmt, arg...) hal_log_info(fmt, ##arg)
#else
#define TPADC_INFO(fmt, arg...) do {}while(0)
#endif

#define TPADC_ERR(fmt, arg...) hal_log_err(fmt, ##arg)

typedef enum
{
    TPADC_IRQ_ERROR = -4,
    TPADC_CHANNEL_ERROR = -3,
    TPADC_CLK_ERROR = -2,
    TPADC_ERROR = -1,
    TPADC_OK = 0,
} hal_tpadc_status_t;

typedef enum
{
	DATA_MOVE = 0,
	DATA_UP,
	DATA_DOWN,
} data_flag_t;

typedef enum
{
	TP_CH_0 = 0,
	TP_CH_1,
	TP_CH_2,
	TP_CH_3,
	TP_CH_MAX,
} tp_channel_id;

typedef int (*tpadc_usercallback_t)(uint16_t x,uint16_t y, data_flag_t flag);
typedef int (*tpadc_adc_usercallback_t)(uint32_t data, tp_channel_id channel);

typedef struct hal_tpadc
{
    unsigned long reg_base;
    uint32_t channel_num;
    uint32_t irq_num;
    uint32_t rate;
    hal_clk_id_t bus_clk_id;
    hal_clk_id_t mod_clk_id;
    hal_reset_id_t rst_clk_id;
    hal_clk_t	bus_clk;
    hal_clk_t	mod_clk;
    struct reset_control	*rst_clk;
    tpadc_usercallback_t callback;
    tpadc_adc_usercallback_t adc_callback[TP_CH_MAX];
} hal_tpadc_t;

hal_tpadc_status_t hal_tpadc_init(void);
hal_tpadc_status_t hal_tpadc_exit(void);
hal_tpadc_status_t hal_tpadc_register_callback(tpadc_usercallback_t user_callback);

hal_tpadc_status_t hal_tpadc_adc_init(void);
hal_tpadc_status_t hal_tpadc_adc_channel_init(tp_channel_id channel);
hal_tpadc_status_t hal_tpadc_adc_channel_exit(tp_channel_id channel);
hal_tpadc_status_t hal_tpadc_adc_exit(void);
hal_tpadc_status_t hal_tpadc_adc_register_callback(tp_channel_id channel , tpadc_adc_usercallback_t user_callback);

hal_tpadc_status_t hal_tpadc_resume(void);
hal_tpadc_status_t hal_tpadc_suspend(void);

#ifdef __cplusplus
}
#endif

#endif
