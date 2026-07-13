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

#include "sunxi_x4b_alsa.h"
#include "sunxi_audio_rpmsg.h"
#include "rpmsg_audio.h"

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

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

static void printf_priv_config(FAR struct sunxi_x4b_dev_s *priv);
static int init_x4b_ringbuf(FAR struct sunxi_x4b_dev_s *priv);
static int deinit_x4b_ringbuf(FAR struct sunxi_x4b_dev_s *priv);
static int create_x4b_memblk_wait_thread(FAR struct sunxi_x4b_dev_s *priv);
static int send_cmd_to_dsp(FAR struct sunxi_x4b_dev_s *priv, RPMSG_RPD_CTRL_TYPE cmd);

static int record_fillbuffer(FAR struct sunxi_x4b_dev_s *priv);
static int record_fillonebuffer(FAR struct sunxi_x4b_dev_s *priv);

static int get_audio_data(struct sunxi_x4b_dev_s *priv,
							uint8_t *data, uint32_t except_len);
static void rpmsg_memblk_delete_dev(struct sunxi_x4b_dev_s *priv);
static void rpmsg_memblk_dev_unbind(struct rpmsg_memblk_dev *dev, void *priv);
static void create_rpmsg_memblk_thread(void *arg);

static void rpmsg_memblk_thread(void *arg);
static void record_workerthread(void *arg);

/* Initialization */

static void sunxi_audio_reset(FAR struct sunxi_x4b_dev_s *priv);

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
 * Name: sunxi_audio_getcaps
 *
 * Description:
 *   Get the audio device capabilities
 *
 ****************************************************************************/

