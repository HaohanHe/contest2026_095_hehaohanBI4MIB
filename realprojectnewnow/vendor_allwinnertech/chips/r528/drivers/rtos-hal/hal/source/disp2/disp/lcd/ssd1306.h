#ifndef __SSD1306_PANEL_H__
#define  __SSD1306_PANEL_H__

#include "panels.h"

extern struct __lcd_panel ssd1306_panel;

extern s32 bsp_disp_get_panel_info(u32 screen_id, struct disp_panel_para *info);
#endif
