#ifndef __R528_RTK_COEX_H__
#define __R528_RTK_COEX_H__
#include <stdint.h>
#include <nuttx/wqueue.h>
#include <nuttx/mutex.h>
#include <nuttx/wireless/bluetooth/bt_driver.h>

typedef uint8_t         u8;
typedef uint16_t        u16;
typedef uint32_t        u32;

struct list_head {
	FAR struct list_head *next, *prev;
};

#define A2DP_SIGNAL	0x01
#define A2DP_MEDIA	0x02
/* Bluetooth profiling */
enum __profile_type {
	PROFILE_SCO = 0,
	PROFILE_HID = 1,
	PROFILE_A2DP_RTK = 2,
	PROFILE_PAN = 3,
	PROFILE_HID2 = 4, /* hid interval */
	PROFILE_HOGP = 5,
	PROFILE_VOICE = 6,
	PROFILE_SINK = 7,
	PROFILE_MAX_RTK = 8
};


struct pf_pkt_icount {
	u32 a2dp;
	u32 pan;
	u32 hogp;
	u32 voice;
};

struct rtl_coex_cmd {
	struct list_head list;
	FAR u8 *data;
	uint16_t len;
};

struct rtl_btpf {
	struct list_head	conn_list;
	struct list_head    pending_cmd_list;

	u8  pending_cmd_num;
	u8	pf_bits;
	u8	pf_state;
	int	pf_refs[PROFILE_MAX_RTK];

	struct pf_pkt_icount	icount;

	struct work_s a2dp_worker;
	struct work_s pan_worker;
	struct work_s cmd_worker;

	mutex_t coexlock;
};

struct rtk_coex_s
{
    struct bt_driver_s dev;
    FAR struct bt_driver_s *drv;
};

FAR struct bt_driver_s *rtk_coex_register(FAR struct bt_driver_s *drv);

void rtk_coex_init(void);
void rtk_coex_deinit(void);

#endif

