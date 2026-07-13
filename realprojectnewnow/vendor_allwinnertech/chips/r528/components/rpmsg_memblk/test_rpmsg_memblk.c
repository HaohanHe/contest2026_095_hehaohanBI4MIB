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
#include <hal_queue.h>
#include <hal_thread.h>
#include <hal_time.h>
#include <nuttx/rpmsg/rpmsg.h>
#include <nuttx/rptun/rptun.h>
#include <openamp/rpmsg.h>
#include <debug.h>

#include <memblk.h>
#include <rpmsg_memblk.h>
#include "md5.h"

#define app_print					_info

#define MEMPOOL_SIZE				(16 * 8 * 1024)
static uint8_t mempool[MEMPOOL_SIZE] __attribute__((aligned(64)));
static struct memblk_pool pool;

static LIST_HEAD(g_rpmsg_epts);

static uint32_t tx_delay_ms = 500;
static uint32_t tx_len = 8192;

struct ept_test_entry {
	struct rpmsg_memblk_dev *dev;
	struct list_head list;

	pthread_t rx_task;
	pthread_t tx_task;

	uint16_t stop;
	uint16_t do_verbose;
};

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
		app_print("]\n");
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
	struct ept_test_entry *eptdev = (struct ept_test_entry *)pram;
	const char *name;
	struct memblk_entry memblock;

	name = rpmsg_memblk_get_name(eptdev->dev);
	app_print("%s rx thread start...\r\n", name);

	while (1) {
		/* get readable buffer */
		ret = rpmsg_memblk_pull_memblk(eptdev->dev, &memblock, 100);

		if (eptdev->stop)
			break;
		if (ret)       /* maybe timeout? */
			continue;

		checkdata(rpmsg_memblk_get_addr(&memblock),
						rpmsg_memblk_get_len(&memblock), eptdev->do_verbose, name);

		/* return buffer */
		rpmsg_memblk_return_memblk(eptdev->dev, &memblock);
	}

	app_print("%s rx thread exit...\r\n", name);

	return NULL;
}

static pthread_addr_t rpmsg_tx_thread(pthread_addr_t pram)
{
	int ret;
	struct ept_test_entry *eptdev = (struct ept_test_entry *)pram;
	const char *name;
	struct memblk_entry memblock;

	name = rpmsg_memblk_get_name(eptdev->dev);
	app_print("%s tx thread start...\r\n", name);

	while (1) {
		ret = rpmsg_memblk_get_memblk(eptdev->dev, &memblock, 100);

		if (eptdev->stop)
			break;
		if (ret)
			continue;

		mkdata(rpmsg_memblk_get_addr(&memblock), rpmsg_memblk_get_len(&memblock),
						eptdev->do_verbose, name);
		ret = rpmsg_memblk_push_memblk(eptdev->dev, &memblock,
						rpmsg_memblk_get_len(&memblock));
		if (ret)
			app_print("rpmsg_memblk_push_memblk Failed, ret=%d\r\n", ret);

		if (tx_delay_ms)
			hal_msleep(tx_delay_ms);
	}

	app_print("%s tx thread exit...\r\n", name);

	return NULL;
}

static void rpmsg_memblk_delete_dev(struct ept_test_entry *eptdev)
{
	list_del(&eptdev->list);
	rpmsg_memblk_dev_delete(eptdev->dev);
	free(eptdev);
}

static void rpmsg_memblk_dev_unbind(struct rpmsg_memblk_dev *dev, void *priv)
{
	struct ept_test_entry *eptdev = priv;

	app_print("rpmsg: %s unbind\r\n", rpmsg_memblk_get_name(eptdev->dev));
	eptdev->stop = 1;
}

static struct ept_test_entry *rpmsg_memblk_create_dev(const char *name)
{
	struct ept_test_entry *eptdev;
	int ret;
	char task_name[64];
	pthread_attr_t tattr;
	struct sched_param sparam;

	eptdev = malloc(sizeof(*eptdev));
	if (!eptdev) {
		app_print("failed to alloc client entry\r\n");
		return NULL;
	}

	memset(eptdev, 0, sizeof(*eptdev));

	eptdev->dev = rpmsg_memblk_dev_create(name, rpmsg_memblk_dev_unbind, eptdev);
	if (!eptdev->dev) {
		app_print("rpmsg_memblk_dev_create failedr\n");
		goto free_eptdev;
	}
	rpmsg_memblk_set_pool(eptdev->dev, &pool);

