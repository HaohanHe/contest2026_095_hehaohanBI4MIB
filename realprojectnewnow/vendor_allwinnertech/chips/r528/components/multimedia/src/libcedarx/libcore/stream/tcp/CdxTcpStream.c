/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxTcpStream.c
 * Description : TcpStream
 * History :
 *
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "tcpStream"

#include <pthread.h>
#include "cdx_config.h"
#include <sys/time.h>
#include <unistd.h>
#include <CdxTypes.h>
#include <errno.h>
//#include <netinet/in.h>
#include <string.h>
//#include "lwip/netdb.h"
//#include "lwip/tcp.h"
//#include "lwip/sockets.h"
#undef connect

//#include "cdx_malloc.h"
#include <CdxStream.h>
#include <CdxAtomic.h>
#include <SmartDnsService.h>
#include <CdxTime.h>
#include <CdxSocketUtil.h>
#include <netdb.h>

#if CDX_BUF_STAT
#include "CdxBufStat.h"
#endif

#define GetNowUs() CdxGetNowUs()

//#define SOCKRECVBUF_LEN 512*1024// 262142 (5*1024*1024)
//#define SOCKRECVBUF_LEN 4*1024
//#define lwip_close close
#if CDX_IOT_OLD_SOCKET
static pthread_once_t once = PTHREAD_ONCE_INIT;
static pthread_key_t oldSocketKey;

void oldSocketDestr(void *p)
{
    int oldFd = (long)p - 1;
    logd("close old socket %d", oldFd);
    lwip_close(oldFd);
}

static void createoldSocketKey()
{
    int ret = pthread_key_create(&oldSocketKey, oldSocketDestr);
    if (ret)
    {
        loge("pthread_key_create failed: %s", strerror(errno));
        abort();
    }
}
#endif /* CDX_IOT_OLD_SOCKET */

static void CdxTcpStreamDecRef(CdxStreamT *stream);

enum HttpStreamStateE
{
    TCP_STREAM_IDLE    = 0x00L,
    TCP_STREAM_CONNECTING = 0x01L,
    TCP_STREAM_READING = 0x02L,
    TCP_STREAM_WRITING = 0x03L,
    TCP_STREAM_FORCESTOPPED = 0x04L,
    //TCP_STREAM_CLOSING
};

typedef struct CdxTcpStreamImpl
{
    CdxStreamT base;
    cdx_int32 ioState;
    cdx_int32 sockRecvBufLen;
    cdx_int8 notBlockFlag;
    cdx_int8 readOnceFlag;
    cdx_int8 reserve[2];
    cdx_int32 forceStopFlag;
    cdx_int32 sockFd;                  //socket fd
    cdx_int32 port;
    cdx_char *hostname;
    cdx_atomic_t ref;                  //reference count, for free resource while still blocking.

    volatile enum HttpStreamStateE state;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    cdx_int64 durationReadTime;       //the duration time that can not read any data from network

#if CDX_IOT_DNS_CACHE
    pthread_cond_t dnsCond;
    pthread_mutex_t* dnsMutex;
    int dnsRet;
    struct addrinfo *dnsAI;
#endif /* CDX_IOT_DNS_CACHE */

#if defined(CONF_YUNOS)
    //YUNOS
    cdx_char mTcpIP[100];
    int mYunOSstatusCode;
#endif
    ParserCallback callback;
    void *pUserData;

#if CDX_IOT_OLD_SOCKET
    int saveOldSocket;
#endif /* CDX_IOT_OLD_SOCKET */
}CdxTcpStreamImplT;

static void CdxTcpStreamDecRef(CdxStreamT *stream);

typedef struct CdxHttpSendBuffer
{
    void *size;
    void *buf;
}CdxHttpSendBufferT;


