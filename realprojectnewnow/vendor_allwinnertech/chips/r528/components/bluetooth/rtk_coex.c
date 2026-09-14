#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <nuttx/kmalloc.h>
#include <debug.h>

#include "rtk_coex.h"

struct rtl_btpf btpf_t;
FAR struct rtl_btpf *btpf = &btpf_t;
FAR static struct rtl_coex_cmd *p_curr_coex_cmd = NULL;

//#define COEX_DEBUG

#define HCI_EV_CMD_COMPLETE		0x0e
#define HCI_EV_CONN_COMPLETE		0x03
#define HCI_EV_SYNC_CONN_COMPLETE	0x2c
#define HCI_EV_DISCONN_COMPLETE		0x05
#define HCI_EV_MODE_CHANGE		0x14
#define HCI_EV_LE_META			0x3e
#define HCI_EV_LE_CONN_COMPLETE		0x01
#define HCI_EV_LE_CONN_UPDATE_COMPLETE	0x03
#define HCI_EV_LE_ENHANCED_CONN_COMPLETE 0x0a

#define L2CAP_CONN_REQ		0x02
#define L2CAP_CONN_RSP		0x03
#define L2CAP_DISCONN_REQ	0x06
#define L2CAP_DISCONN_RSP	0x07

#define RTL_FROM_REMOTE		0
#define RTL_TO_REMOTE		1

#define RTL_PROFILE_MATCH_SCID		(1 << 0)
#define RTL_PROFILE_MATCH_DCID		(1 << 1)

#define PAN_PACKET_COUNT                5

#define ACL_CONN	0x0
#define SYNC_CONN	0x1
#define LE_CONN		0x2

#define PSM_SDP     0x0001
#define PSM_RFCOMM  0x0003
#define PSM_PAN     0x000F
#define PSM_HID     0x0011
#define PSM_HID_INT 0x0013
#define PSM_AVCTP   0x0017
#define PSM_AVDTP   0x0019
#define PSM_FTP     0x1001
#define PSM_BIP     0x1003
#define PSM_OPP     0x1015

#define PAN_COUNT_TIMEOUT MSEC2TICK (1000) /* 1s */
#define A2DP_COUNT_TIMEOUT MSEC2TICK (1000) /* 1s */
#define COEX_CMD_TIMEOUT MSEC2TICK (10) /* 1s */

/* TODO */
#ifndef __LITTLE_ENDIAN
#define __LITTLE_ENDIAN
#endif

#ifdef __LITTLE_ENDIAN
struct sbc_frame_hdr {
	u8 syncword:8;		/* Sync word */
	u8 subbands:1;		/* Subbands */
	u8 allocation_method:1;	/* Allocation method */
	u8 channel_mode:2;		/* Channel mode */
	u8 blocks:2;		/* Blocks */
	u8 sampling_frequency:2;	/* Sampling frequency */
	u8 bitpool:8;		/* Bitpool */
	u8 crc_check:8;		/* CRC check */
} __attribute__ ((packed));

/*
 * only the bit field in 8-bit is affected by endian, not the 16-bit or 32-bit.
 * why?
 */
struct rtp_header {
	unsigned cc:4;
	unsigned x:1;
	unsigned p:1;
	unsigned v:2;

	unsigned pt:7;
	unsigned m:1;

	u16 sequence_number;
	u32 timestamp;
	u32 ssrc;
	u32 csrc[0];
} __attribute__ ((packed));

#else
/* big endian */
struct sbc_frame_hdr {
	u8 syncword:8;		/* Sync word */
	u8 sampling_frequency:2;	/* Sampling frequency */
	u8 blocks:2;		/* Blocks */
	u8 channel_mode:2;		/* Channel mode */
	u8 allocation_method:1;	/* Allocation method */
	u8 subbands:1;		/* Subbands */
	u8 bitpool:8;		/* Bitpool */
	u8 crc_check:8;		/* CRC check */
} __attribute__ ((packed));

struct rtp_header {
	unsigned v:2;
	unsigned p:1;
	unsigned x:1;
	unsigned cc:4;

	unsigned m:1;
	unsigned pt:7;

	u16 sequence_number;
	u32 timestamp;
	u32 ssrc;
	u32 csrc[0];
} __attribute__ ((packed));
#endif /* __LITTLE_ENDIAN */

/* FIXME: osif */
#define GFP_KERNEL	0

#define UINT16_TO_STREAM(p, u) {*(p)++ = (u8)u; *(p)++ = (u8)(u >> 8);}
#define STREAM_TO_UINT16(u, p) {u = ((u16)(*(p)) + (((u16)(*((p) + 1))) << 8)); (p) += 2;}

#define offset_of(TYPE, MEMBER)	((size_t)&((TYPE *)0)->MEMBER)
#define container_of(ptr, type, member) ({			\
	(type *)( (char *)ptr - offset_of(type,member) );})
#define list_entry(ptr, type, member) \
	container_of(ptr, type, member)

#define list_for_each_safe(pos, n, head) \
	for (pos = (head)->next, n = pos->next; pos != (head); \
		pos = n, n = pos->next)

static inline void __list_del(FAR struct list_head * prev, FAR struct list_head * next)
{
	next->prev = prev;
	prev->next = next;
}

static inline void __list_del_entry(FAR struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
}

static inline void list_del(FAR struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
	entry->next = entry;
	entry->prev = entry;
}

