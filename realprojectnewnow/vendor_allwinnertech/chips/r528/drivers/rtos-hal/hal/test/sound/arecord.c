/*
* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
*
* Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
* the the people's Republic of China and other countries.
* All Allwinner Technology Co.,Ltd. trademarks are used with permission.
*
* DISCLAIMER
* THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
* IF YOU NEED TO INTEGRATE THIRD PARTY’S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
* IN ALLWINNERS’SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
* ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
* ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
* COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
* YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY’S TECHNOLOGY.
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
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <hal_cmd.h>
#include <aw-alsa-lib/pcm.h>
#include <aw-alsa-lib/control.h>
#include "common.h"
#include "wav_parser.h"
#include <hal_time.h>
#include <hal_timer.h>

unsigned int g_capture_loop_enable = 0;
static unsigned int g_capture_then_play = 0;
static char g_pcm_name[32];
extern unsigned int g_verbose;
extern int aplay(const char *card_name, snd_pcm_format_t format, unsigned int rate,
		 unsigned int channels, const char *data, unsigned int datalen);

/*
 * arg0: arecord
 * arg1: card
 * arg2: format
 * arg3: rate
 * arg4: channels
 * arg5: data
 * arg6: len
 */
static int arecord(const char *card_name, snd_pcm_format_t format, unsigned int rate,
		   unsigned int channels, const void *data, unsigned int datalen)
{
	int ret = 0;
	snd_pcm_t *handle;
	int mode = 0;
	snd_pcm_uframes_t period_frames = 1024, buffer_frames = 4096;

	syslog(LOG_INFO,"dump args:\n");
	syslog(LOG_INFO,"card:      %s\n", card_name);
	syslog(LOG_INFO,"format:    %u\n", format);
	syslog(LOG_INFO,"rate:      %u\n", rate);
	syslog(LOG_INFO,"channels:  %u\n", channels);
	syslog(LOG_INFO,"data:      %p\n", data);
	syslog(LOG_INFO,"datalen:   %u\n", datalen);

	/* open card */
	ret = snd_vela_pcm_open(&handle, card_name, SND_VELA_PCM_STREAM_CAPTURE, mode);
	if (ret < 0) {
		syslog(LOG_ERR,"audio open error:%d\n", ret);
		return -1;
	}

	ret = set_param(handle, format, rate, channels, period_frames, buffer_frames);
	if (ret < 0)
		goto err1;
	do {
		syslog(LOG_INFO,"pcm_read start...\n");
		ret = pcm_read(handle, data,
			snd_vela_pcm_bytes_to_frames(handle, datalen),
			snd_vela_pcm_frames_to_bytes(handle, 1));
		if (ret < 0) {
			syslog(LOG_ERR,"capture error:%d\n", ret);
			goto err1;
		}
	} while (g_capture_loop_enable);

	ret = snd_vela_pcm_drain(handle);
	if (ret < 0)
		syslog(LOG_ERR,"stop failed!, return %d\n", ret);

err1:
	/* close card */
	ret = snd_vela_pcm_close(handle);
	if (ret < 0) {
		syslog(LOG_ERR,"audio close error:%d\n", ret);
		return ret;
	}

	return ret;
}

static int capture_then_play(audio_mgr_t *audio_mgr)
{
	char *capture_data = NULL;
	unsigned int len = 0;

	if (audio_mgr->capture_duration == 0)
		audio_mgr->capture_duration = 5;

	len = snd_vela_pcm_format_size(audio_mgr->format,
			audio_mgr->capture_duration * audio_mgr->rate * audio_mgr->channels);
	capture_data = malloc(len);
	if (!capture_data) {
		syslog(LOG_ERR,"no memory\n");
		return -1;
	}

	do {
		memset(capture_data, 0, len);

		syslog(LOG_INFO,"arecord start...\n");
		arecord(g_pcm_name, audio_mgr->format, audio_mgr->rate,
					audio_mgr->channels, capture_data, len);
		if (g_capture_then_play) {
			syslog(LOG_INFO,"aplay start...\n");
			/*snd_ctl_set("audiocodec", "LINEOUT volume", 0x1f);*/
			aplay(g_pcm_name, audio_mgr->format, audio_mgr->rate,
					audio_mgr->channels, capture_data, len);
		}
	} while (g_capture_loop_enable);

	free(capture_data);
	capture_data = NULL;

	return 0;
}

