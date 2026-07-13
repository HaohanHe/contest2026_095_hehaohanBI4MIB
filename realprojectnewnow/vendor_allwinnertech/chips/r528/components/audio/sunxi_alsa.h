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

/* Reference:
 *   "R528 record for Portable Audio Applications, Pre-
 *    Production", April 2023, Rev 1.0, Allwinner.
 */

#ifndef __AW_DRIVERS_AUDIO_H
#define __AW_DRIVERS_AUDIO_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdint.h>
#include <stdbool.h>

#include <sys/param.h>

#include <nuttx/compiler.h>

#include <pthread.h>
#include <mqueue.h>

#include <nuttx/wqueue.h>
#include <nuttx/fs/ioctl.h>

#include "sunxi_audio_rpmsg.h"
#include "aweq_interface.h"

#ifdef CONFIG_AUDIO

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************
 * CONFIG_AW_AUDIO_CODEC- Enables audiocodec support
 * CONFIG_AW_AUDIO_CODEC_ADC_INITVOLUME - The initial adc volume level
 *                             				in the range {0..255}
 * CONFIG_AW_AUDIO_CODEC_MSG_PRIO - Priority of messages sent to the audiocodec
 *                                   worker thread.
 * CONFIG_AW_AUDIO_CODEC_BUFFER_SIZE - Preferred buffer size
 * CONFIG_AW_AUDIO_CODEC_NUM_BUFFERS - Preferred number of buffers
 * CONFIG_AW_AUDIO_CODEC_WORKER_STACKSIZE - Stack size to use when creating the the
 *                                           audiocodec worker thread.
 * CONFIG_AW_AUDIO_DMIC - Enables dmic support
 */

/* Pre-requisites */

#ifndef CONFIG_AUDIO
#  error CONFIG_AUDIO is required for audio subsystem support
#endif

#ifndef CONFIG_SCHED_WORKQUEUE
#  error CONFIG_SCHED_WORKQUEUE is required by the dmic driver
#endif

/* Default audiocodec adc configuration values */

#ifndef CONFIG_AW_AUDIO_CODEC_ADC_INITVOLUME
#define CONFIG_AW_AUDIO_CODEC_ADC_INITVOLUME       160
#endif

#if CONFIG_AW_AUDIO_CODEC_ADC_INITVOLUME > 255
#  error CONFIG_AW_AUDIO_CODEC_ADC_INITVOLUME must fit in a uint8_t
#endif

#ifndef CONFIG_AW_AUDIO_CODEC_MSG_PRIO
#define CONFIG_AW_AUDIO_CODEC_MSG_PRIO         1
#endif

#ifndef CONFIG_AW_AUDIO_CODEC_BUFFER_SIZE
#define CONFIG_AW_AUDIO_CODEC_BUFFER_SIZE      4096
#endif

#ifndef CONFIG_AW_AUDIO_CODEC_NUM_BUFFERS
#define CONFIG_AW_AUDIO_CODEC_NUM_BUFFERS      4
#endif

#ifndef CONFIG_AW_AUDIO_CODEC_WORKER_STACKSIZE
#define CONFIG_AW_AUDIO_CODEC_WORKER_STACKSIZE  4096
#endif

#ifndef CONFIG_AW_AUDIO_CODEC_DEFAULT_CARDNAME
#define CONFIG_AW_AUDIO_CODEC_DEFAULT_CARDNAME  "audiocodec"
#endif

/* Default dmic configuration values */

#ifndef CONFIG_AW_AUDIO_DMIC_MSG_PRIO
#define CONFIG_AW_AUDIO_DMIC_MSG_PRIO        1
#endif

#ifndef CONFIG_AW_AUDIO_DMIC_BUFFER_SIZE
#define CONFIG_AW_AUDIO_DMIC_BUFFER_SIZE      4096
#endif

#ifndef CONFIG_AW_AUDIO_DMIC_NUM_BUFFERS
#define CONFIG_AW_AUDIO_DMIC_NUM_BUFFERS      4
#endif

