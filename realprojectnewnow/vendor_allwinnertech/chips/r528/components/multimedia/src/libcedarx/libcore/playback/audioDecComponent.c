/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : audioDecComponent.cpp
 * Description : audio decoder component
 * History :
 *
 */

#include "cdx_log.h"

#include <pthread.h>
#include <semaphore.h>
#include <malloc.h>
#include <memory.h>
#include <time.h>

#include "audioDecComponent.h"
#include "adecoder.h"

#include "AwMessageQueue.h"
#include "streamManager.h"
#include "baseComponent.h"
//#include <dlfcn.h>

static long long GetNowUs(void)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    return now.tv_sec * 1000000 + now.tv_usec;
}

#define MAX_AUDIO_STREAM_BUFFER_SIZE (2*1024*1024)
#define MAX_AUDIO_STREAM_FRAME_COUNT (4096)

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "audioDecItf"

#define ALIB_NAME "libadecoder.so"

typedef struct _AllwinnerAudioItf
{
    void *mLibHandle;
    AudioDecoder* libadecoder;
    int (*ParserRequestBsBuffer)(AudioDecoder* pDecoder,
                              int                 nRequireSize,
                              unsigned char**     ppBuf,
                              int*                 pBufSize,
                              unsigned char**     ppRingBuf,
                              int*                 pRingBufSize,
                              int*                 nOffset);

    int (*ParserUpdateBsBuffer)(AudioDecoder* pDecoder,
                             int                nFilledLen,
                             int64_t            nTimeStamp,
                             int nOffset);

    void (*BsQueryQuality)(AudioDecoder* pDecoder,
                        int*               pValidPercent,
                        int*               vbv);

    int (*AudioStreamDataSize)(AudioDecoder* pDecoder);

    int (*AudioStreamBufferSize)(void);

    int (*AudioStreamBufferMaxFrameNum)(void);

    int (*AudioPCMDataSize)(AudioDecoder* pDecoder);

    int (*DecRequestPcmBuffer)(AudioDecoder* pDecoder, char **pOutWritePtr);

    int (*DecUpdatePcmBuffer)(AudioDecoder* pDecoder, int nPcmOutSize);

    int (*PlybkRequestPcmBuffer)(AudioDecoder* pDecoder, unsigned char **pOutReadPtr, int *psize);

    int (*PlybkUpdatePcmBuffer)(AudioDecoder* pDecoder, int nPcmOutSize);

    int64_t (*PlybkRequestPcmPts)(AudioDecoder* pDecoder);

    void (*PcmQueryQuality)(AudioDecoder* pDecoder, int* pValidPercent, int* vbv);

    void (*AudioDecoderSeek)(AudioDecoder* pDecoder,    int64_t nSeekTime);

    int (*InitializeAudioDecoder)(AudioDecoder*   pDecoder,
                               AudioStreamInfo*     pAudioStreamInfo,
                               BsInFor*             pBsInFor);

    int (*ResetAudioDecoder)(AudioDecoder*    pDecoder, int64_t nSeekTime);

    int (*DecodeAudioStream)(AudioDecoder*    pDecoder,
                          AudioStreamInfo*        pAudioStreamInfo,
                          char*                 ppBuf,
                          int*                    pBufSize);

    int (*DestroyAudioDecoder)(AudioDecoder* pDecoder);

    AudioDecoder* (*CreateAudioDecoder)(void);

#if defined(__ANDROID__)
    void (*SetRawPlayParam)(AudioDecoder* pDecoder,void *self);
#else
    void (*SetRawPlayParam)(AudioDecoder* pDecoder,void *self,int flag);
#endif
}AllwinnerAudioItf;

typedef struct _alib_func_map{
    const char* name;
    size_t      offset;
}alib_func_map;

const alib_func_map ALIB_FUNCS[]={
    {"ParserRequestBsBuffer", CdxOffsetof(AllwinnerAudioItf, ParserRequestBsBuffer)},
    {"ParserUpdateBsBuffer", CdxOffsetof(AllwinnerAudioItf, ParserUpdateBsBuffer)},
    {"BsQueryQuality", CdxOffsetof(AllwinnerAudioItf, BsQueryQuality)},
    {"AudioStreamDataSize", CdxOffsetof(AllwinnerAudioItf, AudioStreamDataSize)},
    {"AudioStreamBufferSize", CdxOffsetof(AllwinnerAudioItf, AudioStreamBufferSize)},
    {"AudioStreamBufferMaxFrameNum", CdxOffsetof(AllwinnerAudioItf, AudioStreamBufferMaxFrameNum)},
    {"AudioPCMDataSize", CdxOffsetof(AllwinnerAudioItf, AudioPCMDataSize)},
    {"DecRequestPcmBuffer", CdxOffsetof(AllwinnerAudioItf, DecRequestPcmBuffer)},
    {"DecUpdatePcmBuffer", CdxOffsetof(AllwinnerAudioItf, DecUpdatePcmBuffer)},
    {"PlybkRequestPcmBuffer", CdxOffsetof(AllwinnerAudioItf, PlybkRequestPcmBuffer)},
    {"PlybkUpdatePcmBuffer", CdxOffsetof(AllwinnerAudioItf, PlybkUpdatePcmBuffer)},
    {"PlybkRequestPcmPts", CdxOffsetof(AllwinnerAudioItf, PlybkRequestPcmPts)},
    {"PcmQueryQuality", CdxOffsetof(AllwinnerAudioItf, PcmQueryQuality)},
    {"AudioDecoderSeek", CdxOffsetof(AllwinnerAudioItf, AudioDecoderSeek)},
    {"InitializeAudioDecoder", CdxOffsetof(AllwinnerAudioItf, InitializeAudioDecoder)},
    {"ResetAudioDecoder", CdxOffsetof(AllwinnerAudioItf, ResetAudioDecoder)},
    {"DecodeAudioStream", CdxOffsetof(AllwinnerAudioItf, DecodeAudioStream)},
    {"DestroyAudioDecoder", CdxOffsetof(AllwinnerAudioItf, DestroyAudioDecoder)},
    {"CreateAudioDecoder", CdxOffsetof(AllwinnerAudioItf, CreateAudioDecoder)},
    {"SetRawPlayParam", CdxOffsetof(AllwinnerAudioItf, SetRawPlayParam)}
};

const int ALIB_FUNC_NUM = sizeof(ALIB_FUNCS)/sizeof(ALIB_FUNCS[0]);

struct AudioDecComp
{
    //* created at initialize time.
    AwMessageQueue*        mq;

    BaseCompCtx             base;

    sem_t                   streamDataSem;
    sem_t                   frameBufferSem;

    pthread_t               sDecodeThread;

    enum EPLAYERSTATUS      eStatus;

    //* objects set by user.
    AvTimer*                pAvTimer;
    PlayerCallback          callback;
    void*                   pUserData;
    int                     bEosFlag;

    int                     nOffset;
    BsInFor                 bsInfo;
    AllwinnerAudioItf       AlibItf;
    int                     nStreamCount;
    int                     nStreamSelected;

