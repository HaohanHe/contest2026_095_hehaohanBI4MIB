/**
 *******************************************************************************
 * Copyright(c) 2023 Realtek Semiconductor Corporation. All rights reserved.
 *******************************************************************************
 */

#ifndef __R528_COMP_BLUETOOTH_H
#define __R528_COMP_BLUETOOTH_H

#include <stdint.h>
#include <sys/types.h>

#define RTKHCI_COMMAND_FRAGMENT_SIZE (252)
#define RTKHCI_COMMAND_DONE (0)
#define RTKHCI_COMMAND_VALID (1)

#define BT_DEFAUT_BAUDRATE 115200
#define BT_CONFIG_BAUDRATE 1500000

int hci_set_init_config_mac (uint8_t *addr, uint8_t diffvalue);
void rtkbt_free_fwc_buf(void);
int rtkbt_board_get_baudrate (uint32_t *bt_baudrate, uint32_t uart_baudrate);
int rtkbt_board_find_fw_patch (uint8_t chipid);
int rtkbt_board_fetch_command (uint8_t *command);
void rtkbt_board_reset (void);
void rtkbt_board_poweroff (void);
#endif