static cdx_int32 __CdxTcpStreamRead(CdxStreamT *stream, void *buf, cdx_uint32 len)
{
    CdxTcpStreamImplT *impl;
    cdx_int32 ret;
    cdx_int32 recvSize = 0;
    cdx_int32 ioErr;
    cdx_int32 num = 0;
    cdx_int64 now, start;
#if CDX_BUF_STAT
    int loop_cnt = 0;
#endif

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxTcpStreamImplT, base);

    if(stream == NULL || buf == NULL || len <= 0)
    {
        CDX_LOGW("check parameter.");
        return -1;
    }

    pthread_mutex_lock(&impl->lock);
    if(impl->forceStopFlag)
    {
        pthread_mutex_unlock(&impl->lock);
        return -2;
    }
    CdxAtomicInc(&impl->ref);
    impl->state = TCP_STREAM_READING;
    pthread_mutex_unlock(&impl->lock);

    while(impl->notBlockFlag)
    {
        if(impl->forceStopFlag)
        {
            //CDX_LOGV("__CdxTcpStreamRead forceStop.");
            ret = -2;
            goto __exit0;
        }
        ret = CdxSockNoblockRecv(impl->sockFd, buf, len);
        if (ret < 0)
        {
            ioErr = errno;
            if (EAGAIN == ioErr)
            {
                num++;
                usleep(5000);
                if(num < 400) //* notBlockFlag, try 2s at most.
                {
                    continue;
                }
                else
                {
                    ret = 0;
                }
            }
            else
            {
                CDX_LOGE("<%s,%d>recv err(%d): %s", __FUNCTION__, __LINE__, errno, strerror(errno));
                impl->ioState = CDX_IO_STATE_ERROR;
                impl->notBlockFlag = 0;
                ret = -1;
                goto __exit0;
            }
        }
        CDX_LOGV("xxx CdxSockNoblockRecv(%d)", ret);
        impl->notBlockFlag = 0;

__exit0:
        pthread_mutex_lock(&impl->lock);
        impl->state = TCP_STREAM_IDLE;
        CdxTcpStreamDecRef(stream);
        pthread_mutex_unlock(&impl->lock);
        pthread_cond_signal(&impl->cond);
        return ret;
    }

#if CDX_BUF_STAT
    CdxBufStatSetStartStamp(&tcp_stat.tcp_read_loop_time);
#endif

    start = GetNowUs();

    while((cdx_uint32)recvSize < len)
    {
        if(impl->forceStopFlag)
        {
            CDX_LOGV("__CdxTcpStreamRead forceStop.");
            if(recvSize > 0)
                break;
            else
            {
                recvSize = -2;
                goto __exit1;
            }
        }

#if CDX_BUF_STAT
        loop_cnt++;
#endif

        if (impl->readOnceFlag)
        {
	    CDX_LOGE("wht>>>>>>>>debug, CdxSockAsynRecv 5000, len -recvSize = %d", len - recvSize);
            ret = CdxSockAsynRecv(impl->sockFd, (char *)buf + recvSize,
                                len - recvSize, 5000, &impl->forceStopFlag);
        }
        else
        {
	    CDX_LOGV("wht>>>>>>>>debug, CdxSockAsynRecv 1000000, len -recvSize = %d", len - recvSize);
            ret = CdxSockAsynRecv(impl->sockFd, (char *)buf + recvSize,
                                len - recvSize, 1000000, &impl->forceStopFlag);
        }

        if(ret < 0)
        {
            if(ret == -2)
            {
                recvSize = recvSize>0 ? recvSize : -2;
                goto __exit1;
            }
            impl->ioState = CDX_IO_STATE_ERROR;
            CDX_LOGE("__CdxTcpStreamRead error(%d): %s. recvSize(%d)",
                errno, strerror(errno), recvSize);
            recvSize = -1;

#if defined(CONF_YUNOS)
            if(impl->callback)
            {
                impl->mYunOSstatusCode = 3003; //Ali YUNOS invoke info
                impl->callback(impl->pUserData, STREAM_EVT_DOWNLOAD_DOWNLOAD_ERROR,
                    &(impl->mYunOSstatusCode));
            }
#endif

            goto __exit1;
        }
        else if(ret == 0)
        {
            now = GetNowUs();
            impl->durationReadTime += (now - start);
extern unsigned int XPlayer_Tcpread_Timeout_ms;
            if (impl->durationReadTime > (XPlayer_Tcpread_Timeout_ms << 10))
                break;
        }
        else
        {
            impl->durationReadTime = 0;
            recvSize += ret;
        }

        if (impl->readOnceFlag)
        {
            impl->readOnceFlag = 0;
            break;
        }
    }

