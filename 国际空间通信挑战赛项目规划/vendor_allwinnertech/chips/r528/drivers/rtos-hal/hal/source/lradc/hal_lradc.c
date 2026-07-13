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


#include <sunxi_hal_lradc.h>


lradc_func_data lradc_priv;
static uint32_t base_addr = LRADC_BASE;

static uint32_t ctrl_para = (LRADC_HOLD_EN & LEVELB_VOL & LRADC_SAMPLE_2KHZ) 
				            | FIRST_CONCERT_DLY | ADC_CHAN_SELECT | KEY_MODE_SELECT | LRADC_EN;

static uint32_t irq_para = LRADC_ADC0_UP_EN | LRADC_ADC0_DOWN_EN | LRADC_ADC0_DATA_EN;
static uint32_t bgr_para = LRADC_RST | LRADC_GATING_EN;

static void lradc_clk_init(void)
{
	hal_writel(bgr_para, LRADC_BGR_REG_ADDR);
}

static void lradc_ctrl_set(void)
{
	uint32_t reg_val = 0;

	reg_val = hal_readl(base_addr + LRADC_CTRL_REG);
	reg_val |= ctrl_para;
	hal_writel(reg_val, base_addr + LRADC_CTRL_REG);
}

static void lradc_ctrl_reset(void)
{
	uint32_t reg_val = 0;

	reg_val = hal_readl(base_addr + LRADC_CTRL_REG);
	reg_val &= ~ctrl_para;
	hal_writel(reg_val, base_addr + LRADC_CTRL_REG);
}

static void lradc_irq_set(void)
{
	uint32_t reg_val = 0;

	reg_val = hal_readl(base_addr + LRADC_INTC_REG);
	reg_val |= irq_para;
	hal_writel(reg_val, base_addr + LRADC_INTC_REG);
}

static void lradc_irq_reset(void)
{
	uint32_t reg_val = 0;

	reg_val = hal_readl(base_addr + LRADC_INTC_REG);
	reg_val &= ~irq_para;
	hal_writel(reg_val, base_addr + LRADC_INTC_REG);
}

static void lradc_register_callback(lradc_callback_t user_callback)
{
	lradc_priv.func = user_callback;
}

static hal_irqreturn_t lradc_irq_handler(void *priv_data)
{
	lradc_func_data *local_lradc_priv = priv_data;
	lradc_callback_t callback = local_lradc_priv->func;

	uint32_t irq_status = hal_readl(base_addr + LRADC_INTS_REG);
	uint32_t reg_val = hal_readl(base_addr + LRADC_DATA0_REG);
	if(irq_status & 0x2){
		syslog(LOG_INFO,"press down,data_val=%lu\n",reg_val);
		if (NULL != callback)
			callback(irq_status, reg_val);
	}else if(irq_status & 0x10){
		syslog(LOG_INFO,"press up,data_val=%lu\n",reg_val);
		if (NULL != callback)
			callback(irq_status, reg_val);
	}
	hal_writel(irq_status, base_addr + LRADC_INTS_REG);

	return 0;
}

static int32_t lradc_init_irq(void)
{
	uint32_t irqn = SUNXI_IRQ_LRADC;
	if (hal_request_irq(irqn, lradc_irq_handler, "lradc", &lradc_priv) < 0) {
		return -1;
	}

	hal_enable_irq(irqn);

	return 0;
}

hal_lradc_status_t hal_lradc_init(void)
{
	//sunxi_clk_init();
	lradc_clk_init();
	lradc_ctrl_set();
	lradc_irq_set();
	lradc_init_irq();

	return HAL_ADC_STATUS_OK;
}

hal_lradc_status_t hal_lradc_deinit(void)
{
	lradc_irq_reset();
	lradc_ctrl_reset();

	return HAL_ADC_STATUS_OK;
}

uint32_t hal_lradc_get_data(void)
{
	uint32_t reg_val;
	reg_val = readl(base_addr+LRADC_DATA0_REG);
	reg_val = reg_val & LRADC_DATA_MARK;

	return reg_val;
}

uint32_t hal_lradc_register_callback(lradc_callback_t callback)
{
	uint32_t retval = 0;
	lradc_register_callback(callback);
	retval = hal_lradc_get_data();
	return retval;
}

