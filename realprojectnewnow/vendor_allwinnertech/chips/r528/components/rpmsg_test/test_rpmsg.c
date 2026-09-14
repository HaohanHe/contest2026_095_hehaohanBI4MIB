/*
 * Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
 *
 * Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
 * the the people's Republic of China and other countries.
 * All Allwinner Technology Co.,Ltd. trademarks are used with permission.
 *
 * DISCLAIMER
 * THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
 * IF YOU NEED TO INTEGRATE THIRD PARTY'S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR
 * MPEGLA, ETC.) IN ALLWINNERS'SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE
 * TO OBTAIN ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES. ALLWINNER SHALL
 * HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
 * COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
 * YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY'S TECHNOLOGY.
 *
 *
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <aw_list.h>
#include <hal_atomic.h>
#include <hal_mutex.h>
#include <hal_mem.h>
#include <hal_sem.h>
#include <hal_queue.h>
#include <hal_thread.h>
#include <hal_time.h>
#include <nuttx/rpmsg/rpmsg.h>
#include <nuttx/rptun/rptun.h>
#include <openamp/rpmsg.h>
#include <debug.h>

#include "md5.h"

#define app_print					_info

#define RPMSG_DRV_NAME				"sunxi,rpmsg_test"

#define RPMSG_TEST_RX_QUEUE			(CONFIG_AW_RPMSG_TEST_QUEUE_LENGTH)

#define log(fmt, ...)	\
		do { \
			if (do_verbose) \
				app_print(fmt, ##__VA_ARGS__); \
		} while(0)


static LIST_HEAD(g_rpmsg_epts);

static uint32_t tx_delay_ms = 500;
static uint32_t tx_len = 32;
static uint32_t do_verbose = 0;

struct ept_test_entry {
	struct rpmsg_endpoint ept;
	struct list_head list;

	pthread_t rx_task;
	pthread_t tx_task;

	hal_sem_t rx_sem;
	hal_spinlock_t lock;
	uint16_t stop;
	/* rx queue */
	uint16_t head, tail, cnt;
	uint8_t rx_queue[RPMSG_TEST_RX_QUEUE][RPMSG_BUFFER_SIZE];
	uint16_t rx_len[RPMSG_TEST_RX_QUEUE];
};

static int rpmsg_ept_callback(struct rpmsg_endpoint *ept, void *data,
		size_t len, uint32_t src, void *priv)
{
	unsigned long flags;
	struct ept_test_entry *eptdev = ept->priv;

	if (eptdev->cnt == RPMSG_TEST_RX_QUEUE) {
		app_print("rpmsg 0x%lx: rx queue is full\r\n", ept->addr);
		return 0;
	}

	flags = hal_spin_lock_irqsave(&eptdev->lock);
	memcpy(eptdev->rx_queue[eptdev->tail], data, len);
	eptdev->rx_len[eptdev->tail] = len;
	eptdev->tail++;
	eptdev->cnt++;
	eptdev->tail %= RPMSG_TEST_RX_QUEUE;
	hal_spin_unlock_irqrestore(&eptdev->lock, flags);

	log("rpmsg 0x%lx: rx %d Bytes\r\n", ept->addr, len);
	hal_sem_post(eptdev->rx_sem);

	return 0;
}

static void mkdata(uint8_t *buffer, int size, int verbose, const char *name)
{
	int i;
	int data_len = size - 16;
	uint32_t *pdate = (uint32_t *)buffer;

	srand((int)time(NULL));

	/* generate random data */
	for (i = 0; i < (data_len / 4); i++)
		pdate[i] = (uint32_t)rand();

	md5(buffer, data_len, &buffer[data_len]);

	if (data_len > 16)
		data_len = 16;

	if (verbose) {
		app_print("[%s]data:", name);
		for (i = 0; i < data_len; i++)
			app_print("%02x", buffer[i]);
		app_print("... [md5:");
		for (i = 0; i < 16; i++)
			app_print("%02x", buffer[size - 16 + i]);
		app_print("]\n\n");
	}
}

static int checkdata(uint8_t *buffer, int size, int verbose, const char *name)
{
	int i;
	int data_len = size - 16;
	uint8_t digest[16];

	md5(buffer, data_len, digest);

	if (data_len > 16)
		data_len = 16;

	if (verbose) {
		app_print("[%s] data:", name);
		for (i = 0; i < data_len; i++)
			app_print("%02x", buffer[i]);
		app_print("\r\n[%s] check:", name);
	}

	for (i = 0; i < 16; i++) {
		if (verbose)
			app_print("%02x", buffer[size - 16 + i]);
		if (buffer[size - 16 + i] != digest[i])
			break;
	}

	if (i != 16) {
		app_print("[%s] failed [target:", name);
		for (i = 0; i < 16; i++)
			app_print("%02x", buffer[size - 16 + i]);
		app_print(" <-> cur:");
		for (i = 0; i < 16; i++)
			app_print("%02x", digest[i]);
		app_print("\n\n");
		return 0;
	} else {
		if (verbose)
			app_print("\r\n[%s] success\n\n", name);
		return 1;
	}
}