static int sunxi_audio_getcaps(FAR struct audio_lowerhalf_s *dev, int type,
							FAR struct audio_caps_s *caps)
{
	FAR struct sunxi_x4b_dev_s *priv = (FAR struct sunxi_x4b_dev_s *)dev;
	uint16_t *ptr;

	/* Validate the structure */
	DEBUGASSERT(caps && caps->ac_len >= sizeof(struct audio_caps_s));
	audinfo("type=%d ac_type=%d\n", type, caps->ac_type);

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
			if (priv->index == AUDIO_WWE ||
					priv->index == AUDIO_MDSPEECH)
			{
					caps->ac_channels = 3;
			}

			switch (caps->ac_subtype)
				{
				case AUDIO_TYPE_QUERY:
					/* The types of audio units we implement */
					caps->ac_controls.b[0] = AUDIO_TYPE_INPUT;
					caps->ac_format.hw = (1 << (AUDIO_FMT_PCM - 1));
					break;
				case AUDIO_FMT_PCM:
					caps->ac_controls.b[0] = AUDIO_SUBFMT_PCM_S32_LE;
					caps->ac_controls.b[1] = AUDIO_SUBFMT_END;
					break;
				default:
					caps->ac_controls.b[0] = AUDIO_SUBFMT_END;
					break;
				}

		break;

		/* Provide capabilities of our INPUT unit */

	case AUDIO_TYPE_INPUT:

		caps->ac_channels = 0x22;

			if (priv->index == AUDIO_WWE ||
					priv->index == AUDIO_MDSPEECH)
			{
					caps->ac_channels = 0x33;
			}

			switch (caps->ac_subtype)
				{
				case AUDIO_TYPE_QUERY:

					/* Report the Sample rates we support */
					ptr = (uint16_t *)caps->ac_controls.b;
					*ptr = AUDIO_SAMP_RATE_16K
											| AUDIO_SAMP_RATE_32K
											| AUDIO_SAMP_RATE_48K;
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

		syslog(LOG_INFO, "driver query input ac_channels = 0!\n");

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
	FAR struct sunxi_x4b_dev_s *priv = (FAR struct sunxi_x4b_dev_s *)dev;
	int ret = OK;
	unsigned long period_size;
	unsigned long buffer_size;

	DEBUGASSERT(priv != NULL && caps != NULL);
	audinfo("ac_type: %d\n", caps->ac_type);

	if (priv->index >= AUDIO_VOICE_CALL && priv->index <= AUDIO_TEST) {
		buffer_size = CONFIG_X4B_BUFFER_SIZE;
		period_size = CONFIG_X4B_BUFFER_SIZE / CONFIG_X4B_NUM_BUFFERS;
	} else {
		ret = -EINVAL;
		syslog(LOG_ERR, "index: %d err\n", priv->index);
		return ret;
	}

	/* Process the configure operation */

	switch (caps->ac_type)
	{
	case AUDIO_TYPE_INPUT:
		audinfo("  AUDIO_TYPE_INPUT:\n");
		audinfo("    Number of channels: %u\n", caps->ac_channels);
		audinfo("    Sample rate:        %u\n", caps->ac_controls.hw[0]);
		audinfo("    Sample width:       %u\n", caps->ac_controls.b[2]);

		{
			/* Verify that all of the requested values are supported */

			ret = -ERANGE;
			if (caps->ac_channels < 0 || caps->ac_channels > 3)
			{
				auderr("ERROR: Unsupported number of channels: %d\n",
						caps->ac_channels);
				break;
			}

			if (caps->ac_controls.b[2] != 16 && caps->ac_controls.b[2] != 32)
			{
				auderr("ERROR: Unsupported bits per sample: %d\n",
						caps->ac_controls.b[2]);
				break;
			}

			/* Save the current stream configuration */

			priv->samprate  = caps->ac_controls.hw[0];
			priv->nchannels = caps->ac_channels;
			priv->bpsamp    = caps->ac_controls.b[2];
			priv->buffer_size = buffer_size;
			priv->period_size = period_size;
			priv->buffer_byte = buffer_size * (priv->bpsamp / 8) * priv->nchannels * 5;

			printf_priv_config(priv);

			ret = init_x4b_ringbuf(priv);
			if (ret)
				return ret;

			ret = create_x4b_memblk_wait_thread(priv);
			if (ret)
				return ret;

			ret = send_cmd_to_dsp(priv, RPMSG_CTL_INIT);
			if (ret)
				return ret;

			/* Wait x4b_memblk bind success */
			ret = nxsem_tickwait(&priv->mb.bind_sem, MS_TO_OSTICK(5000));
			if (ret != OK) {
				syslog(LOG_INFO, "wait bind sem timeout!\n");
				return ret;
			}

			ret = send_cmd_to_dsp(priv, RPMSG_CTL_CONFIG);
			if (ret)
				return ret;

			ret = OK;
		}
		break;

	default:
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
	FAR struct sunxi_x4b_dev_s *priv = (FAR struct sunxi_x4b_dev_s *)dev;

	DEBUGASSERT(priv);

	/* Now issue a software reset. This puts all audio registers back in
	* their default state.
	*/

	sunxi_audio_reset(priv);
	return OK;
}

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
	FAR struct sunxi_x4b_dev_s *priv = (FAR struct sunxi_x4b_dev_s *)dev;
	struct sched_param sparam;
	struct mq_attr attr;
	FAR void *value;
	int ret = -1;

	char name_buf[64];

	syslog(LOG_INFO, "%d, %s\n", __LINE__, __func__);
	audinfo("Entry\n");

	/* Create a message queue for the worker thread */

	snprintf(priv->mqname, sizeof(priv->mqname), "/tmp/%" PRIXPTR,
			(uintptr_t)priv);

	attr.mq_maxmsg  = 16;
	attr.mq_msgsize = sizeof(struct audio_msg_s);
	attr.mq_curmsgs = 0;
	attr.mq_flags   = 0;

	ret = file_mq_open(&priv->mq, priv->mqname,
						O_RDWR | O_CREAT, 0644, &attr);
	if (ret < 0)
	{
		/* Error creating message queue! */
		syslog(LOG_ERR, "ERROR: Couldn't allocate message queue\n");
	}

	pthread_mutex_init(&priv->mutex, NULL);
	pthread_cond_init(&priv->cond_pause, NULL);

	/* Start our thread for getting data to the device */
	audinfo("Starting worker thread\n");
	sem_init(&priv->sem, 0, 0);

	snprintf(name_buf, sizeof(name_buf), "sunxi-audio-%d",
				priv->index);
	priv->record_task = audio_thread_create(&priv->record_sem, record_workerthread, priv, name_buf,
							CONFIG_X4B_WORKER_STACKSIZE, (sched_get_priority_max(SCHED_FIFO) - 3));
	if (!priv->record_task)
	{
		syslog(LOG_ERR, "ERROR: pthread_create failed: %d\n", ret);
	}
	else
	{
		hal_thread_start(priv->record_task);
		syslog(LOG_ERR, "Created worker thread\n");
	}


	priv->mb.thread_stop = 0;

	snprintf(name_buf, sizeof(name_buf), "rpmsg_memblk-%d",
				priv->index);
	priv->rpmsg_task = audio_thread_create(&priv->rpmsg_sem, rpmsg_memblk_thread, priv, name_buf,
							CONFIG_X4B_WORKER_STACKSIZE, (sched_get_priority_max(SCHED_FIFO) - 3));
	if (!priv->rpmsg_task)
	{
		syslog(LOG_ERR, "ERROR: pthread_create failed: %d\n", ret);
	}
	else
	{
		hal_thread_start(priv->rpmsg_task);
		syslog(LOG_ERR, "Created worker thread\n");
	}

	ret = nxsem_tickwait_uninterruptible(&priv->sem, MS_TO_OSTICK(1000));
	if (ret != OK) {
		syslog(LOG_ERR, "wait bind sem timeout!, %d\n", __LINE__);
	}
	ret = send_cmd_to_dsp(priv, RPMSG_CTL_START);

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
	FAR struct sunxi_x4b_dev_s *priv = (FAR struct sunxi_x4b_dev_s *)dev;
	struct audio_msg_s term_msg;
	FAR void *value;
	int ret = 0;
	unsigned int prio;

	syslog(LOG_INFO, "%d, %s, index:%d\n", __LINE__, __func__, priv->index);
	if (priv->index >= AUDIO_VOICE_CALL && priv->index <= AUDIO_TEST)
		prio = CONFIG_X4B_MSG_PRIO;
	else {
		ret = -EINVAL;
		auderr("index: %d err\n", priv->index);
		return ret;
	}

	/* Send a message to stop all audio streaming */
	term_msg.msg_id = AUDIO_MSG_STOP;
	term_msg.u.data = 0;
	priv->running = false;

	pthread_cond_signal(&priv->cond_pause);
	file_mq_send(&priv->mq, (FAR const char *)&term_msg, sizeof(term_msg),
				prio);
	/* Join the worker thread */
	audio_thread_stop(&priv->record_sem, priv->record_task);

	if (!priv->dsp_crash) {
		ret = send_cmd_to_dsp(priv, RPMSG_CTL_STOP);
		if (ret)
			return ret;

		/* to avoid callback function calls oneself, we use
		* rpmsg_memblk_dev_delete instead of
		* rpmsg_memblk_delete_dev
		*/
		rpmsg_memblk_dev_delete(priv->mb.memblk_dev);
	}

	ret = deinit_x4b_ringbuf(priv);
	if (ret)
		return ret;

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
	FAR struct sunxi_x4b_dev_s *priv = (FAR struct sunxi_x4b_dev_s *)dev;

	if (priv->running && !priv->paused)
	{
		priv->paused = true;
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
	FAR struct sunxi_x4b_dev_s *priv = (FAR struct sunxi_x4b_dev_s *)dev;

	if (priv->running && priv->paused)
	{
		priv->paused = false;

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
	FAR struct sunxi_x4b_dev_s *priv = (FAR struct sunxi_x4b_dev_s *)dev;
	struct audio_msg_s term_msg;
	int ret;

	audinfo("Enqueueing: apb=%p curbyte=%d nbytes=%d flags=%04x\n",
			apb, apb->curbyte, apb->nbytes, apb->flags);

	ret = nxmutex_lock(&priv->pendlock);
	if (ret < 0)
	{
		syslog(LOG_ERR, "%d, %s:nxmutex_lock err\n", __LINE__, __func__);
		return ret;
	}

	/* Take a reference on the new audio buffer */

	//apb_reference(apb);

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
		term_msg.msg_id  = AUDIO_MSG_ENQUEUE;
		term_msg.u.data = 0;

		ret = file_mq_send(&priv->mq, (FAR const char *)&term_msg,
							sizeof(term_msg), CONFIG_X4B_MSG_PRIO);
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
	FAR struct sunxi_x4b_dev_s *priv = (FAR struct sunxi_x4b_dev_s *)dev;

	int ret = OK;

	/* Deal with ioctls passed from the upper-half driver */

	switch (cmd)
	{
		/* Check for AUDIOIOC_HWRESET ioctl.  This ioctl is passed straight
		* through from the upper-half audio driver.
		*/

		case AUDIOIOC_HWRESET:
		{
			/* when dsp has crashed，ept will trigger this cmd */

			priv->dsp_crash = (bool)arg;

			ret = nxmutex_lock(&priv->pendlock);
			if (ret < 0)
			{
				syslog(LOG_INFO, "%d, %s, index:%d\n", __LINE__, __func__, priv->index);
				return ret;
			}
			if (priv->dsp_crash) {
				FAR struct ap_buffer_s *apb;
				apb = (FAR struct ap_buffer_s *)dq_peek(&priv->pendq);
#ifdef CONFIG_AUDIO_MULTI_SESSION
				priv->dev.upper(priv->dev.priv, AUDIO_CALLBACK_IOERR, apb, OK, NULL);
#else
				priv->dev.upper(priv->dev.priv, AUDIO_CALLBACK_IOERR, apb, OK);
			}
#endif
			nxmutex_unlock(&priv->pendlock);
			audinfo("AUDIOIOC_HWRESET:\n");
		}
		break;

#ifdef CONFIG_AUDIO_DRIVER_SPECIFIC_BUFFERS
		/* Set driver buffer size and quantity */
		case AUDIOIOC_SETBUFFERINFO:
		{
			audinfo("AUDIOIOC_SETBUFFERINFO:\n");
			FAR struct ap_buffer_info_s *bufinfo = (FAR struct ap_buffer_info_s *)arg;
			priv->period_size = bufinfo->buffer_size / (priv->bpsamp / 8 * priv->nchannels);
			priv->buffer_size = priv->period_size * bufinfo->nbuffers;
		}
		break;

		/* Report our preferred buffer size and quantity */
		case AUDIOIOC_GETBUFFERINFO:
		{
			audinfo("AUDIOIOC_GETBUFFERINFO:\n");
			FAR struct ap_buffer_info_s *bufinfo = (FAR struct ap_buffer_info_s *)arg;
			bufinfo->buffer_size = priv->period_size * (priv->bpsamp / 8 * priv->nchannels);
			bufinfo->nbuffers	 = priv->buffer_size / priv->period_size;
		}
		break;
#endif

		/* Report our preferred buffer size and quantity */

		default:
		ret = -ENOTTY;
		syslog(LOG_INFO, "Ignored\n");
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
	FAR struct sunxi_x4b_dev_s *priv = (FAR struct sunxi_x4b_dev_s *)dev;
	FAR void *value;
	int ret = 0;

	pthread_mutex_destroy(&priv->mutex);
	pthread_cond_destroy(&priv->cond_pause);

	if (!priv->dsp_crash)
		ret = send_cmd_to_dsp(priv, RPMSG_CTL_RELEASE);
	priv->dsp_crash = 0;

	return ret;
}

/****************************************************************************
 * Name: printf_priv_config
 *
 * Description:
 *  print the audio members of sunxi_x4b_dev_s, like channel, samprate etc.
 *
 ****************************************************************************/

static void printf_priv_config(FAR struct sunxi_x4b_dev_s *priv)
{
	syslog(LOG_INFO, "  input stream configuration:\n");
	syslog(LOG_INFO, "    Number of channels: %u\n", priv->nchannels);
	syslog(LOG_INFO, "    Sample rate:        %u\n", priv->samprate);
	syslog(LOG_INFO, "    Sample width:       %u\n", priv->bpsamp);
	syslog(LOG_INFO, "    Period size:       %lu\n", priv->period_size);
	syslog(LOG_INFO, "    Buffer size:       %lu\n", priv->buffer_size);
	syslog(LOG_INFO, "    Buffer byte:       %lu\n", priv->buffer_byte);
}

/****************************************************************************
 * Name: init_x4b_ringbuf
 *
 * Description:
 *  init the members in sunxi_x4b_ringbuf_s struct, malloc buffer,
 *  init ringbuffer and init mutex.
 *
 ****************************************************************************/

static int init_x4b_ringbuf(FAR struct sunxi_x4b_dev_s *priv)
{
	int ret = -1;

	if (priv->rb.is_init) {
		syslog(LOG_ERR, "init failed, x4b ringbuf has init, please check!");
		return ret;
	}

	if (priv->rb.audio_buf) {
		syslog(LOG_ERR, "init failed, x4b audio_buf is not NULL, please check!");
		return ret;
	}

	priv->rb.audio_buf = malloc(priv->buffer_byte);
	if (!priv->rb.audio_buf) {
		syslog(LOG_ERR, "init failed, x4b audio_buf malloc failed, please check!");
		return ret;
	}

	ring_buffer_init(&priv->rb.audio_ringbuf, priv->rb.audio_buf, priv->buffer_byte);
	pthread_mutex_init(&priv->rb.audio_ringbuf_mutex, NULL);
	priv->rb.is_init = 1;

	return 0;
}

/****************************************************************************
 * Name: deinit_x4b_ringbuf
 *
 * Description:
 *   deinit the members in sunxi_x4b_ringbuf_s struct, free buffer
 *   and destroy mutex.
 *
 ****************************************************************************/

static int deinit_x4b_ringbuf(FAR struct sunxi_x4b_dev_s *priv)
{
	int ret = -1;

	syslog(LOG_INFO, "%d, %s\n", __LINE__, __func__);
	if (!priv->rb.is_init) {
		syslog(LOG_ERR, "x4b_ringbuf no init, please check!");
		return ret;
	}

	if (!priv->rb.audio_buf) {
		syslog(LOG_ERR, "x4b audio_buf is NULL, please check!");
		return ret;
	} else {
		free(priv->rb.audio_buf);
		priv->rb.audio_buf = NULL;
	}

	pthread_mutex_destroy(&priv->rb.audio_ringbuf_mutex);
	priv->rb.is_init = 0;

	return 0;
}

/****************************************************************************
 * Name: create_x4b_memblk_wait_thread
 *
 * Description:
 *   init the sem and create create_rpmsg_memblk_thread.
 *
 ****************************************************************************/

static int create_x4b_memblk_wait_thread(FAR struct sunxi_x4b_dev_s *priv)
{
	int ret = -1;
	struct sched_param sparam;
	pthread_attr_t tattr;
	char name_buf[64];


	syslog(LOG_INFO, "%d, %s\n", __LINE__, __func__);
	if (priv->mb.has_create) {
		syslog(LOG_ERR, "create failed, x4b memblk thread has create, please check!");
		return ret;
	}

	sem_init(&priv->mb.bind_sem, 0, 0);

	snprintf(name_buf, sizeof(name_buf), "create-thread-%d",
				priv->index);
	priv->creat_task = hal_thread_create(create_rpmsg_memblk_thread, priv, name_buf,
				CONFIG_X4B_WORKER_STACKSIZE, (sched_get_priority_max(SCHED_FIFO) - 3));
	if (!priv->creat_task)
	{
		syslog(LOG_ERR, "ERROR: pthread_create failed: %d\n", ret);
		return ret;
	}
	else
	{
		hal_thread_start(priv->creat_task);
		syslog(LOG_INFO, "Created create_rpmsg_memblk_thread\n");
	}

	priv->mb.has_create = 1;
	return 0;
}

/****************************************************************************
 * Name: send_cmd_to_dsp
 *
 * Description:
 *   Send control command to remote(DSP) by rpmsg.
 *
 ****************************************************************************/

static int send_cmd_to_dsp(FAR struct sunxi_x4b_dev_s *priv, RPMSG_RPD_CTRL_TYPE cmd)
{
	int ret;
	rpmsg_audio_ctrl_t audio_rpmsg_ctrl_param;

	memset(&audio_rpmsg_ctrl_param, 0 , sizeof(rpmsg_audio_ctrl_t));

	audio_rpmsg_ctrl_param.cmd = cmd;
	audio_rpmsg_ctrl_param.index = priv->index;

	switch (cmd) {
	case RPMSG_CTL_INIT:
		break;
	case RPMSG_CTL_CONFIG:
		audio_rpmsg_ctrl_param.rate = priv->samprate;
		audio_rpmsg_ctrl_param.channels = priv->nchannels;
		audio_rpmsg_ctrl_param.bits = priv->bpsamp;
		break;
	case RPMSG_CTL_START:
		syslog(LOG_INFO, "%d, %s\n", __LINE__, __func__);
		break;
	case RPMSG_CTL_STOP:
		syslog(LOG_INFO, "%d, %s\n", __LINE__, __func__);
		break;
	case RPMSG_CTL_RELEASE:
		break;
	default:
		break;
	}

	ret = audio_rpmsg_ctl_send_cmd(RPMSG_DIR_NAME, &audio_rpmsg_ctrl_param);
	if (ret != 0)
	{
		syslog(LOG_ERR, "rpmsg send err, cmd:%lu\n", audio_rpmsg_ctrl_param.cmd);
		return ret;
	}

	return ret;
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

static int record_fillonebuffer(FAR struct sunxi_x4b_dev_s *priv)
{
	FAR struct ap_buffer_s *apb;

	apb = (FAR struct ap_buffer_s *)dq_peek(&priv->pendq);
	if (apb != NULL)
	{
		/* Take next buffer from the queue of pending transfers */
		get_audio_data(priv, (uint8_t *)apb->samp, apb->nbytes);

		nxmutex_lock(&priv->pendlock);

		dq_remfirst(&priv->pendq);
		audinfo("filling: apb=%p curbyte=%d nbytes=%d flags=%04x\n",
				apb, apb->curbyte, apb->nbytes, apb->flags);
#ifdef CONFIG_AUDIO_MULTI_SESSION
		priv->dev.upper(priv->dev.priv, AUDIO_CALLBACK_DEQUEUE, apb, OK, NULL);
#else
		priv->dev.upper(priv->dev.priv, AUDIO_CALLBACK_DEQUEUE, apb, OK);
#endif
		nxmutex_unlock(&priv->pendlock);
	}

	return 0;
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

static int record_fillbuffer(FAR struct sunxi_x4b_dev_s *priv)
{
	FAR struct ap_buffer_s *apb;
	int ret = 0;

	/* Loop while there are audio buffers to be sent and we have few than
	* CONFIG_DMIC_INFLIGHT then "in-flight"
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
	if (ret < 0)
	{
		return ret;
	}

	apb = (FAR struct ap_buffer_s *)dq_peek(&priv->pendq);
	while (apb != NULL && priv->running)
	{
		/* Take next buffer from the queue of pending transfers */

		audinfo("filling: apb=%p curbyte=%d nbytes=%d flags=%04x\n",
				apb, apb->curbyte, apb->nbytes, apb->flags);

		get_audio_data(priv, (uint8_t *)apb->samp, apb->nbytes);

		dq_remfirst(&priv->pendq);
#ifdef CONFIG_AUDIO_MULTI_SESSION
		priv->dev.upper(priv->dev.priv, AUDIO_CALLBACK_DEQUEUE, apb, OK, NULL);
#else
		priv->dev.upper(priv->dev.priv, AUDIO_CALLBACK_DEQUEUE, apb, OK);
#endif

		apb = (FAR struct ap_buffer_s *)dq_peek(&priv->pendq);
	}

	nxmutex_unlock(&priv->pendlock);
	return ret;
}

/****************************************************************************
 * Name: get_audio_data
 *
 * Description:
 *   get audio data from ringbuffer.
 *
 ****************************************************************************/

static int get_audio_data(struct sunxi_x4b_dev_s *priv,
							uint8_t *data, uint32_t except_len)
{
	uint32_t readable_size;
	int usleep_time = 5000;
	int retry = RETRY_CNT;
	int ret = 0;

	while (1) {
		pthread_mutex_lock(&priv->rb.audio_ringbuf_mutex);
		readable_size = ring_buffer_readable_size(&priv->rb.audio_ringbuf);
		pthread_mutex_unlock(&priv->rb.audio_ringbuf_mutex);

		if (readable_size < except_len) {
			//syslog(LOG_INFO, "readable_size:%lu < except_len:%lu, sleep and try agaib\n",
			//		readable_size, except_len);

			if (retry--) {
				usleep(usleep_time);
				continue;
			} else {
				syslog(LOG_INFO, "remote is no data at least %d ms, break while\n",
						(usleep_time / 1000) * RETRY_CNT);
				ret = -1;
				memset(data, 0x00, except_len);
				break;
			}
		}

		pthread_mutex_lock(&priv->rb.audio_ringbuf_mutex);
		ring_buffer_read(&priv->rb.audio_ringbuf, data, except_len);
		pthread_mutex_unlock(&priv->rb.audio_ringbuf_mutex);
		break;
	}

	return ret;
}

/****************************************************************************
 * Name: rpmsg_memblk_delete_dev
 *
 * Description:
 *   Exit the thread, delete rpmsg_memblk_dev and destroy the sem.
 *
 ****************************************************************************/

static void rpmsg_memblk_delete_dev(struct sunxi_x4b_dev_s *priv)
{
	FAR void *value;
	char name_buf[64];

	if (!priv->mb.memblk_dev) {
		syslog(LOG_INFO, "rpmsg: %s has been deleted\r\n", name_buf);
		return;
	}

	snprintf(name_buf, sizeof(name_buf), "%s",
		rpmsg_memblk_get_name(priv->mb.memblk_dev));

	syslog(LOG_INFO, "rpmsg_memblk: %s delete\r\n", name_buf);

	if (priv->mb.thread_stop == 0) {
		priv->mb.thread_stop = 1;
		audio_thread_stop(&priv->rpmsg_sem, priv->rpmsg_task);
		priv->creat_task = NULL;
	}

	sem_destroy(&priv->sem);
	sem_destroy(&priv->mb.bind_sem);

	/* to avoid callback function calls oneself,
	* we can't use rpmsg_memblk_dev_delete here
	*/
	//rpmsg_memblk_dev_delete(priv->mb.memblk_dev);
	priv->mb.memblk_dev = NULL;
	priv->mb.has_create = 0;

	syslog(LOG_INFO, "rpmsg: %s delete success\r\n", name_buf);
}


/****************************************************************************
 * Name: rpmsg_memblk_dev_unbind
 *
 *  This is a callback function that will be called when rpmsg_memblk_dev
 *  disconnects by remote.
 *
 ****************************************************************************/

static void rpmsg_memblk_dev_unbind(struct rpmsg_memblk_dev *dev, void *priv)
{
	struct sunxi_x4b_dev_s *x4b_priv = priv;

	rpmsg_memblk_delete_dev(x4b_priv);
}

/****************************************************************************
 * Name: create_rpmsg_memblk_thread
 *
 *  This is the thread that create rpmsg memblk dev for getting data
 *  from remote. It will block when calling rpmsg_memblk_dev_create,
 * 	so we create a thread to check if rpmsg_memblk_dev is ready.
 *
 ****************************************************************************/

static void create_rpmsg_memblk_thread(void *arg)
{
	FAR struct sunxi_x4b_dev_s *priv = (struct sunxi_x4b_dev_s *)arg;
	char name_buf[64];

	syslog(LOG_INFO, "create_rpmsg_memblk_thread run\n");

	memset(name_buf, 0, sizeof(name_buf));
	snprintf(name_buf, sizeof(name_buf), "%s_%d",
			RPMSG_MEMBLK_DEV_NAME_PREFIX, priv->index);
	syslog(LOG_INFO, "rpmsg_memblk_create_dev name_buf:%s\n", name_buf);

	priv->mb.memblk_dev = rpmsg_memblk_dev_create(name_buf, rpmsg_memblk_dev_unbind, priv);
	if (!priv->mb.memblk_dev) {
		syslog(LOG_ERR, "rpmsg_memblk_dev_create failed\n");
	} else {
		syslog(LOG_INFO, "rpmsg_memblk_dev_create success, post sem\n");
		sem_post(&priv->mb.bind_sem);
	}

	syslog(LOG_INFO, "create_rpmsg_memblk_thread exit\n");
}

/****************************************************************************
 * Name: rpmsg_memblk_thread
 *
 *  This is the thread that gets data from the remote and save the data
 *  to ringbuffer.
 *
 ****************************************************************************/

static void rpmsg_memblk_thread(void *arg)
{
	FAR struct sunxi_x4b_dev_s *priv = (struct sunxi_x4b_dev_s *)arg;
	int ret, data_len;
	void *data;
	struct memblk_entry memblock;
	uint32_t ret_len;
	int writable_size;
	int count = 0;
	int i = 0;

	usleep(50000);
	syslog(LOG_INFO, "rpmsg_memblk_thread run\n");

	while (!priv->mb.thread_stop) {
		sem_post(&priv->sem);
		/* get readable buffer */
		ret = rpmsg_memblk_pull_memblk(priv->mb.memblk_dev, &memblock, 1000);
		if (ret) {
			count++;
			syslog(LOG_INFO, "rpmsg_memblk_pull_memblk err, maybe timeout, try again!");
			if (count > 9) {
				syslog(LOG_INFO, "%d, %s\n", __LINE__, __func__);
				priv->dev.upper(priv->dev.priv, AUDIO_CALLBACK_IOERR, NULL, ETIMEDOUT);
				count = 0;
			}
			continue;
		}

		data = rpmsg_memblk_get_addr(&memblock);
		data_len = rpmsg_memblk_get_len(&memblock);
		ret_len = 0;
		count = 0;

		//syslog(LOG_INFO, "data:%p, data_len:%d, cnt:%d\n", data, data_len, ++cnt);

		while (data_len > 0) {
			pthread_mutex_lock(&priv->rb.audio_ringbuf_mutex);
			writable_size = ring_buffer_writable_size(&priv->rb.audio_ringbuf);
			if (writable_size < data_len) {
				i++;
				if (i % 100 == 0) {
					_err("ringbuff is not enough, %d\n", writable_size);
					_err("data:%p, data_len:%d\n", data, data_len);
					i = 0;
				}
			}
			ret_len += ring_buffer_write(&priv->rb.audio_ringbuf, data + ret_len, data_len);
			pthread_mutex_unlock(&priv->rb.audio_ringbuf_mutex);

			if (!ret_len)
				break;
			//syslog(LOG_INFO, "ret_len:%lu\n", ret_len);

			data_len -= ret_len;
		}

		//syslog(LOG_INFO, "write end and return buffer \n");

		/* return buffer */
		rpmsg_memblk_return_memblk(priv->mb.memblk_dev, &memblock);
	}

	syslog(LOG_INFO, "rpmsg_memblk_thread exit\n");
	sem_post(&priv->rpmsg_sem);
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
	FAR struct sunxi_x4b_dev_s *priv = (struct sunxi_x4b_dev_s *)arg;
	struct audio_msg_s msg;
	FAR struct ap_buffer_s *apb;
	int msglen = 0;
	unsigned int prio;

	audinfo("Entry\n");

#ifndef CONFIG_AUDIO_EXCLUDE_STOP
	priv->terminating = false;
#endif

/* Mark ourself as running */
	priv->running = true;

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
			auderr("ERROR: Message too small: %d\n", msglen);
			continue;
		}

		/* Process the message */

		switch (msg.msg_id)
		{
			/* The ISR has requested more data.  We will catch this case at
			* the top of the loop.
			*/

			case AUDIO_MSG_DATA_REQUEST:
			audinfo("AUDIO_MSG_DATA_REQUEST\n");
			break;

			/* Stop the playback */

#ifndef CONFIG_AUDIO_EXCLUDE_STOP
			case AUDIO_MSG_STOP:

			/* Indicate that we are terminating */

			audinfo("AUDIO_MSG_STOP: Terminating\n");
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
			audinfo("AUDIO_MSG_COMPLETE\n");
			break;

			default:
			auderr("ERROR: Ignoring message ID %d\n", msg.msg_id);
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

	syslog(LOG_INFO,"dsp record exit\n");
	sem_post(&priv->record_sem);
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

static void sunxi_audio_reset(FAR struct sunxi_x4b_dev_s *priv)
{
	/* Put audio input back to its initial configuration */
	unsigned long period_size = 0;
	unsigned long buffer_size = 0;

	buffer_size = CONFIG_X4B_BUFFER_SIZE;
	period_size = CONFIG_X4B_BUFFER_SIZE / CONFIG_X4B_NUM_BUFFERS;

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
	priv->reserved    = false;

	return;

}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: x4b_record_initialize
 *
 * Description:
 *   Initialize the x4b device.
 *
 * Input Parameters:
 *   index      - 10:voice call, 11:wwe, 12:mdspeech.
 *
 * Returned Value:
 *   A new lower half audio interface for the audio device is returned on
 *   success; NULL is returned on failure.
 *
 ****************************************************************************/

FAR struct audio_lowerhalf_s *
	x4b_record_initialize(int index)
{
	FAR struct sunxi_x4b_dev_s *priv;
	int ret = 0;

	/* Allocate a audio device structure */

	priv = (FAR struct sunxi_x4b_dev_s *)
	kmm_zalloc(sizeof(struct sunxi_x4b_dev_s));
	if (priv)
	{
		/* Initialize the audio device structure.  Since we used kmm_zalloc,
		* only the non-zero elements of the structure need to be initialized.
		*/

		priv->dev.ops	= &g_audio_ops;
		priv->index		= index;

		ret = nxmutex_init(&priv->pendlock);
		if (ret < 0)
		{
			auderr("nxmutex_init error:%d\n", ret);
			goto err_free_priv;
		}

		dq_init(&priv->pendq);

		if (priv->index < AUDIO_VOICE_CALL || priv->index > AUDIO_TEST)
		{
			auderr("ERROR: index %d is not support\n", index);
			goto err_destroy_nxmutex;
		}

		switch (index)
		{
			case AUDIO_VOICE_CALL:
			case AUDIO_WWE:
			case AUDIO_MDSPEECH:
			case AUDIO_TEST:
				break;
			default:
				auderr("ERROR: index %d\n", index);
				goto err_destroy_nxmutex;

		}

		/* Reset and reconfigure the audio */

		sunxi_audio_reset(priv);

		return &priv->dev;
	}

	return NULL;

err_destroy_nxmutex:
	nxmutex_destroy(&priv->pendlock);
err_free_priv:
	if (priv)
		kmm_free(priv);
	return NULL;
}
