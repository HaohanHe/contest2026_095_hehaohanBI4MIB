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

#include <nuttx/config.h>
#include <nuttx/arch.h>

#include <stdint.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/irq.h>
#include <nuttx/kmalloc.h>
#include <nuttx/clock.h>
#include <nuttx/i2c/i2c_master.h>
#include <arch/board/board.h>

#include "chip.h"

#include "sunxi_hal_twi.h"
#include "hal_mem.h"

#if defined(CONFIG_DRIVERS_TWI)

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

struct sunxi_i2c_lowerhalf_s
{
	struct i2c_master_s i2c_s;
	int twi_port;
	void *arg;
};
/* "Lower half" driver methods **********************************************/

static int r528_transfer(FAR struct i2c_master_s *lower,
                         FAR struct i2c_msg_s *msgs, int count);
#ifdef CONFIG_I2C_RESET
static int r528_reset(FAR struct i2c_master_s *lower);
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/
static const struct i2c_ops_s g_i2cops =
{
  .transfer = r528_transfer,
#ifdef CONFIG_I2C_RESET
  .reset    = r528_reset,
#endif
};

/* "Lower half" driver state */

static struct sunxi_i2c_lowerhalf_s g_i2cdev[4];

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int r528_transfer(FAR struct i2c_master_s *lower,
                         FAR  struct i2c_msg_s *msgs, int count)
{
  FAR struct sunxi_i2c_lowerhalf_s *priv = (FAR struct sunxi_i2c_lowerhalf_s *)lower;

  i2cinfo("Entry\n");
  DEBUGASSERT(priv);

  struct twi_msg *twi_msgs = (struct twi_msg *)kmm_malloc(count * sizeof(struct twi_msg));
  if (twi_msgs == NULL) {
	syslog(LOG_ERR, "malloc twi mags fail\n");
	return -1;
  }
  memset(twi_msgs, 0, count * sizeof(struct twi_msg));

  for (int i = 0; i < count; i++) {
    twi_msgs[i].addr = msgs[i].addr;
	twi_msgs[i].flags = msgs[i].flags;
	twi_msgs[i].len = msgs[i].length;
	twi_msgs[i].buf = msgs[i].buffer;
  }

  hal_twi_init(priv->twi_port);

  if (hal_twi_xfer(priv->twi_port, twi_msgs, count)) {
    kmm_free(twi_msgs);
    return -1;
  }

  kmm_free(twi_msgs);
  return OK;
}

#ifdef CONFIG_I2C_RESET
static int r528_reset(FAR struct i2c_master_s *lower)
{
  FAR struct sunxi_i2c_lowerhalf_s *priv = (FAR struct sunxi_i2c_lowerhalf_s *)lower;

  hal_twi_soft_reset(priv->twi_port);
  return OK;
}
#endif

FAR struct i2c_master_s *r528_i2c_initialize(FAR const char *devpath, int i2c_id)
{
  FAR struct sunxi_i2c_lowerhalf_s *priv = &g_i2cdev[i2c_id];
  FAR int handle;

  i2cinfo("Entry: devpath=%s\n", devpath);

  priv->twi_port = i2c_id;
  priv->i2c_s.ops = &g_i2cops;

  hal_twi_init(priv->twi_port);

  handle = i2c_register((FAR struct i2c_master_s *)priv, i2c_id);

  if (!handle) {
      i2cinfo("i2c%d init success!\n", i2c_id);
      return (FAR struct i2c_master_s *)priv;
  } else {
      i2cinfo("i2c%d register faile\n", i2c_id);
      return NULL;
  }
}

#endif