static pthread_addr_t rpmsg_rx_thread(pthread_addr_t pram)
{
	int ret;
	unsigned long flags;
	struct ept_test_entry *eptdev = (struct ept_test_entry *)pram;
	int rx_len;
	uint8_t tmpbuf[RPMSG_BUFFER_SIZE];
	char name[64];

	snprintf(name, sizeof(name), "%s.%lx", eptdev->ept.name,
					eptdev->ept.addr);
	app_print("rpmsg 0x%lx rx thread start...\r\n", eptdev->ept.addr);

	while (1) {
		ret = hal_sem_timedwait(eptdev->rx_sem, MS_TO_OSTICK(100));

		if (eptdev->stop)
			break;
		if (ret || eptdev->cnt == 0)
			continue;

		flags = hal_spin_lock_irqsave(&eptdev->lock);
		rx_len = eptdev->rx_len[eptdev->head];
		memcpy(tmpbuf, eptdev->rx_queue[eptdev->head], rx_len);
		eptdev->cnt--;
		eptdev->head++;
		eptdev->head %= RPMSG_TEST_RX_QUEUE;
		hal_spin_unlock_irqrestore(&eptdev->lock, flags);

		checkdata(tmpbuf, rx_len, do_verbose, name);
	}

	app_print("rpmsg 0x%lx rx thread exit...\r\n", eptdev->ept.addr);

	return NULL;
}

static pthread_addr_t rpmsg_tx_thread(pthread_addr_t pram)
{
	int ret;
	struct ept_test_entry *eptdev = (struct ept_test_entry *)pram;
	uint8_t tmpbuf[RPMSG_BUFFER_SIZE];
	char name[64];

	snprintf(name, sizeof(name), "%s.%lx", eptdev->ept.name,
					eptdev->ept.addr);

	app_print("rpmsg 0x%lx tx thread start...\r\n", eptdev->ept.addr);

	while (1) {
		mkdata(tmpbuf, tx_len, do_verbose, name);
try_send:
		ret = rpmsg_trysend(&eptdev->ept, tmpbuf, tx_len);

		if (eptdev->stop)
			break;

		if (ret == RPMSG_ERR_NO_BUFF) {
			metal_sleep_usec(RPMSG_TICKS_PER_INTERVAL);
			goto try_send;
		}

		if (ret < 0)
			app_print("rpmsg 0x%lx: Failed to send data\n", eptdev->ept.addr);

		if (tx_delay_ms)
			hal_msleep(tx_delay_ms);
	}

	app_print("rpmsg 0x%lx tx thread exit...\r\n", eptdev->ept.addr);

	return NULL;
}

static bool rpmsg_test_ns_match(struct rpmsg_device *rdev,
                             void *priv_, const char *name,
                             uint32_t dest)
{
	return !strncmp(name, RPMSG_DRV_NAME, strlen(RPMSG_DRV_NAME));
}

static void rpmsg_test_unbind_cb(struct rpmsg_endpoint *ept)
{
	struct ept_test_entry *eptdev = ept->priv;

	app_print("rpmsg: %s.%lx.%lx unbinding\r\n", ept->name, ept->addr,
					ept->dest_addr);
	eptdev->stop = 1;
	list_del(&eptdev->list);
	rpmsg_destroy_ept(&eptdev->ept);
	hal_sem_post(eptdev->rx_sem);
	pthread_join(eptdev->tx_task, NULL);
	pthread_join(eptdev->rx_task, NULL);
	hal_sem_delete(eptdev->rx_sem);
	hal_spin_lock_deinit(&eptdev->lock);
	free(eptdev);
}

