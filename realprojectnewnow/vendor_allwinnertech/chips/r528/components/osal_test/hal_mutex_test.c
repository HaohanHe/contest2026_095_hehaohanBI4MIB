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
#include <hal_mutex.h>
#include <hal_thread.h>
#include <hal_time.h>

volatile static int hal_mutex_test_flag = 0;
static hal_mutex_t mutex = NULL;

static void hal_mutex_test_entry(void *args)
{
	hal_mutex_lock(mutex);
	hal_mutex_test_flag = 1;
	hal_msleep(100);
	hal_mutex_test_flag = 2;
	hal_mutex_unlock(mutex);
	hal_thread_stop(NULL);
}

int hal_mutex_test_main(int argc, char **argv)
{
	mutex = hal_mutex_create();
	if (!mutex) {
		_err("HAL_MUTEX_TEST1 failed\r\n");
		return -1;
	}

	hal_thread_t th = hal_thread_create(hal_mutex_test_entry, NULL, "test-mutex", 4096, 100);
	if (th) {
		hal_thread_start(th);
		hal_msleep(100);
		if (hal_mutex_test_flag != 1) {
			_err("HAL_MUTEX_TEST1 failed\r\n");
		}
		hal_mutex_lock(mutex);
		if (hal_mutex_test_flag != 2) {
			_err("HAL_MUTEX_TEST1 failed\r\n");
		}
		hal_mutex_unlock(mutex);
	} else {
		_err("HAL_MUTEX_TEST1 failed\r\n");
	}
	hal_mutex_delete(mutex);
	_info("HAL_MUTEX_TEST1 pass\r\n");

    return 0;
}
