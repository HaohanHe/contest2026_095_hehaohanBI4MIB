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
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <sys/time.h>
#include <hal_msgbox.h>
#include <hal_queue.h>
#include <hal_status.h>
#include <debug.h>

#include <sunxi_hal_common.h>

#define app_print             _info

#define RECEIVE_QUEUE_LENGTH 16
#define RECEIVE_QUEUE_WAIT_MS 100

struct msgbox_demo {
	int remote_id;
	int read_ch;
	int write_ch;
	hal_queue_t recv_queue;
};

static void print_help_msg(void)
{
	app_print("\n");
	app_print("USAGE:\n");
	app_print("  test_msgbox <REQUIRED_ARGUMENTS> [OPTIONS]\n");
	app_print("\n");
	app_print("REQUIRED_ARGUMENTS:\n");
	app_print("  -E REMOTE_ID: specify remote id\n");
	app_print("  -R READ_CH  : specify read channel\n");
	app_print("  -W WRITE_CH : specify write channel\n");
	app_print("OPTIONS:\n");
	app_print("  -s MESSAGE  : send MESSAGE\n");
	app_print("  -r          : receive messages\n");
	app_print("  -t TIMEOUT  : exit in TIMEOUT seconds when receive\n");
	app_print("e.g. (communicate with remote 0, use read channel 3 and write channel 3):\n");
	app_print("  test_msgbox -E 0 -R 3 -W 3 -s \"hello\" : send string \"hello\"\n");
	app_print("  test_msgbox -E 0 -R 3 -W 3 -r           : receive messages (default in 10 seconds)\n");
	app_print("  test_msgbox -E 0 -R 3 -W 3 -r -t 20     : receive messages in 20 seconds\n");
	app_print("\n");
}

static int recv_callback(unsigned long data, void *private_data)
{
	struct msgbox_demo *demo = private_data;
	int ret = hal_queue_send(demo->recv_queue, &data);

	_info("Receive callback (data: 0x%lx)\n", data);
	if (ret != HAL_OK) {
		_err("recv_queue is full\n");
		return -1;
	}

	return 0;
}

int main(int argc, char **argv)
{
	int ret = 0;
	int c;

	struct msgbox_demo demo= {
		.remote_id = -1,
		.read_ch = -1,
		.write_ch = -1,
		.recv_queue = NULL,
	};
	struct msg_endpoint ept;

	struct timeval start_sec, current_sec;
	int do_send = 0;
	const char *data_send= NULL;
	int do_recv = 0;
	int timeout_sec = 10;
	uint32_t data_recv;

	if (argc <= 1) {
		print_help_msg();
		ret = -1;
		goto out;
	}

	while ((c = getopt(argc, argv, "hs:rt:E:W:R:")) != -1) {
		switch (c) {
		case 'h' :
			print_help_msg();
			ret = 0;
			goto out;
		case 'E':
			demo.remote_id = atoi(optarg);
			break;
		case 'R':
			demo.read_ch = atoi(optarg);
			break;
		case 'W':
			demo.write_ch = atoi(optarg);
			break;
		case 's':
			do_send = 1;
			data_send = optarg;
			break;
		case 'r':
			do_recv = 1;
			break;
		case 't':
			timeout_sec = atoi(optarg);
			break;
		default:
			print_help_msg();
			ret = -1;
			goto out;
		}
	}

	if (demo.remote_id < 0 || demo.read_ch < 0 || demo.write_ch < 0) {
		app_print("Error. Please specify remote id, read channel and write channel\n");
		print_help_msg();
		ret = -1;
		goto out;
	}

	app_print("remote id: %d, write channel: %d, read channel: %d\n",
			demo.remote_id, demo.write_ch, demo.read_ch);

	if (do_recv) {
		demo.recv_queue = hal_queue_create("msg_queue", sizeof(uint32_t), RECEIVE_QUEUE_LENGTH);
		if (!demo.recv_queue) {
			app_print("Failed to create receive queue\n");
			ret = -1;
			goto out;
		}
		ept.rec = (void *)recv_callback;
		ept.private = &demo;
	}

	ret = hal_msgbox_alloc_channel(&ept, demo.remote_id, demo.read_ch, demo.write_ch);
	if (ret != 0) {
		app_print("Failed to allocate msgbox channel\n");
		goto delete_recv_queue;
	}

	if (do_send) {
		ret = hal_msgbox_channel_send(&ept, (unsigned char *)data_send, strlen(data_send));
		if (ret != 0) {
			app_print("Failed to send message\n");
			goto free_channel;
		}
	}

	if (do_recv) {
		app_print("test_msgbox will exit in %d seconds\n", timeout_sec);
		gettimeofday(&start_sec, NULL);
		app_print("start_sec: %lu\n", (unsigned long)start_sec.tv_sec);

		while (1) {
			if (HAL_OK == hal_queue_recv(demo.recv_queue, &data_recv,
						RECEIVE_QUEUE_WAIT_MS)) {
				app_print("Received from queue: 0x%" PRIx32 "\n", (uint32_t)data_recv);
			}
			gettimeofday(&current_sec, NULL);
			if ((current_sec.tv_sec - start_sec.tv_sec) >= timeout_sec) {
				app_print("current_sec: %lu\n", (unsigned long)current_sec.tv_sec);
				break;
			}
		}
	}

	app_print("test_msgbox exited\n");
	ret = 0;

free_channel:
	hal_msgbox_free_channel(&ept);
delete_recv_queue:
	if (do_recv) {
		hal_queue_delete(demo.recv_queue);
	}
out:
	return ret;
}