    AudioStreamInfo*        pStreamInfoArr;
    StreamManager**         pStreamManagerArr;
    pthread_mutex_t         streamManagerMutex;

    pthread_mutex_t         decoderDestroyMutex;    //* to protect decoder from destroyed.

    int                     bCrashFlag;

    int                     afterSwitchStream;
    int                     bFirstFramePtsValid;
};

static void handleStart(AwMessage *msg, void *arg);
static void handleStop(AwMessage *msg, void *arg);
static void handlePause(AwMessage *msg, void *arg);
static void handleReset(AwMessage *msg, void *arg);
static void handleSetEos(AwMessage *msg, void *arg);
static void handleQuit(AwMessage *msg, void *arg);
static void doDecode(AwMessage *msg, void *arg);

static void* AudioDecodeThread(void* arg);
#if 0
static void FlushStreamManagerBuffers(AudioDecComp* p, int64_t curTime,
                                    int bIncludeSeletedStream);
#endif

#define CONFIG_COMPILE_STATIC_LIB (1)
AudioDecComp* AudioDecCompCreate(void)
{
    AudioDecComp* p;
    int                  err;
    int idx = 0;
    void *dst = 0, *func = 0;

    p = (AudioDecComp*)malloc(sizeof(AudioDecComp));
    if(p == NULL)
    {
        loge("memory alloc fail.");
        return NULL;
    }
    memset(p, 0, sizeof(*p));

#ifndef CONFIG_COMPILE_STATIC_LIB
    p->AlibItf.mLibHandle = dlopen(ALIB_NAME, RTLD_NOW);
    if(!p->AlibItf.mLibHandle)
    {
        loge("libadecoder.so dlopen fail for %s", dlerror());
        free(p);
        return NULL;
    }
    logd("libadecoder.so dlopen success, to load internal symbols...");

    err = 0;

    while(idx < ALIB_FUNC_NUM)
    {
        dst = (void*)((char*)(&p->AlibItf) + ALIB_FUNCS[idx].offset);
        func = (void*)(dlsym(p->AlibItf.mLibHandle, ALIB_FUNCS[idx].name));

        if(!func)
        {
            logd("func(%s) connot link!", ALIB_FUNCS[idx].name);
            err = -1;
            break;
        }
        logd("func(%s) link success...", ALIB_FUNCS[idx].name);
        memcpy(dst, &func, sizeof(void*));
        idx++;
    }

    if((&p->AlibItf.SetRawPlayParam) != dst)
    {
        loge("SetRawPlayParam(%p) compare to last offset begin from &p->AlibItf(%p) is different!",
            &p->AlibItf.SetRawPlayParam, dst);
        err = -1;
    }

    if(err != 0)
    {
        loge("dlclose libadecoder.so for some reason...");
        dlclose(p->AlibItf.mLibHandle);
        p->AlibItf.mLibHandle = 0;
        free(p);
        return NULL;
    }
#else
    p->AlibItf.ParserRequestBsBuffer        = ParserRequestBsBuffer;
    p->AlibItf.ParserUpdateBsBuffer         = ParserUpdateBsBuffer;
    p->AlibItf.BsQueryQuality               = BsQueryQuality;
    p->AlibItf.AudioStreamDataSize          = AudioStreamDataSize;
    p->AlibItf.AudioStreamBufferSize        = AudioStreamBufferSize;
    p->AlibItf.AudioStreamBufferMaxFrameNum = AudioStreamBufferMaxFrameNum;
    p->AlibItf.AudioPCMDataSize             = AudioPCMDataSize;
    p->AlibItf.DecRequestPcmBuffer          = DecRequestPcmBuffer;
    p->AlibItf.DecUpdatePcmBuffer           = DecUpdatePcmBuffer;
    p->AlibItf.PlybkRequestPcmBuffer        = PlybkRequestPcmBuffer;
    p->AlibItf.PlybkUpdatePcmBuffer         = PlybkUpdatePcmBuffer;
    p->AlibItf.PlybkRequestPcmPts           = PlybkRequestPcmPts;
    p->AlibItf.PcmQueryQuality              = PcmQueryQuality;
    p->AlibItf.AudioDecoderSeek             = AudioDecoderSeek;
    p->AlibItf.InitializeAudioDecoder       = InitializeAudioDecoder;
    p->AlibItf.ResetAudioDecoder            = ResetAudioDecoder;
    p->AlibItf.DecodeAudioStream            = DecodeAudioStream;
    p->AlibItf.DestroyAudioDecoder          = DestroyAudioDecoder;
    p->AlibItf.CreateAudioDecoder           = CreateAudioDecoder;
    p->AlibItf.SetRawPlayParam              = SetRawPlayParam;
#endif
    p->mq = AwMessageQueueCreate(4, "AudioDecodeMq");
    if(p->mq == NULL)
    {
        loge("audio decoder component create message queue fail.");
        free(p);
        return NULL;
    }

    BaseMsgHandler handler = {
        .start = handleStart,
        .stop = handleStop,
        .pause = handlePause,
        .reset = handleReset,
        .setEos = handleSetEos,
        .quit = handleQuit,
        .decode = doDecode,
    };

    if (BaseCompInit(&p->base, "audio decoder", p->mq, &handler))
    {
        AwMessageQueueDestroy(p->mq);
        free(p);
        return NULL;
    }

    sem_init(&p->streamDataSem, 0, 0);
    sem_init(&p->frameBufferSem, 0, 0);
    pthread_mutex_init(&p->streamManagerMutex, NULL);
    pthread_mutex_init(&p->decoderDestroyMutex, NULL);

    p->eStatus = PLAYER_STATUS_STOPPED;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    struct sched_param param;
    param.sched_priority = 45;//16 17 45
    pthread_attr_setschedparam(&attr, &param);
    pthread_attr_setstacksize(&attr, 32768);

    err = pthread_create(&p->sDecodeThread, &attr, AudioDecodeThread, p);
    if(err != 0)
    {
        loge("audio decode component create thread fail.");
        BaseCompDestroy(&p->base);
        sem_destroy(&p->streamDataSem);
        sem_destroy(&p->frameBufferSem);
        pthread_mutex_destroy(&p->streamManagerMutex);
        pthread_mutex_destroy(&p->decoderDestroyMutex);
        AwMessageQueueDestroy(p->mq);
        free(p);
        return NULL;
    }

    pthread_setname_np(p->sDecodeThread, "AudioDecode");
    return p;
}

static void WakeUpThread(void *arg)
{
    AudioDecComp *p = arg;
    //* wake up the thread if it is pending for stream data or frame buffer.
    sem_post(&p->streamDataSem);
    sem_post(&p->frameBufferSem);
}

