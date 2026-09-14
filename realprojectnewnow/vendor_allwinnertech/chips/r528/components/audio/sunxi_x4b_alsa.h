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

#ifndef __SUNXI_X4B_ALSA_H
#define __SUNXI_X4B_ALSA_H

#include <nuttx/config.h>
#include <stdint.h>
#include <stdbool.h>

#include <sys/param.h>

#include <nuttx/compiler.h>

#include <pthread.h>
#include <mqueue.h>

#include <nuttx/wqueue.h>
#include <nuttx/fs/ioctl.h>

#include "memblk.h"
#include "rpmsg_memblk.h"
#include "ring_buffer.h"

#ifdef CONFIG_AUDIO

/* Default x4b configuration values */

#ifndef CONFIG_X4B_MSG_PRIO
#define CONFIG_X4B_MSG_PRIO						1
#endif

#ifndef CONFIG_X4B_BUFFER_SIZE
#define CONFIG_X4B_BUFFER_SIZE     				1920
#endif

#ifndef CONFIG_X4B_NUM_BUFFERS
#define CONFIG_X4B_NUM_BUFFERS      			4
#endif

#ifndef CONFIG_X4B_WORKER_STACKSIZE
#define CONFIG_X4B_WORKER_STACKSIZE  			4096
#endif

/* Register Default Values **************************************************/

/* Registers have some undocumented bits set on power up.
 * These probably should be retained on writes (?).
 */

/* Default configuration */

#define REOCRD_DEFAULT_SAMPRATE      			48000     /* Initial sample rate */
#define REOCRD_DEFAULT_NCHANNELS     			3         /* Initial number of channels */
#define REOCRD_DEFAULT_BPSAMP        			32        /* Initial bits per sample */

/* x4b driver index */
#define AUDIO_VOICE_CALL					10
#define AUDIO_WWE           					11
#define AUDIO_MDSPEECH          				12
#define AUDIO_TEST						13

#define RPMSG_MEMBLK_DEV_NAME_PREFIX			"x4b_memblk"

#define RETRY_CNT								40

typedef struct _sunxi_x4b_ringbuf_s
{
	bool					is_init;
	void 					*audio_buf;
	ring_buffer_t			audio_ringbuf;
	pthread_mutex_t         audio_ringbuf_mutex;
}sunxi_x4b_ringbuf_s;

typedef struct _sunxi_x4b_memblk_s
{
	bool					has_create;
	bool					thread_stop;
	sem_t 					bind_sem;
	struct rpmsg_memblk_dev *memblk_dev;
}sunxi_x4b_memblk_s;

/****************************************************************************
 * Public Types
 ****************************************************************************/

struct sunxi_x4b_dev_s
{
	struct audio_lowerhalf_s dev;             /* audio lower half (this device) */

	uint32_t 				period_size;
	uint32_t 				buffer_size;
	uint32_t 				buffer_byte;

	/* Our specific driver data goes here */
	struct dq_queue_s       pendq;            /* Queue of pending buffers to be recv */
	struct file             mq;               /* Message queue for receiving messages */
	char                    mqname[16];       /* Our message queue name */
	pthread_mutex_t         mutex;            /* Thread sync mutex */
	pthread_cond_t          cond_pause;       /* For synchronization */
	int16_t                 index;            /* index: 0:audiocodec, 1:dmic, >=10:customize*/
	mutex_t                 pendlock;         /* Protect pendq */
	uint16_t                samprate;         /* Configured samprate (samples/sec) */
	uint8_t                 nchannels;        /* Number of channels (1~8) */
	uint8_t                 bpsamp;           /* Bits per sample (16 or 32) */
	bool                    running;          /* True: Worker thread is running */
	bool                    paused;           /* True: Recording is paused */
#ifndef CONFIG_AUDIO_EXCLUDE_STOP
	bool                    terminating;      /* True: Stop requested */
#endif
	sunxi_x4b_memblk_s		mb;
	sunxi_x4b_ringbuf_s		rb;
	hal_thread_t            creat_task;
	hal_thread_t            record_task;
	hal_thread_t            rpmsg_task;
	sem_t                   record_sem;
	sem_t                   rpmsg_sem;
	sem_t				sem;
	bool                    reserved;         /* True: Device is reserved */
	bool			dsp_crash;
};

/****************************************************************************
 * Public Data
 ****************************************************************************/

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/
/****************************************************************************
 * Name: x4b_record_initialize
 *
 * Description:
 *   Initialize the x4b record device.
 *
 * Input Parameters:
 *   index      - 10:voice call, 11:wwe, 12:mdspeech.
 *
 * Returned Value:
 *   A new lower half audio interface for the x4b recorf device is returned on
 *   success; NULL is returned on failure.
 *
 ****************************************************************************/

FAR struct audio_lowerhalf_s *
	x4b_record_initialize(int index);



#endif /* CONFIG_AUDIO */
#endif /* __SUNXI_X4B_ALSA_H */
