#include <nuttx/config.h>
#include <stddef.h>
#include <stdint.h>

/* up_perf_init - Stub needed by r528_boot.c */
void up_perf_init(void *arg) { }

/* Bluetooth AVRCP stubs needed by frameworks/connectivity/bluetooth/service/stacks/zephyr/sal_avrcp_interface.c */

#ifndef CONFIG_BLUETOOTH_STACK_BREDR_ZBLUE

void bt_avrcp_cttg_register_cb(void *cb) { }
int bt_avrcp_cttg_connect(void *conn) { return 0; }
int bt_avrcp_cttg_disconnect(void *conn) { return 0; }

int bt_avrcp_ct_pass_through_cmd(void *conn, int op_id, int state) { return 0; }
int bt_avrcp_ct_get_play_status(void *conn) { return 0; }
int bt_avrcp_ct_get_id3_info(void *conn) { return 0; }
int bt_pts_avrcp_ct_get_capabilities(void *conn) { return 0; }
#endif