int AudioDecCompDestroy(AudioDecComp* p)
{
    void*                status;
    int                  i;

    BaseCompQuit(&p->base, WakeUpThread, p);

    pthread_join(p->sDecodeThread, &status);

    BaseCompDestroy(&p->base);

    sem_destroy(&p->streamDataSem);
    sem_destroy(&p->frameBufferSem);
    pthread_mutex_destroy(&p->streamManagerMutex);
    pthread_mutex_destroy(&p->decoderDestroyMutex);

    if(p->pStreamInfoArr != NULL)
    {
        for(i=0; i<p->nStreamCount; i++)
        {
            if(p->pStreamInfoArr[i].pCodecSpecificData != NULL &&
                    p->pStreamInfoArr[i].nCodecSpecificDataLen > 0)
            {
                free(p->pStreamInfoArr[i].pCodecSpecificData);
                p->pStreamInfoArr[i].pCodecSpecificData = NULL;
            }
        }
        free(p->pStreamInfoArr);
    }

    if(p->pStreamManagerArr != NULL)
    {
        for(i=0; i<p->nStreamCount; i++)
        {
            if(p->pStreamManagerArr[i] != NULL)
            {
                StreamManagerDestroy(p->pStreamManagerArr[i]);
                p->pStreamManagerArr[i] = NULL;
            }
        }
        free(p->pStreamManagerArr);
    }

    AwMessageQueueDestroy(p->mq);

    if(p->AlibItf.libadecoder)
    {
        logd("DestroyAudioDecoder!");
        p->AlibItf.DestroyAudioDecoder(p->AlibItf.libadecoder);
        p->AlibItf.libadecoder = NULL;
    }

#ifndef CONFIG_COMPILE_STATIC_LIB
    if(p->AlibItf.mLibHandle)
    {
        logd("libadecoder.so close success......");
        dlclose(p->AlibItf.mLibHandle);
        p->AlibItf.mLibHandle = 0;
    }
#endif
    free(p);

    return 0;
}

int AudioDecCompStart(AudioDecComp* p)
{
    return BaseCompStart(&p->base, WakeUpThread, p);
}

int AudioDecCompStop(AudioDecComp* p)
{
    return BaseCompStop(&p->base, WakeUpThread, p);
}

int AudioDecCompPause(AudioDecComp* p)
{
    return BaseCompPause(&p->base, WakeUpThread, p);
}

enum EPLAYERSTATUS AudioDecCompGetStatus(AudioDecComp* p)
{
    return p->eStatus;
}

int AudioDecCompReset(AudioDecComp* p, int64_t nSeekTime)
{
    return BaseCompReset(&p->base, nSeekTime, WakeUpThread, p);
}

int AudioDecCompSetEOS(AudioDecComp* p)
{
    return BaseCompSetEos(&p->base, WakeUpThread, p);
}

int AudioDecCompSetCallback(AudioDecComp* p, PlayerCallback callback, void* pUserData)
{
    p->callback  = callback;
    p->pUserData = pUserData;

    return 0;
}

int AudioDecCompSetAudioStreamInfo(AudioDecComp*    p,
                                   AudioStreamInfo* pStreamInfo,
                                   int              nStreamCount,
                                   int              nDefaultStreamIndex)
{
    int i;

    if(p->pStreamInfoArr != NULL && p->nStreamCount > 0)
    {
        for(i=0; i<p->nStreamCount; i++)
        {
            if(p->pStreamInfoArr[i].pCodecSpecificData != NULL &&
               p->pStreamInfoArr[i].nCodecSpecificDataLen > 0)
            {
                free(p->pStreamInfoArr[i].pCodecSpecificData);
                p->pStreamInfoArr[i].pCodecSpecificData = NULL;
                p->pStreamInfoArr[i].nCodecSpecificDataLen = 0;
            }
        }

        free(p->pStreamInfoArr);
        p->pStreamInfoArr = NULL;
    }

    if(p->pStreamManagerArr != NULL)
    {
        for(i=0; i<p->nStreamCount; i++)
        {
            if(p->pStreamManagerArr[i] != NULL)
            {
                StreamManagerDestroy(p->pStreamManagerArr[i]);
                p->pStreamManagerArr[i] = NULL;
            }
        }
        free(p->pStreamManagerArr);
    }

    p->nStreamSelected = 0;
    p->nStreamCount = 0;

    p->pStreamInfoArr = (AudioStreamInfo*)malloc(sizeof(AudioStreamInfo)*nStreamCount);
    if(p->pStreamInfoArr == NULL)
    {
        loge("memory malloc fail!");
        return -1;
    }
    memset(p->pStreamInfoArr, 0, sizeof(AudioStreamInfo)*nStreamCount);

    for(i=0; i<nStreamCount; i++)
    {
        memcpy(&p->pStreamInfoArr[i], &pStreamInfo[i], sizeof(AudioStreamInfo));
        if(pStreamInfo[i].pCodecSpecificData != NULL && pStreamInfo[i].nCodecSpecificDataLen > 0)
        {
            p->pStreamInfoArr[i].pCodecSpecificData =
                            (char*)malloc(pStreamInfo[i].nCodecSpecificDataLen);
            if(p->pStreamInfoArr[i].pCodecSpecificData == NULL)
            {
                loge("malloc memory fail.");
                p->pStreamInfoArr[i].nCodecSpecificDataLen = 0;
                break;
            }
            memcpy(p->pStreamInfoArr[i].pCodecSpecificData,
                   pStreamInfo[i].pCodecSpecificData,
                   pStreamInfo[i].nCodecSpecificDataLen);
        }
    }

    if(i != nStreamCount)
    {
        //* memory alloc fail break.
        i--;
        for(; i>=0; i--)
        {
            if(p->pStreamInfoArr[i].pCodecSpecificData != NULL &&
                    p->pStreamInfoArr[i].nCodecSpecificDataLen > 0)
            {
                free(p->pStreamInfoArr[i].pCodecSpecificData);
                p->pStreamInfoArr[i].pCodecSpecificData = NULL;
                p->pStreamInfoArr[i].nCodecSpecificDataLen = 0;
            }
        }
        free(p->pStreamInfoArr);
        return -1;
    }

    p->pStreamManagerArr = (StreamManager**)malloc(nStreamCount*sizeof(StreamManager*));
    if(p->pStreamManagerArr == NULL)
    {
        loge("malloc memory fail.");
        for(i=0; i<nStreamCount; i++)
        {
            if(p->pStreamInfoArr[i].pCodecSpecificData != NULL &&
                    p->pStreamInfoArr[i].nCodecSpecificDataLen > 0)
            {
                free(p->pStreamInfoArr[i].pCodecSpecificData);
                p->pStreamInfoArr[i].pCodecSpecificData = NULL;
                p->pStreamInfoArr[i].nCodecSpecificDataLen = 0;
            }
        }
        free(p->pStreamInfoArr);
        return -1;
    }

    for(i=0; i<nStreamCount; i++)
    {
        p->pStreamManagerArr[i] = StreamManagerCreate(MAX_AUDIO_STREAM_BUFFER_SIZE,
                                                      MAX_AUDIO_STREAM_FRAME_COUNT,
                                                      i);
        if(p->pStreamManagerArr[i] == NULL)
        {
            loge("create stream manager for audio stream %d fail", i);
            break;
        }
    }

    if(i != nStreamCount)
    {
        //* memory alloc fail break.
        i--;
        for(; i>=0; i--)
        {
            StreamManagerDestroy(p->pStreamManagerArr[i]);
            p->pStreamManagerArr[i] = NULL;
        }
        free(p->pStreamManagerArr);
        p->pStreamManagerArr = NULL;

        for(i=0; i<nStreamCount; i++)
        {
            if(p->pStreamInfoArr[i].pCodecSpecificData != NULL &&
                    p->pStreamInfoArr[i].nCodecSpecificDataLen > 0)
            {
                free(p->pStreamInfoArr[i].pCodecSpecificData);
                p->pStreamInfoArr[i].pCodecSpecificData = NULL;
                p->pStreamInfoArr[i].nCodecSpecificDataLen = 0;
            }
        }
        free(p->pStreamInfoArr);
        return -1;
    }

    p->nStreamSelected = nDefaultStreamIndex;
    p->nStreamCount = nStreamCount;

    return 0;
}

