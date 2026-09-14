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

#include <stdlib.h>
#include <stdio.h>
#include <debug.h>
#include <hal_queue.h>
#include <hal_thread.h>
#include <hal_time.h>
#include <hal_status.h>

int hal_queue_test_main(int argc, char **argv)
{
    hal_queue_t queue = NULL;
	unsigned int value;
	unsigned int recv;

	queue = hal_queue_create("test-queue", sizeof(int), 4);
	if (!queue) {
		_err("HAL_QUEUE_TEST1 failed\r\n");
		return -1;
	}
	value = 0;
	if (hal_queue_send(queue, &value) != HAL_OK) {
		goto error;
	}
	value = 1;
	if (hal_queue_send(queue, &value) != HAL_OK) {
		goto error;
	}
	value = 2;
	if (hal_queue_send(queue, &value) != HAL_OK) {
		goto error;
	}
	value = 3;
	if (hal_queue_send(queue, &value) != HAL_OK) {
		goto error;
	}
	if (hal_queue_recv(queue, &recv, HAL_WAIT_FOREVER) != HAL_OK) {
		goto error;
	} else if (recv != 0) {
		goto error;
	}
	if (hal_queue_recv(queue, &recv, HAL_WAIT_FOREVER) != HAL_OK) {
		goto error;
	} else if (recv != 1) {
		goto error;
	}
	if (hal_queue_recv(queue, &recv, HAL_WAIT_FOREVER) != HAL_OK) {
		goto error;
	} else if (recv != 2) {
		goto error;
	}
	if (hal_queue_recv(queue, &recv, HAL_WAIT_FOREVER) != HAL_OK) {
		goto error;
	} else if (recv != 3) {
		goto error;
	}
	if (hal_queue_is_empty(queue) != 1) {
		_err("HAL_QUEUE_TEST1 failed\r\n");
	}
	hal_queue_delete(queue);
	_info("HAL_QUEUE_TEST1 pass\r\n");
	return 0;

error:
	_err("HAL_QUEUE_TEST1 failed\r\n");
	hal_queue_delete(queue);

    return 0;
}
