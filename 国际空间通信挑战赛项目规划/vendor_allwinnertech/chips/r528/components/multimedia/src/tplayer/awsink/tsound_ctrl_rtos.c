#define LOG_TAG "tsoundcontrol"
#include "tlog.h"
#include "tsound_ctrl.h"
#include <pthread.h>
#include <sys/time.h>
#include <assert.h>
#include "auGaincom.h"
#include "PostProcessCom.h"


PostProcessSt   PostProcPar;
int BLOCK_MODE = 0;
int NON_BLOCK_MODE = 1;
static int openSoundDevice(SoundCtrlContext* sc,int mode);
static int closeSoundDevice(SoundCtrlContext* sc);
static int setSoundDeviceParams(SoundCtrlContext* sc);
static int openSoundDevice(SoundCtrlContext* sc ,int mode)
{
    int ret = 0;
    logd("openSoundDevice()\n");
    if(!sc->alsa_handler){
        if((ret = snd_pcm_open(&sc->alsa_handler, "default",SND_PCM_STREAM_PLAYBACK ,mode))<0)
        {
            loge("open audio device failed:%s, errno = %d",strerror(errno),errno);
        }
    }else{
        logd("the audio device has been opened\n");
    }
    return ret;
}

static int closeSoundDevice(SoundCtrlContext* sc)
{
    int ret = 0;
    logd("closeSoundDevice()\n");
    if (sc->alsa_handler){
        if ((ret = snd_pcm_close(sc->alsa_handler)) < 0)
        {
            loge("snd_pcm_close failed:%s\n",strerror(errno));
        }
        else
        {
            sc->alsa_handler = NULL;
            logd("alsa-uninit: pcm closed\n");
        }
    }
    return ret;
}