int AudioDecCompAddAudioStream(AudioDecComp* p, AudioStreamInfo* pStreamInfo)
{
    pthread_mutex_lock(&p->streamManagerMutex);

    if(p->nStreamCount > 0)
    {
        AudioStreamInfo*  pStreamInfoArr;
        StreamManager**   pStreamManagerArr;
        int               nStreamCount;

        nStreamCount = p->nStreamCount + 1;
        pStreamManagerArr = (StreamManager**)malloc(sizeof(StreamManager*)*nStreamCount);
        if(pStreamManagerArr == NULL)
        {
            loge("malloc memory fail.");
            pthread_mutex_unlock(&p->streamManagerMutex);
            return -1;
        }

        pStreamInfoArr = (AudioStreamInfo*)malloc(sizeof(AudioStreamInfo)*nStreamCount);
        if(pStreamInfoArr == NULL)
        {
            loge("malloc memory fail.");
            free(pStreamManagerArr);
            pthread_mutex_unlock(&p->streamManagerMutex);
            return -1;
        }

        memcpy(pStreamManagerArr, p->pStreamManagerArr, p->nStreamCount * sizeof(StreamManager*));
        pStreamManagerArr[nStreamCount-1] = StreamManagerCreate(MAX_AUDIO_STREAM_BUFFER_SIZE,
                                                                MAX_AUDIO_STREAM_FRAME_COUNT,
                                                                nStreamCount-1);
        if(pStreamManagerArr[nStreamCount-1] == NULL)
        {
            loge("create stream manager fail.");
            free(pStreamManagerArr);
            free(pStreamInfoArr);
            pthread_mutex_unlock(&p->streamManagerMutex);
            return -1;
        }

        memcpy(pStreamInfoArr, p->pStreamInfoArr, p->nStreamCount*sizeof(AudioStreamInfo));
        memcpy(&pStreamInfoArr[nStreamCount-1], pStreamInfo, sizeof(AudioStreamInfo));
        if(pStreamInfo->pCodecSpecificData != NULL && pStreamInfo->nCodecSpecificDataLen > 0)
        {
            pStreamInfoArr[nStreamCount-1].pCodecSpecificData =
                                (char*)malloc(pStreamInfo->nCodecSpecificDataLen);
            if(pStreamInfoArr[nStreamCount-1].pCodecSpecificData == NULL)
            {
                loge("malloc memory fail.");
                free(pStreamManagerArr);
                free(pStreamInfoArr);
                pthread_mutex_unlock(&p->streamManagerMutex);
                return -1;
            }
            memcpy(pStreamInfoArr[nStreamCount-1].pCodecSpecificData,
                   pStreamInfo->pCodecSpecificData,
                   pStreamInfo->nCodecSpecificDataLen);
        }

        free(p->pStreamInfoArr);
        free(p->pStreamManagerArr);
        p->pStreamInfoArr    = pStreamInfoArr;
        p->pStreamManagerArr = pStreamManagerArr;
        p->nStreamCount      = nStreamCount;

        pthread_mutex_unlock(&p->streamManagerMutex);

        return 0;
    }
    else
    {
        pthread_mutex_unlock(&p->streamManagerMutex);
        return AudioDecCompSetAudioStreamInfo(p, pStreamInfo, 1, 0);
    }
}

int AudioDecCompGetAudioStreamCnt(AudioDecComp* p)
{
    return p->nStreamCount;
}

int AudioDecCompCurrentStreamIndex(AudioDecComp* p)
{
    return p->nStreamSelected;
}

int AudioDecCompGetAudioSampleRate(AudioDecComp* p,
                                   unsigned int* pSampleRate,
                                   unsigned int* pChannelNum,
                                   unsigned int* pBitRate)
{
    int                  i;

    if(p->nStreamCount <= 0)
        return -1;
    i = p->nStreamSelected;
    *pSampleRate = p->pStreamInfoArr[i].nSampleRate;
    *pChannelNum = p->pStreamInfoArr[i].nChannelNum;
    *pBitRate    = p->pStreamInfoArr[i].nAvgBitrate;
    return 0;
}

int AudioDecCompGetAudioStreamInfo(AudioDecComp* p, int* pStreamNum, AudioStreamInfo** ppStreamInfo)
{
    int                  i;
    AudioStreamInfo*     pStreamInfo;
    int                  nStreamCount;

    nStreamCount = p->nStreamCount;

    pStreamInfo = (AudioStreamInfo*)malloc(sizeof(AudioStreamInfo)*nStreamCount);
    if(pStreamInfo == NULL)
    {
        loge("memory malloc fail!");
        return -1;
    }
    memset(pStreamInfo, 0, sizeof(AudioStreamInfo)*nStreamCount);

    for(i=0; i<nStreamCount; i++)
    {
        memcpy(&pStreamInfo[i], &p->pStreamInfoArr[i], sizeof(AudioStreamInfo));
        if(p->pStreamInfoArr[i].pCodecSpecificData != NULL &&
                        p->pStreamInfoArr[i].nCodecSpecificDataLen > 0)
        {
            pStreamInfo[i].pCodecSpecificData =
                    (char*)malloc(p->pStreamInfoArr[i].nCodecSpecificDataLen);
            if(pStreamInfo[i].pCodecSpecificData == NULL)
            {
                loge("malloc memory fail.");
                pStreamInfo[i].nCodecSpecificDataLen = 0;
                break;
            }
            memcpy(pStreamInfo[i].pCodecSpecificData,
                   p->pStreamInfoArr[i].pCodecSpecificData,
                   p->pStreamInfoArr[i].nCodecSpecificDataLen);
        }
    }

    if(i != nStreamCount)
    {
        //* memory alloc fail break.
        i--;
        for(; i>=0; i--)
        {
            if(pStreamInfo[i].pCodecSpecificData != NULL &&
                    pStreamInfo[i].nCodecSpecificDataLen > 0)
            {
                free(pStreamInfo[i].pCodecSpecificData);
                pStreamInfo[i].pCodecSpecificData = NULL;
                pStreamInfo[i].nCodecSpecificDataLen = 0;
            }
        }
        free(pStreamInfo);
        return -1;
    }

    *pStreamNum = nStreamCount;
    *ppStreamInfo = pStreamInfo;

    return 0;
}