__exit1:

#if CDX_BUF_STAT
    CdxBufStatSetEndStamp(&tcp_stat.tcp_read_loop_time);
    CdxBufStatIncProcTime(&tcp_stat.tcp_read_loop_time);
    CdxBufStatIncStat(&tcp_stat.tcp_read_loop_cnts, loop_cnt);
#endif

    pthread_mutex_lock(&impl->lock);
    impl->state = TCP_STREAM_IDLE;
    CdxTcpStreamDecRef(stream);
    pthread_mutex_unlock(&impl->lock);
    pthread_cond_signal(&impl->cond);

    return recvSize;
}

static cdx_int32 __CdxTcpStreamGetIOState(CdxStreamT *stream)
{
    CdxTcpStreamImplT *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxTcpStreamImplT, base);

    return impl->ioState;
}
static cdx_int32 __CdxTcpStreamWrite(CdxStreamT *stream, void *buf, cdx_uint32 len)
{
    CdxTcpStreamImplT *impl;
    size_t size = 0;
    ssize_t ret = 0;

#if CDX_BUF_STAT
    int loop_cnt = 0;
#endif

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxTcpStreamImplT, base);

    pthread_mutex_lock(&impl->lock);
    if(impl->forceStopFlag)
    {
        pthread_mutex_unlock(&impl->lock);
        return -1;
    }
    CdxAtomicInc(&impl->ref);
    impl->state = TCP_STREAM_WRITING;
    pthread_mutex_unlock(&impl->lock);

#if CDX_BUF_STAT
    CdxBufStatSetStartStamp(&tcp_stat.tcp_write_loop_time);
#endif

    while(size < len)
    {
#if CDX_BUF_STAT
        loop_cnt++;
#endif
        ret = CdxSockAsynSend(impl->sockFd, (char *)buf + size, len - size,
                                                        0, &impl->forceStopFlag);
        if(ret < 0)
        {
            CDX_LOGE("send failed. error(%d): %s.", errno, strerror(errno));
            break;
        }
        else if(ret == 0)
        {
            break;
        }
        size += ret;
    }
#if CDX_BUF_STAT
    CdxBufStatSetEndStamp(&tcp_stat.tcp_write_loop_time);
    CdxBufStatIncProcTime(&tcp_stat.tcp_write_loop_time);
    CdxBufStatIncStat(&tcp_stat.tcp_write_loop_cnts, loop_cnt);
#endif

    pthread_mutex_lock(&impl->lock);
    impl->state = TCP_STREAM_IDLE;
    CdxTcpStreamDecRef(stream);
    pthread_mutex_unlock(&impl->lock);
    pthread_cond_signal(&impl->cond);

    return (size == len) ? 0 : -1;
}
static cdx_int32 CdxTcpStreamForceStop(CdxStreamT *stream)
{
    CdxTcpStreamImplT *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxTcpStreamImplT, base);

    CDX_LOGV("begin tcp force stop");
#if CDX_BUF_STAT
    CdxBufStatSetStartStamp(&tcp_stat.tcp_close_getlock_time);
#endif
    pthread_mutex_lock(&impl->lock);
#if CDX_BUF_STAT
    CdxBufStatSetEndStamp(&tcp_stat.tcp_close_getlock_time);
    CdxBufStatIncProcTime(&tcp_stat.tcp_close_getlock_time);