	pthread_attr_init(&tattr);
	sparam.sched_priority = CONFIG_AW_RPMSG_MEMBLK_THTEAD_PRIORITY;
	pthread_attr_setschedparam(&tattr, &sparam);
	pthread_attr_setstacksize(&tattr, CONFIG_AW_RPMSG_MEMBLK_THTEAD_STACK_SIZE);

	ret = pthread_create(&eptdev->rx_task, &tattr, rpmsg_rx_thread,
						 (pthread_addr_t)eptdev);
	if (ret != OK) {
		app_print("Failed to create %s rx_task\r\n", name);
		goto free_memblk_dev;
	}
	snprintf(task_name, sizeof(task_name), "rpmemblk-rx");
	pthread_setname_np(eptdev->rx_task, task_name);

	ret = pthread_create(&eptdev->tx_task, &tattr, rpmsg_tx_thread,
						 (pthread_addr_t)eptdev);
	if (ret != OK) {
		app_print("Failed to create %s tx_task\r\n", name);
		goto free_rx_task;
	}
	snprintf(task_name, sizeof(task_name), "rpmemblk-tx");
	pthread_setname_np(eptdev->tx_task, task_name);

	list_add(&eptdev->list, &g_rpmsg_epts);

	return eptdev;

free_rx_task:
	eptdev->stop = 1;
	pthread_join(eptdev->rx_task, NULL);
free_memblk_dev:
	rpmsg_memblk_dev_delete(eptdev->dev);
free_eptdev:
	free(eptdev);

	return NULL;
}

static void print_help_msg(void)
{
	app_print("\n");
	app_print("USAGE:\n");
	app_print("  rpmsg_test [OPTIONS]\n");
	app_print("OPTIONS:\n");
	app_print("  -h          : print help message\n");
	app_print("  -c name     : register driver\n");
	app_print("  -d name     : unregister driver\n");
	app_print("  -v          : verbosely print check result\n");
	app_print("  -l          : list active endpoints\n");
	app_print("  -t tx_delay : specify tx_delay_ms (Global variable, default: 500)\n");
	app_print("  -b batchsize: specify batch size (Global variable, default: 8K)\n");
	app_print("\n");
	app_print("e.g.\n");
	app_print("      rpmsg_test -L LENGTH -t tx_interval -c\n");
	app_print("      rpmsg_test -d\n");
	app_print("\n");
}

static int is_init = 0;

int main(int argc, char *argv[])
{
	int ret = 0;
	unsigned long len = 32;
	int delay_ms = 500;
	const char *name = NULL;
	int c;
	int do_create = 0;
	int do_delete = 0;
	int do_list = 0;
	int do_verbose = 0;
	struct ept_test_entry *dev;

	if (argc <= 1) {
		print_help_msg();
		ret = -1;
		goto out;
	}


	while ((c = getopt(argc, argv, "hc:d:vlt:b:")) != -1) {
		switch(c) {
		case 'h':
			print_help_msg();
			ret = 0;
			goto out;
		case 'c':
			do_create = 1;
			name = optarg;
			break;
		case 'd':
			do_delete = 1;
			name = optarg;
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
		case 'b':
			len = strtol(optarg, NULL, 0);
			if (len == LONG_MIN || len == LONG_MAX) {
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

	if (!is_init) {
		memblk_pool_init(&pool, mempool, tx_len, MEMPOOL_SIZE / tx_len);
		is_init = 1;
	}

	if (do_create) {
		app_print("create ept %s\r\n", name);
		dev = rpmsg_memblk_create_dev(name);
		dev->do_verbose = do_verbose;
		pthread_join(dev->tx_task, NULL);
		pthread_join(dev->rx_task, NULL);
		app_print("delete ept %s \r\n", name);
		rpmsg_memblk_delete_dev(dev);
	}

	if (do_delete) {
		struct ept_test_entry *pos, *tmp;

		app_print("delete ept %s\r\n", name);
		list_for_each_entry_safe(pos, tmp, &g_rpmsg_epts, list) {
			if (strcmp(name, rpmsg_memblk_get_name(pos->dev)))
				continue;
			pos->stop = 1;
			break;
		}
	}

	if (do_list) {
		struct ept_test_entry *pos, *tmp;

		app_print("available dev\t\r\n");
		list_for_each_entry_safe(pos, tmp, &g_rpmsg_epts, list)
			app_print("\t%s\r\n", rpmsg_memblk_get_name(pos->dev));
	}

	return 0;
out:
	return ret;
}