int AudioDecCompSetTimer(AudioDecComp* p, AvTimer* timer)
{
    p->pAvTimer = timer;
    return 0;
}

int AudioDecCompRequestStreamBuffer(AudioDecComp* p,
                                    int           nRequireSize,
                                    char**        ppBuf,
                                    int*          pBufSize,
                                    char**        ppRingBuf,
                                    int*          pRingBufSize,
                                    int           nStreamIndex)
{
    StreamManager*       pSm;
    char*                pStreamBufEnd;
    StreamFrame*         pTmpFrame;
    char*                pBuf0;
    char*                pBuf1;
    int                  nBufSize0;
    int                  nBufSize1;

    *ppBuf        = NULL;
    *ppRingBuf    = NULL;
    *pBufSize     = 0;
    *pRingBufSize = 0;

    pBuf0      = NULL;
    pBuf1      = NULL;
    nBufSize0  = 0;
    nBufSize1  = 0;

    pthread_mutex_lock(&p->streamManagerMutex);

    if(nStreamIndex < 0 || nStreamIndex >= p->nStreamCount)
    {
        loge("stream index invalid, stream index = %d, audio stream num = %d",
                    nStreamIndex, p->nStreamCount);
        pthread_mutex_unlock(&p->streamManagerMutex);
        return -1;
    }

    /* This is a waste of time. If decoder is crashed, p->bCrashFlag != 0,
     * we will call StreamManagerRequestStream in this function when necessary,
     * and free some space.
     *
     * 2016-05-17
     * You can remove this block of code after, say half a year.
     */
#if 0
    //* when decoder crashed, the main thread is not running,
    //* we need to flush stream buffers for demux keep going.
    if (p->bCrashFlag &&
            p->pAvTimer->GetStatus(p->pAvTimer) == TIMER_STATUS_START)
    {
        int64_t nCurTime = p->pAvTimer->GetTime(p->pAvTimer);
        for (int i = 0; i < p->nStreamCount; i++)
        {
            pSm = p->pStreamManagerArr[i];
            StreamManagerRewind(pSm, nCurTime);
        }
    }
#endif

    pSm = p->pStreamManagerArr[nStreamIndex];

    if(pSm == NULL)
    {
        loge("buffer for selected stream is not created, request buffer fail.");
        pthread_mutex_unlock(&p->streamManagerMutex);
        return -1;
    }

    if(nRequireSize > StreamManagerBufferSize(pSm))
    {
        loge("require size too big.");
        pthread_mutex_unlock(&p->streamManagerMutex);
        return -1;
    }

    if(nStreamIndex == p->nStreamSelected && p->bCrashFlag == 0)
    {
        if(StreamManagerRequestBuffer(pSm, nRequireSize, &pBuf0, &nBufSize0) < 0)
        {
            pthread_mutex_unlock(&p->streamManagerMutex);
            logv("request buffer fail.");
            return -1;
        }
    }
    else
    {
        while(StreamManagerRequestBuffer(pSm, nRequireSize, &pBuf0, &nBufSize0) < 0)
        {
            pTmpFrame = StreamManagerRequestStream(pSm);
            if(pTmpFrame != NULL)
                StreamManagerFlushStream(pSm, pTmpFrame);
            else
            {
                loge("all stream flushed but still can not allocate buffer.");
                pthread_mutex_unlock(&p->streamManagerMutex);
                return -1;
            }
        }
    }

    //* output the buffer.
    *ppBuf    = pBuf0;
    *pBufSize = nBufSize0;

    pthread_mutex_unlock(&p->streamManagerMutex);
    return 0;
}

int AudioDecCompSubmitStreamData(AudioDecComp*        p,
                                 AudioStreamDataInfo* pDataInfo,
                                 int                  nStreamIndex)
{
    int                  nSemCnt;
    StreamManager*       pSm;
    StreamFrame          streamFrame;
    //* submit data to stream manager
    int64_t asubmit_lock1 = GetNowUs();
    pthread_mutex_lock(&p->streamManagerMutex);
    
    int64_t asubmit_lock2 = GetNowUs();
    if(asubmit_lock2 -asubmit_lock1 > 15000)
    {
	loge("wht>>>demux,audio Submit mutex lock timeout:%lldms\n",(asubmit_lock2 - asubmit_lock1)/1000);
    }


    pSm = p->pStreamManagerArr[nStreamIndex];

    int64_t manager_time = GetNowUs();
    if(manager_time -asubmit_lock2 > 15000)
    {
	loge("wht>>>demux,audio pStreamManagerArr timeout:%lldms\n",(manager_time - asubmit_lock2/1000));
    }


    streamFrame.pData   = pDataInfo->pData;
    streamFrame.nLength = pDataInfo->nLength;
    
    if(pDataInfo->bIsFirstPart)
    {
        streamFrame.nPts = pDataInfo->nPts;
        streamFrame.nPcr = pDataInfo->nPcr;
    }
    else
    {
        streamFrame.nPts = -1;
        streamFrame.nPcr = -1;
    }

    int64_t asubmit_addstream1 = GetNowUs();

    if(asubmit_addstream1 - manager_time >15000)
    {
	loge("wht>>>demux,audio Submit stream frame timeout:%lldms\n",(asubmit_addstream1 - manager_time)/1000);
    }

    StreamManagerAddStream(pSm, &streamFrame);

    int64_t asubmit_addstream2 = GetNowUs();
    if(asubmit_addstream2 -asubmit_addstream1 > 15000)
    {
	loge("wht>>>demux,audio Submit add stream timeout:%lldms\n",(asubmit_addstream2 - asubmit_addstream1)/1000);
    }


    int64_t asubmit_unlock1 = GetNowUs();
    pthread_mutex_unlock(&p->streamManagerMutex);

    int64_t asubmit_unlock2 = GetNowUs();
    if(asubmit_unlock2 -asubmit_unlock1 > 15000)
    {
	loge("wht>>>demux,audio Submit mutex unlock timeout:%lldms\n",(asubmit_unlock2 - asubmit_unlock1)/1000);
    }


    int64_t asem1 = GetNowUs();
    if(sem_getvalue(&p->streamDataSem, &nSemCnt) == 0)
    {

    int64_t asem2 = GetNowUs();
    if(asem2 -asem1 > 15000)
    {
	loge("wht>>>demux,audio Submit sem_getvalue timeout:%lldms\n",(asem2 - asem1)/1000);
    }

        if(nSemCnt == 0)
            sem_post(&p->streamDataSem);

    int64_t asem3 = GetNowUs();
    if(asem3 -asem2 > 15000)
    {
	logv("wht>>>demux,audio Submit sem_post timeout:%lldms\n",(asem3 - asem2)/1000);
    }

    }

    return 0;
}