#endif
    if(impl->forceStopFlag == 1)
    {
        pthread_mutex_unlock(&impl->lock);
        return 0;
    }
    CdxAtomicInc(&impl->ref);
    impl->forceStopFlag = 1;
    while(impl->state != TCP_STREAM_IDLE)
    {
#if CDX_BUF_STAT
        CdxBufStatSetStartStamp(&tcp_stat.tcp_close_wait_time);
#endif
        pthread_cond_wait(&impl->cond, &impl->lock);
#if CDX_BUF_STAT
        CdxBufStatSetEndStamp(&tcp_stat.tcp_close_wait_time);
        CdxBufStatIncProcTime(&tcp_stat.tcp_close_wait_time);
#endif
    }
    pthread_mutex_unlock(&impl->lock);

    CdxTcpStreamDecRef(stream);
    CDX_LOGV("finish tcp force stop");
    return 0;
}
static cdx_int32 CdxTcpStreamClrForceStop(CdxStreamT *stream)
{
    CdxTcpStreamImplT *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxTcpStreamImplT, base);

    pthread_mutex_lock(&impl->lock);
    impl->forceStopFlag = 0;
    impl->state = TCP_STREAM_IDLE;
    pthread_mutex_unlock(&impl->lock);

    return 0;
}

static cdx_int32 __CdxTcpStreamControl(CdxStreamT *stream, cdx_int32 cmd, void *param)
{
    CdxTcpStreamImplT *impl;

    impl = CdxContainerOf(stream, CdxTcpStreamImplT, base);
#if CDX_BUF_STAT
    CdxBufStatSetStartStamp(&tcp_stat.tcp_control_time);
#endif
    switch(cmd)
    {
        case STREAM_CMD_READ_NOBLOCK:
        {
            impl->notBlockFlag = 1;
            break;
        }
        case STREAM_CMD_GET_SOCKRECVBUFLEN:
        {
            *(int *)param = impl->sockRecvBufLen;
            break;
        }
        case STREAM_CMD_SET_FORCESTOP:
        {
            return CdxTcpStreamForceStop(stream);
        }
        case STREAM_CMD_CLR_FORCESTOP:
        {
            return CdxTcpStreamClrForceStop(stream);
        }

#if defined(CONF_YUNOS)
        case STREAM_CMD_GET_IP:
        {
            if(impl->mTcpIP[0])
                strcpy((char *)param,impl->mTcpIP);
            break;
        }
#endif
        case STREAM_CMD_SET_CALLBACK:
        {
            struct CallBack *cb = (struct CallBack *)param;
            impl->callback = cb->callback;
            impl->pUserData = cb->pUserData;
            break;
        }
        case STREAM_CMD_SET_EOF:
#if CDX_IOT_OLD_SOCKET
            impl->saveOldSocket = 1;
#endif /* CDX_IOT_OLD_SOCKET */
            break;
        case STREAM_CMD_READ_ONCE:
        {
            impl->readOnceFlag = 1;
            break;
        }
        default:
        {
            CDX_LOGV("control cmd %d is not supported by tcp",cmd);
            break;
        }
    }

#if CDX_BUF_STAT
    CdxBufStatSetEndStamp(&tcp_stat.tcp_control_time);
    CdxBufStatIncProcTime(&tcp_stat.tcp_control_time);
#endif

    return 0;
}

static cdx_int32 __CdxTcpStreamClose(CdxStreamT *stream)
{
    CdxTcpStreamImplT *impl;

#if CDX_BUF_STAT
    CdxBufStatSetStartStamp(&tcp_stat.tcp_close_time);
#endif

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxTcpStreamImplT, base);
    CDX_LOGV("xxx tcp close begin.");
    CdxAtomicInc(&impl->ref);

#if CDX_BUF_STAT
    CdxBufStatSetStartStamp(&tcp_stat.tcp_close_force_time);
#endif
    CdxTcpStreamForceStop(stream);
#if CDX_BUF_STAT
    CdxBufStatSetEndStamp(&tcp_stat.tcp_close_force_time);
    CdxBufStatIncProcTime(&tcp_stat.tcp_close_force_time);
#endif

#if CDX_BUF_STAT
    CdxBufStatSetStartStamp(&tcp_stat.tcp_close_dec1_time);
#endif
    CdxTcpStreamDecRef(stream);
#if CDX_BUF_STAT
    CdxBufStatSetEndStamp(&tcp_stat.tcp_close_dec1_time);
    CdxBufStatIncProcTime(&tcp_stat.tcp_close_dec1_time);