int capture_fs_wav(audio_mgr_t *mgr, const char *path)
{
	int ret = 0, fd = 0;
	wav_header_t header;
	unsigned int written = 0;
	long rest = -1, c = 0;
	char *audiobuf = NULL;
	unsigned int chunk_bytes, frame_bytes = 0;
	int save_fs = 0;
	struct stat statbuf;

	syslog(LOG_INFO,"card:		%s\n", g_pcm_name);
	syslog(LOG_INFO,"period_size:	%ld\n", mgr->period_size);
	syslog(LOG_INFO,"buffer_size:	%ld\n", mgr->buffer_size);

	if (path != NULL)
		save_fs = 1;
	/* open card */
	ret = snd_vela_pcm_open(&mgr->handle, g_pcm_name, SND_VELA_PCM_STREAM_CAPTURE, 0);
	if (ret < 0) {
		syslog(LOG_ERR,"audio open error:%d\n", ret);
		return -1;
	}

	ret = set_param(mgr->handle, mgr->format, mgr->rate, mgr->channels,
			mgr->period_size, mgr->buffer_size);
	if (ret < 0)
		goto err;

	frame_bytes = snd_vela_pcm_frames_to_bytes(mgr->handle, 1);
	chunk_bytes = snd_vela_pcm_frames_to_bytes(mgr->handle, mgr->period_size);
	if (mgr->capture_duration > 0)
		rest = mgr->capture_duration * snd_vela_pcm_frames_to_bytes(mgr->handle, mgr->rate);

	create_wav(&header, mgr->format, mgr->rate, mgr->channels);
	if (save_fs) {
		if (!stat(path, &statbuf)) {
			if (S_ISREG(statbuf.st_mode))
				remove(path);
		}
		fd = open(path, O_RDWR | O_CREAT, 0644);
		if (fd < 0) {
			syslog(LOG_ERR,"create wav file failed\n");
			goto err;
		}
		write(fd, &header, sizeof(header));
	}

	audiobuf = malloc(chunk_bytes);
	if (!audiobuf) {
		syslog(LOG_ERR,"no memory...\n");
		goto err;
	}

	if (save_fs && fd > 0) {
		if (rest < 0) {
			syslog(LOG_INFO,"please set capture duration..\n");
			goto err;
		}
	}
	while ((rest > 0 || g_capture_loop_enable) && !mgr->in_aborting) {
		long f = mgr->period_size;
		if (rest <= chunk_bytes && !g_capture_loop_enable)
			c = rest;
		else
			c = chunk_bytes;
		f = pcm_read(mgr->handle, audiobuf, f, frame_bytes);
		if (f < 0) {
			syslog(LOG_ERR,"pcm read error, return %ld\n", f);
			break;
		}
		if (save_fs && fd > 0) {
			ret = write(fd, audiobuf, c);
			if (ret != c) {
				syslog(LOG_ERR,"write audiobuf to wav file failed, return %d\n", ret);
				goto err;
			}
		}

		if (rest > 0)
			rest -= c;
		written += c;
	}

	ret = snd_vela_pcm_drain(mgr->handle);
	if (ret < 0)
		syslog(LOG_ERR,"stop failed!, return %d\n", ret);

err:
	/* close card */
	ret = snd_vela_pcm_close(mgr->handle);
	if (ret < 0) {
		syslog(LOG_ERR,"audio close error:%d\n", ret);
		return ret;
	}

	if (save_fs && fd > 0 && ret == 0) {
		resize_wav(&header, written);
		lseek(fd, 0, SEEK_SET);
		write(fd, &header, sizeof(header));
	}

	if (save_fs) {
		if (fd > 0)
			close(fd);
	}
	if (audiobuf)
		free(audiobuf);

	return ret;
}