int AudioDecCompStreamBufferSize(AudioDecComp* p, int nStreamIndex)
{
    StreamManager*       pSm;

    pSm = p->pStreamManagerArr[nStreamIndex];

    return StreamManagerBufferSize(pSm);
}

int AudioDecCompStreamDataSize(AudioDecComp* p, int nStreamIndex)
{
    int                  nStreamDataSize;

    pthread_mutex_lock(&p->streamManagerMutex);
    nStreamDataSize = 0;
    if(p->pStreamManagerArr[nStreamIndex] != NULL)
    {
        nStreamDataSize = StreamManagerStreamDataSize(p->pStreamManagerArr[nStreamIndex]);

        //* this method is called by the demux thread, the decoder may be destroyed when
        //* switching audio, so we should lock the decoderDestroyMutex to protect the
        //* decoder from destroyed.
        pthread_mutex_lock(&p->decoderDestroyMutex);
        if(p->AlibItf.libadecoder != NULL)
            nStreamDataSize += p->AlibItf.AudioStreamDataSize(p->AlibItf.libadecoder);
        pthread_mutex_unlock(&p->decoderDestroyMutex);
    }
    pthread_mutex_unlock(&p->streamManagerMutex);

    return nStreamDataSize;
}

int AudioDecCompStreamFrameNum(AudioDecComp* p, int nStreamIndex)
{
    int                  nStreamFrameNum;

    pthread_mutex_lock(&p->streamManagerMutex);
    nStreamFrameNum = 0;
    if(p->pStreamManagerArr[nStreamIndex] != NULL)
    {
        nStreamFrameNum = StreamManagerStreamFrameNum(p->pStreamManagerArr[nStreamIndex]);

        //* this method is called by the demux thread, the decoder may be destroyed when
        //* switching audio, so we should lock the decoderDestroyMutex to protect the
        //* decoder from destroyed.
//        pthread_mutex_lock(&p->decoderDestroyMutex);
//        if(p->pDecoder != NULL)
//            nStreamDataSize += AudioStreamDataSize(p->pDecoder);
//        pthread_mutex_unlock(&p->decoderDestroyMutex);
    }
    pthread_mutex_unlock(&p->streamManagerMutex);

    return nStreamFrameNum;
}

int AudioDecCompRequestPcmData(AudioDecComp*   p,
                               unsigned char** ppData,
                               unsigned int*   pSize,
                               int64_t*        pPts,
                               CdxPlaybkCfg*   cfg)
{
    AudioStreamInfo pStreamInfo = p->pStreamInfoArr[p->nStreamSelected];
    //* this method is called by the audio render thread,
    //* the audio render thread is paused or stop before the audio decoder thread,
    //* so here we do not need to lock the decoderDestroyMutex.
    if(p->AlibItf.libadecoder != NULL)
    {
        *pPts = p->AlibItf.PlybkRequestPcmPts(p->AlibItf.libadecoder);
        memcpy(cfg,&(pStreamInfo.raw_data),sizeof(CdxPlaybkCfg));
        return  p->AlibItf.PlybkRequestPcmBuffer(p->AlibItf.libadecoder, ppData, (int*)pSize);
    }
    else
    {
        *ppData = NULL;
        *pPts = -1;
        memset(cfg,0,sizeof(CdxPlaybkCfg));
        return -1;
    }
}

int AudioDecCompReleasePcmData(AudioDecComp* p, int nReleaseSize)
{
    int ret;
    int nSemCnt;

    //* this method is called by the audio render thread,
    //* the audio render thread is paused or stop before the audio decoder thread,
    //* so here we do not need to lock the decoderDestroyMutex.
    if(p->AlibItf.libadecoder != 0)
        ret = p->AlibItf.PlybkUpdatePcmBuffer(p->AlibItf.libadecoder, nReleaseSize);
    else
        ret = -1;

    if(sem_getvalue(&p->frameBufferSem, &nSemCnt) == 0)
    {
        if(nSemCnt == 0)
            sem_post(&p->frameBufferSem);
    }

    return ret;
}

int AudioDecCompPcmDataSize(AudioDecComp* p, int nStreamIndex)
{
    int                  nPcmDataSize;

    nPcmDataSize = 0;

    CEDARX_UNUSE(nStreamIndex);

    //* this method is called by the demux thread, the decoder may be destroyed when
    //* switching audio, so we should lock the decoderDestroyMutex to protect the
    //* decoder from destroyed.
    pthread_mutex_lock(&p->decoderDestroyMutex);
    if(p->AlibItf.libadecoder != NULL)
        nPcmDataSize = p->AlibItf.AudioPCMDataSize(p->AlibItf.libadecoder);
    pthread_mutex_unlock(&p->decoderDestroyMutex);

    return nPcmDataSize;
}

//* must be called at stopped status.
int AudioDecCompSwitchStream(AudioDecComp* p, int nStreamIndex)
{
    if(p->eStatus != PLAYER_STATUS_STOPPED)
    {
        loge("can not switch status when audio decoder is not in stopped status.");
        return -1;
    }

    pthread_mutex_lock(&p->streamManagerMutex);
    p->nStreamSelected = nStreamIndex;
    p->afterSwitchStream = 1;
    pthread_mutex_unlock(&p->streamManagerMutex);
    return 0;
}

void AudioDecRawSendCmdToHalClbk(void *pself,void *param)
{
    AudioDecComp* p = (AudioDecComp*)pself;
    if(p->callback)
        p->callback(p->pUserData, PLAYER_AUDIO_DECODER_NOTIFY_AUDIORAWPLAY, param);
}

static void* AudioDecodeThread(void* arg)
{
    AudioDecComp *p = arg;
    AwMessage msg;

    while (AwMessageQueueGetMessage(p->mq, &msg) == 0)
    {
        if (msg.execute != NULL)
            msg.execute(&msg, p);
        else
            loge("msg with msg_id %d doesn't have a handler", msg.messageId);
    }

    return NULL;
}

