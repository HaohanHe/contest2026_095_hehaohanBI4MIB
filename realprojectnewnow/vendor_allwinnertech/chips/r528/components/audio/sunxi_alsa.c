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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/types.h>
#include <sys/ioctl.h>

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/kmalloc.h>
#include <nuttx/mqueue.h>
#include <nuttx/queue.h>
#include <nuttx/clock.h>
#include <nuttx/wqueue.h>
#include <nuttx/fs/fs.h>
#include <nuttx/fs/ioctl.h>
#include <nuttx/audio/audio.h>

#include <aw-alsa-lib/pcm.h>
#include <aw-alsa-lib/control.h>
#include <hal_time.h>
#include "common.h"
#include "sunxi_alsa.h"
#include "sunxi_audio_rpmsg.h"
#include "rpmsg_audio.h"

#include <arch/chip/mi_hw_version.h>
#include <hal_thread.h>

#define EQ_CONFIG_FILE_AD1       "/etc/EQ_AD51652.conf"
#define EQ_CONFIG_FILE_ACM       "/etc/EQ_ACM.conf"
#define EQ_CONFIG_FILE_AD2       "/etc/EQ_AD52058.conf"
#define EQ_MAX_BIN_NUM           10
#define EQ_ENABLE_BIT            (1 << 0)

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

#ifndef CONFIG_AUDIO_EXCLUDE_VOLUME
static void sunxi_audio_setvolume(FAR struct sunxi_dev_s *priv,
								uint16_t volume);
#endif

/* Audio lower half methods (and close friends) */

static int sunxi_audio_getcaps(FAR struct audio_lowerhalf_s *dev, int type,
							FAR struct audio_caps_s *caps);
#ifdef CONFIG_AUDIO_MULTI_SESSION
static int sunxi_audio_configure(FAR struct audio_lowerhalf_s *dev,
								FAR void *session,
								FAR const struct audio_caps_s *caps);
#else
static int sunxi_audio_configure(FAR struct audio_lowerhalf_s *dev,
								FAR const struct audio_caps_s *caps);
#endif
static int sunxi_audio_shutdown(FAR struct audio_lowerhalf_s *dev);
static int record_fillbuffer(FAR struct sunxi_dev_s *priv);
static int record_fillonebuffer(FAR struct sunxi_dev_s *priv);

#ifdef CONFIG_AUDIO_MULTI_SESSION
static int sunxi_audio_start(FAR struct audio_lowerhalf_s *dev,
							FAR void *session);
#else
static int sunxi_audio_start(FAR struct audio_lowerhalf_s *dev);
#endif
#ifndef CONFIG_AUDIO_EXCLUDE_STOP
#ifdef CONFIG_AUDIO_MULTI_SESSION
static int  sunxi_audio_stop(FAR struct audio_lowerhalf_s *dev,
							FAR void *session);
#else
static int sunxi_audio_stop(FAR struct audio_lowerhalf_s *dev);
#endif
#endif
#ifndef CONFIG_AUDIO_EXCLUDE_PAUSE_RESUME
#ifdef CONFIG_AUDIO_MULTI_SESSION
static int sunxi_audio_pause(FAR struct audio_lowerhalf_s *dev,
							FAR void *session);
static int sunxi_audio_resume(FAR struct audio_lowerhalf_s *dev,
							FAR void *session);
#else
static int sunxi_audio_pause(FAR struct audio_lowerhalf_s *dev);
static int sunxi_audio_resume(FAR struct audio_lowerhalf_s *dev);
#endif
#endif
static int sunxi_audio_enqueuebuffer(FAR struct audio_lowerhalf_s *dev,
									FAR struct ap_buffer_s *apb);
static int sunxi_audio_ioctl(FAR struct audio_lowerhalf_s *dev, int cmd,
							unsigned long arg);
#ifdef CONFIG_AUDIO_MULTI_SESSION
static int sunxi_audio_reserve(FAR struct audio_lowerhalf_s *dev,
							FAR void **session);
#else
static int sunxi_audio_reserve(FAR struct audio_lowerhalf_s *dev);
#endif
#ifdef CONFIG_AUDIO_MULTI_SESSION
static int  sunxi_audio_release(FAR struct audio_lowerhalf_s *dev,
							FAR void *session);
#else
static int sunxi_audio_release(FAR struct audio_lowerhalf_s *dev);
#endif


static void record_workerthread(void *arg);
static void play_workerthread(void *arg);

/* Initialization */

static void sunxi_audio_reset(FAR struct sunxi_dev_s *priv);

static long long start_before;
static long long dq_before;
static long long dq_after;
/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct audio_ops_s g_audio_ops =
{
	.getcaps       = sunxi_audio_getcaps,
	.configure     = sunxi_audio_configure,
	.shutdown      = sunxi_audio_shutdown,
	.start         = sunxi_audio_start,
#ifndef CONFIG_AUDIO_EXCLUDE_STOP
	.stop          = sunxi_audio_stop,
#endif
#ifndef CONFIG_AUDIO_EXCLUDE_PAUSE_RESUME
	.pause         = sunxi_audio_pause,
	.resume        = sunxi_audio_resume,
#endif
	.enqueuebuffer = sunxi_audio_enqueuebuffer,
	.ioctl         = sunxi_audio_ioctl,
	.reserve       = sunxi_audio_reserve,
	.release       = sunxi_audio_release,
};

/****************************************************************************
 * Name: sunxi_get_latency
 *
 * Description:
 *   latency = alsa-lib pcm delay + pendq buffer
 *   if snd driver not start,we also count pendq buffer
 ****************************************************************************/

static int sunxi_get_latency(struct audio_lowerhalf_s *dev,
								unsigned long arg)
{
	struct sunxi_dev_s *priv = (struct sunxi_dev_s *)dev;
	long *latency = (long *)arg;
	struct ap_buffer_s *apb;
	dq_entry_t *cur;
	long remain = 0;
	int ret;

	if (priv->handle)
		snd_vela_pcm_delay(priv->handle, latency);
	ret = nxmutex_lock(&priv->pendlock);
	if (ret < 0)
	{
		syslog(LOG_ERR, "%d, %s\n", __LINE__, __func__);
		return ret;
	}

	for (cur = dq_peek(&priv->pendq); cur; cur = dq_next(cur))
	{
		apb = (struct ap_buffer_s *)cur;
		remain += apb->nbytes - apb->curbyte;
	}

	nxmutex_unlock(&priv->pendlock);
	*latency += remain / (priv->bpsamp / 8 * priv->nchannels);

	return ret;
}


/****************************************************************************
 * Name: sunxi_audio_setvolume
 *
 * Description:
 *   Set the DACL and DACR volumes values in the audiocodec device based on the
 *   current volume settings.
 *
 ****************************************************************************/