static int setSoundDeviceParams(SoundCtrlContext* sc)
{
    int ret = 0;
    logd("setSoundDeviceParams()\n");
    sc->bytes_per_sample = sc->nChannelNum*sunxi_snd_pcm_format_physical_width(sc->alsa_format) / 8;
    sc->alsa_fragcount = 8;//cache count
    if(sc->nSampleRate==44100)
        sc->chunk_size = 1470;//each cache size,unit : sample
    else
        sc->chunk_size = 1024;
    if ((ret = snd_pcm_hw_params_malloc(&(sc->alsa_hwparams))) < 0)
    {
        loge("snd_pcm_hw_params_alloca failed:%s\n",strerror(errno));
        return ret;
    }

    //snd_pcm_hw_params_alloca(&(sc->alsa_hwparams));

    if ((ret = snd_pcm_hw_params_any(sc->alsa_handler, sc->alsa_hwparams)) < 0)
    {
        loge("snd_pcm_hw_params_any failed:%s\n",strerror(errno));
        goto SET_PAR_ERR;
    }

    if ((ret = snd_pcm_hw_params_set_access(sc->alsa_handler, sc->alsa_hwparams,
                SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
    {
        loge("snd_pcm_hw_params_set_access failed:%s\n",strerror(errno));
        goto SET_PAR_ERR;
    }

    if ((ret = snd_pcm_hw_params_set_format(sc->alsa_handler, sc->alsa_hwparams,
                sc->alsa_format)) < 0)
    {
        loge("snd_pcm_hw_params_set_format failed:%s\n",strerror(errno));
        goto SET_PAR_ERR;
    }

    if ((ret = snd_pcm_hw_params_set_channels(sc->alsa_handler,
            sc->alsa_hwparams, sc->nChannelNum)) < 0) {
        loge("snd_pcm_hw_params_set_channels failed:%s\n",strerror(errno));
        goto SET_PAR_ERR;
    }

    if ((ret = snd_pcm_hw_params_set_rate(sc->alsa_handler, sc->alsa_hwparams,
            sc->nSampleRate, NULL)) < 0) {
        loge("snd_pcm_hw_params_set_rate failed:%s\n",strerror(errno));
        goto SET_PAR_ERR;
    }

    snd_pcm_uframes_t period_size_tmp = sc->chunk_size;
    if ((ret = snd_pcm_hw_params_set_period_size_near(sc->alsa_handler,
    sc->alsa_hwparams, &period_size_tmp, NULL)) < 0) {
        loge("snd_pcm_hw_params_set_period_size fail , MSGTR_AO_ALSA_UnableToSetPeriodSize\n");
        goto SET_PAR_ERR;
    } else {
        logd("alsa-init: chunksize set to %ld(%ld)\n", period_size_tmp, sc->chunk_size);
        if (period_size_tmp != sc->chunk_size) {
                logd("period size changed (request: %lu, get: %lu)\n",
                                sc->chunk_size, period_size_tmp);
                sc->chunk_size = period_size_tmp;
        }
    }

//    if ((ret = snd_pcm_hw_params_set_periods(sc->alsa_handler,
//            sc->alsa_hwparams, sc->alsa_fragcount, 0)) < 0)
//    {
//        loge("snd_pcm_hw_params_set_periods fail , MSGTR_AO_ALSA_UnableToSetPeriods\n");
//        goto SET_PAR_ERR;
//    } else {
//        logd("alsa-init: fragcount=%d\n", sc->alsa_fragcount);
//    }
    logd("format:%d, SampleRate:%d, ch:%d, mode:%d", sc->alsa_format, sc->nSampleRate, sc->nChannelNum, sc->alsa_open_mode);
    if ((ret = snd_pcm_hw_params(sc->alsa_handler, sc->alsa_hwparams)) < 0) {
        loge("snd_pcm_hw_params failed ret:%d, err:%s\n", ret, strerror(errno));
        goto SET_PAR_ERR;
    }

    sc->alsa_can_pause = snd_pcm_hw_params_can_pause(sc->alsa_hwparams);

    logd("setSoundDeviceParams():sc->alsa_can_pause = %d\n",sc->alsa_can_pause);
SET_PAR_ERR:
    snd_pcm_hw_params_free(sc->alsa_hwparams);

    return ret;

}

SoundCtrl* TSoundDeviceCreate(AudioFrameCallback callback,void* pUser){
    SoundCtrlContext* s;
    s = (SoundCtrlContext*)malloc(sizeof(SoundCtrlContext));
    TLOGD("TinaSoundDeviceInit()");
    if(s == NULL)
    {
        TLOGE("malloc SoundCtrlContext fail.");
        return NULL;
    }
    memset(s, 0, sizeof(SoundCtrlContext));
    s->base.ops = &mSoundControlOps;
    s->alsa_access_type = SND_PCM_ACCESS_RW_INTERLEAVED;
    s->nSampleRate = 48000;
    s->nChannelNum = 2;
    s->alsa_format = SND_PCM_FORMAT_S16_LE;
    s->alsa_can_pause = 0;
    s->sound_status = STATUS_STOP;
    s->mVolume = 0;
    s->mSoundChannelMode = s->nChannelNum;
    s->mAudioframeCallback = callback;
    s->pUserData = pUser;
    pthread_mutex_init(&s->mutex, NULL);
    int ret = openSoundDevice(s, BLOCK_MODE);
    if(ret != 0){
        TLOGD("open sound device fail");
        free(s);
        s = NULL;
        return NULL;
    }

    //Initialize parameter for aduio post pocess
    s->fadein_flag = 0;
    s->spectrum_flag = 0;
    s->eq_flag = AUD_EQ_TYPE_NORMAL;
    memset(s->usr_eq_filter, 0, USR_EQ_BAND_CNT * sizeof(short));
    s->vps_flag = 0;
    s->vps_change = 0;
    s->outputPcmBuf = (char*)malloc(AUDIO_POST_PROC_PCM_BUFSIZE);
    memset(&PostProcPar, 0, sizeof(PostProcessSt));
    PostProcPar.InputPcmBuf = (databuf *)malloc(sizeof(databuf));
    PostProcPar.OutputPcmBuf = (databuf *)malloc(sizeof(databuf));
    if(!PostProcPar.InputPcmBuf || !PostProcPar.OutputPcmBuf || !s->outputPcmBuf)
    {
        TLOGW("Tsound:PostProcPar: malloc databuf or outputPcmBuf failed!");
        free(s);
        s = NULL;
        return NULL;
    }
    return (SoundCtrl*)&s->base;
}

void TSoundDeviceDestroy(SoundCtrl* s){
    SoundCtrlContext* sc;
    sc = (SoundCtrlContext*)s;
    pthread_mutex_lock(&sc->mutex);
    logd("TSoundDeviceDestroy(),close sound device\n");
    closeSoundDevice(sc);
    pthread_mutex_unlock(&sc->mutex);
    pthread_mutex_destroy(&sc->mutex);
    free(sc);
    sc = NULL;
}

void TSoundDeviceSetFormat(SoundCtrl* s, CdxPlaybkCfg* cfg){
    SoundCtrlContext* sc;
    sc = (SoundCtrlContext*)s;
    pthread_mutex_lock(&sc->mutex);
    logd("TSoundDeviceSetFormat(),sc->sound_status == %d\n",sc->sound_status);
    if(sc->sound_status == STATUS_STOP){
    	sc->nSampleRate = cfg->nSamplerate;
    	sc->nChannelNum = cfg->nChannels;
    	sc->alsa_format = SND_PCM_FORMAT_S16_LE;
    	sc->bytes_per_sample = sc->nChannelNum*sunxi_snd_pcm_format_physical_width(sc->alsa_format) / 8;
    	logd("TSoundDeviceSetFormat()>>>sample_rate:%d,channel_num:%d,sc->bytes_per_sample:%d\n",
    		cfg->nSamplerate,cfg->nChannels,sc->bytes_per_sample);
    }
    pthread_mutex_unlock(&sc->mutex);
}

int TSoundDeviceStart(SoundCtrl* s){
    SoundCtrlContext* sc;
    sc = (SoundCtrlContext*)s;
    pthread_mutex_lock(&sc->mutex);
    int ret = 0;
    logd("TinaSoundDeviceStart(): sc->sound_status = %d\n",sc->sound_status);
    if(sc->sound_status == STATUS_START){
    	logd("Sound device already start.\n");
    	pthread_mutex_unlock(&sc->mutex);
    	return ret;
    }else if(sc->sound_status == STATUS_PAUSE){
    	if(snd_pcm_state(sc->alsa_handler) == SND_PCM_STATE_SUSPENDED){
    		logd("MSGTR_AO_ALSA_PcmInSuspendModeTryingResume\n");
    		while((ret = snd_pcm_resume(sc->alsa_handler)) == -EAGAIN){
    			sleep(1);
    		}
    	}
    	if(sc->alsa_can_pause){
            if((ret = snd_pcm_pause(sc->alsa_handler, 0))<0){
            	loge("snd_pcm_pause failed:%s\n",strerror(errno));
            	pthread_mutex_unlock(&sc->mutex);
            	return ret;
            }
        }else{
            if ((ret = snd_pcm_prepare(sc->alsa_handler)) < 0)
            {
            	loge("snd_pcm_prepare failed:%s\n",strerror(errno));
            	pthread_mutex_unlock(&sc->mutex);
            	return ret;
            }
        }
    	sc->sound_status = STATUS_START;
    }
    else if(sc->sound_status == STATUS_STOP){
        ret = setSoundDeviceParams(sc);
        if(ret < 0){
            loge("setSoundDeviceParams fail , ret = %d\n",ret);
            pthread_mutex_unlock(&sc->mutex);
            return ret;
        }
        sc->sound_status = STATUS_START;
    }
    pthread_mutex_unlock(&sc->mutex);
    return ret;
}

int TSoundDeviceStop(SoundCtrl* s){
    int ret = 0;
    SoundCtrlContext* sc;
    sc = (SoundCtrlContext*)s;
    pthread_mutex_lock(&sc->mutex);
    logd("TSoundDeviceStop():sc->sound_status = %d\n",sc->sound_status);
    if(sc->sound_status == STATUS_STOP)
    {
        logd("Sound device already stopped.\n");
    	pthread_mutex_unlock(&sc->mutex);
    	return ret;
    }else{
        if ((ret = snd_pcm_drop(sc->alsa_handler)) < 0)
            {
                loge("snd_pcm_drop():MSGTR_AO_ALSA_PcmPrepareError\n");
                pthread_mutex_unlock(&sc->mutex);
                return ret;
    	}
    	if ((ret = snd_pcm_prepare(sc->alsa_handler)) < 0)
        {
            loge("snd_pcm_prepare():MSGTR_AO_ALSA_PcmPrepareError\n");
            pthread_mutex_unlock(&sc->mutex);
            return ret;
    	}
    	sc->sound_status = STATUS_STOP;
    }
    pthread_mutex_unlock(&sc->mutex);
    return ret;
}

int TSoundDevicePause(SoundCtrl* s){
    SoundCtrlContext* sc;
    sc = (SoundCtrlContext*)s;
    pthread_mutex_lock(&sc->mutex);
    int ret = 0;
    logd("TSoundDevicePause(): sc->sound_status = %d\n",sc->sound_status);
    if(sc->sound_status == STATUS_START){
    	if(sc->alsa_can_pause){
            logd("alsa can pause,use snd_pcm_pause\n");
            ret = snd_pcm_pause(sc->alsa_handler, 1);
            if(ret<0){
    		    loge("snd_pcm_pause failed:%s\n",strerror(errno));
    		    pthread_mutex_unlock(&sc->mutex);
                return ret;
            }
        }else{
            logd("alsa can not pause,use snd_pcm_drop\n");
            if ((ret = snd_pcm_drop(sc->alsa_handler)) < 0)
            {
        		loge("snd_pcm_drop failed:%s\n",strerror(errno));
        		pthread_mutex_unlock(&sc->mutex);
        		return ret;
            }
    	}
	    sc->sound_status = STATUS_PAUSE;
    }else{
	    logd("RTSoundDevicePause(): pause in an invalid status,status = %d\n",sc->sound_status);
    }
    pthread_mutex_unlock(&sc->mutex);
    return ret;
}

int TSoundDeviceFlush(SoundCtrl* s,void *block){
    logd("to do");
    return 0;
}


int TSoundDeviceWrite(SoundCtrl* s, void* pData, int nDataSize){
    int ret = 0;
    SoundCtrlContext* sc;
    sc = (SoundCtrlContext*)s;
    //logd("TinaSoundDeviceWrite:sc->bytes_per_sample = %d\n",sc->bytes_per_sample);
    if(sc->bytes_per_sample == 0){
	    sc->bytes_per_sample = 4;
    }
    if(sc->sound_status == STATUS_STOP || sc->sound_status == STATUS_PAUSE)
    {
        return ret;
    }
    //logd("TinaSoundDeviceWrite>>> pData = %p , nDataSize = %d\n",pData,nDataSize);
    int num_frames = nDataSize / sc->bytes_per_sample;
    snd_pcm_sframes_t res = 0;

    if (!sc->alsa_handler)
    {
        loge("MSGTR_AO_ALSA_DeviceConfigurationError\n");
	    return -1;
    }

    if (num_frames == 0){
	    loge("num_frames == 0\n");
	    return -1;
    }

    //store the pcm data and callback to tplayer
    /*
    SoundPcmData pcmData;
    memset(&pcmData, 0x00, sizeof(SoundPcmData));
    pcmData.pData = pData;
    pcmData.nSize = nDataSize;
    pcmData.samplerate = sc->nSampleRate;
    pcmData.channels = sc->nChannelNum;
    pcmData.accuracy = 16;
    */
    //sc->mAudioframeCallback(sc->pUserData,&pcmData);
#if 0
    //adjust the pcm data
    AudioGain audioGain;
    audioGain.preamp = sc->mVolume;
    audioGain.InputChan = (int)sc->nChannelNum;
    audioGain.OutputChan = (int)sc->nChannelNum;
    audioGain.InputLen = nDataSize;
    audioGain.InputPtr = (short*)pData;
    audioGain.OutputPtr = (short*)pData;
    int gainRet = tina_do_AudioGain(&audioGain);
    if(gainRet == 0){
	TLOGE("tina_do_AudioGain fail");
    }
#endif

    do {
    	res = snd_pcm_writei(sc->alsa_handler, pData, num_frames);
    	if (res == -EINTR)
            {
    		/* nothing to do */
    		res = 0;
    	} else if (res == -ESTRPIPE)
    	{ /* suspend */
                logd("MSGTR_AO_ALSA_PcmInSuspendModeTryingResume\n");
                while ((res = snd_pcm_resume(sc->alsa_handler)) == -EAGAIN)
    		        sleep(1);
    	}
    	if (res < 0)
        {
            loge("MSGTR_AO_ALSA_WriteError,res = %ld\n",res);
            if ((res = snd_pcm_prepare(sc->alsa_handler)) < 0)
            {
                loge("MSGTR_AO_ALSA_PcmPrepareError\n");
                return res;
    	    }
    	}
    } while (res == 0);
    return res < 0 ? res : res * sc->bytes_per_sample;
}

int TSoundDeviceReset(SoundCtrl* s){
	logd("TSoundDeviceReset()\n");
	return TSoundDeviceStop(s);
}

int TSoundDeviceGetCachedTime(SoundCtrl* s){
    int ret = 0;
    SoundCtrlContext* sc;
    sc = (SoundCtrlContext*)s;
    //TLOGD("TinaSoundDeviceGetCachedTime()");
    
    if (sc->alsa_handler)
    {
        snd_pcm_sframes_t delay = 0;
        //notify:snd_pcm_delay means the cache has how much data(the cache has been filled with pcm data),
        //snd_pcm_avail_update means the free cache,
        if ((ret = snd_pcm_delay(sc->alsa_handler, &delay)) < 0){
		    loge("TinaSoundDeviceGetCachedTime(),ret = %d , delay = %ld\n",ret,delay);
		    return ret;
        }
		//logd("TinaSoundDeviceGetCachedTime(),ret = %d , delay = %ld",ret,delay);
        ret = ((int)((float) delay * 1000000 / (float) sc->nSampleRate));
    }
    
    return ret;
}

int TSoundDeviceGetFrameCount(SoundCtrl* s){
    //to do
    return 0;
}

int TSoundDeviceSetPlaybackRate(SoundCtrl* s,const XAudioPlaybackRate *rate){
    //to do
    return 0;
}
int TSoundDeviceSetVolume(SoundCtrl* s,int volume){
    SoundCtrlContext* sc;
    sc = (SoundCtrlContext*)s;
    if(sc){
        sc->mVolume = volume;
        return 0;
    }else{
        return -1;
    }
}

int TSoundDeviceControl(SoundCtrl* s, int cmd, void* para)
{
    SoundCtrlContext* sc;
    sc = (SoundCtrlContext*)s;
    if(sc){
        return 0;
    }else{
        return -1;
    }
}