static void handleStart(AwMessage *msg, void *arg)
{
    AudioDecComp *p = arg;

    if (p->eStatus == PLAYER_STATUS_STARTED)
    {
        loge("invalid start operation, already in started status.");
        if (msg->result)
            *msg->result = -1;
        sem_post(msg->replySem);
        return;
    }

    if (p->eStatus == PLAYER_STATUS_PAUSED)
    {
        //* send a decode message to start decoding.
        if (p->bCrashFlag == 0)
            BaseCompContinue(&p->base);
        p->eStatus = PLAYER_STATUS_STARTED;
        if (msg->result)
            *msg->result = 0;
        sem_post(msg->replySem);
        return;
    }

    //* create a decoder.
    //* lock the decoderDestroyMutex to prevend the demux thread from getting
    //* stream data size or pcm data size when the decoder is being created.
    //* see AudioDecCompStreamDataSize() and AudioDecCompPcmDataSize().
    pthread_mutex_lock(&p->decoderDestroyMutex);

    p->AlibItf.libadecoder = p->AlibItf.CreateAudioDecoder();
    if (p->AlibItf.libadecoder == NULL)
    {
        pthread_mutex_unlock(&p->decoderDestroyMutex);
        loge("audio decoder component create decoder fail.");
        p->bCrashFlag = 1;
        p->callback(p->pUserData, PLAYER_AUDIO_DECODER_NOTIFY_CRASH, NULL);
        if (msg->result)
            *msg->result = -1;
        sem_post(msg->replySem);
        return;
    }
    logd("Create libadecoder success...");
    memset(&p->bsInfo, 0, sizeof(BsInFor));
    if (p->AlibItf.InitializeAudioDecoder(p->AlibItf.libadecoder,
                              &p->pStreamInfoArr[p->nStreamSelected],
                              &p->bsInfo) != 0)
    {
        loge("initialize audio decoder fail.");
        p->AlibItf.DestroyAudioDecoder(p->AlibItf.libadecoder);
        p->AlibItf.libadecoder = NULL;
        pthread_mutex_unlock(&p->decoderDestroyMutex);
        p->bCrashFlag = 1;
        p->callback(p->pUserData, PLAYER_AUDIO_DECODER_NOTIFY_CRASH, NULL);
        if (msg->result)
            *msg->result = -1;
        sem_post(msg->replySem);
        return;
    }

#ifdef __ANDROID__
    p->AlibItf.SetRawPlayParam(p->AlibItf.libadecoder,(void*)p);
#else
    p->AlibItf.SetRawPlayParam(p->AlibItf.libadecoder,(void*)p, 0);
#endif
    //* demux thread can use the decoder now.
    pthread_mutex_unlock(&p->decoderDestroyMutex);

    //* send a decode message.
    BaseCompContinue(&p->base);
    p->bEosFlag = 0;
    p->eStatus = PLAYER_STATUS_STARTED;

    if (msg->result)
        *msg->result = 0;
    sem_post(msg->replySem);
}

static void handleStop(AwMessage *msg, void *arg)
{
    AudioDecComp *p = arg;

    if (p->eStatus == PLAYER_STATUS_STOPPED)
    {
        loge("invalid stop operation, already in stopped status.");
        if (p->bCrashFlag == 1)
            p->bCrashFlag = 0;

        if (msg->result)
            *msg->result = -1;
        sem_post(msg->replySem);
        return;
    }

    //* destroy decoder.
    //* lock the decoderDestroyMutex to prevend the demux thread from getting
    //* stream data size or pcm data size when the decoder is being created.
    //* see AudioDecCompStreamDataSize() and AudioDecCompPcmDataSize().
    pthread_mutex_lock(&p->decoderDestroyMutex);
    p->bFirstFramePtsValid = 0;
    if (p->AlibItf.libadecoder != NULL)
    {
        logd("destroy libadecoder...");
        p->AlibItf.DestroyAudioDecoder(p->AlibItf.libadecoder);
        p->AlibItf.libadecoder = NULL;
    }
    pthread_mutex_unlock(&p->decoderDestroyMutex);
    memset(&p->bsInfo, 0, sizeof(BsInFor));

    p->bCrashFlag = 0;
    p->eStatus = PLAYER_STATUS_STOPPED;
    if (msg->result)
        *msg->result = 0;
    sem_post(msg->replySem);
}

static void handlePause(AwMessage *msg, void *arg)
{
    AudioDecComp *p = arg;

    if (p->eStatus != PLAYER_STATUS_STARTED)
    {
        loge("invalid pause operation, component not in started status.");
        if (msg->result)
            *msg->result = -1;
        sem_post(msg->replySem);
        return;
    }

    p->eStatus = PLAYER_STATUS_PAUSED;
    if (msg->result)
        *msg->result = 0;
    sem_post(msg->replySem);
}

static void handleReset(AwMessage *msg, void *arg)
{
    AudioDecComp *p = arg;

    pthread_mutex_lock(&p->streamManagerMutex);
    int i;
    for(i=0; i<p->nStreamCount; i++)
    {
        if(p->pStreamManagerArr[i] != NULL)
            StreamManagerReset(p->pStreamManagerArr[i]);
    }
    pthread_mutex_unlock(&p->streamManagerMutex);

    p->bFirstFramePtsValid = 0;
    p->bEosFlag = 0;
    p->bCrashFlag = 0;

    if(p->AlibItf.libadecoder != NULL)
    {
        p->AlibItf.ResetAudioDecoder(p->AlibItf.libadecoder, msg->seekTime);
    }

    if (msg->result)
        *msg->result = 0;
    sem_post(msg->replySem);

    //* send a message to continue the thread.
    if (p->eStatus == PLAYER_STATUS_STARTED)
        BaseCompContinue(&p->base);
}

static void handleSetEos(AwMessage *msg, void *arg)
{
    AudioDecComp *p = arg;

    p->bEosFlag = 1;
    sem_post(msg->replySem);

    //* send a message to continue the thread.
    if(p->bCrashFlag == 0 && p->eStatus == PLAYER_STATUS_STARTED)
        BaseCompContinue(&p->base);
}

static void handleQuit(AwMessage *msg, void *arg)
{
    AudioDecComp *p = arg;

    //* destroy decoder and break.
    //* lock the decoderDestroyMutex to prevend the demux thread from getting
    //* stream data size or pcm data size when the decoder is being created.
    //* see AudioDecCompStreamDataSize() and AudioDecCompPcmDataSize().
    pthread_mutex_lock(&p->decoderDestroyMutex);
    if (p->AlibItf.libadecoder != NULL)
    {
        p->AlibItf.DestroyAudioDecoder(p->AlibItf.libadecoder);
        p->AlibItf.libadecoder = NULL;
    }
    pthread_mutex_unlock(&p->decoderDestroyMutex);

    p->eStatus = PLAYER_STATUS_STOPPED;
    if (msg->result)
        *msg->result = 0;
    sem_post(msg->replySem);
    pthread_exit(NULL);
}