#ifndef CONFIG_AUDIO_EXCLUDE_VOLUME
static void
sunxi_audio_setvolume(FAR struct sunxi_dev_s *priv, uint16_t volume)
{
	//char *card_name = "audiocodec";
	int numid = 6;      /* DACL digital volume control ID */
	int ret = snd_ctl_set_bynum(CONFIG_AW_AUDIO_CODEC_DEFAULT_CARDNAME, numid, volume);
	if (ret < 0)
		syslog(LOG_INFO, "set volume wrong\n");

	numid = 7;      /* DACR digital volume control ID */
	ret = snd_ctl_set_bynum(CONFIG_AW_AUDIO_CODEC_DEFAULT_CARDNAME, numid, volume);
	if (ret < 0)
		syslog(LOG_INFO, "set volume wrong\n");

	syslog(LOG_INFO,"volume=%u\n", volume);
#warning Missing logic
}

#endif /* CONFIG_AUDIO_EXCLUDE_VOLUME */


/****************************************************************************
 * Name: sunxi_audio_getcaps
 *
 * Description:
 *   Get the audio device capabilities
 *
 ****************************************************************************/

static int sunxi_audio_getcaps(FAR struct audio_lowerhalf_s *dev, int type,
							FAR struct audio_caps_s *caps)
{
	/* Validate the structure */
	DEBUGASSERT(caps && caps->ac_len >= sizeof(struct audio_caps_s));
	syslog(LOG_INFO,"type=%d ac_type=%d\n", type, caps->ac_type);

	struct sunxi_dev_s *priv = (struct sunxi_dev_s *)dev;
	uint16_t *ptr;

	/* Fill in the caller's structure based on requested info */

	caps->ac_format.hw  = 0;
	caps->ac_controls.w = 0;

	switch (caps->ac_type)
	{
		/* Caller is querying for the types of units we support */

		case AUDIO_TYPE_QUERY:

		/* Provide our overall capabilities.  The interfacing software
			* must then call us back for specific info for each capability.
			*/

		caps->ac_channels = 2;

		switch (caps->ac_subtype)
			{
			case AUDIO_TYPE_QUERY:

				/* The types of audio units we implement */

				caps->ac_controls.b[0] = (priv->record ?
										AUDIO_TYPE_INPUT :
										AUDIO_TYPE_OUTPUT);

				caps->ac_format.hw = (1 << (AUDIO_FMT_PCM - 1));

				break;
			case AUDIO_FMT_PCM:
				caps->ac_controls.b[0] = AUDIO_SUBFMT_PCM_S16_LE;
				caps->ac_controls.b[1] = AUDIO_SUBFMT_END;
				break;
			default:
				caps->ac_controls.b[0] = AUDIO_SUBFMT_END;
				break;
			}

		break;

		/* Provide capabilities of our INPUT unit */

		case AUDIO_TYPE_OUTPUT:
		case AUDIO_TYPE_INPUT:

			caps->ac_channels = 2;

		switch (caps->ac_subtype)
			{
			case AUDIO_TYPE_QUERY:

				/* Report the Sample rates we support */
				ptr = (uint16_t *)caps->ac_controls.b;
				*ptr =
				AUDIO_SAMP_RATE_8K | AUDIO_SAMP_RATE_16K | AUDIO_SAMP_RATE_24K |
				AUDIO_SAMP_RATE_32K | AUDIO_SAMP_RATE_44K |
				AUDIO_SAMP_RATE_48K;
				break;
			default:
				break;
			}

		break;

		/* All others we don't support */

		default:

		/* Zero out the fields to indicate no support */

		caps->ac_subtype = 0;
		caps->ac_channels = 0;

		break;
	}

	/* Return the length of the audio_caps_s struct for validation of
	* proper Audio device type.
	*/

	return caps->ac_len;
}

/****************************************************************************
 * Name: sunxi_audio_configure
 *
 * Description:
 *   Configure the audio device for the specified  mode of operation.
 *
 ****************************************************************************/

#ifdef CONFIG_AUDIO_MULTI_SESSION
static int
sunxi_audio_configure(FAR struct audio_lowerhalf_s *dev,
					FAR void *session, FAR const struct audio_caps_s *caps)
#else
static int
sunxi_audio_configure(FAR struct audio_lowerhalf_s *dev,
					FAR const struct audio_caps_s *caps)
