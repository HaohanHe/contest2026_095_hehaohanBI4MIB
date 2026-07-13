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

#ifndef _DISP_SYS_INTF_
#define _DISP_SYS_INTF_

#include "de/include.h"
#include <sunxi_hal_regulator.h>

#if defined(CONFIG_ARCH_SUN8IW19) || defined(CONFIG_ARCH_SUN8IW20)
#define RTOS_SYS_CONFIG_USE 0
#else
#define RTOS_SYS_CONFIG_USE 1
#endif


struct disp_gpio_set_t {
	char gpio_name[32];
	u32 port;
	u32 port_num;
	u32 mul_sel;
	u32 pull;
	u32 drv_level;
	u32 data;
	u32 gpio;
};

struct disp_pwm_dev {
	u32 pwm_channel_id;
	struct pwm_config cfg;
	bool enable;
};

/**
 * disp_power_t
 */
struct disp_power_t {
	char power_name[32];
	/*see sunxi_hal_regulator.h */
	enum REGULATOR_TYPE_ENUM power_type;
	enum REGULATOR_ID_ENUM power_id;
	/*unit:uV, 1V=1000000uV */
	u32 power_vol;
	bool always_on;
};

#define DISP_PIN_STATE_ACTIVE "active"
#define DISP_PIN_STATE_SLEEP "sleep"

#define DISP_BYTE_ALIGN(x) (((x + (4*1024-1)) >> 12) << 12)

void disp_sys_cache_flush(void *address, u32 length, u32 flags);

int disp_sys_register_irq(u32 IrqNo, u32 Flags, void *Handler, void *pArg,
			  u32 DataSize, u32 Prio);
void disp_sys_unregister_irq(u32 IrqNo, void *Handler, void *pArg);
void disp_sys_disable_irq(u32 IrqNo);
int disp_get_compat_lcd_panel_num(int disp);
void disp_sys_enable_irq(u32 IrqNo);

/* returns: 0:invalid, 1: int; 2:str, 3: gpio */
s32 disp_sys_script_get_item(char *main_name, char *sub_name, s32 value[],
			     s32 type);

int disp_sys_get_ic_ver(void);

int disp_sys_gpio_request(struct disp_gpio_set_t *gpio_list,
			  u32 group_count_max);
int disp_sys_gpio_request_simple(struct disp_gpio_set_t *gpio_list,
				 u32 group_count_max);
int disp_sys_gpio_release(int p_handler, s32 if_release_to_default_status);

/* direction: 0:input, 1:output */
int disp_sys_gpio_set_direction(u32 p_handler, u32 direction,
				const char *gpio_name);
int disp_sys_gpio_get_value(u32 p_handler, const char *gpio_name);
int disp_sys_gpio_set_value(u32 p_handler, u32 value_to_gpio,
			    const char *gpio_name);
int disp_sys_pin_set_state(char *dev_name, char *name);

s32 disp_sys_power_enable(void *p_power);
s32 disp_sys_power_disable(void *p_power);
void *disp_sys_malloc(u32 size);

uintptr_t disp_sys_pwm_request(u32 pwm_id);
int disp_sys_pwm_free(uintptr_t p_handler);
int disp_sys_pwm_enable(uintptr_t p_handler);
int disp_sys_pwm_disable(uintptr_t p_handler);
int disp_sys_pwm_config(uintptr_t p_handler, int duty_ns, int period_ns);
int disp_sys_pwm_set_polarity(uintptr_t p_handler, int polarity);
s32 disp_delay_us(u32 us);
s32 disp_delay_ms(u32 ms);
u32 disp_getprop_regbase(char *main_name, u32 index);
u32 disp_getprop_irq(char *main_name, u32 index);
#ifdef CONFIG_ARCH_SUN8IW19
u32 disp_getprop_clk(char *main_name, u32 index);
#else
u32 disp_getprop_clk(char *main_name);
#endif
struct reset_control *disp_get_rst_by_name(char *main_name);
void disp_sys_free(void *ptr);
int disp_sys_mutex_init(hal_sem_t *lock);
int disp_sys_mutex_unlock(hal_sem_t *sem);
int disp_sys_mutex_lock(hal_sem_t *sem);
void *disp_dma_malloc(u32 num_bytes, void *phys_addr);
void disp_dma_free(void *virt_addr, void *phys_addr, u32 num_bytes);
s32 disp_sys_clk_set_rate(hal_clk_id_t p_clk, u32 rate);
u32 disp_sys_clk_get_rate(hal_clk_id_t p_clk);
s32 disp_sys_clk_set_parent(hal_clk_id_t clk, hal_clk_id_t parent);
hal_clk_id_t disp_sys_clk_get_parent(hal_clk_id_t clk);
s32 disp_sys_clk_enable(hal_clk_id_t clk);
s32 disp_sys_clk_disable(hal_clk_id_t clk);
bool disp_clock_is_enabled(hal_clk_id_t clk);

#endif