#endif
#if CDX_BUF_STAT
    CdxBufStatSetStartStamp(&tcp_stat.tcp_close_dec2_time);
#endif
    CdxTcpStreamDecRef(stream);
#if CDX_BUF_STAT
    CdxBufStatSetEndStamp(&tcp_stat.tcp_close_dec2_time);
    CdxBufStatIncProcTime(&tcp_stat.tcp_close_dec2_time);
#endif

#if CDX_BUF_STAT
    CdxBufStatSetEndStamp(&tcp_stat.tcp_close_time);
    CdxBufStatIncProcTime(&tcp_stat.tcp_close_time);
#endif
    return 0;
}
static void CdxTcpStreamDecRef(CdxStreamT *stream)
{
    CdxTcpStreamImplT *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxTcpStreamImplT, base);

    CdxAtomicDec(&impl->ref);
    if(CdxAtomicRead(&impl->ref) != 0)
        return;

    if (impl->sockFd >= 0)
    {
#if CDX_IOT_OLD_SOCKET
        if (impl->saveOldSocket)
        {
            logd("save old socket");
            /* sockFd can be zero (at least in theory).
             * The value initially associated with oldSocketKey is NULL.
             * If we don't add one to sockFd, it's difficult to figure out
             * whether pthread_getspecific return a valid file descriptor or
             * not.
             */
            int oldsock = (long)pthread_getspecific(oldSocketKey) - 1;
            if (oldsock != impl->sockFd)
            {
                if (oldsock >= 0)
                {
                    logd("stream open and close in different threads, "
                            "socket reuse like HTTP keep alive won't work");
                    lwip_close(oldsock);
                }
                pthread_setspecific(oldSocketKey, (void *)(long)(impl->sockFd + 1));
            }
        }
        else
#endif /* CDX_IOT_OLD_SOCKET */
        {
            /* 保留此函数，原因是曾经遇到基地抓包分析，发现虽然客户端关闭了fd，
             * 但是服务器未关，导致抓包里出现探查报文。
             */
            CdxSockDisableTcpKeepalive(impl->sockFd);

            shutdown(impl->sockFd, SHUT_RDWR);
            lwip_close(impl->sockFd);
        }
    }
    pthread_mutex_destroy(&impl->lock);
    pthread_cond_destroy(&impl->cond);
#if CDX_IOT_DNS_CACHE
    pthread_mutex_destroy(impl->dnsMutex);
    pthread_cond_destroy(&impl->dnsCond);
    free(impl->dnsMutex);
    impl->dnsMutex = NULL;
#endif
    free(impl);
    impl=NULL;
    CDX_LOGV("xxx tcp close end.");
}

#if CDX_IOT_DNS_CACHE
static void DnsResponeHook(void *userhdr, int ret, struct addrinfo *ai)
{
    CdxTcpStreamImplT *impl = (CdxTcpStreamImplT *)userhdr;

    if (impl == NULL)
      return;

    if (ret == SDS_OK)
    {
        impl->dnsAI = ai;
    }
    impl->dnsRet = ret;
    if (impl->dnsMutex != NULL)
    {
        pthread_mutex_lock(impl->dnsMutex);
        pthread_cond_signal(&impl->dnsCond);
        pthread_mutex_unlock(impl->dnsMutex);
    }

    CdxTcpStreamDecRef(&impl->base);
    return ;
}
#endif /* CDX_IOT_DNS_CACHE */

static int StartTcpStreamConnect(CdxStreamT *stream)
{
    CdxTcpStreamImplT *impl;
    cdx_int32 ret;
    int64_t start, end;
    struct addrinfo *ai = NULL;
#if CDX_BUF_STAT
    int loop_cnt = 0;
#endif

    CDX_UNUSE(start);
    CDX_UNUSE(end);

    start = GetNowUs();

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxTcpStreamImplT, base);
    CDX_FORCE_CHECK(impl);
    if(impl->callback)
    {
        int flag = DETAIL_INFO_STREAM_DNS_START;
        //loge("RPT: STREAM_EVT_DETAIL_INFO, cb:%08lx, flag:%d", impl->callback, flag);
        impl->callback(impl->pUserData, STREAM_EVT_DETAIL_INFO, &flag);
    }