#endif
{
	FAR struct sunxi_dev_s *priv = (FAR struct sunxi_dev_s *)dev;
	int ret = OK;
	unsigned long period_size;
	unsigned long buffer_size;

	if (priv->index == AW_AUDIO_CODEC) {
		buffer_size = CONFIG_AW_AUDIO_CODEC_BUFFER_SIZE;
		period_size = CONFIG_AW_AUDIO_CODEC_BUFFER_SIZE / CONFIG_AW_AUDIO_CODEC_NUM_BUFFERS;
	} else if (priv->index == AW_AUDIO_DMIC) {
		buffer_size = CONFIG_AW_AUDIO_DMIC_BUFFER_SIZE;
		period_size = CONFIG_AW_AUDIO_DMIC_BUFFER_SIZE / CONFIG_AW_AUDIO_DMIC_NUM_BUFFERS;
	} else {
		ret = -EINVAL;
		syslog(LOG_ERR, "index: %d err\n", priv->index);
		return ret;
	}

	DEBUGASSERT(priv != NULL && caps != NULL);
	syslog(LOG_INFO,"ac_type: %d\n", caps->ac_type);

	/* Process the configure operation */

	switch (caps->ac_type)
	{
	case AUDIO_TYPE_FEATURE:
		syslog(LOG_INFO,"  AUDIO_TYPE_FEATURE\n");

		/* Process based on Feature Unit */

		switch (caps->ac_format.hw)
		{
#ifndef CONFIG_AUDIO_EXCLUDE_VOLUME
		case AUDIO_FU_VOLUME:
			{
			/* Set the volume */

			uint16_t volume = caps->ac_controls.hw[0]/10;
			sunxi_audio_setvolume(priv, volume);
			}
			break;
#endif /* CONFIG_AUDIO_EXCLUDE_VOLUME */

		default:
			syslog(LOG_ERR, "ERROR: Unrecognized feature unit\n");
			ret = -ENOTTY;
			break;
		}
		break;

	case AUDIO_TYPE_INPUT:
		syslog(LOG_INFO,"  AUDIO_TYPE_INPUT:\n");
		syslog(LOG_INFO,"    Number of channels: %u\n", caps->ac_channels);
		syslog(LOG_INFO,"    Sample rate:        %u\n", caps->ac_controls.hw[0]);
		syslog(LOG_INFO,"    Sample width:       %u\n", caps->ac_controls.b[2]);

	case AUDIO_TYPE_OUTPUT:
		{

		/* Verify that all of the requested values are supported */

		ret = -ERANGE;
		if (caps->ac_channels < 0 || caps->ac_channels > 3)
			{
			syslog(LOG_ERR, "ERROR: Unsupported number of channels: %d\n",
					caps->ac_channels);
			break;
			}

		if (caps->ac_controls.b[2] != 16 && caps->ac_controls.b[2] != 32)
			{
			syslog(LOG_ERR, "ERROR: Unsupported bits per sample: %d\n",
					caps->ac_controls.b[2]);
			break;
			}

		/* Save the current stream configuration */

		priv->samprate  = caps->ac_controls.hw[0];
		priv->nchannels = caps->ac_channels;
		priv->bpsamp    = caps->ac_controls.b[2];
		priv->buffer_size = buffer_size;
		priv->period_size  = period_size;

		/* Save configure, and then call set_param to support the resulting number or channels,
		* bits per sample, and bitrate.
		*/

		switch (priv->samprate) {
			case 8000:
			case 16000:
			case 24000:
			case 32000:
			case 44100:
			case 48000:
				break;
			default:
				syslog(LOG_ERR, "%u samprate not supprot\n", priv->samprate);
				ret = -EINVAL;
				return ret;
			}

		switch (priv->nchannels) {
			case 1:
			case 2:
			case 3:
				break;
			default:
				syslog(LOG_ERR, "%u channels not supprot\n", priv->nchannels);
				ret = -EINVAL;
				return ret;
			}

		switch (priv->bpsamp) {
			case 16:
				priv->format = SND_PCM_FORMAT_S16_LE;
				break;
			case 24:
				priv->format = SND_PCM_FORMAT_S24_LE;
				break;
			case 32:
				priv->format = SND_PCM_FORMAT_S32_LE;
				break;
			default:
				syslog(LOG_ERR, "%u bits not supprot\n", priv->bpsamp);
				ret = -EINVAL;
				return ret;
			}

#if (defined CONFIG_AW_AUDIO_RPMSG_CTRL)
		if (priv->record)
		{
			rpmsg_audio_ctrl_t audio_rpmsg_ctrl_param;
			memset(&audio_rpmsg_ctrl_param, 0 , sizeof(rpmsg_audio_ctrl_t));
			audio_rpmsg_ctrl_param.cmd = RPMSG_CTL_CONFIG;
			audio_rpmsg_ctrl_param.rate = caps->ac_controls.hw[0];
			audio_rpmsg_ctrl_param.channels = caps->ac_channels;
			audio_rpmsg_ctrl_param.bits = caps->ac_controls.b[2];
			ret = audio_rpmsg_ctl_send_cmd(RPMSG_DIR_NAME, &audio_rpmsg_ctrl_param);
			if (ret != 0)
			  {
				syslog(LOG_ERR, "rpmsg send err\n");
				return ret;
			  }
		}
#endif
#if 0
		int hw_ver = 0;
		char eq_file[64]={0};
		hw_ver = hw_version_get();

		syslog(LOG_INFO, "hw_ver = dvt%d\n", hw_ver);

		if (hw_ver == 1)
			init_eq_prms(EQ_MAX_BIN_NUM, &priv->eqprms, EQ_CONFIG_FILE_AD1, &ret);
		else if (hw_ver == 2)
			init_eq_prms(EQ_MAX_BIN_NUM, &priv->eqprms, EQ_CONFIG_FILE_ACM, &ret);
		else
			init_eq_prms(EQ_MAX_BIN_NUM, &priv->eqprms, EQ_CONFIG_FILE_AD2, &ret);
		if (ret) {
			priv->eq_enable = true;

			if (priv->samprate != priv->eqprms.sampling_rate||
				priv->nchannels != priv->eqprms.chan||
				priv->bpsamp != 16) {
				syslog(LOG_INFO, "samprate and chan [%u, %u, %u] no match eq file [%u, %u, 16],\
						deinit eq paramter\n", priv->samprate, priv->nchannels,
						priv->bpsamp, priv->eqprms.sampling_rate, priv->eqprms.chan);
				deinit_eq_prms(&priv->eqprms);
				priv->eq_enable = false;
			}
		} else {
			syslog(LOG_INFO, "eq config file enabled is 0, deinit eq paramter\n");
			deinit_eq_prms(&priv->eqprms);
			priv->eq_enable = false;
		}
#endif
		ret = OK;
		}
		break;

	default:
			syslog(LOG_ERR, "%d, %s\n", __LINE__, __func__);
			ret = -ENOTTY;
		break;
	}

	return ret;
}

/****************************************************************************
 * Name: sunxi_audio_shutdown
 *
 * Description:
 *   Shutdown the audio device and put it in the lowest power state possible.
 *
 ****************************************************************************/

static int sunxi_audio_shutdown(FAR struct audio_lowerhalf_s *dev)
{
	FAR struct sunxi_dev_s *priv = (FAR struct sunxi_dev_s *)dev;

	DEBUGASSERT(priv);

	/* Now issue a software reset. This puts all audio registers back in
	* their default state.
	*/

	sunxi_audio_reset(priv);
	return OK;
}

/****************************************************************************
 * Name: record_fillonebuffer
 *
 * Description:
 *   Start the receive one audio buffer from the record.  This
 *   will not wait for the transfer to complete but will return immediately.
 *   the wmd8904_senddone called will be invoked when the transfer
 *   completes, stimulating the worker thread to call this function again.
 *
 ****************************************************************************/

static int record_fillonebuffer(FAR struct sunxi_dev_s *priv)
{
	FAR struct ap_buffer_s *apb;
	int ret = 0;

	if (dq_peek(&priv->pendq) != NULL)
	{
		/* Take next buffer from the queue of pending transfers */

		apb = (FAR struct ap_buffer_s *)dq_remfirst(&priv->pendq);

		audinfo("filling: apb=%p curbyte=%d nbytes=%d flags=%04x\n",
				apb, apb->curbyte, apb->nbytes, apb->flags);

		/* Get the one audio buffer from record.
		*/
#if (defined CONFIG_AW_AUDIO_RPMSG_CTRL)

#endif

		if (priv->handle)
		{
			unsigned int frame_bytes = snd_vela_pcm_frames_to_bytes(priv->handle, 1);

			long f = snd_vela_pcm_bytes_to_frames(priv->handle, apb->nbytes);

			f = pcm_read(priv->handle, (char *)apb->samp, f, frame_bytes);
			if (f < 0)
			{
				syslog(LOG_ERR, "pcm read error, return %ld\n", f);
				return f;
			}
		}

#ifdef CONFIG_AUDIO_MULTI_SESSION
		priv->dev.upper(priv->dev.priv, AUDIO_CALLBACK_DEQUEUE, apb, OK, NULL);
#else
		priv->dev.upper(priv->dev.priv, AUDIO_CALLBACK_DEQUEUE, apb, OK);
#endif

	}

	return ret;
}

/****************************************************************************
 * Name: record_fillbuffer
 *
 * Description:
 *   Start the receive an audio buffer from the record.  This
 *   will not wait for the transfer to complete but will return immediately.
 *   the wmd8904_senddone called will be invoked when the transfer
 *   completes, stimulating the worker thread to call this function again.
 *
 ****************************************************************************/

