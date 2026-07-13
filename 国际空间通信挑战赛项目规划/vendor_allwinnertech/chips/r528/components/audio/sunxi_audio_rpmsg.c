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
#include <fcntl.h>
#include <time.h>
#include <aw_list.h>
#include <hal_mutex.h>
#include <hal_mem.h>
#include <hal_queue.h>
#include <hal_time.h>
#include <hal_thread.h>
#include <nuttx/rpmsg/rpmsg.h>
#include <nuttx/rptun/rptun.h>
#include <nuttx/audio/audio.h>
#include <nuttx/fs/fs.h>
#include <sys/ioctl.h>
#include <openamp/rpmsg.h>
#include <debug.h>

#include "rpmsg_audio.h"

static LIST_HEAD(g_rpmsg_epts);
static int unbind;

struct sunxi_audio_ept_entry {
	struct rpmsg_endpoint 	ept;
	struct list_head 		list;
	rpmsg_audio_ctrl_t 		ctrl_param;
};

void *audio_thread_create(sem_t *sem, void (*entry)(void *data), void *data, const char *name, int stack_size, int priority)
{
	hal_thread_t audio_task;

	sem_init(sem, 0, 0);
	audio_task = hal_thread_create(entry, data, name, stack_size, priority);

	return (void *)audio_task;
}

int audio_thread_stop(sem_t *sem, void *thread)
{
	int ret;

	ret = nxsem_tickwait_uninterruptible(sem, MS_TO_OSTICK(10000));
	if (ret != OK) {
		syslog(LOG_INFO, "ret:%d, wait thread quit timeout!\n", ret);
		hal_thread_stop(thread);
	}

	sem_destroy(sem);
	thread = NULL;

	return 0;
}

static int audio_rpmsg_ctrl_ept_callback(struct rpmsg_endpoint *ept, void *data,
		size_t len, uint32_t src, void *priv)
{
	int i;

	syslog(LOG_INFO, "ept name \"%s\" src:0x%lx dest:0x%lx received data (addr: %p, len: %d):\n",
			ept->name, ept->addr, ept->dest_addr, data, len);

	for (i = 0; i < len; ++i)
		syslog(LOG_INFO, " 0x%x,", *((uint8_t *)(data) + i));
	syslog(LOG_INFO, "\n");

	return 0;
}

int audio_rpmsg_ctl_send_cmd(const char *name, void *rpmsg_ctl_arg)
{
	int ret;
	rpmsg_audio_ctrl_t *audio_rpmsg_ctrl_param = (rpmsg_audio_ctrl_t*)rpmsg_ctl_arg;
	struct sunxi_audio_ept_entry *pos = NULL, *tmp = NULL;
	struct rpmsg_endpoint *audioeptdev = NULL;

	list_for_each_entry_safe(pos, tmp, &g_rpmsg_epts, list) {
		if (!strncmp(name, pos->ept.name, strlen(pos->ept.name))) {
			audioeptdev = &pos->ept;
			break;
		}
	}

	if (audioeptdev == NULL || audio_rpmsg_ctrl_param == NULL) {
		syslog(LOG_ERR, "audioeptdev is not found, and rpmsg_ctl_arg is null\n");
		return -1;
	}

	tmp = (struct sunxi_audio_ept_entry *)audioeptdev->priv;

	memcpy(&tmp->ctrl_param, audio_rpmsg_ctrl_param, sizeof(rpmsg_audio_ctrl_t));

	while (!is_rpmsg_ept_ready(audioeptdev)) {
		hal_msleep(10);
	}

	ret = rpmsg_send(audioeptdev, &tmp->ctrl_param, sizeof(rpmsg_audio_ctrl_t));
	if (ret < 0) {
		syslog(LOG_ERR, "rpmsg_send failed!\n");
		return ret;
	}

	return 0;
}

static void rpmsg_audio_ept_release(struct rpmsg_endpoint *ept)
{
	struct sunxi_audio_ept_entry *eptdev = ept->priv;

	syslog(LOG_INFO, "rpmsg: %s.%lx.%lx release\r\n", ept->name, ept->addr,
					ept->dest_addr);
	list_del(&eptdev->list);
	free(eptdev);
}

