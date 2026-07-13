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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <debug.h>

#include <nuttx/board.h>

#include <nuttx/rptun/rptun.h>

#include "hwspinlock.h"
#include "r528_board.h"
#ifdef CONFIG_ARCH_TRUSTZONE_SECURE
#include "sm.h"
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Interrupt ids:
 * ID0-ID7 for Non-secure interrupts
 * ID8-ID15 for Secure interrupts.
 */

#define X4B_NOSECURE_INTTERRUPT     0

#define X4B_SECURE_INTTERRUPT       SUNXI_IRQ_HWSPINLOCK

/* OpenAMP shared memory, this address must be not used by vela */

#define X4B_SHMEM_ADDR              (CONFIG_ARMV7A_SMP_BUSY_WAIT_FLAG_ADDR + 0x4)

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: r528_boardinitialize
 *
 * Description:
 *   All r528 architectures must provide the following entry point.  This entry
 *   point is called early in the initialization -- after all memory has been
 *   configured and mapped but before any devices have been initialized.
 *
 ****************************************************************************/

void r528_boardinitialize(void)
{
  /* Configure on-board LEDs. */

  r528_led_initialize();
}

#ifdef CONFIG_BOARD_EARLY_INITIALIZE
extern void r528_early_initialize(void);
void board_early_initialize(void)
{
  r528_early_initialize();
}
#endif

/****************************************************************************
 * Name: board_late_initialize
 *
 * Description:
 *   If CONFIG_BOARD_LATE_INITIALIZE is selected, then an additional
 *   initialization call will be performed in the boot-up sequence to a
 *   function called board_late_initialize().  board_late_initialize() will be
 *   called immediately after up_initialize() is called and just before the
 *   initial application is started.  This additional initialization phase
 *   may be used, for example, to initialize board-specific device drivers.
 *
 ****************************************************************************/

#ifdef CONFIG_BOARD_LATE_INITIALIZE

/****************************************************************************
 * Name: rptun_setup_shmem
 *
 * Description:
 *   Initialize shared memory buffer
 *
 * Input Parameters:
 *   base - Start shared memory address for OpenAMP
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

#if defined(CONFIG_X4B_TEE)
static void rptun_setup_shmem(struct rptun_rsc_s *rsc)
{
  memset(rsc, 0, sizeof(struct rptun_rsc_s));
  rsc->rsc_tbl_hdr.ver          = 1;
  rsc->rsc_tbl_hdr.num          = 1;
  rsc->offset[0]                = offsetof(struct rptun_rsc_s,
                                           rpmsg_vdev);
  rsc->rpmsg_vdev.type          = RSC_VDEV;
  rsc->rpmsg_vdev.id            = VIRTIO_ID_RPMSG;
  rsc->rpmsg_vdev.dfeatures     = 1 << VIRTIO_RPMSG_F_NS
                                | 1 << VIRTIO_RPMSG_F_ACK
                                | 1 << VIRTIO_RPMSG_F_BUFSZ
                                | VIRTIO_RING_F_MUST_NOTIFY;
  rsc->rpmsg_vdev.num_of_vrings = 2;
  rsc->rpmsg_vdev.notifyid      = RSC_NOTIFY_ID_ANY;
  rsc->rpmsg_vdev.config_len    = sizeof(struct fw_rsc_config);
  rsc->rpmsg_vring0.align       = 8;
  rsc->rpmsg_vring0.num         = 8;
  rsc->rpmsg_vring0.notifyid    = RSC_NOTIFY_ID_ANY;
  rsc->rpmsg_vring1.align       = 8;
  rsc->rpmsg_vring1.num         = 8;
  rsc->rpmsg_vring1.notifyid    = RSC_NOTIFY_ID_ANY;
  rsc->config.r2h_buf_size      = 0x200;
  rsc->config.h2r_buf_size      = 0x200;
}
#endif

/****************************************************************************
 * Name: r528_rptun_init
 *
 * Description:
 *   Initialize TEE or AP rptun
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   Return 0 on success. Otherwise, return a negated errno.
 *
 ****************************************************************************/

#ifdef CONFIG_AW_RPTUN_SECURE
static int r528_rptun_init(void)
{
  struct rptun_rsc_s *rsc = (struct rptun_rsc_s *)X4B_SHMEM_ADDR;
  int ret = 0;

  int r528_rptun_secure_init(FAR const char *cpuname, bool master,
                      FAR struct rptun_rsc_s *rsc, int irq_event,
                      int irq_trigger);
#if defined(CONFIG_X4B_TEE)
  rptun_setup_shmem(rsc);
  ret = r528_rptun_secure_init("ap", false, rsc,
                          X4B_SECURE_INTTERRUPT, X4B_NOSECURE_INTTERRUPT);
#elif defined(CONFIG_X4B_AP) || defined(CONFIG_X4B_FACTEST)
  ret = r528_rptun_secure_init("tee", true, rsc,
                          X4B_NOSECURE_INTTERRUPT, X4B_SECURE_INTTERRUPT);
#endif

  if (ret < 0)
    {
      berr("Failed to init rptun : %d\n", ret);
    }

  return ret;
}
#endif

#ifdef CONFIG_SYSLOG_RPMSG
static char r528_logbuffer[4096];
#endif

extern void r528_late_initialize(void);
void board_late_initialize(void)
{
	r528_late_initialize();
	r528_bringup();

#ifdef CONFIG_AW_RPTUN_SECURE
  r528_rptun_init();
#endif

#ifdef CONFIG_X4B_TEE
#ifdef CONFIG_SYSLOG_RPMSG
  extern void syslog_rpmsg_init_early(FAR void *buffer, size_t size);
  syslog_rpmsg_init_early(r528_logbuffer, sizeof(r528_logbuffer));
#endif
#endif

}
#endif /* CONFIG_BOARD_LATE_INITIALIZE */