static int record_fillbuffer(FAR struct sunxi_dev_s *priv)
{
	FAR struct ap_buffer_s *apb;
	int ret = 0;
	unsigned int frame_bytes = 0;

	/* Loop while there are audio buffers to be sent and we have few than
	* CONFIG_AW_AUDIO_DMIC_INFLIGHT then "in-flight"
	*
	* The 'inflight' value might be modified from the interrupt level in some
	* implementations.  We will use interrupt controls to protect against
	* that possibility.
	*
	* The 'pendq', on the other hand, is protected via a semaphore.  Let's
	* hold the semaphore while we are busy here and disable the interrupts
	* only while accessing 'inflight'.
	*/

	if (!priv->running)
		return ret;

	ret = nxmutex_lock(&priv->pendlock);

	if (priv->handle)
		frame_bytes = snd_vela_pcm_frames_to_bytes(priv->handle, 1);

	while (dq_peek(&priv->pendq) != NULL)
	{
		/* Take next buffer from the queue of pending transfers */

		apb = (FAR struct ap_buffer_s *)dq_remfirst(&priv->pendq);

		syslog(LOG_INFO,"filling: apb=%p curbyte=%d nbytes=%d flags=%04x\n",
				apb, apb->curbyte, apb->nbytes, apb->flags);


		/* Get the entire audio buffer from record.
		*
		* frame_bits :
		*   = sample_bits * nchannels
		* frames_total (samples):
		*   = nbytes * 8 / frame_bits;
		* frame_bytes one frame byte:
		*   = 1 * frame_bits / 8;
		*
		*/
#if (defined CONFIG_AW_AUDIO_RPMSG_CTRL)

#endif

		if (priv->handle)
		{
			long f = snd_vela_pcm_bytes_to_frames(priv->handle, apb->nbytes);

			f = pcm_read(priv->handle, (char *)apb->samp, f, frame_bytes);
			if (f < 0)
			{
				syslog(LOG_ERR, "pcm read error, return %ld\n", f);
				nxmutex_unlock(&priv->pendlock);
				return f;
			}
		}

#ifdef CONFIG_AUDIO_MULTI_SESSION
		priv->dev.upper(priv->dev.priv, AUDIO_CALLBACK_DEQUEUE, apb, OK, NULL);
#else
		priv->dev.upper(priv->dev.priv, AUDIO_CALLBACK_DEQUEUE, apb, OK);
#endif

	}

	nxmutex_unlock(&priv->pendlock);
	return ret;
}

#if !(defined CONFIG_AW_AUDIO_RPMSG_CTRL)
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
#endif

/****************************************************************************
 * Name: sunxi_audio_start
 *
 * Description:
 *   Start the configured operation (audio streaming, volume enabled, etc.).
 *
 ****************************************************************************/

#ifdef CONFIG_AUDIO_MULTI_SESSION
static int sunxi_audio_start(FAR struct audio_lowerhalf_s *dev,
			FAR void *session)
#else
static int sunxi_audio_start(FAR struct audio_lowerhalf_s *dev)
#endif
{
	FAR struct sunxi_dev_s *priv = (FAR struct sunxi_dev_s *)dev;
	struct mq_attr attr;
	int ret = -1;
	unsigned int stack_size;

	if (priv->index == AW_AUDIO_CODEC)
		stack_size = CONFIG_AW_AUDIO_CODEC_WORKER_STACKSIZE;
	else if (priv->index == AW_AUDIO_DMIC)
		stack_size = CONFIG_AW_AUDIO_DMIC_WORKER_STACKSIZE;
	else
	{
		ret = -EINVAL;
		syslog(LOG_ERR, "index: %d err\n", priv->index);
		return ret;
	}

	syslog(LOG_INFO, "sunxi_audio_start index:%d\n", priv->index);

	/* Create a message queue for the worker thread */

	snprintf(priv->mqname, sizeof(priv->mqname), "/tmp/%" PRIXPTR,
			(uintptr_t)priv);

	attr.mq_maxmsg  = 16;
	attr.mq_msgsize = sizeof(struct audio_msg_s);
	attr.mq_curmsgs = 0;
	attr.mq_flags   = 0;

	ret = file_mq_open(&priv->mq, priv->mqname,
						O_RDWR | O_CREAT | O_NONBLOCK, 0644, &attr);
	if (ret < 0)
	{
		/* Error creating message queue! */

		syslog(LOG_ERR, "ERROR: Couldn't allocate message queue\n");
		return ret;
	}

#if (defined CONFIG_AW_AUDIO_RPMSG_CTRL)
	if (priv->record)
	{
		rpmsg_audio_ctrl_t audio_rpmsg_ctrl_param;
		memset(&audio_rpmsg_ctrl_param, 0 , sizeof(rpmsg_audio_ctrl_t));
		audio_rpmsg_ctrl_param.cmd = RPMSG_CTL_START;
		ret = audio_rpmsg_ctl_send_cmd(RPMSG_DIR_NAME, &audio_rpmsg_ctrl_param);
		if (ret != 0)
		{
			syslog(LOG_ERR, "rpmsg send err\n");
			return ret;
		}
	}

#else

	if (priv->record)
	{
		if (priv->nchannels == 1)
		{
			snd_ctl_set(CONFIG_AW_AUDIO_CODEC_DEFAULT_CARDNAME, "MIC1 input switch", 1);
			snd_ctl_set(CONFIG_AW_AUDIO_CODEC_DEFAULT_CARDNAME, "ADC1_2 digital volume switch", 1);
			snd_ctl_set(CONFIG_AW_AUDIO_CODEC_DEFAULT_CARDNAME, "ADC1 digital volume", CONFIG_AW_AUDIO_CODEC_ADC_INITVOLUME);
		}
		else if (priv->nchannels == 2)
		{
			snd_ctl_set(CONFIG_AW_AUDIO_CODEC_DEFAULT_CARDNAME, "MIC1 input switch", 1);
			snd_ctl_set(CONFIG_AW_AUDIO_CODEC_DEFAULT_CARDNAME, "MIC2 input switch", 1);
			snd_ctl_set(CONFIG_AW_AUDIO_CODEC_DEFAULT_CARDNAME, "ADC1_2 digital volume switch", 1);
			snd_ctl_set(CONFIG_AW_AUDIO_CODEC_DEFAULT_CARDNAME, "ADC1 digital volume", CONFIG_AW_AUDIO_CODEC_ADC_INITVOLUME);
			snd_ctl_set(CONFIG_AW_AUDIO_CODEC_DEFAULT_CARDNAME, "ADC2 digital volume", CONFIG_AW_AUDIO_CODEC_ADC_INITVOLUME);
		}
		else if (priv->nchannels == 3)
		{
			snd_ctl_set(CONFIG_AW_AUDIO_CODEC_DEFAULT_CARDNAME, "MIC1 input switch", 1);
			snd_ctl_set(CONFIG_AW_AUDIO_CODEC_DEFAULT_CARDNAME, "MIC2 input switch", 1);
			snd_ctl_set(CONFIG_AW_AUDIO_CODEC_DEFAULT_CARDNAME, "MIC3 input switch", 1);
			snd_ctl_set(CONFIG_AW_AUDIO_CODEC_DEFAULT_CARDNAME, "ADC1_2 digital volume switch", 1);
			snd_ctl_set(CONFIG_AW_AUDIO_CODEC_DEFAULT_CARDNAME, "ADC3 digital volume switch", 1);
			snd_ctl_set(CONFIG_AW_AUDIO_CODEC_DEFAULT_CARDNAME, "ADC1 digital volume", CONFIG_AW_AUDIO_CODEC_ADC_INITVOLUME);
			snd_ctl_set(CONFIG_AW_AUDIO_CODEC_DEFAULT_CARDNAME, "ADC2 digital volume", CONFIG_AW_AUDIO_CODEC_ADC_INITVOLUME);
			snd_ctl_set(CONFIG_AW_AUDIO_CODEC_DEFAULT_CARDNAME, "ADC3 digital volume", CONFIG_AW_AUDIO_CODEC_ADC_INITVOLUME);
		}
		else
		{
			syslog(LOG_ERR, "channel %d\n", priv->nchannels);
			ret = -EINVAL;
			return ret;
		}
	}

#endif

	pthread_mutex_init(&priv->mutex, NULL);
	pthread_cond_init(&priv->cond_pause, NULL);
	sem_init(&priv->start_handle, 0, 0);

	/* Start our thread for sending data to the device */

	syslog(LOG_INFO,"Starting worker thread\n");

	if (priv->record)
		priv->task = audio_thread_create(&priv->sem, record_workerthread, priv, "sunxi-audio-1",
							stack_size, (sched_get_priority_max(SCHED_FIFO) - 3));
	else
		priv->task = audio_thread_create(&priv->sem, play_workerthread, priv, "sunxi-audio-0",
							stack_size, (sched_get_priority_max(SCHED_FIFO) - 3));
	if (!priv->task)
	{
		syslog(LOG_ERR, "ERROR: pthread_create failed: %d\n", ret);
	}
	else
	{
		hal_thread_start(priv->task);
		syslog(LOG_INFO,"Created worker thread\n");
	}

	ret = nxsem_tickwait_uninterruptible(&priv->start_handle, MS_TO_OSTICK(1000));
	if (ret != OK) {
		syslog(LOG_ERR, "wait bind start_handle timeout!\n");
		return ret;
	}

	return ret;

}