static void usage(void)
{
	syslog(LOG_INFO,"Usage: arecord [option]\n");
	syslog(LOG_INFO,"-D,          pcm device name\n");
	syslog(LOG_INFO,"-r,          sample rate\n");
	syslog(LOG_INFO,"-f,          sample bits\n");
	syslog(LOG_INFO,"-c,          channels\n");
	syslog(LOG_INFO,"-p,          period size\n");
	syslog(LOG_INFO,"-b,          buffer size\n");
	syslog(LOG_INFO,"-d,          capture duration(second)\n");
	syslog(LOG_INFO,"-k,          kill last record\n");
	syslog(LOG_INFO,"-t,          record and then play\n");
	syslog(LOG_INFO,"\n");
}

static audio_mgr_t *g_last_audio_mgr;
#ifdef CONFIG_KERNEL_FREERTOS
int cmd_arecord(int argc, char ** argv)
#elif defined(CONFIG_OS_NUTTX)
int main(int argc, char **argv)
#endif
{
	int c;
	unsigned int bits = 16;
	audio_mgr_t *audio_mgr = NULL;
	g_verbose = 0;
	g_capture_then_play = 0;

	audio_mgr = audio_mgr_create();
	if (!audio_mgr)
		return -1;

	/* default param */
	audio_mgr->rate = 16000;
	audio_mgr->channels = 3;
	strncpy(g_pcm_name, "default", sizeof(g_pcm_name));

	optind = 0;
	while ((c = getopt(argc, argv, "D:r:f:c:p:b:d:khlvt")) != -1) {
		switch (c) {
		case 'D':
			strncpy(g_pcm_name, optarg, sizeof(g_pcm_name));
			g_pcm_name[sizeof(g_pcm_name)-1]='\0';
			break;
		case 'r':
			audio_mgr->rate = atoi(optarg);
			break;
		case 'f':
			bits = atoi(optarg);
			break;
		case 'c':
			audio_mgr->channels = atoi(optarg);
			break;
		case 'p':
			audio_mgr->period_size = atoi(optarg);
			break;
		case 'b':
			audio_mgr->buffer_size = atoi(optarg);
			break;
		case 'd':
			audio_mgr->capture_duration = atoi(optarg);
			break;
		case 'l':
			if (!g_last_audio_mgr)
				g_last_audio_mgr = audio_mgr;
			g_capture_loop_enable = 1;
			break;
		case 'k':
			if (g_last_audio_mgr)
				g_last_audio_mgr->in_aborting = 1;
			g_capture_loop_enable = 0;
			goto err;
		case 't':
			g_capture_then_play = 1;
			break;
		case 'v':
			g_verbose = 1;
			break;
		default:
			usage();
			goto err;
		}
	}

	switch (bits) {
	case 16:
		audio_mgr->format = SND_PCM_FORMAT_S16_LE;
		break;
	case 24:
		audio_mgr->format = SND_PCM_FORMAT_S24_LE;
		break;
	default:
		syslog(LOG_ERR,"%u bits not supprot\n", bits);
		return -1;
	}

	if (optind < argc) {
		capture_fs_wav(audio_mgr, argv[optind]);
	} else {
		if (g_capture_then_play)
			capture_then_play(audio_mgr);
		else
			capture_fs_wav(audio_mgr, NULL);
	}

err:
	audio_mgr_release(audio_mgr);
	g_last_audio_mgr = NULL;

	return 0;
}
#ifdef CONFIG_KERNEL_FREERTOS
FINSH_FUNCTION_EXPORT_CMD(cmd_arecord, arecord, Record voice);
#endif