static bool audio_rpmsg_ctrl_ns_match(struct rpmsg_device *rdev,
							void *priv_, const char *name,
							uint32_t dest)
{
	return !strncmp(name, RPMSG_DIR_NAME, strlen(RPMSG_DIR_NAME));
}

static void audio_rpmsg_ctrl_unbind_cb(struct rpmsg_endpoint *ept)
{
	struct sunxi_audio_ept_entry *eptdev = ept->priv;

	syslog(LOG_INFO, "rpmsg: %s.%lx.%lx unbinding\r\n", ept->name, ept->addr,
					ept->dest_addr);

	/* this function will trigger rpmsg_audio_ept_release */
	rpmsg_destroy_ept(&eptdev->ept);
}

static void audio_rpmsg_ctrl_ns_bind(struct rpmsg_device *rdev,
								 void *priv_, const char *name,
								 uint32_t dest)
{
	struct sunxi_audio_ept_entry *audioeptdev;
	FAR struct file file;
	int ret;
	char audioctl[20];

	for (int i = 0; i < 4; i++) {
		snprintf(audioctl, 20, "/dev/audio/pcm1%dc", i);

		ret = file_open(&file, audioctl, O_RDONLY);
		if (ret < 0)
			syslog(LOG_ERR, "ERROR:open node failed: %d\n", ret);

		if (unbind) {
			ret = file_ioctl(&file, AUDIOIOC_HWRESET, 1);
			if (ret < 0) {
				syslog(LOG_ERR, "ERROR: AUDIOIOC_SETPARAMTER ioctl failed: %s, %d\n", audioctl, ret);
			}
		}
		memset(audioctl, 0, sizeof(audioctl));
		ret = file_close(&file);
		if (ret < 0)
			syslog(LOG_ERR, "ERROR:close node failed: %d\n", ret);
	}

	unbind = 0;
	syslog(LOG_INFO, "%s: binding\r\n", name);

	audioeptdev = hal_malloc(sizeof(*audioeptdev));
	if (audioeptdev == NULL) {
		syslog(LOG_ERR, "failed to alloc client entry\r\n");
		return;
	}

	memset(audioeptdev, 0, sizeof(*audioeptdev));
	audioeptdev->ept.priv = audioeptdev;

	audioeptdev->ept.release_cb = rpmsg_audio_ept_release;
	ret = rpmsg_create_ept(&audioeptdev->ept, rdev, name,
							 RPMSG_ADDR_ANY, RPMSG_ADDR_ANY,
							 audio_rpmsg_ctrl_ept_callback, audio_rpmsg_ctrl_unbind_cb);
	if (ret) {
		syslog(LOG_ERR, "failed to rpmsg_create_ept\r\n");
		goto free_eptdev;
	}

	audioeptdev->ept.dest_addr = dest;

	list_add(&audioeptdev->list, &g_rpmsg_epts);

	return;

free_eptdev:
	free(audioeptdev);
}

static void audio_rpmsg_ctrl_destroy(FAR struct rpmsg_device *rdev,
							 FAR void *priv)
{
	struct sunxi_audio_ept_entry *pos, *tmp;

	unbind = 1;

	list_for_each_entry_safe(pos, tmp, &g_rpmsg_epts, list) {
		audio_rpmsg_ctrl_unbind_cb(&pos->ept);
	}
}

void sunxi_audio_rpmsg_ctrl_init(void)
{

	syslog(LOG_INFO, "bind %s\r\n", RPMSG_DIR_NAME);

	rpmsg_register_callback(NULL, NULL, audio_rpmsg_ctrl_destroy,
					audio_rpmsg_ctrl_ns_match, audio_rpmsg_ctrl_ns_bind);

}

void sunxi_audio_rpmsg_ctrl_deinit(void)
{

	struct sunxi_audio_ept_entry *pos, *tmp;

	syslog(LOG_INFO, "unbind %s\r\n", RPMSG_DIR_NAME);

	list_for_each_entry_safe(pos, tmp, &g_rpmsg_epts, list) {
		audio_rpmsg_ctrl_unbind_cb(&pos->ept);
	}

	rpmsg_unregister_callback(NULL, NULL, audio_rpmsg_ctrl_destroy,
					audio_rpmsg_ctrl_ns_match, audio_rpmsg_ctrl_ns_bind);
}