/****************************************************************************
 * Name: sunxi_audio_stop
 *
 * Description:
 *   Stop the configured operation (audio streaming, volume disabled, etc.).
 *
 ****************************************************************************/

#ifndef CONFIG_AUDIO_EXCLUDE_STOP
#  ifdef CONFIG_AUDIO_MULTI_SESSION
static int sunxi_audio_stop(FAR struct audio_lowerhalf_s *dev, FAR void *session)
#  else
static int sunxi_audio_stop(FAR struct audio_lowerhalf_s *dev)
#  endif
{
	FAR struct sunxi_dev_s *priv = (FAR struct sunxi_dev_s *)dev;
	struct audio_msg_s term_msg;
	int ret = 0;
	unsigned int prio;
	syslog(LOG_INFO, "%d, %s\n", __LINE__, __func__);

	if (priv->index == AW_AUDIO_CODEC)
		prio = CONFIG_AW_AUDIO_CODEC_MSG_PRIO;
	else if (priv->index == AW_AUDIO_DMIC)
		prio = CONFIG_AW_AUDIO_DMIC_MSG_PRIO;
	else {
		ret = -EINVAL;
		syslog(LOG_ERR, "index: %d err\n", priv->index);
		return ret;
	}

	/* Send a message to stop all audio streaming */
	term_msg.msg_id = AUDIO_MSG_STOP;
	term_msg.u.data = 0;
	priv->running = false;
	//priv->paused = false;

	pthread_cond_signal(&priv->cond_pause);
	file_mq_send(&priv->mq, (FAR const char *)&term_msg, sizeof(term_msg),
				prio);

#if (defined CONFIG_AW_AUDIO_RPMSG_CTRL)
	if (priv->record)
	{
		rpmsg_audio_ctrl_t audio_rpmsg_ctrl_param;
		memset(&audio_rpmsg_ctrl_param, 0 , sizeof(rpmsg_audio_ctrl_t));
		audio_rpmsg_ctrl_param.cmd = RPMSG_CTL_STOP;
		ret = audio_rpmsg_ctl_send_cmd(RPMSG_DIR_NAME, &audio_rpmsg_ctrl_param);
		if (ret != 0)
		{
			syslog(LOG_ERR, "rpmsg send err\n");
			return ret;
		}
	}
#endif

	if (priv->handle != NULL) {
		if (priv->paused) {
			ret = snd_vela_pcm_reset(priv->handle);
			if (ret < 0)
				syslog(LOG_ERR, "%d, reset failed!, return %d\n", __LINE__, ret);
		}
		priv->paused = false;

		ret = snd_vela_pcm_drain(priv->handle);
		if (ret < 0)
		{
			syslog(LOG_ERR, "stop failed!, return %d\n", ret);
		}

		audio_thread_stop(&priv->sem, priv->task);
		/* close card */
		ret = snd_vela_pcm_close(priv->handle);
		if (ret < 0)
		{
			syslog(LOG_ERR, "audio close error:%d\n", ret);
			return ret;
		}
		priv->handle = NULL;
	} else {
		audio_thread_stop(&priv->sem, priv->task);
	}

	pthread_mutex_destroy(&priv->mutex);
	pthread_cond_destroy(&priv->cond_pause);
	sem_destroy(&priv->start_handle);

	/* REVISIT: */
	return OK;
}
#endif

/****************************************************************************
 * Name: sunxi_audio_pause
 *
 * Description:
 *   Pauses the playback.
 *
 ****************************************************************************/

#ifndef CONFIG_AUDIO_EXCLUDE_PAUSE_RESUME
#  ifdef CONFIG_AUDIO_MULTI_SESSION
static int sunxi_audio_pause(FAR struct audio_lowerhalf_s *dev,
							FAR void *session)
#  else
static int sunxi_audio_pause(FAR struct audio_lowerhalf_s *dev)
#  endif
{
	FAR struct sunxi_dev_s *priv = (FAR struct sunxi_dev_s *)dev;
	int ret = 0;

	if (priv->running && !priv->paused)
	{
		priv->paused = true;

#if (defined CONFIG_AW_AUDIO_RPMSG_CTRL)

#endif
		if (priv->handle)
		{
			ret = snd_vela_pcm_pause(priv->handle, 1);
			if (ret < 0)
				syslog(LOG_ERR, "pause failed!, return %d\n", ret);
		}

	}

	return OK;
}
#endif /* CONFIG_AUDIO_EXCLUDE_PAUSE_RESUME */

/****************************************************************************
 * Name: sunxi_audio_resume
 *
 * Description:
 *   Resumes the playback.
 *
 ****************************************************************************/

#ifndef CONFIG_AUDIO_EXCLUDE_PAUSE_RESUME
#  ifdef CONFIG_AUDIO_MULTI_SESSION
static int sunxi_audio_resume(FAR struct audio_lowerhalf_s *dev,
							FAR void *session)
#  else
static int sunxi_audio_resume(FAR struct audio_lowerhalf_s *dev)
#  endif
{
	FAR struct sunxi_dev_s *priv = (FAR struct sunxi_dev_s *)dev;
	int ret = 0;
	if (priv->running && priv->paused)
	{
		priv->paused = false;

#if (defined CONFIG_AW_AUDIO_RPMSG_CTRL)

#endif

		if (priv->handle)
		{
			ret = snd_vela_pcm_reset(priv->handle);
			if (ret < 0)
				syslog(LOG_ERR, "%d, reset failed!, return %d\n", __LINE__, ret);

			ret = snd_vela_pcm_pause(priv->handle, 0);
			if (ret < 0)
				syslog(LOG_ERR, "pause failed!, return %d\n", ret);
		}

		pthread_cond_signal(&priv->cond_pause);

	}

	return OK;
}
#endif /* CONFIG_AUDIO_EXCLUDE_PAUSE_RESUME */