static void rpmsg_test_ns_bind(struct rpmsg_device *rdev,
								 void *priv_, const char *name,
								 uint32_t dest)
{
	struct ept_test_entry *eptdev;
	int ret;
	char task_name[64];
	pthread_attr_t tattr;
	struct sched_param sparam;

	app_print("%s: binding\r\n", name);

	eptdev = malloc(sizeof(*eptdev));
	if (!eptdev) {
		app_print("failed to alloc client entry\r\n");
		return;
	}

	memset(eptdev, 0, sizeof(*eptdev));
	eptdev->ept.priv = eptdev;

	ret = rpmsg_create_ept(&eptdev->ept, rdev, name,
							 RPMSG_ADDR_ANY, dest,
							 rpmsg_ept_callback, rpmsg_test_unbind_cb);
	if (ret) {
		app_print("failed to rpmsg_create_ept\r\n");
		goto free_eptdev;
	}

	eptdev->rx_sem = hal_sem_create(0);
	if (!eptdev->rx_sem) {
		app_print("Failed to create %s rx_sem\n", name);
		goto free_ept;
	}

	hal_spin_lock_init(&eptdev->lock);

	pthread_attr_init(&tattr);
	sparam.sched_priority = CONFIG_AW_RPMSG_TEST_PRIORITY;
	pthread_attr_setschedparam(&tattr, &sparam);
	pthread_attr_setstacksize(&tattr, CONFIG_AW_RPMSG_TEST_STACKSIZE);

	ret = pthread_create(&eptdev->rx_task, &tattr, rpmsg_rx_thread,
						 (pthread_addr_t)eptdev);
	if (ret != OK) {
		app_print("Failed to create %s rx_task\r\n", name);
		goto free_sem;
	}
	snprintf(task_name, sizeof(task_name), "rpmsg-rx");
	pthread_setname_np(eptdev->rx_task, task_name);

	ret = pthread_create(&eptdev->tx_task, &tattr, rpmsg_tx_thread,
						 (pthread_addr_t)eptdev);
	if (ret != OK) {
		app_print("Failed to create %s tx_task\r\n", name);
		goto free_rx_task;
	}
	snprintf(task_name, sizeof(task_name), "rpmsg-tx");
	pthread_setname_np(eptdev->tx_task, task_name);

	list_add(&eptdev->list, &g_rpmsg_epts);
	return;
free_rx_task:
	eptdev->stop = 1;
	pthread_join(eptdev->rx_task, NULL);
free_sem:
	hal_sem_delete(eptdev->rx_sem);
free_ept:
	rpmsg_destroy_ept(&eptdev->ept);
free_eptdev:
	free(eptdev);
}

void rpmsg_test_destroy(FAR struct rpmsg_device *rdev,
							 FAR void *priv)
{
	struct ept_test_entry *pos, *tmp;

	list_for_each_entry_safe(pos, tmp, &g_rpmsg_epts, list)
		rpmsg_test_unbind_cb(&pos->ept);
}

static void print_help_msg(void)
{
	app_print("\n");
	app_print("USAGE:\n");
	app_print("  rpmsg_test [OPTIONS]\n");
	app_print("OPTIONS:\n");
	app_print("  -h          : print help message\n");
	app_print("  -c          : register driver\n");
	app_print("  -d          : unregister driver\n");
	app_print("  -v          : verbosely print check result\n");
	app_print("  -l          : list active endpoints\n");
	app_print("  -t tx_delay : specify tx_delay_ms (Global variable, default: 500)\n");
	app_print("  -L tx_len   : specify tx length (Global variable, default: 32 bytes)\n");
	app_print("\n");
	app_print("e.g.\n");
	app_print("      rpmsg_test -L LENGTH -t tx_interval -c\n");
	app_print("      rpmsg_test -d\n");
	app_print("\n");
}

int main(int argc, char *argv[])
{
	int ret = 0;
	unsigned long len = 32;
	int delay_ms = 500;
	const char *name = RPMSG_DRV_NAME;
	int c;
	int do_create = 0;
	int do_delete = 0;
	int do_list = 0;

	if (argc <= 1) {
		print_help_msg();
		ret = -1;
		goto out;
	}

	do_verbose = 0;

	while ((c = getopt(argc, argv, "hcdvlt:L:")) != -1) {
		switch(c) {
		case 'h':
			print_help_msg();
			ret = 0;
			goto out;
		case 'c':
			do_create = 1;
			break;
		case 'd':
			do_delete = 1;
			break;
		case 'v':
			do_verbose = 1;
			break;
		case 'l':
			do_list = 1;
			break;
		case 't':
			delay_ms = atoi(optarg);
			if (delay_ms == 0) {
				ret = -1;
				app_print("Invalid cnt arg.\r\n");
				goto out;
			}
			tx_delay_ms = delay_ms;
			break;
		case 'L':
			len = strtol(optarg, NULL, 0);
			if (len == -ERANGE || len > RPMSG_BUFFER_SIZE) {
				app_print("Invalid length arg.\r\n");
				ret = -1;
				goto out;
			}
			tx_len = len;
			break;
		default:
			app_print("Invalid option: -%c\n", c);
			print_help_msg();
			ret = -1;
			goto out;
		}
	}

	if (do_create) {
		app_print("bind %s\r\n", name);
		rpmsg_register_callback(NULL, NULL, rpmsg_test_destroy,
						rpmsg_test_ns_match, rpmsg_test_ns_bind);
	}

	if (do_delete) {
		struct ept_test_entry *pos, *tmp;

		app_print("unbind %s\r\n", name);
		list_for_each_entry_safe(pos, tmp, &g_rpmsg_epts, list)
			rpmsg_test_unbind_cb(&pos->ept);

		rpmsg_unregister_callback(NULL, NULL, rpmsg_test_destroy,
						rpmsg_test_ns_match, rpmsg_test_ns_bind);
	}

	if (do_list) {
		struct ept_test_entry *pos, *tmp;

		app_print("src\tdst\t\r\n");
		list_for_each_entry_safe(pos, tmp, &g_rpmsg_epts, list)
			app_print("0x%lx\t0x%lx\r\n", pos->ept.addr, pos->ept.dest_addr);
	}

	return 0;
out:
	return ret;
}