#if CDX_IOT_DNS_CACHE
    CdxAtomicInc(&impl->ref);
    //impl->dnsRet = SDSRequest(impl->hostname, impl->port, &ai, impl, DnsResponeHook);
    ret = SDSRequest(impl->hostname, impl->port, &ai, impl, DnsResponeHook);
    if (ret == SDS_OK)
    {
        CdxTcpStreamDecRef(&impl->base);
        CDX_FORCE_CHECK(ai);
    }
    else if (ret == SDS_PENDING)
    {
        while (1)
        {
            struct timespec abstime;

            abstime.tv_sec = time(0);
            abstime.tv_nsec = 100000000L;

            pthread_mutex_lock(impl->dnsMutex);
            pthread_cond_timedwait(&impl->dnsCond, impl->dnsMutex, &abstime); /* wait 100 ms */
            pthread_mutex_unlock(impl->dnsMutex);

            if (impl->forceStopFlag)
            {
                ai = NULL;
                break;
            }

            if (impl->dnsRet == SDS_OK)
            {
                ai = impl->dnsAI;
                break;
            }
            else if (impl->dnsRet != SDS_PENDING)
            {
                ai = NULL;
                break;
            }

        }

    }
    else
    {
        CdxTcpStreamDecRef(&impl->base);
    }
#else /* CDX_IOT_DNS_CACHE */
    cdx_char strPort[10] = {0};
    sprintf(strPort, "%d", impl->port);
    ret = lwip_getaddrinfo(impl->hostname, strPort, NULL, &ai);
    struct addrinfo *ai_backup = ai;
#endif /* CDX_IOT_DNS_CACHE */
    if(impl->callback)
    {
        int flag = DETAIL_INFO_STREAM_DNS_END;
        //loge("RPT: STREAM_EVT_DETAIL_INFO, cb:%08lx, flag:%d", impl->callback, flag);
        impl->callback(impl->pUserData, STREAM_EVT_DETAIL_INFO, &flag);
    }
    if (ai == NULL)
    {
        goto err_out;
    }

#if defined(CONF_YUNOS)
    //* Ali YUNOS invoke info
    //* get tcp IP in control STREAM_CMD_GET_IP
    struct addrinfo *curInfo;
    struct sockaddr_in *addrInfo;
    curInfo = ai;
    cdx_char ipbufInfo[100];
    addrInfo = (struct sockaddr_in *)curInfo->ai_addr;
    memcpy(impl->mTcpIP,inet_ntop(AF_INET, &addrInfo->sin_addr, ipbufInfo, 100),100);
#endif