/****************************************************************************
 * Name: sunxi_audio_enqueuebuffer
 *
 * Description:
 *   Enqueue an Audio Pipeline Buffer for capture/ processing.
 *
 ****************************************************************************/

static int sunxi_audio_enqueuebuffer(FAR struct audio_lowerhalf_s *dev,
									FAR struct ap_buffer_s *apb)
{
	FAR struct sunxi_dev_s *priv = (FAR struct sunxi_dev_s *)dev;
	struct audio_msg_s term_msg;
	int ret;
	unsigned int prio;

	if (priv->index == AW_AUDIO_CODEC)
		prio = CONFIG_AW_AUDIO_CODEC_MSG_PRIO;
	else if (priv->index == AW_AUDIO_DMIC)
		prio = CONFIG_AW_AUDIO_DMIC_MSG_PRIO;
	else
	{
		ret = -EINVAL;
		syslog(LOG_ERR, "index: %d err\n", priv->index);
		return ret;
	}

	audinfo("Enqueueing: apb=%p curbyte=%d nbytes=%d flags=%04x\n",
			apb, apb->curbyte, apb->nbytes, apb->flags);

	ret = nxmutex_lock(&priv->pendlock);
	if (ret < 0)
	{
		syslog(LOG_INFO, "%d, %s\n", __LINE__, __func__);
		return ret;
	}

	/* Add the new buffer to the tail of pending audio buffers */
	dq_addlast(&apb->dq_entry, &priv->pendq);
	nxmutex_unlock(&priv->pendlock);

	/* Send a message to the worker thread indicating that a new buffer has
	* been enqueued.If mq is NULL, then the playing has not yet started.
	* In that case we are just "priming the pump" and we don't need to send
	* any message.
	*/

	ret = OK;
	if (priv->mq.f_inode != NULL)
	{
		term_msg.msg_id = AUDIO_MSG_ENQUEUE;
		term_msg.u.data = 0;

		ret = file_mq_send(&priv->mq, (FAR const char *)&term_msg,
							sizeof(term_msg), prio);
		if (ret < 0)
		{
			syslog(LOG_ERR, "ERROR: file_mq_send failed: %d\n", ret);
		}
	}

	return ret;
}

/****************************************************************************
 * Name: sunxi_audio_ioctl
 *
 * Description:
 *   Perform a device ioctl
 *
 ****************************************************************************/

static int sunxi_audio_ioctl(FAR struct audio_lowerhalf_s *dev, int cmd,
							unsigned long arg)
{
	int ret = OK;
#ifdef CONFIG_AUDIO_DRIVER_SPECIFIC_BUFFERS
	FAR struct sunxi_dev_s *priv = (FAR struct sunxi_dev_s *)dev;
#endif

	/* Deal with ioctls passed from the upper-half driver */

	switch (cmd)
	{
		/* Provide cache frames for upper level applications */
		case AUDIOIOC_GETLATENCY:
		{
			ret = sunxi_get_latency(dev, arg);
			if (ret < 0)
			{
				return ret;
			}
		}
		break;

		/* Check for AUDIOIOC_HWRESET ioctl. This ioctl is passed straight
		* through from the upper-half audio driver.
		*/

		case AUDIOIOC_HWRESET:
		{
			/* REVISIT:  Should we completely re-initialize the chip?   We
			* can't just issue a software reset; that would puts all WM8904
			* registers back in their default state.
			*/

			syslog(LOG_INFO,"AUDIOIOC_HWRESET:\n");
		}
		break;

#ifdef CONFIG_AUDIO_DRIVER_SPECIFIC_BUFFERS
		/* Set driver buffer size and quantity */
		case AUDIOIOC_SETBUFFERINFO:
		{
			syslog(LOG_INFO,"AUDIOIOC_SETBUFFERINFO:\n");
			FAR struct ap_buffer_info_s *bufinfo = (FAR struct ap_buffer_info_s *)arg;
			priv->period_size = bufinfo->buffer_size / (priv->bpsamp / 8 * priv->nchannels);
			priv->buffer_size = priv->period_size * bufinfo->nbuffers;
		}
		break;

		/* Report our preferred buffer size and quantity */
		case AUDIOIOC_GETBUFFERINFO:
		{
			syslog(LOG_INFO,"AUDIOIOC_GETBUFFERINFO:\n");
			FAR struct ap_buffer_info_s *bufinfo = (FAR struct ap_buffer_info_s *)arg;
			bufinfo->buffer_size = priv->period_size * (priv->bpsamp / 8 * priv->nchannels);
			bufinfo->nbuffers    = priv->buffer_size / priv->period_size;
		}
		break;
#endif

		default:
		ret = -ENOTTY;
		syslog(LOG_INFO,"Ignored\n");
		break;
	}

	return ret;
}

/****************************************************************************
 * Name: sunxi_audio_reserve
 *
 * Description:
 *   Reserves a session (the only one we have).
 *
 ****************************************************************************/

#ifdef CONFIG_AUDIO_MULTI_SESSION
static int
sunxi_audio_reserve(FAR struct audio_lowerhalf_s *dev, FAR void **session)
#else
static int sunxi_audio_reserve(FAR struct audio_lowerhalf_s *dev)
#endif
{
	return OK;
}

/****************************************************************************
 * Name: sunxi_audio_release
 *
 * Description:
 *   Releases the session (the only one we have).
 *
 ****************************************************************************/

#ifdef CONFIG_AUDIO_MULTI_SESSION
static int sunxi_audio_release(FAR struct audio_lowerhalf_s *dev,
							FAR void *session)
#else
static int sunxi_audio_release(FAR struct audio_lowerhalf_s *dev)
#endif
{
	return OK;
}

/****************************************************************************
 * Name: record_workerthread
 *
 *  This is the thread that gets data from the chip and keeps the audio
 *  stream going.
 *
 ****************************************************************************/

