/*
* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
*
* Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
* the the people's Republic of China and other countries.
* All Allwinner Technology Co.,Ltd. trademarks are used with permission.
*
* DISCLAIMER
* THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
* IF YOU NEED TO INTEGRATE THIRD PARTY'S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
* IN ALLWINNERS'SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
* ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
* ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
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
#include <string.h>
#include <debug.h>

#include <hal_log.h>
#include <hal_mem.h>
#include <hal_cache.h>
#include <hal_dma.h>
#include <hal_timer.h>

#include <sunxi_hal_common.h>
#include <nuttx/mmcsd.h>

#define DMA_TEST_LEN	1024

enum cmd_status {
    CMD_STATUS_ACKED        = 100,  /* already acked, no need to send respond */

    /* success status */
    CMD_STATUS_SUCCESS_MIN  = 200,
    CMD_STATUS_OK           = 200,  /* command exec success */
    CMD_STATUS_SUCCESS_MAX  = 200,

    /* error status */
    CMD_STATUS_ERROR_MIN    = 400,
    CMD_STATUS_UNKNOWN_CMD  = 400,  /* unknown command */
    CMD_STATUS_INVALID_ARG  = 401,  /* invalid argument */
    CMD_STATUS_FAIL         = 402,  /* command exec failed */
    CMD_STATUS_ERROR_MAX    = 402,
};

extern enum cmd_status cmd_sd_test_exec(char *cmd);
struct sdio_dev_s *sdio_initialize(int sdcno);
int main(int argc, char **argv)
{
    //int i = 0;
	/*
    while(argc--)
    {
	_info("argv[%d] = %s\n", i, argv[i]);
	i++;
    }
    */
	_info("******start mmc test******\n");
#if 0
	cmd_sd_test_exec(NULL);
#else
	void *p = sdio_initialize(0);
	mmcsd_slotinitialize(100, p);
#endif
	_info("******mmc test finish******\n");
    return 0;
}

