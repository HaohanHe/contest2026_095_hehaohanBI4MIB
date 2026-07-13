#ifndef __CIR_TX_SUN20IW1_H_
#define __CIR_TX_SUN20IW1_H_

#define SUNXI_IRQ_IRTX 51

// clang-format off
#define SUNXI_IRTX_PBASE        0x02003000
#define IRADC_PIN               GPIO_PB0
#define IR_MUXSEL               3

#define SUNXI_IRTX_RESET_TYPE   HAL_SUNXI_RESET
#define SUNXI_IRTX_CLK_TYPE     HAL_SUNXI_CCU
#define SUNXI_RST_IRTX          RST_BUS_IR_TX
#define SUNXI_CLK_BUS_IRTX      CLK_BUS_IR_TX
#define SUNXI_CLK_MODULE_IRTX   CLK_IR_TX
// clang-format on

#endif /* __CIR_TX_SUN20IW1_H_ */