static void record_workerthread(void *arg)
{
	FAR struct sunxi_dev_s *priv = (struct sunxi_dev_s *)arg;
	struct audio_msg_s msg;
	FAR struct ap_buffer_s *apb;
	int msglen;
	unsigned int prio;

	syslog(LOG_INFO,"Entry\n");

#ifndef CONFIG_AUDIO_EXCLUDE_STOP
	priv->terminating = false;
#endif

/* Mark ourself as running */
	priv->running = true;

#if (!defined CONFIG_AW_AUDIO_RPMSG_CTRL)

	int direction, ret;

	direction = priv->record ? SND_VELA_PCM_STREAM_CAPTURE
								: SND_VELA_PCM_STREAM_PLAYBACK;

	/* open card */
	ret = snd_vela_pcm_open(&priv->handle, priv->pcm_name, direction, 0);
	if (ret < 0)
	{
		syslog(LOG_ERR, "audio open error:%d\n", ret);
		return;
	}

	ret = set_param(priv->handle, priv->format, priv->samprate, priv->nchannels,
			priv->period_size, priv->buffer_size);
	if (ret < 0)
	{
		syslog(LOG_ERR, "set_param error:%d\n", ret);
		priv->running = false;
	}
	sem_post(&priv->start_handle);

#endif
	/* Loop as long as we are supposed to be running and as long as we have
	* buffers in-flight.
	*/
	record_fillbuffer(priv);

	while (priv->running)
	{
		/* Check if we have been asked to terminate.  We have to check if we
		* still have buffers in-flight.  If we do, then we can't stop until
		* birds come back to roost.
		*/

		if (priv->terminating)
		{
			/* We are IDLE.  Break out of the loop and exit. */

			break;
		}

		if (priv->paused)
		{
			/* We are pause.. */
			pthread_mutex_lock(&priv->mutex);
			pthread_cond_wait(&priv->cond_pause, &priv->mutex);
			pthread_mutex_unlock(&priv->mutex);
			if (!priv->running)
				break;
		}

		/* Wait for messages from our message queue */

		msglen = file_mq_receive(&priv->mq, (FAR char *)&msg,
								sizeof(msg), &prio);

		/* Handle the case when we return with no message */

		if (msglen < sizeof(struct audio_msg_s))
		{
			syslog(LOG_ERR, "ERROR: Message too small: %d\n", msglen);
			continue;
		}

		/* Process the message */

		switch (msg.msg_id)
		{
			/* Stop the playback */

#ifndef CONFIG_AUDIO_EXCLUDE_STOP
			case AUDIO_MSG_STOP:

			/* Indicate that we are terminating */

			syslog(LOG_INFO,"AUDIO_MSG_STOP: Terminating\n");
			priv->terminating = true;
			break;
#endif

			/* We have a new buffer to fill.  We will catch this case at
			* the top of the loop.
			*/

			case AUDIO_MSG_ENQUEUE:
			audinfo("AUDIO_MSG_ENQUEUE\n");
			record_fillonebuffer(priv);
			break;

			case AUDIO_MSG_COMPLETE:
			syslog(LOG_INFO,"AUDIO_MSG_COMPLETE\n");
			break;

			default:
			syslog(LOG_ERR, "ERROR: Ignoring message ID %d\n", msg.msg_id);
			break;
		}
	}

	/* Reset the audio param */

	sunxi_audio_reset(priv);

	/* Return any pending buffers in our pending queue */

	nxmutex_lock(&priv->pendlock);
	while ((apb = (FAR struct ap_buffer_s *)dq_remfirst(&priv->pendq)) != NULL)
	{

		/* Send the buffer back up to the previous level. */

#ifdef CONFIG_AUDIO_MULTI_SESSION
		priv->dev.upper(priv->dev.priv, AUDIO_CALLBACK_DEQUEUE, apb, OK, NULL);
#else
		priv->dev.upper(priv->dev.priv, AUDIO_CALLBACK_DEQUEUE, apb, OK);
#endif
	}

	nxmutex_unlock(&priv->pendlock);

	/* Close the message queue */

	file_mq_close(&priv->mq);
	file_mq_unlink(priv->mqname);

	/* Send an AUDIO_MSG_COMPLETE message to the client */

#ifdef CONFIG_AUDIO_MULTI_SESSION
	priv->dev.upper(priv->dev.priv, AUDIO_CALLBACK_COMPLETE, NULL, OK, NULL);
#else
	priv->dev.upper(priv->dev.priv, AUDIO_CALLBACK_COMPLETE, NULL, OK);
#endif

	syslog(LOG_INFO, "A7 record exit\n");
	sem_post(&priv->sem);
}

/****************************************************************************
 * Name: play_workerthread
 *
 *  This is the thread that write data to the chip and keeps the playback
 *  stream going.
 *
 ****************************************************************************/

static void play_workerthread(void *arg)
{
	FAR struct sunxi_dev_s *priv = (struct sunxi_dev_s *)arg;
	struct audio_msg_s msg;
	struct ap_buffer_s *apb;
	unsigned int prio;
	int ret, msglen;
	void *equalizer = NULL;

	snd_pcm_sframes_t samples;
	snd_pcm_uframes_t frames = 0;
	int direction;

#ifndef CONFIG_AUDIO_EXCLUDE_STOP
	priv->terminating = false;
#endif
	priv->running = true;

	direction = priv->record ? SND_VELA_PCM_STREAM_CAPTURE
								: SND_VELA_PCM_STREAM_PLAYBACK;

	/* open card */
	ret = snd_vela_pcm_open(&priv->handle, priv->pcm_name, direction, 0);
	if (ret < 0)
	{
		syslog(LOG_ERR, "audio open error:%d\n", ret);
		return;
	}

	syslog(LOG_INFO,"%s:set param format:%d rate:%d ch:%d period_size:%ld buffer_size:%ld\n", __func__, priv->format,
					priv->samprate, priv->nchannels,priv->period_size, priv->buffer_size);

	ret = set_param(priv->handle, priv->format, priv->samprate, priv->nchannels,
			priv->period_size / 4, priv->buffer_size / 4);
	if (ret < 0)
	{
		syslog(LOG_ERR, "set_param error:%d\n", ret);
		priv->running = false;
	}

	ret = snd_vela_pcm_prepare(priv->handle);
	if (ret < 0) {
		syslog(LOG_INFO, "prepare failed. return %d\n", ret);
	}
	sem_post(&priv->start_handle);

	if (priv->eq_enable) {
		syslog(LOG_INFO, "eq_create\n");
		equalizer = eq_create(&priv->eqprms);
		if (equalizer == NULL)
			syslog(LOG_ERR, "create equalizer handle error!\n");
		deinit_eq_prms(&priv->eqprms);
	}

	while (priv->running) {
		apb = (struct ap_buffer_s *)dq_peek(&priv->pendq);
		if (!apb) {
			usleep(5000);
			continue;
		}
		frames = apb->nbytes / (priv->bpsamp / 8 * priv->nchannels);

		start_before++;
		if (priv->eq_enable && equalizer)
			eq_process(equalizer, (short*)apb->samp, frames);

		samples = snd_vela_pcm_writei(priv->handle, apb->samp, frames);
		if (samples != frames) {
			syslog(LOG_INFO, "snd_vela_pcm_writei return %ld\n", samples);
			if (!priv->handle) {
				syslog(LOG_INFO, "handle is null\n");
				usleep(5000);
				continue;
			}
		}
		if (samples == -EAGAIN) {
			usleep(10000);
			continue;
		} else if ((samples == -EPIPE) && (msg.msg_id == AUDIO_MSG_ENQUEUE)) {
			if (!priv->running) {
				syslog(LOG_INFO, "%d, receive the stop cmd\n", __LINE__);
				break;
			}
			xrun(priv->handle);
			continue;
		} else if ((samples == -EPIPE) && (msg.msg_id == AUDIO_MSG_NONE)) {
			syslog(LOG_INFO, "upper buffer sending is slow\n");
			usleep(5000);
			msglen = file_mq_receive(&priv->mq, (FAR char *)&msg,
								sizeof(msg), &prio);
			continue;
		} else if (samples == -EPIPE) {
			syslog(LOG_INFO, "underrun occured\n");
			continue;
		} else if (samples == -ESTRPIPE) {
			continue;
		} else if ((samples < 0) && (priv->running)) {
			syslog(LOG_INFO, "-----snd_vela_pcm_writei failed!!, return %ld\n", samples);
			ret = snd_vela_pcm_prepare(priv->handle);
			if (ret < 0) {
				syslog(LOG_INFO, "prepare failed in write. return %d\n", ret);
			}
			continue;
		} else if ((samples < 0) && (!priv->running)) {
			syslog(LOG_INFO, "%d, receive the stop cmd\n", __LINE__);
			break;
		}

		nxmutex_lock(&priv->pendlock);
		dq_remfirst(&priv->pendq);
		nxmutex_unlock(&priv->pendlock);

		dq_before++;
		priv->dev.upper(priv->dev.priv, AUDIO_CALLBACK_DEQUEUE, apb, OK);
		dq_after++;

		msg.msg_id = 0;
		msglen = file_mq_receive(&priv->mq, (FAR char *)&msg,
								sizeof(msg), &prio);

		/* Handle the case when we return with no message */

		if (msglen < sizeof(struct audio_msg_s)) {
			syslog(LOG_ERR, "ERROR: Message too small: %d\n", msglen);
			continue;
		}

		switch (msg.msg_id)
		{
			case AUDIO_MSG_ENQUEUE:
			audinfo("AUDIO_MSG_ENQUEUE\n");
			break;
			/* Stop the playback */
#ifndef CONFIG_AUDIO_EXCLUDE_STOP
			case AUDIO_MSG_STOP:
			/* Indicate that we are terminating */

			syslog(LOG_INFO,"AUDIO_MSG_STOP: Terminating\n");
			priv->terminating = true;
			break;
#endif
			/* We have a new buffer to fill.  We will catch this case at
			* the top of the loop.
			*/

			case AUDIO_MSG_COMPLETE:
			syslog(LOG_INFO,"AUDIO_MSG_COMPLETE\n");
			break;

			default:
			syslog(LOG_ERR, "ERROR: Ignoring message ID %d\n", msg.msg_id);
			break;
		}
		if (priv->terminating == true)
			break;
	}

	if (priv->eq_enable && equalizer) {
		syslog(LOG_INFO, "eq_destroy\n");
		eq_destroy(equalizer);
		equalizer = NULL;
	}

	nxmutex_lock(&priv->pendlock);
	while ((apb = (FAR struct ap_buffer_s *)dq_remfirst(&priv->pendq)) != NULL)
	{

		/* Send the buffer back up to the previous level. */

#ifdef CONFIG_AUDIO_MULTI_SESSION
		priv->dev.upper(priv->dev.priv, AUDIO_CALLBACK_DEQUEUE, apb, OK, NULL);
#else
		priv->dev.upper(priv->dev.priv, AUDIO_CALLBACK_DEQUEUE, apb, OK);
#endif
	}
	nxmutex_unlock(&priv->pendlock);

	/* Close the message queue */
	file_mq_close(&priv->mq);
	file_mq_unlink(priv->mqname);

  /* Send an AUDIO_MSG_COMPLETE message to the client */
#ifdef CONFIG_AUDIO_MULTI_SESSION
	priv->dev.upper(priv->dev.priv, AUDIO_CALLBACK_COMPLETE, NULL, OK, NULL);
#else
	priv->dev.upper(priv->dev.priv, AUDIO_CALLBACK_COMPLETE, NULL, OK);
#endif

	syslog(LOG_INFO, "playback exit\n");
	sem_post(&priv->sem);
}