typedef int (*print_function)(const char *fmt, ...);
extern int backtrace(char *taskname, void *output[], int size, int offset, print_function print_func);
static void doDecode(AwMessage *msg, void *arg)
{
    int ret = 0;
    AudioDecComp *p = arg;
    (void)msg;

    char*          pOutputBuf  = NULL;
    int            nPcmDataLen = 0;
    StreamManager* pSm         = p->pStreamManagerArr[p->nStreamSelected];
    StreamFrame*   pFrame      = NULL;
    unsigned char* pBuf0       = NULL;
    unsigned char* pBuf1       = NULL;
    int            nBufSize0   = 0;
    int            nBufSize1   = 0;

    if (p->eStatus != PLAYER_STATUS_STARTED)
    {
        logw("not in started status, ignore decode message.");
        return;
    }

    pthread_mutex_lock(&p->streamManagerMutex);
    if (p->afterSwitchStream &&
            p->pAvTimer->GetStatus(p->pAvTimer) == TIMER_STATUS_START)
    {
        p->afterSwitchStream = 0;
        int64_t nCurTime = p->pAvTimer->GetTime(p->pAvTimer);
        StreamManagerRewind(pSm, nCurTime);
    }
    pthread_mutex_unlock(&p->streamManagerMutex);

    if (p->AlibItf.DecRequestPcmBuffer(p->AlibItf.libadecoder, &pOutputBuf) < 0)
    {
        //* no pcm buffer, wait for the pcm buffer semaphore.
        logv("no pcm buffer, wait.");
        CdxSemTimedWait(&p->frameBufferSem, 20);    //* wait for frame buffer.
        BaseCompContinue(&p->base);
        return;
    }

    //* Add stream to decoder.
    pFrame = StreamManagerGetFrameInfo(pSm, 0);
    if (pFrame != NULL)
    {
        if((p->bFirstFramePtsValid == 0) && (pFrame->nPts == -1))
        {
            pFrame = StreamManagerRequestStream(pSm);
            StreamManagerFlushStream(pSm, pFrame);

            if(p->bEosFlag &&
                StreamManagerStreamFrameNum(p->pStreamManagerArr[p->nStreamSelected]) == 0)
            {
                logd("audio decoder notify eos.");
                p->callback(p->pUserData, PLAYER_AUDIO_DECODER_NOTIFY_EOS, NULL);
            }
            else
            {
                BaseCompContinue(&p->base);
            }
            return;
        }
        ret = p->AlibItf.ParserRequestBsBuffer(p->AlibItf.libadecoder,
                                    pFrame->nLength,
                                    &pBuf0,
                                    &nBufSize0,
                                    &pBuf1,
                                    &nBufSize1,
                                    &p->nOffset);
        if ((nBufSize0+nBufSize1) >= pFrame->nLength)
        {
            pFrame = StreamManagerRequestStream(pSm);
            if(nBufSize0 >= pFrame->nLength)
                memcpy(pBuf0, pFrame->pData, pFrame->nLength);
            else
            {
                memcpy(pBuf0, pFrame->pData, nBufSize0);
                memcpy(pBuf1, (char*)pFrame->pData + nBufSize0,
                        pFrame->nLength - nBufSize0);
            }
            p->AlibItf.ParserUpdateBsBuffer(p->AlibItf.libadecoder,
                                 pFrame->nLength,
                                 pFrame->nPts,
                                 p->nOffset);
            StreamManagerFlushStream(pSm, pFrame);
            p->bFirstFramePtsValid = 1;
        }
    }
	
    int64_t dec_time1 = GetNowUs();

    ret = p->AlibItf.DecodeAudioStream(p->AlibItf.libadecoder,
                            &p->pStreamInfoArr[p->nStreamSelected],
                            pOutputBuf,
                            &nPcmDataLen);
    int64_t dec_time2 = GetNowUs();

    if(dec_time2 -dec_time1 > 15000)
    {
	logv("wht>>>decoder,dec timeout:%lldms\n",(dec_time2 - dec_time1)/1000);
    }

    logv("DecodeAudioStream, ret = %d",ret);
    if(ret == ERR_AUDIO_DEC_NONE)
    {
        if(p->pStreamInfoArr[p->nStreamSelected].nSampleRate != p->bsInfo.out_samplerate ||
            p->pStreamInfoArr[p->nStreamSelected].nChannelNum != p->bsInfo.out_channels)
        {
            p->pStreamInfoArr[p->nStreamSelected].nSampleRate = p->bsInfo.out_samplerate;
            p->pStreamInfoArr[p->nStreamSelected].nChannelNum = p->bsInfo.out_channels;
        }
        p->AlibItf.DecUpdatePcmBuffer(p->AlibItf.libadecoder, nPcmDataLen);
        BaseCompContinue(&p->base);
        return;
    }
    else if(ret == ERR_AUDIO_DEC_NO_BITSTREAM || ret == ERR_AUDIO_DEC_ABSEND)
    {
        if(p->bEosFlag &&
           StreamManagerStreamFrameNum(p->pStreamManagerArr[p->nStreamSelected]) == 0)
        {
            logv("audio decoder notify eos.");
            p->callback(p->pUserData, PLAYER_AUDIO_DECODER_NOTIFY_EOS, NULL);
            return;
        }
        else
        {
	logv("wht>>>>>debug, no audio bitstream~~~~~~");
	int64_t sem_time_wait1 = GetNowUs();
            if(StreamManagerStreamFrameNum(p->pStreamManagerArr[p->nStreamSelected]) == 0)
	    {
                CdxSemTimedWait(&p->streamDataSem, 50);//50
	    }
	    int64_t sem_time_wait2 = GetNowUs();
    if(sem_time_wait2 -sem_time_wait1 > 15000)
    {
	logv("wht>>>demux,audio Submit sem_time_wait  timeout:%lldms\n",(sem_time_wait2 - sem_time_wait1)/1000);
    }

            BaseCompContinue(&p->base);
            return;
        }
    }
    else if(ret == ERR_AUDIO_DEC_EXIT || ret == ERR_AUDIO_DEC_ENDINGCHKFAIL)
    {
        p->bCrashFlag = 1;
        p->callback(p->pUserData, PLAYER_AUDIO_DECODER_NOTIFY_CRASH, NULL);
        return;
    }
    else
    {
        logw("DecodeAudioStream() return %d, continue to decode", ret);
        BaseCompContinue(&p->base);
    }
}

#if 0
static void FlushStreamManagerBuffers(AudioDecComp* p, int64_t curTime,
                int bIncludeSeletedStream)
{
    //* to prevent from flush incorrectly when pts loop back,
    //* we find the frame who's pts is near the current timer value,
    //* and flush frames before this frame.

    int            i;
    int            nFrameIndex;
    int            nFlushPos;
    int            nFrameCount;
    StreamFrame*   pFrame;
    StreamManager* pSm;
    int64_t        nMinPtsDiff;
    int64_t        nPtsDiff;

    for(i=0; i<p->nStreamCount; i++)
    {
        if(i == p->nStreamSelected && bIncludeSeletedStream == 0)
            continue;

        pSm = p->pStreamManagerArr[i];
        nFrameCount = StreamManagerStreamFrameNum(pSm);
        nMinPtsDiff = 0x7fffffffffffffffLL; //* set it to the max value.
        nFlushPos   = nFrameCount;
        for(nFrameIndex=0; nFrameIndex<nFrameCount; nFrameIndex++)
        {
            pFrame = StreamManagerGetFrameInfo(pSm, nFrameIndex);
            if(pFrame->nPts == -1)
                continue;

            nPtsDiff = pFrame->nPts - curTime;
            if(nPtsDiff >= 0 && nPtsDiff < nMinPtsDiff)
            {
                nMinPtsDiff = nPtsDiff;
                nFlushPos   = nFrameIndex;
            }
        }

        //* flush frames before nFlushPos.
        for(nFrameIndex=0; nFrameIndex<nFlushPos; nFrameIndex++)
        {
            pFrame = StreamManagerRequestStream(pSm);
            StreamManagerFlushStream(pSm, pFrame);
        }
    }

    return;
}
#endif