#if CDX_IOT_OLD_SOCKET
    int oldFd = (long)pthread_getspecific(oldSocketKey) - 1;
    pthread_setspecific(oldSocketKey, NULL);
    if (oldFd >= 0)
    {
        struct sockaddr peerAddr;
        socklen_t addrlen = sizeof(peerAddr);

        getpeername(oldFd, (struct sockaddr *)&peerAddr, &addrlen);
        /* not robust. should check ai_next in a loop */
        if (addrlen == ai->ai_addrlen &&
                memcmp(&peerAddr, ai->ai_addr, addrlen) == 0)
        {
            char c;
            ret = recv(oldFd, &c, 1, MSG_DONTWAIT);
            if (ret == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
            {
                // Todo: set impl->sockRecvBufLen or remove this field completely
                impl->sockFd = oldFd;
                logd("reuse old socket");
                return 0;
            }
            logd("ret %d, error: %s", ret, strerror(errno));
        }

        logd("close old socket");
        lwip_close(oldFd);
    }
#endif /* CDX_IOT_OLD_SOCKET */

    extern unsigned int XPlayer_Playerback_Timeout_ms;
    cdx_long timeoutUs = XPlayer_Playerback_Timeout_ms << 10;
    if (timeoutUs < CDX_SELECT_TIMEOUT)
        timeoutUs = CDX_SELECT_TIMEOUT;

#if CDX_BUF_STAT
    CdxBufStatSetStartStamp(&tcp_stat.tcp_connect_loop_time);
#endif

    do
    {
        impl->sockRecvBufLen = 0;
        impl->sockFd = CdxAsynSocket(ai->ai_family, &impl->sockRecvBufLen);
        if(impl->sockFd < 0)
            continue;

#if CDX_BUF_STAT
        loop_cnt++;
#endif
#if CDX_BUF_STAT
        CdxBufStatSetStartStamp(&tcp_stat.tcp_conn_asyn_conn_time);
#endif
        if(impl->callback)
        {
            int flag = DETAIL_INFO_STREAM_CONNECT_START;
            //loge("RPT: STREAM_EVT_DETAIL_INFO, cb:%08lx, flag:%d", impl->callback, flag);
            impl->callback(impl->pUserData, STREAM_EVT_DETAIL_INFO, &flag);
        }
        ret = CdxSockAsynConnect(impl->sockFd, ai->ai_addr, ai->ai_addrlen, timeoutUs,
            &impl->forceStopFlag);
        if(impl->callback)
        {
            int flag = DETAIL_INFO_STREAM_CONNECT_END;
            //logd("RPT: STREAM_EVT_DETAIL_INFO, cb:%08lx, flag:%d", impl->callback, flag);
            impl->callback(impl->pUserData, STREAM_EVT_DETAIL_INFO, &flag);
        }
#if CDX_BUF_STAT
        CdxBufStatSetEndStamp(&tcp_stat.tcp_conn_asyn_conn_time);
        CdxBufStatIncProcTime(&tcp_stat.tcp_conn_asyn_conn_time);
#endif
        if(ret == 0)
        {
            break;
        }
        else if(ret < 0)
        {
            CDX_LOGE("connect failed. error(%d): %s.", errno, strerror(errno));

#if defined(CONF_YUNOS)
            if(impl->callback)
            {
                impl->mYunOSstatusCode = 3002; //Ali YUNOS invoke info
                impl->callback(impl->pUserData, STREAM_EVT_DOWNLOAD_DOWNLOAD_ERROR,
                    &(impl->mYunOSstatusCode));
            }
#endif
            lwip_close(impl->sockFd);
            impl->sockFd = -1;
        }

        if(impl->forceStopFlag == 1)
        {
            CDX_LOGV("force stop connect.");
#if CDX_BUF_STAT
            CdxBufStatSetEndStamp(&tcp_stat.tcp_connect_loop_time);
            CdxBufStatIncProcTime(&tcp_stat.tcp_connect_loop_time);
            CdxBufStatIncStat(&tcp_stat.tcp_connect_loop_cnts, loop_cnt);
#endif
            goto err_out;
        }
    } while ((ai = ai->ai_next) != NULL);

#if CDX_BUF_STAT
    CdxBufStatSetEndStamp(&tcp_stat.tcp_connect_loop_time);
    CdxBufStatIncProcTime(&tcp_stat.tcp_connect_loop_time);
    CdxBufStatIncStat(&tcp_stat.tcp_connect_loop_cnts, loop_cnt);
#endif

    if (ai == NULL)
    {
        CDX_LOGE("connect failed. error(%d): %s.", errno, strerror(errno));
        goto err_out;
    }

#if (!CDX_IOT_DNS_CACHE)
    lwip_freeaddrinfo(ai_backup);
#endif /* (!CDX_IOT_DNS_CACHE) */

    end = GetNowUs();
    return 0;

err_out:
#if (!CDX_IOT_DNS_CACHE)
    if (ai_backup)
        lwip_freeaddrinfo(ai_backup);
#endif /* (!CDX_IOT_DNS_CACHE) */

    end = GetNowUs();
    if(errno == 101)
    {
        CDX_LOGD("errno 101, reconnect");
        return -2;
    }
    return -1;
}