/****************************************************************************
 * Name: sunxi_audio_reset
 *
 * Description:
 *   Reset and re-initialize the audio
 *
 * Input Parameters:
 *   priv - A reference to the driver state structure
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

static void sunxi_audio_reset(FAR struct sunxi_dev_s *priv)
{
	/* Put audio input back to its initial configuration */
	unsigned long period_size = 0;
	unsigned long buffer_size = 0;

	if (priv->index == AW_AUDIO_CODEC)
	{
		buffer_size = CONFIG_AW_AUDIO_CODEC_BUFFER_SIZE;
		period_size = CONFIG_AW_AUDIO_CODEC_BUFFER_SIZE / CONFIG_AW_AUDIO_CODEC_NUM_BUFFERS;
	}
	else if (priv->index == AW_AUDIO_DMIC)
	{
		buffer_size = CONFIG_AW_AUDIO_DMIC_BUFFER_SIZE;
		period_size = CONFIG_AW_AUDIO_DMIC_BUFFER_SIZE / CONFIG_AW_AUDIO_DMIC_NUM_BUFFERS;
	}
	else
	{
		syslog(LOG_ERR, "index: %d err\n", priv->index);
		return;
	}

	priv->samprate   = REOCRD_DEFAULT_SAMPRATE;
	priv->nchannels  = REOCRD_DEFAULT_NCHANNELS;
	priv->bpsamp     = REOCRD_DEFAULT_BPSAMP;
	priv->buffer_size = buffer_size;
	priv->period_size	 = period_size;
	priv->running     = false;
	priv->paused      = false;
#ifndef CONFIG_AUDIO_EXCLUDE_STOP
	priv->terminating = false;
#endif
	priv->reserved    = true;

}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: sunxi_audio_initialize
 *
 * Description:
 *   Initialize the audio device.
 *
 * Input Parameters:
 *   record     - 0 playback, 1 record.
 *   index      - 0:audiocodec, 1:dmic.
 *
 * Returned Value:
 *   A new lower half audio interface for the audio device is returned on
 *   success; NULL is returned on failure.
 *
 ****************************************************************************/

FAR struct audio_lowerhalf_s *
  sunxi_audio_initialize(bool record, int index)
{
	FAR struct sunxi_dev_s *priv;
	int ret = 0;

	/* Allocate a audio device structure */

	priv = (FAR struct sunxi_dev_s *)
	kmm_zalloc(sizeof(struct sunxi_dev_s));
	if (!priv)
		return NULL;

	/* Initialize the audio device structure.  Since we used kmm_zalloc,
	* only the non-zero elements of the structure need to be initialized.
	*/
	priv->dev.ops	= &g_audio_ops;
	priv->record	= record;
	priv->index		= index;

	ret = nxmutex_init(&priv->pendlock);
	if (ret < 0)
	{
		syslog(LOG_ERR, "nxmutex_init error:%d\n", ret);
		goto errout_with_dev;
	}

	dq_init(&priv->pendq);

	switch (index)
	{
		case AW_AUDIO_CODEC:
			snprintf(priv->pcm_name, sizeof(priv->pcm_name), "hw:%s", CONFIG_AW_AUDIO_CODEC_DEFAULT_CARDNAME);
			break;
		case AW_AUDIO_DMIC:
			snprintf(priv->pcm_name, sizeof(priv->pcm_name), "hw:%s", CONFIG_AW_AUDIO_DMIC_DEFAULT_CARDNAME);
			break;
		default:
			syslog(LOG_ERR, "ERROR: index %d\n", index);
			goto errout_with_dev;
	}
	/* Reset and reconfigure the audio */

	sunxi_audio_reset(priv);
	return &priv->dev;

errout_with_dev:
	nxmutex_destroy(&priv->pendlock);
	if (priv)
		kmm_free(priv);
	return NULL;
}