static inline void __list_add(FAR struct list_head *new,
			      FAR struct list_head *prev,
			      FAR struct list_head *next)
{
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

static inline void list_add_tail(FAR struct list_head *new, FAR struct list_head *head)
{
	__list_add(new, head->prev, head);
}

static inline void INIT_LIST_HEAD(FAR struct list_head *list)
{
	list->next = list;
	list->prev = list;
}

#ifdef COEX_DEBUG
#define rtlbt_dbg(fmt, ...) wlerr(fmt, ##__VA_ARGS__)
#else
#define rtlbt_dbg(fmt, ...)	do { } while (0)
#endif

/* HCI conn information */
struct rtl_hci_conn {
	struct list_head list;
	u8 type;
	u8 pf_bits;
	u16 handle;
	u8 pf_refs[PROFILE_MAX_RTK];
	struct list_head pf_list;
};

struct rtl_profile_id {
	u16 match_flags;
	u16 dcid;
	u16 scid;
};

/* profile information */
struct rtl_profile {
	struct list_head list;
	u16 psm;
	u16 dcid;
	u16 scid;
	u8  idx;
	u8  flags;
	FAR void *conn; /* point to HCI conn information */
};

static int psm_to_profile(u16 psm)
{
	switch (psm) {
	case PSM_AVCTP:
	case PSM_SDP:
		return -1; /* ignore */

	case PSM_HID:
	case PSM_HID_INT:
		return PROFILE_HID;

	case PSM_AVDTP:
		return PROFILE_A2DP_RTK;

	case PSM_PAN:
	case PSM_OPP:
	case PSM_FTP:
	case PSM_BIP:
	case PSM_RFCOMM:
		return PROFILE_PAN;

	default:
		return -1;
	}
}

FAR static struct rtl_profile *lookup_pf_by_psm(FAR struct rtl_hci_conn *conn,
					    u16 psm)
{
	FAR struct list_head *head = &conn->pf_list;
	FAR struct list_head *pos;
	FAR struct list_head *n;
	FAR struct rtl_profile *p;

	list_for_each_safe(pos, n, head) {
		p = list_entry(pos, struct rtl_profile, list);
		if (p->psm == psm)
			return p;
	}

	return NULL;
}

FAR static struct rtl_hci_conn *rtl_hci_conn_lookup(FAR struct bt_driver_s *driver,
						u16 handle)
{
	FAR struct list_head *head = &btpf->conn_list;
	FAR struct list_head *p, *n;
	FAR struct rtl_hci_conn *conn;

	list_for_each_safe(p, n, head) {
		conn = list_entry(p, struct rtl_hci_conn, list);
		if ((handle & 0xfff) == conn->handle)
			return conn;
	}

	return NULL;
}

static void rtl_profile_list_purge(FAR struct rtl_hci_conn *conn)
{
	FAR struct list_head *p, *n;
	FAR struct rtl_profile *pf;

	list_for_each_safe(p, n, &conn->pf_list) {
		pf = list_entry(p, struct rtl_profile, list);
		list_del(&pf->list);
		kmm_free(pf);
	}
}

static void rtl_hci_conn_list_purge(void)
{
	FAR struct list_head *head = &btpf->conn_list;
	FAR struct list_head *p, *n;
	FAR struct rtl_hci_conn *conn;

	list_for_each_safe(p, n, head) {
		conn = list_entry(p, struct rtl_hci_conn, list);
		if (conn) {
			rtl_profile_list_purge(conn);
			list_del(&conn->list);
			kmm_free(conn);
		}
	}
}
/*Need Confirm*/
static void rtl_pending_cmd_list_purge(void)
{
	FAR struct list_head *head = &btpf->pending_cmd_list;
	FAR struct list_head *p, *n;
	FAR struct rtl_coex_cmd *cmd;

	list_for_each_safe(p, n, head) {
		cmd = list_entry(p, struct rtl_coex_cmd, list);
		if (cmd) {
			list_del(&cmd->list);
			kmm_free(cmd->data);
			kmm_free(cmd);
		}
	}
}

FAR static struct rtl_coex_cmd *hci_pending_cmd_lookup(void)
{
	FAR struct list_head *head = &btpf->pending_cmd_list;
	FAR struct list_head *p, *n;
	FAR struct rtl_coex_cmd *cmd;

	list_for_each_safe(p, n, head) {
		cmd = list_entry(p, struct rtl_coex_cmd, list);
		return cmd;
	}

	return NULL;
}

FAR static struct rtl_profile *profile_alloc(u16 psm, u8 idx, u16 dcid, u16 scid)
{
	FAR struct rtl_profile *pf;

	pf = kmm_zalloc(sizeof(struct rtl_profile));
	if (!pf)
		return NULL;

	pf->psm = psm;
	pf->scid = scid;
	pf->dcid = dcid;
	pf->idx = idx;
	INIT_LIST_HEAD(&pf->list);

	return pf;
}

FAR static struct rtl_profile *rtl_profile_lookup(FAR struct list_head *head,
					      FAR struct rtl_profile_id *id)
{
	FAR struct list_head *p, *n;
	FAR struct rtl_profile *tmp;

	if (!id->match_flags) {
		wlwarn("RTKCOEX: no match flags");
		return NULL;
	}

	list_for_each_safe(p, n, head) {
		tmp = list_entry(p, struct rtl_profile, list);

		if ((id->match_flags & RTL_PROFILE_MATCH_SCID) &&
		    id->scid != tmp->scid)
			continue;

		if ((id->match_flags & RTL_PROFILE_MATCH_DCID) &&
		    id->dcid != tmp->dcid)
			continue;

		return tmp;
	}

	return NULL;
}

static void cmd_timer_func(FAR void *param)
{
	FAR struct bt_driver_s *driver = (FAR struct bt_driver_s *)param;
	FAR struct rtl_coex_cmd *cached_cmd;
	int ret;

	ret = nxmutex_lock(&btpf->coexlock);
	if (ret < 0)
	{
		wlerr("RTKCOEX: cannot get coexlock");
		return;
	}

	if (p_curr_coex_cmd == NULL && btpf->pending_cmd_num)
	{
		cached_cmd = hci_pending_cmd_lookup();
		if (cached_cmd)
		{
			list_del(&cached_cmd->list);
			p_curr_coex_cmd = cached_cmd;
			btpf->pending_cmd_num--;
			rtlbt_dbg("RTKCOEX: send coex cmd %04x to controller, cmd addr %p, data addr %p", *(FAR u16 *)(p_curr_coex_cmd->data), p_curr_coex_cmd, p_curr_coex_cmd->data);
			nxmutex_unlock(&btpf->coexlock);
			driver->send(driver, BT_CMD, p_curr_coex_cmd->data, p_curr_coex_cmd->len);
			return;
		}
	}
	else
	{
		rtlbt_dbg("RTKCOEX: Sending cmd now, cached cmd num is %u", btpf->pending_cmd_num);
	}

	nxmutex_unlock(&btpf->coexlock);
	return;
}

static void rtk_vendor_cmd_to_fw(FAR struct bt_driver_s *driver, u16 opcode, u32 buf_sz, FAR u8 *buf)
{
	FAR struct rtl_coex_cmd *pending_cmd = NULL;
	FAR u8 *cmd_buf;
	FAR u8 *p;
	u32 cmd_len;
	cmd_len = buf_sz + 3;

	cmd_buf = kmm_zalloc(cmd_len);
	if (!cmd_buf) {
		wlerr("RTKCOEX: alloc error");
		return;
	}

	p = cmd_buf;

	UINT16_TO_STREAM(p, opcode);
	*p++ = buf_sz;
	if (buf_sz)
		memcpy(p, buf, buf_sz);

	pending_cmd = kmm_zalloc(sizeof(struct rtl_coex_cmd));
	if (!pending_cmd)
	{
		kmm_free(cmd_buf);
		wlerr("RTKCOEX: alloc error");
		return;
	}
	pending_cmd->data = cmd_buf;
	pending_cmd->len = cmd_len;
	list_add_tail(&pending_cmd->list, &btpf->pending_cmd_list);
	btpf->pending_cmd_num++;

	rtlbt_dbg("RTKCOEX: add coex cmd %04x to pending list, cmd addr %p, data addr %p", *(FAR u16 *)(pending_cmd->data), pending_cmd, cmd_buf);

	if (work_available(&btpf->cmd_worker))
	{
		work_queue(LPWORK, &btpf->cmd_worker, cmd_timer_func, driver, 0);
	}
}

static void btpf_update_to_controller(FAR struct bt_driver_s *driver)
{
	FAR struct list_head *head = &btpf->conn_list;
	FAR struct list_head *pos, *n;
	FAR struct rtl_hci_conn *conn;
	u8 handle_num = 0;
	u32 buf_sz;
	FAR u8 *buf;
	FAR u8 *p;

	list_for_each_safe(pos, n, head) {
		conn = list_entry(pos, struct rtl_hci_conn, list);
		if (conn && conn->pf_bits)
			handle_num++;
	}

	buf_sz = 1 + handle_num * 3 + 1;

	buf = kmm_zalloc(buf_sz);

	if (!buf) {
		wlerr("RTKCOEX: alloc error");
		return;
	}

	p = buf;
	wlinfo("RTKCOEX: buf_sz %lu", buf_sz);
	wlinfo("RTKCOEX: handle num %u", handle_num);

	*p++ = handle_num;
	list_for_each_safe(pos, n, head) {
		conn = list_entry(pos, struct rtl_hci_conn, list);
		if (conn && conn->pf_bits) {
			wlinfo("RTKCOEX: handle 0x%04x", conn->handle);
			wlinfo("RTKCOEX: profile_bitmap 0x%02x",
				   conn->pf_bits);
			*p++ = (conn->handle & 0xff);
			*p++ = ((conn->handle >> 8) & 0xff);
			*p++ = conn->pf_bits;
			handle_num--;
		}

		if (!handle_num)
			break;
	}

	rtlbt_dbg("RTKCOEX: profile state 0x%02x", btpf->pf_state);

	*p++ = btpf->pf_state;

	rtk_vendor_cmd_to_fw(driver, 0xfc19, buf_sz, buf);

	kmm_free(buf);
}

static void update_profile_state(FAR struct bt_driver_s *driver, u8 idx, u8 busy)
{
	u8 update = 0;

	if (!(btpf->pf_bits & (1 << idx))) {
		wlerr("RTKCOEX: profile(%x) not exist", idx);
		return;
	}

	if (busy) {
		if (!(btpf->pf_state & (1 << idx))) {
			update = 1;
			btpf->pf_state |= (1 << idx);
		}
	} else {
		if (btpf->pf_state & (1 << idx)) {
			update = 1;
			btpf->pf_state &= ~(1 << idx);
		}
	}

	if (update) {
		wlinfo("RTKCOEX: pf_bits 0x%02x", btpf->pf_bits);
		wlinfo("RTKCOEX: pf_state 0x%02x", btpf->pf_state);
		btpf_update_to_controller(driver);
	}
}

static void a2dp_timer_func(FAR void *param)
{
	FAR struct bt_driver_s *driver = param;
	rtlbt_dbg("RTKCOEX: a2dp packets %lu", btpf->icount.a2dp);
	/*
	if (btpf->icount.a2dp)
		wlinfo("RTKCOEX: a2dp packets %u", btpf->icount.a2dp);
	*/
	if (!btpf->icount.a2dp) {
		/* TODO: if there are two a2dp links */
		if (btpf->pf_state & (1 << PROFILE_A2DP_RTK)) {
			wlinfo("RTKCOEX: a2dp busy->idle!");
			update_profile_state(driver, PROFILE_A2DP_RTK, 0);
			if (btpf->pf_bits & (1 << PROFILE_SINK))
				update_profile_state(driver, PROFILE_SINK, 0);
		}
	}
	btpf->icount.a2dp = 0;

	work_queue(LPWORK, &btpf->a2dp_worker, a2dp_timer_func,
				driver, A2DP_COUNT_TIMEOUT);
	/* mod_timer(&btpf->a2dp_count_timer,
	 * 	  jiffies + msecs_to_jiffies(1000));
	 */
}

static void pan_timer_func(FAR void *param)
{
	FAR struct bt_driver_s *driver = param;
	wlinfo("RTKCOEX: pan packets %lu", btpf->icount.pan);
	/*
	if (btpf->icount.pan)
		wlinfo("RTKCOEX: pan packets %d", btpf->icount.pan);
	*/
	if (btpf->icount.pan < PAN_PACKET_COUNT) {
		if (btpf->pf_state & (1 << PROFILE_PAN)) {
			wlinfo("RTKCOEX: pan busy->idle!");
			update_profile_state(driver, PROFILE_PAN, 0);
		}
	} else {
		if (!(btpf->pf_state & (1 << PROFILE_PAN))) {
			wlinfo("RTKCOEX: timeout_handler: pan idle->busy!");
			update_profile_state(driver, PROFILE_PAN, 1);
		}
	}
	btpf->icount.pan = 0;

	work_queue(LPWORK, &btpf->pan_worker, pan_timer_func,
				driver, PAN_COUNT_TIMEOUT);
	/* mod_timer(&btpf->pan_count_timer,
	 * 	  jiffies + msecs_to_jiffies(1000));
	 */
}

/* TODO */
int setup_monitor_timer(FAR struct bt_driver_s *driver, u8 idx)
{
	if (idx == PROFILE_A2DP_RTK)
	{
		wlinfo("RTKCOEX: ~~~~~set a2dp timer~~~~~~");
		work_queue(LPWORK, &btpf->a2dp_worker, a2dp_timer_func,
					driver, A2DP_COUNT_TIMEOUT);
	}

	if (idx == PROFILE_PAN)
	{
		wlinfo("RTKCOEX: ~~~~~set pan timer~~~~~~");
		work_queue(LPWORK, &btpf->pan_worker, pan_timer_func,
					driver, PAN_COUNT_TIMEOUT);
	}
	return 0;
}

void del_monitor_timer(FAR struct bt_driver_s *driver, u8 idx)
{
    if (idx == PROFILE_A2DP_RTK)
    {
        wlinfo("RTKCOEX: ~~~~~del a2dp timer~~~~~~");
		/*Need confirm*/
        work_cancel(LPWORK, &btpf->a2dp_worker);
    }
    if (idx == PROFILE_PAN)
    {
		wlinfo("RTKCOEX: ~~~~~del pan timer~~~~~~");
        work_cancel(LPWORK, &btpf->pan_worker);
    }
}

static int profile_conn_get(FAR struct bt_driver_s *driver, FAR struct rtl_hci_conn *conn,
			    u8 idx)
{
	int update = 0;
	u8 i;

	rtlbt_dbg("idx %u", idx);

	if (!conn || idx >= PROFILE_MAX_RTK)
		return -1;

	if (!btpf->pf_refs[idx]) {
		update = 1;
		btpf->pf_bits |= (1 << idx);

		/* SCO is always busy */
		if (idx == PROFILE_SCO)
			btpf->pf_state |= (1 << idx);

		/* TODO: */
		setup_monitor_timer(driver, idx);
	}
	btpf->pf_refs[idx]++;

	if (!conn->pf_refs[idx]) {
		update = 1;
		conn->pf_bits |= (1 << idx);
	}
	conn->pf_refs[idx]++;

	wlinfo("RTKCOEX: btpf->pf_bits 0x%02x", btpf->pf_bits);
	for (i = 0; i < PROFILE_MAX_RTK; i++)
		wlinfo("RTKCOEX: btpf->pf_refs[%u] %d", i,
			   btpf->pf_refs[i]);

	if (update)
		btpf_update_to_controller(driver);

	return 0;
}

static int profile_conn_put(FAR struct bt_driver_s *driver, FAR struct rtl_hci_conn *conn,
			    u8 idx)
{
	int need_update = 0;
	u8 i;

	rtlbt_dbg("idx %u", idx);

	if (!conn || idx >= PROFILE_MAX_RTK)
		return -1;

	if (!btpf->pf_refs[idx]) {
		wlinfo("RTKCOEX: profile refcount is 0");
		return -1;
	}

	btpf->pf_refs[idx]--;
	if (!btpf->pf_refs[idx]) {
		need_update = 1;
		btpf->pf_bits &= ~(1 << idx);
		btpf->pf_state &= ~(1 << idx);
		/* TODO: */
		del_monitor_timer(driver, idx);
	}

	conn->pf_refs[idx]--;
	if (!conn->pf_refs[idx]) {
		need_update = 1;
		conn->pf_bits &= ~(1 << idx);

		/* Clear hid interval if needed */
		if (idx == PROFILE_HID &&
		    (conn->pf_bits & (1 << PROFILE_HID2))) {
			conn->pf_bits &= ~(1 << PROFILE_HID2);
			btpf->pf_refs[PROFILE_HID2]--;
		}
	}

	wlinfo("RTKCOEX: btpf->pf_refs[%u] %d", idx,
		   btpf->pf_refs[idx]);
	wlinfo("RTKCOEX: pf_bits 0x%02x", btpf->pf_bits);
	for (i = 0; i < PROFILE_MAX_RTK; i++)
		wlinfo("RTKCOEX: btpf->pf_refs[%u] %d", i,
			   btpf->pf_refs[i]);

	if (need_update)
		btpf_update_to_controller(driver);

	return 0;
}

static void hid_state_update(FAR struct bt_driver_s *driver, u16 handle,
			     u16 interval)
{
	u8 update = 0;
	FAR struct rtl_hci_conn *conn;

	conn = rtl_hci_conn_lookup(driver, handle);
	if (!conn)
		return;

	wlinfo("RTKCOEX: handle 0x%04x, interval 0x%x", handle,
		   interval);
	if (!(conn->pf_bits & (1 << PROFILE_HID))) {
		rtlbt_dbg("hid not connected in the handle");
		return;
	}

	if (interval < 60) {
		if (!(conn->pf_bits & (1 << PROFILE_HID2))) {
			update = 1;
			conn->pf_bits |= (1 << PROFILE_HID2);

			btpf->pf_refs[PROFILE_HID2]++;
			if (btpf->pf_refs[PROFILE_HID2] == 1)
				btpf->pf_state |= (1 << PROFILE_HID);
		}
	} else {
		if (conn->pf_bits & (1 << PROFILE_HID2)) {
			update = 1;
			conn->pf_bits &= ~(1 << PROFILE_HID2);

			btpf->pf_refs[PROFILE_HID2]--;
			if (!btpf->pf_refs[PROFILE_HID2])
				btpf->pf_state &= ~(1 << PROFILE_HID);
		}
	}

	if (update)
		btpf_update_to_controller(driver);
}

static int handle_l2cap_conn_req(FAR struct bt_driver_s *driver, u16 handle, u16 psm,
				 u16 cid, u8 dir)
{
	FAR struct rtl_profile *pf;
	int idx = psm_to_profile(psm);
	struct rtl_profile_id id = { 0 };
	FAR struct rtl_hci_conn *conn;

	if (idx < 0) {
		wlinfo("RTKCOEX: no need to parse psm %04x", psm);
		return 0;
	}

	conn = rtl_hci_conn_lookup(driver, handle);
	if (!conn)
		return -1;

	if (dir == RTL_TO_REMOTE) {
		id.match_flags = RTL_PROFILE_MATCH_SCID;
		id.scid = cid;
	} else {
		id.match_flags = RTL_PROFILE_MATCH_DCID;
		id.dcid = cid;
	}

	pf = rtl_profile_lookup(&conn->pf_list, &id);
	if (pf) {
		wlwarn("RTKCOEX: profile already exists");
		return -1;
	}

	if (dir == RTL_TO_REMOTE)
		pf = profile_alloc(psm, (u8)idx, 0, cid);
	else
		pf = profile_alloc(psm, (u8)idx, cid, 0);

	if (!pf) {
		wlerr("RTKCOEX: allocate profile failed");
		return -1;
	}

	if (psm == PSM_AVDTP) {
		FAR struct rtl_profile *pinfo = lookup_pf_by_psm(conn, psm);

		if (!pinfo) {
			wlinfo("RTKCOEX: Add a2dp signal channel");
			pf->flags = A2DP_SIGNAL;
		} else {
			wlinfo("RTKCOEX: Add a2dp media channel");
			pf->flags = A2DP_MEDIA;
		}
	}

	pf->conn = (FAR void *)conn;
	list_add_tail(&pf->list, &conn->pf_list);

	return 0;
}

static int handle_l2cap_conn_rsp(FAR struct bt_driver_s *driver,
		u16 handle, u16 dcid,
		u16 scid, u8 dir, u8 result)
{
	FAR struct rtl_profile *pf;
	FAR struct rtl_hci_conn *conn;
	struct rtl_profile_id id = { 0 };

	conn = rtl_hci_conn_lookup(driver, handle);
	if (!conn) {
		wlerr("RTKCOEX: no acl connection\n");
		return -1;
	}

	if (dir == RTL_FROM_REMOTE) {
		id.match_flags = RTL_PROFILE_MATCH_SCID;
		id.scid = scid;
		pf = rtl_profile_lookup(&conn->pf_list, &id);
	} else {
		id.match_flags = RTL_PROFILE_MATCH_DCID;
		id.dcid = scid;
		pf = rtl_profile_lookup(&conn->pf_list, &id);
	}

	if (!pf) {
		wlerr("RTKCOEX: profile not found");
		return -1;
	}

	if (!result) {
		wlinfo("RTKCOEX: l2cap connection success");
		if (dir == RTL_FROM_REMOTE)
			pf->dcid = dcid;
		else
			pf->scid = dcid;

		profile_conn_get(driver, conn, pf->idx);
	}

	return 0;
}

static int handle_l2cap_disconn_req(FAR struct bt_driver_s *driver,
		u16 handle, u16 dcid,
		u16 scid, u8 dir)
{
	FAR struct rtl_profile *pf;
	FAR struct rtl_hci_conn *conn;
	int err = 0;
	struct rtl_profile_id id = {
		.match_flags = RTL_PROFILE_MATCH_SCID |
			       RTL_PROFILE_MATCH_DCID,
		.scid   = scid,
		.dcid   = dcid,
	};

	conn = rtl_hci_conn_lookup(driver, handle);
	if (!conn) {
		wlerr("RTKCOEX: no connection");
		err = -1;
		goto done;
	}

	if (dir == RTL_FROM_REMOTE) {
		id.scid = dcid;
		id.dcid = scid;
		pf = rtl_profile_lookup(&conn->pf_list, &id);
	} else {
		pf = rtl_profile_lookup(&conn->pf_list, &id);
	}

	if (!pf) {
		wlerr("RTKCOEX: no profile");
		err = -1;
		goto done;
	}

	profile_conn_put(driver, conn, pf->idx);
	/* if the profile is a2dp sink */
	if (pf->idx == PROFILE_A2DP_RTK && (conn->pf_bits & (1 << PROFILE_SINK)))
		profile_conn_put(driver, conn, PROFILE_SINK);
	list_del(&pf->list);
	kmm_free(pf);

done:
	wlinfo("RTKCOEX: handle %04x, dcid %04x, scid %04x, dir %x", handle, dcid, scid, dir);

	return err;
}

static void hci_reset_conn(FAR struct bt_driver_s *driver, FAR struct rtl_hci_conn *conn)
{
	FAR struct list_head *pos, *next;
	FAR struct rtl_profile *pf;

	list_for_each_safe(pos, next, &conn->pf_list) {
		pf = list_entry(pos, struct rtl_profile, list);
		if (pf->scid && pf->dcid) {
			/* If both scid and dcid are bigger than zero,
			 * L2cap connection exists.
			 */
			profile_conn_put(driver, conn, pf->idx);
			list_del(&pf->list);
			kmm_free(pf);
		}
	}

	conn->pf_bits = 0;
	memset(conn->pf_refs, 0, PROFILE_MAX_RTK);
}

static void hci_coex_cmd_complete_evt(FAR struct bt_driver_s *driver)
{
	//list_del(&p_curr_coex_cmd->list);
	kmm_free(p_curr_coex_cmd->data);
	kmm_free(p_curr_coex_cmd);
	p_curr_coex_cmd = NULL;

	/*Check pending cmd list*/
	if (work_available(&btpf->cmd_worker) && btpf->pending_cmd_num)
	{
		rtlbt_dbg("RTKCOEX: Send cached coex cmd");
		work_queue(LPWORK, &btpf->cmd_worker, cmd_timer_func, driver, 0);
	}
}

static void hci_conn_complete_evt(FAR struct bt_driver_s *driver, u16 handle,
				  u8 link_type)
{
	FAR struct rtl_hci_conn *conn;

	rtlbt_dbg("RTKCOEX: recv conn complete evt, handle %04x, link_type %d", handle, link_type);
	conn = rtl_hci_conn_lookup(driver, handle);
	if (!conn) {
		conn = kmm_zalloc(sizeof(struct rtl_hci_conn));
		if (conn) {
			conn->handle = handle;
			INIT_LIST_HEAD(&conn->pf_list);
			list_add_tail(&conn->list, &btpf->conn_list);
			conn->pf_bits = 0;
			memset(conn->pf_refs, 0, PROFILE_MAX_RTK);
			/* sco or esco */
			if (link_type == 0 || link_type == 2) {
				conn->type = SYNC_CONN;
				profile_conn_get(driver, conn, PROFILE_SCO);
			} else {
				conn->type = ACL_CONN;
			}
		} else {
			wlerr("RTKCOEX: hci conn allocate fail.");
			return;
		}
	} else {
		/* If the connection has already existed, reset connection
		 * information
		 */
		wlwarn("RTKCOEX: hci conn handle(0x%x) already existed", handle);
		hci_reset_conn(driver, conn);
		/* sco or esco */
		if (link_type == 0 || link_type == 2) {
			conn->type = SYNC_CONN;
			profile_conn_get(driver, conn, PROFILE_SCO);
		} else {
			conn->type = ACL_CONN;
		}
	}
}

static void disconn_profiles(FAR struct bt_driver_s *driver, FAR struct rtl_hci_conn *conn)
{
	FAR struct list_head *pos, *next;
	FAR struct rtl_profile *pf;

	list_for_each_safe(pos, next, &conn->pf_list) {
		pf = list_entry(pos, struct rtl_profile, list);
		if (pf->scid && pf->dcid) {
			/* If both scid and dcid are bigger than zero,
			 * L2cap connection exists.
			 */
			profile_conn_put(driver, conn, pf->idx);
			/* Check if there is a2dp sink */
			if (pf->flags == A2DP_MEDIA &&
			    (conn->pf_bits & (1 << PROFILE_SINK)))
				profile_conn_put(driver, conn, PROFILE_SINK);

			list_del(&pf->list);
			kmm_free(pf);
		}
	}

	btpf_update_to_controller(driver);
}

static int hci_disconn_complete_evt(FAR struct bt_driver_s *driver, u16 handle)
{
	FAR struct rtl_hci_conn *conn;

	rtlbt_dbg("RTKCOEX: recv disconn complete evt, handle %04x", handle);
	conn = rtl_hci_conn_lookup(driver, handle);
	if (!conn) {
		wlerr("RTKCOEX: hci conn handle(0x%x) not found", handle);
		return -1;
	}

	switch (conn->type) {
	case ACL_CONN:
		disconn_profiles(driver, conn);
		break;

	case SYNC_CONN:
		profile_conn_put(driver, conn, PROFILE_SCO);
		break;

	case LE_CONN:
		profile_conn_put(driver, conn, PROFILE_HID);
		break;

	default:
		break;
	}

	list_del(&conn->list);
	kmm_free(conn);

	return 0;
}

static void rtl_le_conn_compl_evt(FAR struct bt_driver_s *driver, u16 handle,
				  u16 interval)
{
	FAR struct rtl_hci_conn *conn;

	rtlbt_dbg("RTKCOEX: recv le (enhanced) conn complete evt, handle %04x, interval %04x", handle, interval);
	conn = rtl_hci_conn_lookup(driver, handle);
	if (!conn) {
		conn = kmm_zalloc(sizeof(struct rtl_hci_conn));
		if (conn) {
			conn->handle = handle;
			INIT_LIST_HEAD(&conn->pf_list);
			list_add_tail(&conn->list, &btpf->conn_list);
			conn->pf_bits = 0;
			memset(conn->pf_refs, 0, PROFILE_MAX_RTK);
			conn->type = LE_CONN;
			/* We consider le is the same as hid */
			profile_conn_get(driver, conn, PROFILE_HID);
			hid_state_update(driver, handle, interval);
		} else {
			wlerr("RTKCOEX: hci conn allocate fail.");
		}
	} else {
		wlwarn("RTKCOEX: hci conn handle(%x) already existed.",
			   handle);
		hci_reset_conn(driver, conn);
		conn->type = LE_CONN;
		profile_conn_get(driver, conn, PROFILE_HID);
		hid_state_update(driver, handle, interval);
	}
}

static const char sample_freqs[4][8] = {
	"16", "32", "44.1", "48"
};

static const u8 sbc_blocks[4] = { 4, 8, 12, 16 };

static const char chan_modes[4][16] = {
	"MONO", "DUAL_CHANNEL", "STEREO", "JOINT_STEREO"
};

static const char alloc_methods[2][12] = {
	"LOUDNESS", "SNR"
};

static const u8 subbands[2] = { 4, 8 };

void print_sbc_header(FAR struct sbc_frame_hdr *hdr)
{
	wlinfo("RTKCOEX: syncword: %02x", hdr->syncword);
	wlinfo("RTKCOEX: freq %skHz", sample_freqs[hdr->sampling_frequency]);
	wlinfo("RTKCOEX: blocks %u", sbc_blocks[hdr->blocks]);
	wlinfo("RTKCOEX: channel mode %s", chan_modes[hdr->channel_mode]);
	wlinfo("RTKCOEX: allocation method %s",
		   alloc_methods[hdr->allocation_method]);
	wlinfo("RTKCOEX: subbands %u", subbands[hdr->subbands]);
}

static void parse_media_header(FAR struct bt_driver_s *driver, FAR u8 *data)
{
	FAR struct sbc_frame_hdr *sbc_header;
	FAR struct rtp_header *rtph;
	u8 bitpool;

	rtph = (FAR struct rtp_header *)data;

	wlinfo("RTKCOEX: rtp: v %u, cc %u, pt %u", rtph->v, rtph->cc, rtph->pt);
	/* move forward */
	data += sizeof(struct rtp_header) + rtph->cc * 4 + 1;

	/* point to the sbc frame header */
	sbc_header = (FAR struct sbc_frame_hdr *)data;
	bitpool = sbc_header->bitpool;

	print_sbc_header(sbc_header);

	rtlbt_dbg("RTKCOEX: bitpool %u", bitpool);

	rtk_vendor_cmd_to_fw(driver, 0xfc51, 1, &bitpool);
}

static void packet_increment(FAR struct bt_driver_s *driver, u16 handle,
		u16 ch_id, FAR u8 *payload, u16 length, u8 dir)
{
	FAR struct rtl_profile *pf;
	FAR struct rtl_hci_conn *conn;
	struct rtl_profile_id id = { 0 };

	conn = rtl_hci_conn_lookup(driver, handle);
	if (!conn)
		goto done;

	if (conn->type != ACL_CONN)
		return;

	memset(&id, 0, sizeof(id));
	if (dir == RTL_FROM_REMOTE) {
		id.match_flags = RTL_PROFILE_MATCH_SCID;
		id.scid = ch_id;
	} else {
		id.match_flags = RTL_PROFILE_MATCH_DCID;
		id.dcid = ch_id;
	}
	pf = rtl_profile_lookup(&conn->pf_list, &id);
	if (!pf)
		goto done;

	if (pf->idx == PROFILE_A2DP_RTK && pf->flags == A2DP_MEDIA) {
		/* avdtp media data */
		if (!(btpf->pf_state & (1 << PROFILE_A2DP_RTK))) {
			update_profile_state(driver, PROFILE_A2DP_RTK, 1);
			if (dir == RTL_FROM_REMOTE) {
				if (!(conn->pf_bits & (1 << PROFILE_SINK))) {
					btpf->pf_bits |= (1 << PROFILE_SINK);
					conn->pf_bits |= (1 << PROFILE_SINK);
					profile_conn_get(driver, conn, PROFILE_SINK);
				}
				update_profile_state(driver, PROFILE_SINK, 1);
			}

			/* We assume it is SBC if the packet length is bigger
			 * than 100 bytes
			 */
			wlinfo("RTKCOEX: length %u", length);
			if (length > 100)
				parse_media_header(driver, payload);
		}
		btpf->icount.a2dp++;
	}

	if (pf->idx == PROFILE_PAN)
		btpf->icount.pan++;

done:
	return;
}

void hci_process_evt(FAR struct bt_driver_s *driver, FAR u8 *p, u16 len)
{
	u8 evt;
	//u8 plen;
	u8 status;
	u8 link_type;
	u8 subevt;
	u16 handle;
	u16 interval;
	u16 opcode;
	int ret;

	ret = nxmutex_lock(&btpf->coexlock);
	if (ret < 0)
	{
		wlerr("RTKCOEX: cannot get coexlock");
		return;
	}
	evt = p[0];
	//plen = p[1];

	p += 2;
	switch (evt) {
	case HCI_EV_CMD_COMPLETE:
		p++;
		STREAM_TO_UINT16(opcode, p);
		status = p[0];
		if (status)
			goto done;
		if (opcode == 0xFC51 || opcode == 0xFC19)
		{
			rtlbt_dbg("RTKCOEX: recv cc evt for coex cmd %04x", opcode);
			hci_coex_cmd_complete_evt(driver);
		}
		break;
	case HCI_EV_CONN_COMPLETE:
	case HCI_EV_SYNC_CONN_COMPLETE:
		status = p[0];
		if (status)
			goto done;
		handle = ((u16)p[2]) << 8 | p[1];
		link_type = p[9];
		hci_conn_complete_evt(driver, handle, link_type);
		break;
	case HCI_EV_DISCONN_COMPLETE:
		/* TODO: Is it needed to check status ? */
		handle = ((u16)p[2]) << 8 | p[1];
		hci_disconn_complete_evt(driver, handle);
		break;
	case HCI_EV_MODE_CHANGE:
		status = p[0];
		if (status)
			goto done;
		handle = ((u16)p[2]) << 8 | p[1];
		interval = ((u16)p[5] << 8 | p[4]);
		hid_state_update(driver, handle, interval);
		break;
	case HCI_EV_LE_META:
		subevt = p[0];
		p++;
		status = p[0];
		if (status)
			goto done;
		switch (subevt) {
		case HCI_EV_LE_CONN_COMPLETE:
			handle = ((u16)p[2] << 8 | p[1]);
			interval = ((u16)p[12] << 8 | p[11]);
			rtl_le_conn_compl_evt(driver, handle, interval);
			break;
		case HCI_EV_LE_ENHANCED_CONN_COMPLETE:
			handle = ((u16)p[2] << 8 | p[1]);
			interval = ((u16)p[24] << 8 | p[23]);
			rtl_le_conn_compl_evt(driver, handle, interval);
			break;
		case HCI_EV_LE_CONN_UPDATE_COMPLETE:
			handle = ((u16)p[2] << 8 | p[1]);
			interval = ((u16)p[4] << 8 | p[3]);
			hid_state_update(driver, handle, interval);
			break;
		}
		break;
	default:
		break;
	}
done:
	nxmutex_unlock(&btpf->coexlock);
	return;
}

void l2_process_frame_out(FAR struct bt_driver_s *driver, FAR u8 *data, u16 len)
{
	u16 handle;
	u16 flags;
	u16 chan_id;
	u16 psm, scid, dcid, res;
	u16 l2_len;
	u8 code;
	u8 out = 1;
	int ret;

	ret = nxmutex_lock(&btpf->coexlock);
	if (ret < 0)
	{
		wlerr("RTKCOEX: cannot get coexlock");
		return;
	}

	handle = ((u16)data[1] << 8 | data[0]);
	flags = (handle >> 12);
	handle = (handle & 0x0fff);
	data += 4;
	len -= 4;

	/* continuing fragment */
	if (flags == 0x01)
		goto done;

	l2_len = ((u16)data[1] << 8 | data[0]);
	chan_id = ((u16)data[3] << 8 | data[2]);
	data += 4;
	len -= 4;

	if (chan_id != 0x0001) {
		if (btpf->pf_bits & (1 << PROFILE_A2DP_RTK) ||
		    btpf->pf_bits & (1 << PROFILE_PAN))
			packet_increment(driver, handle, chan_id, data, l2_len,
					 out);
		goto done;
	}

	if (len < 3)
		goto done;

	code = data[0];
	data += 4;
	switch (code) {
	case L2CAP_CONN_REQ:
		psm = ((u16)data[1] << 8 | data[0]);
		scid = ((u16)data[3] << 8 | data[2]);
		wlinfo("RTKCOEX: l2cap conn req: psm %04x, scid %04x, out %u",
			   psm, scid, out);
		handle_l2cap_conn_req(driver, handle, psm, scid, out);
		break;

	case L2CAP_CONN_RSP:
		dcid = ((u16)data[1] << 8 | data[0]);
		scid = ((u16)data[3] << 8 | data[2]);
		res = ((u16)data[5] << 8 | data[4]);
		wlinfo("RTKCOEX: l2cap conn rsp: dcid %04x, scid %04x, res %04x, out %u",
			   dcid, scid, res, out);
		handle_l2cap_conn_rsp(driver, handle, dcid, scid, out, res);
		break;

	case L2CAP_DISCONN_REQ:
		dcid = ((u16)data[1] << 8 | data[0]);
		scid = ((u16)data[3] << 8 | data[2]);
		wlinfo("RTKCOEX: l2cap disconn req: dcid %04x, scid %04x, out %u",
			   dcid, scid, out);
		handle_l2cap_disconn_req(driver, handle, dcid, scid, out);
		break;

	case L2CAP_DISCONN_RSP: /* TODO */
	default:
		break;
	}
done:
	nxmutex_unlock(&btpf->coexlock);
	return;
}

void l2_process_frame_in(FAR struct bt_driver_s *driver, FAR u8 *data, u16 len)
{
	u16 handle;
	u16 flags;
	u16 chan_id;
	u16 psm, scid, dcid, res;
	u16 l2_len;
	u8 code;
	u8 out = 0;
	int ret;

	ret = nxmutex_lock(&btpf->coexlock);
	if (ret < 0)
	{
		wlerr("RTKCOEX: cannot get coexlock");
		return;
	}

	handle = ((u16)data[1] << 8 | data[0]);
	flags = (handle >> 12);
	handle = (handle & 0x0fff);
	data += 4;
	len -= 4;

	/* continuing fragment */
	if (flags == 0x01)
		goto done;

	l2_len = ((u16)data[1] << 8 | data[0]);
	chan_id = ((u16)data[3] << 8 | data[2]);
	data += 4;
	len -= 4;

	if (chan_id != 0x0001) {
		if (btpf->pf_bits & (1 << PROFILE_A2DP_RTK) ||
		    btpf->pf_bits & (1 << PROFILE_PAN))
			packet_increment(driver, handle, chan_id, data, l2_len,
					 out);
		goto done;
	}

	if (len < 3)
		goto done;

	code = data[0];
	data += 4;
	switch (code) {
	case L2CAP_CONN_REQ:
		psm = ((u16)data[1] << 8 | data[0]);
		scid = ((u16)data[3] << 8 | data[2]);
		wlinfo("RTKCOEX: l2cap conn req: psm %04x, scid %04x, out %u",
			   psm, scid, out);
		handle_l2cap_conn_req(driver, handle, psm, scid, out);
		break;

	case L2CAP_CONN_RSP:
		dcid = ((u16)data[1] << 8 | data[0]);
		scid = ((u16)data[3] << 8 | data[2]);
		res = ((u16)data[5] << 8 | data[4]);
		wlinfo("RTKCOEX: l2cap conn rsp: dcid %04x, scid %04x, res %04x, out %u",
			   dcid, scid, res, out);
		handle_l2cap_conn_rsp(driver, handle, dcid, scid, out, res);
		break;

	case L2CAP_DISCONN_REQ:
		dcid = ((u16)data[1] << 8 | data[0]);
		scid = ((u16)data[3] << 8 | data[2]);
		wlinfo("RTKCOEX: l2cap disconn req: dcid %04x, scid %04x, out %u",
			   dcid, scid, out);
		handle_l2cap_disconn_req(driver, handle, dcid, scid, out);
		break;

	case L2CAP_DISCONN_RSP: /* TODO */
	default:
		break;
	}
done:
	nxmutex_unlock(&btpf->coexlock);
	return;
}

static int rtk_coex_open(FAR struct bt_driver_s *dev)
{
	FAR struct rtk_coex_s *priv;
	FAR struct bt_driver_s *drv;

	DEBUGASSERT(dev != NULL);

	priv = (FAR struct rtk_coex_s *)dev;
	drv = priv->drv;

	rtk_coex_init();

	return drv->open(drv);
}

static int rtk_coex_send(FAR struct bt_driver_s *dev,
                        enum bt_buf_type_e type,
                        FAR void *data, size_t len)
{
	FAR struct rtk_coex_s *priv;
	FAR struct bt_driver_s *drv;

	DEBUGASSERT(dev != NULL);

	priv = (FAR struct rtk_coex_s *)dev;
	drv = priv->drv;

	if (type == BT_ACL_OUT)
	{
		l2_process_frame_out(drv, data, len);
	}

	return drv->send(drv, type, data, len);
}

static int rtk_coex_receive(FAR struct bt_driver_s *drv,
                             enum bt_buf_type_e type,
                             FAR void *data, size_t len)
{
	FAR struct rtk_coex_s *priv;

	priv = drv->priv;

	if (type == BT_ACL_IN)
	{
		l2_process_frame_in(drv, data, len);
	}
	else if (type == BT_EVT)
	{
		hci_process_evt(drv, data, len);
	}

	return bt_netdev_receive(&priv->dev, type, data, len);
}

static int rtk_coex_ioctl(FAR struct bt_driver_s *dev, int cmd,
                         unsigned long arg)
{
	FAR struct rtk_coex_s *priv;
	FAR struct bt_driver_s *drv;

	DEBUGASSERT(dev != NULL);

	priv = (FAR struct rtk_coex_s *)dev;
	drv = priv->drv;

	return drv->ioctl(drv, cmd, arg);
}

static void rtk_coex_close(FAR struct bt_driver_s *dev)
{
	FAR struct rtk_coex_s *priv;
	FAR struct bt_driver_s *drv;

	DEBUGASSERT(dev != NULL);

	priv = (FAR struct rtk_coex_s *)dev;
	drv = priv->drv;

	rtk_coex_deinit();

	drv->close(drv);
}

FAR struct bt_driver_s *rtk_coex_register(FAR struct bt_driver_s *hcidrv)
{
	wlerr("RTKCOEX: +++++RTK coex register++++++++");

	DEBUGASSERT(hcidrv != NULL);

	FAR struct rtk_coex_s *rtk_coex = NULL;
	rtk_coex = kmm_zalloc(sizeof(struct rtk_coex_s));
	if (!rtk_coex)
	{
		wlerr("RTKCOEX: alloc error");
		return NULL;
	}

	rtk_coex->drv = hcidrv;
	hcidrv->receive = rtk_coex_receive;
	hcidrv->priv = rtk_coex;

	rtk_coex->dev.open = rtk_coex_open;
	rtk_coex->dev.send = rtk_coex_send;
	rtk_coex->dev.ioctl = rtk_coex_ioctl;
	rtk_coex->dev.close = rtk_coex_close;

	return &rtk_coex->dev;
}

void rtk_coex_init(void)
{
	wlerr("RTKCOEX: +++++RTK coex init++++++++");

	memset(btpf, 0, sizeof(*btpf));
	nxmutex_init(&btpf->coexlock);
	INIT_LIST_HEAD(&btpf->conn_list);
	INIT_LIST_HEAD(&btpf->pending_cmd_list);
}

void rtk_coex_deinit(void)
{
	wlerr("RTKCOEX: +++++RTK coex deinit++++++++");

	work_cancel(LPWORK, &btpf->a2dp_worker);
	work_cancel(LPWORK, &btpf->pan_worker);
	work_cancel(LPWORK, &btpf->cmd_worker);
	rtl_pending_cmd_list_purge();
	rtl_hci_conn_list_purge();
	nxmutex_destroy(&btpf->coexlock);
}