#ifndef CONFIG_AW_AUDIO_DMIC_WORKER_STACKSIZE
#define CONFIG_AW_AUDIO_DMIC_WORKER_STACKSIZE  4096
#endif

#ifndef CONFIG_AW_AUDIO_DMIC_DEFAULT_CARDNAME
#define CONFIG_AW_AUDIO_DMIC_DEFAULT_CARDNAME  "snddmic"
#endif

/* Register Default Values **************************************************/

/* Registers have some undocumented bits set on power up.
 * These probably should be retained on writes (?).
 */

/* Default configuration */

#define REOCRD_DEFAULT_SAMPRATE      16000     /* Initial sample rate */
#define REOCRD_DEFAULT_NCHANNELS     2         /* Initial number of channels */
#define REOCRD_DEFAULT_BPSAMP        16        /* Initial bits per sample */

#define AW_AUDIO_CODEC             0
#define AW_AUDIO_DMIC           	1


/****************************************************************************
 * Public Types
 ****************************************************************************/

struct sunxi_dev_s
{
	/* We are an audio lower half driver (We are also the upper "half" of
	* the dmic driver with respect to the board lower half driver).
	*
	* Terminology:
	* Our "lower" half audio instances will be called dev for the publicly
	* visible version and "priv" for the version that only this driver
	* knows.  From the point of view of this driver, it is the board lower
	* "half" that is referred to as "lower".
	*/

	struct audio_lowerhalf_s dev;             /* audio lower half (this device) */

	snd_pcm_t 				*handle;          /* audio handle */
	snd_pcm_format_t 		format;
	snd_pcm_uframes_t 		period_size;
	snd_pcm_uframes_t 		buffer_size;

	/* Our specific driver data goes here */
	struct dq_queue_s       pendq;            /* Queue of pending buffers to be recv */
	struct file             mq;               /* Message queue for receiving messages */
	char                    mqname[16];       /* Our message queue name */
	char                    pcm_name[32];     /* Our pcm name */
	pthread_mutex_t         mutex;            /* Thread sync mutex */
	pthread_cond_t          cond_pause;       /* For synchronization */
	int16_t                 index;            /* index: 0:audiocodec, 1:dmic */
	mutex_t                 pendlock;         /* Protect pendq */
	uint16_t                samprate;         /* Configured samprate (samples/sec) */
#ifndef CONFIG_AUDIO_EXCLUDE_VOLUME
	uint8_t                 volume;           /* Current volume level {0..63} */
#endif /* CONFIG_AUDIO_EXCLUDE_VOLUME */
	uint8_t                 nchannels;        /* Number of channels (1~8) */
	uint8_t                 bpsamp;           /* Bits per sample (16 or 32) */
	bool                    running;          /* True: Worker thread is running */
	bool                    paused;           /* True: Recording is paused */
	bool                    mute;             /* True: Input is muted */
	bool                    record;           /* True: Record, false: Play */
#ifndef CONFIG_AUDIO_EXCLUDE_STOP
	bool                    terminating;      /* True: Stop requested */
#endif
	bool                    reserved;         /* True: Device is reserved */
	eq_prms_t               eqprms;
	bool                    eq_enable;
	bool                    eq_tunning;
	hal_thread_t            task;
	sem_t                   sem;
	sem_t			start_handle;
};

/****************************************************************************
 * Public Data
 ****************************************************************************/

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/
/****************************************************************************
 * Name: dmic_initialize
 *
 * Description:
 *   Initialize the dmic device.
 *
 * Input Parameters:
 *   record     - Enable recording. dmic only support record.
 *   index      - 0:audiocodec adc, 1:dmic.
 *
 * Returned Value:
 *   A new lower half audio interface for the dmic device is returned on
 *   success; NULL is returned on failure.
 *
 ****************************************************************************/

FAR struct audio_lowerhalf_s *
	record_initialize(bool record, int index);



#endif /* CONFIG_AUDIO */
#endif /* __AW_DRIVERS_AUDIO_H */