static cdx_int32 __CdxTcpStreamConnect(CdxStreamT *stream)
{
    CdxTcpStreamImplT *impl;
    cdx_int32 result;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxTcpStreamImplT, base);

    pthread_mutex_lock(&impl->lock);
    if(impl->forceStopFlag)
    {
        pthread_mutex_unlock(&impl->lock);
        return -1;
    }
    impl->state = TCP_STREAM_CONNECTING;
    CdxAtomicInc(&impl->ref);
    pthread_mutex_unlock(&impl->lock);

    impl->durationReadTime = 0;
    result = StartTcpStreamConnect(stream);
    if (result < 0)
    {
        CDX_LOGE("StartTcpStreamConnect failed! : %d", result);
        pthread_mutex_lock(&impl->lock);
        impl->ioState = CDX_IO_STATE_ERROR;
        pthread_mutex_unlock(&impl->lock);
    }
    else
    {
        pthread_mutex_lock(&impl->lock);
        impl->ioState = CDX_IO_STATE_OK;
        pthread_mutex_unlock(&impl->lock);
    }

    pthread_mutex_lock(&impl->lock);
    impl->state = TCP_STREAM_IDLE;
    CdxTcpStreamDecRef(&impl->base);
    pthread_mutex_unlock(&impl->lock);
    pthread_cond_signal(&impl->cond);
    return (impl->ioState == CDX_IO_STATE_ERROR) ? -1 : 0;
}

static const struct CdxStreamOpsS CdxTcpStreamOps = {
    .connect    = __CdxTcpStreamConnect,
    .read       = __CdxTcpStreamRead,
    .close      = __CdxTcpStreamClose,
    .getIOState = __CdxTcpStreamGetIOState,
//    .forceStop  = __CdxTcpStreamForceStop,
    .write      = __CdxTcpStreamWrite,
    .control    = __CdxTcpStreamControl
};

static CdxStreamT *__CdxTcpStreamCreate(CdxDataSourceT *source)
{
    CdxTcpStreamImplT *impl = NULL;

    impl = (CdxTcpStreamImplT *)malloc(sizeof(CdxTcpStreamImplT));
    if(NULL == impl)
    {
        CDX_LOGE("malloc failed");
        return NULL;
    }

#if CDX_BUF_STAT
    CdxBufStatSetStartStamp(&tcp_stat.tcp_create_time);
#endif

    memset(impl, 0x00, sizeof(CdxTcpStreamImplT));
    impl->base.ops = &CdxTcpStreamOps;
    impl->ioState = CDX_IO_STATE_INVALID;

    impl->sockFd = -1;
    impl->port = *(cdx_int32 *)((CdxHttpSendBufferT *)source->extraData)->size;
    impl->hostname = (char *)((CdxHttpSendBufferT *)source->extraData)->buf;
    //CDX_LOGV("port (%d), hostname(%s)", impl->port, impl->hostname);
    CdxAtomicSet(&impl->ref, 1);
    pthread_mutex_init(&impl->lock, NULL);
    pthread_cond_init(&impl->cond, NULL);

#if CDX_IOT_DNS_CACHE
    impl->dnsMutex = (pthread_mutex_t*)calloc(1,sizeof(pthread_mutex_t));
    if (impl->dnsMutex == NULL)
    {
        CDX_LOGE("malloc failed");
        return NULL;
    }

    pthread_mutex_init(impl->dnsMutex, NULL);
    pthread_cond_init(&impl->dnsCond, NULL);
    impl->dnsRet = SDS_PENDING;
#endif /* CDX_IOT_DNS_CACHE */

    impl->state = TCP_STREAM_IDLE;

#if CDX_IOT_OLD_SOCKET
    pthread_once(&once, createoldSocketKey);
#endif /* CDX_IOT_OLD_SOCKET */

#if CDX_BUF_STAT
    CdxBufStatSetEndStamp(&tcp_stat.tcp_create_time);
    CdxBufStatIncProcTime(&tcp_stat.tcp_create_time);
#endif

    return &impl->base;
}

const CdxStreamCreatorT tcpStreamCtor = {
    .create = __CdxTcpStreamCreate
};

