/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxSSLStream.c
 * Description : SSLStream
 * History :
 *
 */

#define LOG_NDEBUG 0
#define LOG_TAG "sslStream"
#include "cdx_config.h"
#include <CdxTypes.h>
#include <errno.h>
#include <string.h>
#include <debug.h>
#undef connect

//#include "cdx_malloc.h"
#include <CdxStream.h>
#include <CdxAtomic.h>
//#include <openssl/ssl.h>
//#include <openssl/err.h>

#include <SmartDnsService.h>

#include <CdxSocketUtil.h>
#include "lwip/netdb.h"
#include "lwip/tcp.h"
#include "lwip/sockets.h"

#define CEDARX_MBEDTLS

#ifdef CEDARX_MBEDTLS
#include "mbedtls/net.h"
#include "mbedtls/debug.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/certs.h"
#endif

#define CEDARX_MBEDTLS_DEBUG_ON   0
#if CEDARX_MBEDTLS_DEBUG_ON
#define MBEDTLS_DEBUG_LEVEL   3   /*according to mbedtls, debug level can be 0, 1, 2, 3, 4*/
#endif

typedef struct CdxSSLStreamImpl
{
    CdxStreamT base;
    cdx_int32 ioState;
    cdx_int32 sockRecvBufLen;
    cdx_int8 notBlockFlag;
    cdx_int8 readOnceFlag;
    cdx_int8 reserve[2];
    cdx_int32 exitFlag;                  //when close, exit
    cdx_int32 forceStopFlag;
    cdx_int32 sockFd;                    //socket fd
    cdx_int32 port;
    cdx_char *hostname;
    cdx_atomic_t ref;                    //reference count, for free resource while still blocking.
    cdx_atomic_t state;
    pthread_mutex_t stateLock;
    cdx_char *certificate;

#ifdef CEDARX_MBEDTLS
    mbedtls_net_context *fd_ctx;
    mbedtls_ssl_context *ssl_ctx;
    mbedtls_ssl_config *ssl_conf;
    mbedtls_x509_crt *ssl_cacert;
//    mbedtls_entropy_context *ssl_entropy;
//    mbedtls_ctr_drbg_context *ssl_ctr_drbg;
#else
    SSL *ssl;
    SSL_CTX *ctx;
#endif

    pthread_cond_t dnsCond;
    pthread_mutex_t* dnsMutex;
    int dnsRet;
    struct addrinfo *dnsAI;

    ParserCallback callback;
    void *pUserData;

    //add more
}CdxSSLStreamImplT;

static void CdxSSLStreamDecRef(CdxStreamT *stream);

typedef struct CdxHttpSendBuffer
{
    void *size;
    void *buf;
}CdxHttpSendBufferT;

enum HttpStreamStateE
{
    SSL_STREAM_IDLE    = 0x00L,
    SSL_STREAM_CONNECTING = 0x01L,
    SSL_STREAM_READING = 0x02L,
    SSL_STREAM_WRITING = 0x03L,
    SSL_STREAM_FORCESTOPPED = 0x04L,
    //TCP_STREAM_CLOSING
};
#define SSL_FREE(impl) do{      \
    if(impl->ssl)               \
    {                           \
        SSL_free(impl->ssl);    \
        impl->ssl = NULL;       \
    }                           \
    if(impl->ctx)               \
    {                           \
        SSL_CTX_free(impl->ctx);\
        impl->ctx = NULL;       \
    }                           \
}while(0)

#ifdef CEDARX_MBEDTLS
static void CdxstreamDeinitTls(CdxSSLStreamImplT *impl)
{
    if (impl->ssl_ctx)
    {
        mbedtls_ssl_free(impl->ssl_ctx);
    }
    if (impl->ssl_conf)
    {
        mbedtls_ssl_config_free(impl->ssl_conf);
    }
//    if (impl->ssl_ctr_drbg)
//    {
//        mbedtls_ctr_drbg_free(impl->ssl_ctr_drbg);
//    }
    if ((impl->certificate) && (impl->ssl_cacert))
    {
        mbedtls_x509_crt_free(impl->ssl_cacert);
    }
//    if (impl->ssl_entropy)
//    {
//        mbedtls_entropy_free(impl->ssl_entropy);
//    }
}

static void CdxstreamFreeTls(CdxSSLStreamImplT *impl)
{
    if (impl->ssl_ctx)
    {
        free(impl->ssl_ctx);
        impl->ssl_ctx = NULL;
    }
    if (impl->ssl_conf)
    {
        free(impl->ssl_conf);
        impl->ssl_conf = NULL;
    }
//    if (impl->ssl_ctr_drbg)
//    {
//        free(impl->ssl_ctr_drbg);
//        impl->ssl_ctr_drbg = NULL;
//    }
    if (impl->ssl_cacert)
    {
        free(impl->ssl_cacert);
        impl->ssl_cacert = NULL;
    }
//   if (impl->ssl_entropy)
//    {
//        free(impl->ssl_entropy);
//        impl->ssl_entropy = NULL;
//    }
    if (impl->fd_ctx)
    {
        free(impl->fd_ctx);
        impl->fd_ctx = NULL;
    }
}

#endif

#ifdef CEDARX_MBEDTLS
cdx_int32  CdxSSLConnect(cdx_int32 sockfd, mbedtls_ssl_context *ssl_ctx,
                        cdx_long timeoutUs, cdx_int32 *pForceStop, cdx_bool verify)
#else
cdx_int32  CdxSSLConnect(cdx_int32 sockfd, SSL *ssl,
                        cdx_long timeoutUs, cdx_int32 *pForceStop)
#endif
{
    cdx_int32 ret = 0, retval = 0;
    cdx_long loopTimes = 0, i = 0;

    if ((CdxSockIsBlocking(sockfd)) || (timeoutUs == 0))
    {
        loopTimes = ((cdx_ulong)(-1L))>> 1;
    }
    else
    {
        loopTimes = timeoutUs/CDX_SELECT_TIMEOUT;
    }

//    if(impl->callback)
//    {
//        int flag = DETAIL_INFO_STREAM_HANDSHAKE_START;
//        logd("RPT: STREAM_EVT_DETAIL_INFO, cb:%08lx, flag:%d", impl->callback, flag);
//        impl->callback(impl->pUserData, STREAM_EVT_DETAIL_INFO, &flag);
//    }
    for (i = 0; i < loopTimes; i++)
    {
        if (pForceStop && *pForceStop)
        {
            CDX_LOGE("<%s,%d>force stop", __FUNCTION__, __LINE__);
            retval = -2;
            goto exit;
        }

#ifdef CEDARX_MBEDTLS
        ret = mbedtls_ssl_handshake(ssl_ctx);
        if (ret == 0)
        {
            //handshake success, check cacert verify result if necessary
            if (verify) {
                ret = mbedtls_ssl_get_verify_result(ssl_ctx);
                if (ret != 0) {
                    CDX_LOGE("cacert verify fail.\n");
                    retval = -1;
                    goto exit;
                }
            }
            retval = 0;
            goto exit;
        }
        else if ((ret != MBEDTLS_ERR_SSL_WANT_WRITE) && (ret != MBEDTLS_ERR_SSL_WANT_READ) && (ret != MBEDTLS_ERR_SSL_TIMEOUT))
        {
            CDX_LOGE("mbedtls_ssl_handshake ret:-0x%x", -(int)ret);
            retval = -1;
            goto exit;
        }
        else
        {
            usleep(10000);
            continue;
        }
#else
        ret = SSL_connect(ssl);
        if (ret > 0)
        {
            //success
            retval = 0;
            goto exit;
        }
        else if(ret == 0)
            /*The TLS/SSL handshake was not successful but was shut down
              controlled and by the specifications of the TLS/SSL protocol.
              Call SSL_get_error() with the return value ret to find out the reason.*/
        {
            CDX_LOGE("xxx ret(%d), %s", ret, ERR_error_string(ERR_get_error(), NULL));
            retval = -1;
            goto exit;
        }
        else
        {
            ret = SSL_get_error(ssl, ret);
            if(ret == SSL_ERROR_WANT_WRITE)
            {
                usleep(20000);
                continue;
            }
            else
            {
                CDX_LOGE("xxx ret(%d), %s", ret, ERR_error_string(ERR_get_error(), NULL));
                retval = -1;
                goto exit;
            }
        }
#endif
    }
    retval = -1;
exit:
//    if(impl->callback)
//    {
//        int flag = DETAIL_INFO_STREAM_HANDSHAKE_END;
//        logd("RPT: STREAM_EVT_DETAIL_INFO, cb:%08lx, flag:%d", impl->callback, flag);
//        impl->callback(impl->pUserData, STREAM_EVT_DETAIL_INFO, &flag);
//    }
    return retval;
}
#ifdef CEDARX_MBEDTLS
cdx_ssize CdxSSLNoblockRecv(mbedtls_ssl_context *ssl_ctx, void *buf, cdx_size len)
{
    return mbedtls_ssl_read(ssl_ctx, buf, len);
}
#else
cdx_ssize CdxSSLNoblockRecv(SSL *ssl, void *buf, cdx_size len)
{
    return SSL_read(ssl, buf, len);
}
#endif

#if 0
#ifdef CEDARX_MBEDTLS
cdx_ssize CdxSSLRecv(cdx_int32 sockfd, mbedtls_ssl_context *ssl_ctx, void *buf, cdx_size len,
                        cdx_long timeoutUs, cdx_int32 *pForceStop)
#else
cdx_ssize CdxSSLRecv(cdx_int32 sockfd, SSL *ssl, void *buf, cdx_size len,
                        cdx_long timeoutUs, cdx_int32 *pForceStop)
#endif
{
    cdx_ssize ret = 0, recvSize = 0;
    cdx_long loopTimes = 0, i = 0;

    if ((CdxSockIsBlocking(sockfd)) || (timeoutUs == 0))
    {
        loopTimes = ((cdx_ulong)(-1L))>> 1;
    }
    else
    {
        loopTimes = timeoutUs/CDX_SELECT_TIMEOUT;
    }

    for (i = 0; i < loopTimes; i++)
    {
        if (pForceStop && *pForceStop)
        {
            CDX_LOGE("<%s,%d>force stop", __FUNCTION__, __LINE__);
            return (recvSize > 0) ? recvSize : -2;
        }

#ifdef CEDARX_MBEDTLS
	int64_t mbedtls_ssl_read1 = CdxGetNowUs();
        ret = mbedtls_ssl_read(ssl_ctx, ((unsigned char *)buf) + recvSize, len - recvSize);

	int64_t mbedtls_ssl_read2 = CdxGetNowUs();
	if(mbedtls_ssl_read2 - mbedtls_ssl_read1 > 15000)
	{
		CDX_LOGE("wht>>>SSL, mbedtls_ssl_read timeout:%lldms, read ret = %d\n",(mbedtls_ssl_read2 - mbedtls_ssl_read1)/1000, ret);
	}

        if (ret < 0)
        {
            if ((ret == MBEDTLS_ERR_SSL_WANT_READ) || (ret == MBEDTLS_ERR_SSL_WANT_WRITE) || (ret == MBEDTLS_ERR_SSL_TIMEOUT))
            {
                usleep(20000);
                continue;
            }
            else
            {
                CDX_LOGE("read error, -0x%x, len -recvSize = %d", -(int)ret, len - recvSize);
                return -1;
            }
        }
        else if (ret == 0)//socket is close by peer?
        {
            CDX_LOGD("xxx recvSize(%ld),sockfd(%d), want to read(%lu), errno(%d),"
                " shut down by peer?", recvSize, sockfd, len, errno);
            return recvSize;
        }
        else
        {
            recvSize += ret;
            if ((cdx_size)recvSize == len)
            {
                return recvSize;
            }
        }
#else
        ret = SSL_read(ssl, ((char *)buf) + recvSize, len - recvSize);
        if (ret < 0)
        {
            ret = SSL_get_error(ssl, ret);
            if(ret == SSL_ERROR_WANT_READ)
            {
                usleep(20000);
                continue;
            }
            else
            {
                CDX_LOGE("ret(%ld), read error, %s", ret,
                    ERR_error_string(ERR_get_error(), NULL));
                ERR_print_errors_fp(stderr);
                return -1;
            }
        }
        else if (ret == 0)//socket is close by peer?
        {
            CDX_LOGD("xxx recvSize(%ld),sockfd(%d), want to read(%lu), errno(%d),"
                " shut down by peer?", recvSize, sockfd, len, errno);
            return recvSize;
        }
        else
        {
            recvSize += ret;
            if ((cdx_size)recvSize == len)
            {
                return recvSize;
            }
        }
#endif
    }

    return recvSize;
}
#endif

cdx_ssize CdxSSLRecv(cdx_int32 sockfd, mbedtls_ssl_context *ssl, void *buf, cdx_size len,
                        cdx_long timeoutUs, cdx_int32 *pForceStop)
{
    fd_set rs;
    fd_set errs;
    struct timeval tv;
    cdx_ssize ret = 0, recvSize = 0;
    cdx_long loopTimes = 0, i = 0;
    cdx_int32 ioErr;

    //blocking read
	int64_t mbedtls_blocking_read1 = CdxGetNowUs();
    if (CdxSockIsBlocking(sockfd))
    {
        while(1)
        {
            if (pForceStop && *pForceStop)
            {
                CDX_LOGE("<%s,%d>force stop.recvSize(%ld)", __FUNCTION__, __LINE__, recvSize);
                return recvSize>0 ? recvSize : -2;
            }
			ret = mbedtls_ssl_read(ssl, ((unsigned char *)buf) + recvSize, len - recvSize);
            if (ret < 0)
            {
				if ((ret == MBEDTLS_ERR_SSL_WANT_READ) || (ret == MBEDTLS_ERR_SSL_WANT_WRITE) || (ret == MBEDTLS_ERR_SSL_TIMEOUT))
				{
					usleep(20000);//20000
					continue;
				}
				else
				{
					CDX_LOGE("read error, -0x%x, len -recvSize = %d", -(int)ret, len - recvSize);
					return -1;
				}
            }
            else if(ret == 0)
            {
                CDX_LOGD("xxx recvSize(%ld),sockfd(%d), want to read(%lu), errno(%d),"
                    " shut down by peer?", recvSize, sockfd, len, errno);
                return recvSize;
            }
            else
            {
                recvSize += ret;
                if ((cdx_size)recvSize == len)
                {
					int64_t mbedtls_blocking_read2 = CdxGetNowUs();
					if(mbedtls_blocking_read2 - mbedtls_blocking_read1 > 15000)
					{
						CDX_LOGE("wht>>>SSL, blocking read timeout:%lldms \n",(mbedtls_blocking_read2 - mbedtls_blocking_read1)/1000);
					}
                    return recvSize;
                }
            }
        }
    }
	int64_t mbedtls_loop1 = CdxGetNowUs();
    // non-blocking read
    if (timeoutUs == 0)
    {
        loopTimes = ((unsigned long)(-1L))>> 1;
    }
    else
    {
        loopTimes = timeoutUs/CDX_SELECT_TIMEOUT;
    }
	
	int64_t mbedtls_loop2 = CdxGetNowUs();
	if(mbedtls_loop2 - mbedtls_loop1 > 15000)
	{
		CDX_LOGE("wht>>>SSL, mbedtls_loop timeout:%lldms \n",(mbedtls_loop2 - mbedtls_loop1)/1000);
	}
	
    for (i = 0; i < loopTimes; i++)
    {
        if (pForceStop && *pForceStop)
        {
            CDX_LOGE("<%s,%d>force stop", __FUNCTION__, __LINE__);
            return recvSize>0 ? recvSize : -2;
        }

        FD_ZERO(&rs);
        FD_SET(sockfd, &rs);
        FD_ZERO(&errs);
        FD_SET(sockfd, &errs);
        tv.tv_sec = 0;
        tv.tv_usec = CDX_SELECT_TIMEOUT;
        ret = select(sockfd + 1, &rs, NULL, &errs, &tv);
        if (ret < 0)
        {
            ioErr = errno;
            if (EINTR == ioErr)
            {
                continue;
            }
            CDX_LOGE("<%s,%d>select err(%d)", __FUNCTION__, __LINE__, ioErr);
            return -1;
        }
        else if (ret == 0)
        {
            //("timeout\n");
            //CDX_LOGV("xxx timeout, select again...");
            continue;
        }

		int64_t mbedtls_ssl_time1 = CdxGetNowUs();
		
        while (1)
        {
            if (pForceStop && *pForceStop)
            {
                CDX_LOGE("<%s,%d>force stop.recvSize(%ld)", __FUNCTION__, __LINE__, recvSize);
                return recvSize>0 ? recvSize : -2;
            }
            if(FD_ISSET(sockfd,&errs))
            {
                CDX_LOGE("<%s,%d>errs ", __FUNCTION__, __LINE__);
                break;
            }
            if(!FD_ISSET(sockfd, &rs))
            {
                CDX_LOGV("select > 0, but sockfd is not ready?");
                break;
            }
			
			ret = mbedtls_ssl_read(ssl, ((unsigned char *)buf) + recvSize, len - recvSize);
			int64_t mbedtls_ssl_time2 = CdxGetNowUs();
				
			if(mbedtls_ssl_time2 - mbedtls_ssl_time1 > 15000)
			{
				CDX_LOGD("wht>>>SSL, mbedtls_ssl_read1111 timeout:%lldms , ret = 0x%08x, len = %d, recvSize =%d\n",(mbedtls_ssl_time2 - mbedtls_ssl_time1)/1000, ret, len, recvSize);
			}
            if (ret < 0)
            {
                if ((ret == MBEDTLS_ERR_SSL_WANT_READ) || (ret == MBEDTLS_ERR_SSL_WANT_WRITE) || (ret == MBEDTLS_ERR_SSL_TIMEOUT))
				{
					usleep(20000);//20000
					continue;
				}
				else
				{
					CDX_LOGE("read error, -0x%x, len -recvSize = %d", -(int)ret, len - recvSize);
					return -1;
				}
            }
            else if (ret == 0)//socket is close by peer?
            {
                CDX_LOGD("xxx recvSize(%ld),sockfd(%d), want to read(%lu), errno(%d),"
                    " shut down by peer?", recvSize, sockfd, len, errno);
                return recvSize;
            }
            else // ret > 0
            {
                recvSize += ret;
                if ((cdx_size)recvSize == len)
                {
					int64_t mbedtls_ssl_time3 = CdxGetNowUs();
					if(mbedtls_ssl_time3 - mbedtls_ssl_time1 > 15000)
					{
						CDX_LOGE("wht>>>SSL, mbedtls_ssl_read recvSize==len timeout:%lldms \n",(mbedtls_ssl_time3 - mbedtls_ssl_time1)/1000);
					}
                    return recvSize;
                }
            }
        }

    }

    return recvSize;
}
#ifdef CEDARX_MBEDTLS
cdx_ssize CdxSSLSend(cdx_int32 sockfd, mbedtls_ssl_context *ssl_ctx, const void *buf, cdx_size len,
                        cdx_long timeoutUs, cdx_int32 *pForceStop)
#else
cdx_ssize CdxSSLSend(cdx_int32 sockfd, SSL *ssl, const void *buf, cdx_size len,
                        cdx_long timeoutUs, cdx_int32 *pForceStop)
#endif
{
    fd_set ws;
    struct timeval tv;
    cdx_ssize ret = 0, sendSize = 0;
    cdx_long loopTimes = 0, i = 0;
    cdx_int32 ioErr;

    //blocking send
    if (CdxSockIsBlocking(sockfd))
    {
        while(1)
        {
            if (pForceStop && *pForceStop)
            {
                CDX_LOGE("<%s,%d>force stop. sendSize(%ld)", __FUNCTION__, __LINE__, sendSize);
                return sendSize>0 ? sendSize : -2;
            }

#ifdef CEDARX_MBEDTLS
            ret = mbedtls_ssl_write(ssl_ctx, ((const unsigned char *)buf) + sendSize, len - sendSize);
            if (ret < 0)
            {
                if ((ret == MBEDTLS_ERR_SSL_WANT_READ) || (ret == MBEDTLS_ERR_SSL_WANT_WRITE) || (ret == MBEDTLS_ERR_SSL_TIMEOUT))
                {
                    usleep(20000);
                    continue;
                }
                else
                {
                    CDX_LOGE("write error, -0x%x", -(int)ret);
                    return -1;
                }
            }
            else if(ret == 0)
            {
                CDX_LOGD("xxx sendSize(%ld),sockfd(%d), want to read(%lu), errno(%d),"
                    " shut down by peer?", sendSize, sockfd, len, errno);
                return sendSize;
            }
            else
            {
                sendSize += ret;
                if ((cdx_size)sendSize == len)
                {
                    return sendSize;
                }
            }
#else
            ret = SSL_write(ssl, ((char *)buf) + sendSize, len - sendSize);
            if (ret < 0)
            {
                ret = SSL_get_error(ssl, ret);
                if(ret == SSL_ERROR_WANT_WRITE)
                {
                    usleep(20000);
                    continue;
                }
                else
                {
                    CDX_LOGE("ret(%ld), write error, %s", ret,
                        ERR_error_string(ERR_get_error(), NULL));
                    return -1;
                }
            }
            else if(ret == 0)
            {
                CDX_LOGD("xxx sendSize(%ld),sockfd(%d), want to read(%lu), errno(%d),"
                    " shut down by peer?", sendSize, sockfd, len, errno);
                return sendSize;
            }
            else
            {
                sendSize += ret;
                if ((cdx_size)sendSize == len)
                {
                    return sendSize;
                }
            }
#endif
        }
    }

    //non-blocking send
    if (timeoutUs == 0)
    {
        loopTimes = ((unsigned long)(-1L))>> 1;
    }
    else
    {
        loopTimes = timeoutUs/CDX_SELECT_TIMEOUT;
    }

    for (i = 0; i < loopTimes; i++)
    {
        if (pForceStop && *pForceStop)
        {
            CDX_LOGE("<%s,%d>force stop", __FUNCTION__, __LINE__);
            return sendSize>0 ? sendSize : -2;
        }

        FD_ZERO(&ws);
        FD_SET(sockfd, &ws);
        tv.tv_sec = 0;
        tv.tv_usec = CDX_SELECT_TIMEOUT;
        ret = select(sockfd + 1, NULL, &ws, NULL, &tv);
        if (ret < 0)
        {
            ioErr = errno;
            if (EINTR == ioErr)
            {
                continue;
            }
            CDX_LOGE("<%s,%d>select err(%d)", __FUNCTION__, __LINE__, errno);
            return -1;
        }
        else if (ret == 0)
        {
            //("timeout\n");
            continue;
        }

#ifdef CEDARX_MBEDTLS
        while (1)
        {
            if (pForceStop && *pForceStop)
            {
                CDX_LOGE("<%s,%d>force stop", __FUNCTION__, __LINE__);
                return sendSize>0 ? sendSize : -2;
            }

            ret = mbedtls_ssl_write(ssl_ctx, ((const unsigned char *)buf) + sendSize, len - sendSize);
            if (ret < 0)
            {
                if ((ret == MBEDTLS_ERR_SSL_WANT_READ) || (ret == MBEDTLS_ERR_SSL_WANT_WRITE) || (ret == MBEDTLS_ERR_SSL_TIMEOUT))
                {
                    break;
                }
                else
                {
                    CDX_LOGE("xxx ret(%ld)", ret);
                    return -1;
                }
            }
            else if (ret == 0)
            {
                //buffer is full?
                return sendSize;
            }
            else
            {
                sendSize += ret;
                if ((cdx_size)sendSize == len)
                {
                    return sendSize;
                }
            }
        }
#else
        while (1)
        {
            if (pForceStop && *pForceStop)
            {
                CDX_LOGE("<%s,%d>force stop", __FUNCTION__, __LINE__);
                return sendSize>0 ? sendSize : -2;
            }

            ret = SSL_write(ssl, ((char *)buf) + sendSize, len - sendSize);
            if (ret < 0)
            {
                ret = SSL_get_error(ssl, ret);
                if(ret == SSL_ERROR_WANT_WRITE)
                {
                    break;
                }
                else
                {
                    CDX_LOGE("xxx ret(%ld)", ret);
                    ERR_print_errors_fp(stderr);
                    return -1;
                }
            }
            else if (ret == 0)
            {
                //buffer is full?
                return sendSize;
            }
            else
            {
                sendSize += ret;
                if ((cdx_size)sendSize == len)
                {
                    return sendSize;
                }
            }
        }
#endif
    }

    return sendSize;
}


cdx_int32 count_ret = 0;
static cdx_int32 __CdxSSLStreamRead(CdxStreamT *stream, void *buf, cdx_uint32 len)
{
    CdxSSLStreamImplT *impl;
    cdx_int32 ret;
    cdx_int32 recvSize = 0;
    cdx_int32 ioErr;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxSSLStreamImplT, base);

    if(stream == NULL || buf == NULL || len <= 0)
    {
        CDX_LOGW("check parameter.");
        return -1;
    }

    pthread_mutex_lock(&impl->stateLock);
    if(impl->forceStopFlag)
    {
        pthread_mutex_unlock(&impl->stateLock);
        return -2;
    }
    CdxAtomicInc(&impl->ref);
    CdxAtomicSet(&impl->state, SSL_STREAM_READING);
    pthread_mutex_unlock(&impl->stateLock);

    while(impl->notBlockFlag)
    {
        if(impl->forceStopFlag)
        {
            ret = -2;
            goto __exit0;
        }
#ifdef CEDARX_MBEDTLS
        ret = CdxSSLNoblockRecv(impl->ssl_ctx, buf, len);
#else
        ret = CdxSSLNoblockRecv(impl->ssl, buf, len);
#endif
        if (ret < 0) //TODO
        {
            ioErr = errno;
            if (EAGAIN == ioErr)
            {
                usleep(5000);
                continue;
            }
            else
            {
                CDX_LOGE("<%s,%d>recv err(%d), ret(%d)", __FUNCTION__, __LINE__, errno, ret);
#ifndef CEDARX_MBEDTLS
                ret = SSL_get_error(impl->ssl, ret);
                if(ret == SSL_ERROR_WANT_READ)
                {
                    CDX_LOGD("SSL_ERROR_WANT_READ");
                }
                else if(ret == SSL_ERROR_WANT_WRITE)
                {
                    CDX_LOGD("SSL_ERROR_WANT_WRITE");
                }
                else
                {
                    CDX_LOGE("xxx %s", ERR_error_string(ERR_get_error(), NULL));
                }
#endif
                impl->ioState = CDX_IO_STATE_ERROR;
                impl->notBlockFlag = 0;
                ret = -1;
                goto __exit0;
            }
        }
        impl->notBlockFlag = 0;

__exit0:
        pthread_mutex_lock(&impl->stateLock);
        CdxAtomicSet(&impl->state, SSL_STREAM_IDLE);
        CdxSSLStreamDecRef(stream);
        pthread_mutex_unlock(&impl->stateLock);

        return ret;
    }

    while((cdx_uint32)recvSize < len)
    {
        if(impl->forceStopFlag)
        {
            CDX_LOGV("__CdxSSLStreamRead forceStop.");
            if(recvSize > 0)
                break;
            else
            {
                recvSize = -2;
                goto __exit1;
            }
        }
#ifdef CEDARX_MBEDTLS
        if (impl->readOnceFlag)
        {
	    CDX_LOGE("wht>>>>>>>>debug, readOnceFlag CdxSSLRecv 5000, len -recvSize = %d", len - recvSize);
            ret = CdxSSLRecv(impl->sockFd, impl->ssl_ctx, (char *)buf + recvSize,
                                len - recvSize, 5000, &impl->forceStopFlag);
        }
        else
        {
	    CDX_LOGV("wht>>>>>>>>debug, CdxSSLRecv 0, len -recvSize = %d", len - recvSize);
	int64_t ssl_recv_read1 = CdxGetNowUs();
            ret = CdxSSLRecv(impl->sockFd, impl->ssl_ctx, (char *)buf + recvSize,
                                len - recvSize, 0, &impl->forceStopFlag);
	    count_ret +=ret;
	int64_t ssl_recv_read2 = CdxGetNowUs();
	if(ssl_recv_read2 - ssl_recv_read1 > 15000)
	{
		CDX_LOGE("wht>>>SSL, CdxSSLRecv timeout:%lldms, read ret = %d, count_read = %d\n",(ssl_recv_read2- ssl_recv_read1)/1000, ret, count_ret);
	}

        }
#else
        if (impl->readOnceFlag)
        {
            ret = CdxSSLRecv(impl->sockFd, impl->ssl, (char *)buf + recvSize,
                                len - recvSize, 5000, &impl->forceStopFlag);
        }
        else
        {
            ret = CdxSSLRecv(impl->sockFd, impl->ssl, (char *)buf + recvSize,
                                len - recvSize, 0, &impl->forceStopFlag);
        }
#endif
        if(ret < 0)
        {
            if(ret == -2)
            {
                recvSize = recvSize>0 ? recvSize : -2;
                goto __exit1;
            }
            impl->ioState = CDX_IO_STATE_ERROR;
            CDX_LOGE("__CdxSSLStreamRead error(%d).", errno);
            recvSize = -1;
            goto __exit1;
        }
        else if(ret == 0)
        {
            break;
        }
        else
        {
            recvSize += ret;
        }

        if (impl->readOnceFlag)
        {
            impl->readOnceFlag = 0;
            break;
        }
    }

__exit1:
    pthread_mutex_lock(&impl->stateLock);
    CdxAtomicSet(&impl->state, SSL_STREAM_IDLE);
    CdxSSLStreamDecRef(stream);
    pthread_mutex_unlock(&impl->stateLock);

    return recvSize;
}

static cdx_int32 __CdxSSLStreamGetIOState(CdxStreamT *stream)
{
    CdxSSLStreamImplT *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxSSLStreamImplT, base);

    return impl->ioState;
}
static cdx_int32 __CdxSSLStreamWrite(CdxStreamT *stream, void *buf, cdx_uint32 len)
{
    CdxSSLStreamImplT *impl;
    size_t size = 0;
    ssize_t ret = 0;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxSSLStreamImplT, base);

    pthread_mutex_lock(&impl->stateLock);
    if(impl->forceStopFlag)
    {
        pthread_mutex_unlock(&impl->stateLock);
        return -1;
    }
    CdxAtomicInc(&impl->ref);
    CdxAtomicSet(&impl->state, SSL_STREAM_WRITING);
    pthread_mutex_unlock(&impl->stateLock);

    while(size < len)
    {
#ifdef CEDARX_MBEDTLS
        ret = CdxSSLSend(impl->sockFd, impl->ssl_ctx, (char *)buf + size, len - size,
                                                        0, &impl->forceStopFlag);
#else
        ret = CdxSSLSend(impl->sockFd, impl->ssl, (char *)buf + size, len - size,
                                                        0, &impl->forceStopFlag);
#endif
        if(ret < 0)
        {
            CDX_LOGE("send failed.");
            break;
        }
        else if(ret == 0)
        {
            break;
        }
        size += ret;
    }

    pthread_mutex_lock(&impl->stateLock);
    CdxAtomicSet(&impl->state, SSL_STREAM_IDLE);
    CdxSSLStreamDecRef(stream);
    pthread_mutex_unlock(&impl->stateLock);

    return (size == len) ? 0 : -1;

}
static cdx_int32 CdxSSLStreamForceStop(CdxStreamT *stream)
{
    CdxSSLStreamImplT *impl;
    long ref;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxSSLStreamImplT, base);

    CDX_LOGV("begin SSL force stop");
    pthread_mutex_lock(&impl->stateLock);
    CdxAtomicInc(&impl->ref);
    impl->forceStopFlag = 1;
    pthread_mutex_unlock(&impl->stateLock);

    while((ref = CdxAtomicRead(&impl->state)) != SSL_STREAM_IDLE)
    {
        CDX_LOGV("xxx state(%ld)", ref);
        usleep(10*1000);
    }

    pthread_mutex_lock(&impl->stateLock);
    pthread_mutex_unlock(&impl->stateLock);
    CdxSSLStreamDecRef(stream);
    CDX_LOGV("finish SSL force stop");
    return 0;
}
static cdx_int32 CdxSSLStreamClrForceStop(CdxStreamT *stream)
{
    CdxSSLStreamImplT *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxSSLStreamImplT, base);

    pthread_mutex_lock(&impl->stateLock);
    impl->forceStopFlag = 0;
    CdxAtomicSet(&impl->state, SSL_STREAM_IDLE);
    pthread_mutex_unlock(&impl->stateLock);

    return 0;
}

static cdx_int32 __CdxSSLStreamControl(CdxStreamT *stream, cdx_int32 cmd, void *param)
{
    CdxSSLStreamImplT *impl;

    impl = CdxContainerOf(stream, CdxSSLStreamImplT, base);

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
            return CdxSSLStreamForceStop(stream);
        }
        case STREAM_CMD_CLR_FORCESTOP:
        {
            return CdxSSLStreamClrForceStop(stream);
        }
        case STREAM_CMD_READ_ONCE:
        {
            impl->readOnceFlag = 1;
            break;
        }
        case STREAM_CMD_SET_CALLBACK:
        {
            struct CallBack *cb = (struct CallBack *)param;
            impl->callback = cb->callback;
            impl->pUserData = cb->pUserData;
            break;
        }
        default:
        {
            CDX_LOGV("control cmd %d is not supported by ssl", cmd);
            break;
        }
    }
    return 0;
}

static cdx_int32 __CdxSSLStreamClose(CdxStreamT *stream)
{
    CdxSSLStreamImplT *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxSSLStreamImplT, base);
    CDX_LOGV("xxx SSL close begin.");
    CdxAtomicInc(&impl->ref);

    CdxSSLStreamForceStop(stream);

    CdxSSLStreamDecRef(stream);
    CdxSSLStreamDecRef(stream);
    CDX_LOGV("xxx SSL close finish.");

    return 0;
}
static void CdxSSLStreamDecRef(CdxStreamT *stream)
{
    CdxSSLStreamImplT *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxSSLStreamImplT, base);

    CdxAtomicDec(&impl->ref);
    if(CdxAtomicRead(&impl->ref) == 0)
    {
#ifdef CEDARX_MBEDTLS
        if (impl->ssl_ctx)
        {
            mbedtls_ssl_close_notify(impl->ssl_ctx);
        }
#endif
        shutdown(impl->sockFd, SHUT_RDWR);
        lwip_close(impl->sockFd);
#ifdef CEDARX_MBEDTLS
        CdxstreamDeinitTls(impl);
        CdxstreamFreeTls(impl);
#else
        if(impl->ssl)
        {
            SSL_shutdown(impl->ssl);
        }
        SSL_FREE(impl);
        ERR_free_strings();
#endif

        pthread_mutex_destroy(&impl->stateLock);
        pthread_mutex_destroy(impl->dnsMutex);
        pthread_cond_destroy(&impl->dnsCond);
        free(impl->dnsMutex);
        impl->dnsMutex = NULL;
        free(impl->certificate);
        impl->certificate = NULL;
        free(impl);
        impl = NULL;
    }
    return ;
}

#if CDX_IOT_DNS_CACHE
static void DnsResponeHook(void *userhdr, int ret, struct addrinfo *ai)
{
    CdxSSLStreamImplT *impl = (CdxSSLStreamImplT *)userhdr;

    if (impl == NULL)
      return;

    if (ret == SDS_OK)
    {
        impl->dnsAI = ai;
        /*CDX_LOGD("%x%x%x", ai->ai_addr->sa_data[0],
                                                  ai->ai_addr->sa_data[1],
                                                  ai->ai_addr->sa_data[2]);*/
    }

    impl->dnsRet = ret;
    if (impl->dnsMutex != NULL)
    {
        pthread_mutex_lock(impl->dnsMutex);
        pthread_cond_signal(&impl->dnsCond);
        pthread_mutex_unlock(impl->dnsMutex);
    }

    CdxSSLStreamDecRef(&impl->base);
    return ;
}
#endif

#if CEDARX_MBEDTLS_DEBUG_ON
static void mbedtls_debug(void *ctx, int level,const char *file,
                                 int line, const char *str)
{
    _info("%s:%04d: %s", file, line, str);
}
#endif
static int tls_example_random(void *handle, unsigned char *output, size_t output_len)
{
    uint32_t idx = 0, bytes = 0, rand_num = 0;
    struct timeval time;

    memset(&time, 0, sizeof(struct timeval));
    gettimeofday(&time, NULL);

    srand((unsigned int)(time.tv_sec * 1000 + time.tv_usec / 1000) + rand());

    for (idx = 0; idx < output_len;) {
        if (output_len - idx < 4) {
            bytes = output_len - idx;
        } else {
            bytes = 4;
        }
        rand_num = rand();
        while (bytes-- > 0) {
            output[idx++] = (uint8_t)(rand_num >> bytes * 8);
        }
    }
    return 0;
}

static int StartSSLStreamConnect(CdxStreamT *stream)
{
    CdxSSLStreamImplT *impl;
    cdx_int32 ret;
    struct addrinfo *ai = NULL;

#ifdef CEDARX_MBEDTLS
    const char *pers = "ssl_client1";
    cdx_uint32 mbedtls_timeout;
#endif

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxSSLStreamImplT, base);

    CDX_FORCE_CHECK(impl);
    if(impl->callback)
    {
        int flag = DETAIL_INFO_STREAM_DNS_START;
        //logd("RPT: STREAM_EVT_DETAIL_INFO, cb:%08lx, flag:%d", impl->callback, flag);
        impl->callback(impl->pUserData, STREAM_EVT_DETAIL_INFO, &flag);
    }
#if CDX_IOT_DNS_CACHE
    CdxAtomicInc(&impl->ref);
    impl->dnsRet = SDSRequest(impl->hostname, impl->port, &ai, impl, DnsResponeHook);

    if (impl->dnsRet == SDS_OK)
    {
        CdxSSLStreamDecRef(&impl->base);
        CDX_FORCE_CHECK(ai);
    }
    else if (impl->dnsRet == SDS_PENDING)
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
        CdxSSLStreamDecRef(&impl->base);
    }
#else
    cdx_char strPort[10] = {0};
    sprintf(strPort, "%d", impl->port);
    ret = lwip_getaddrinfo(impl->hostname, strPort, NULL, &ai);
    struct addrinfo *ai_backup = ai;
#endif
    if(impl->callback)
    {
        int flag = DETAIL_INFO_STREAM_DNS_END;
        //logd("RPT: STREAM_EVT_DETAIL_INFO, cb:%08lx, flag:%d", impl->callback, flag);
        impl->callback(impl->pUserData, STREAM_EVT_DETAIL_INFO, &flag);
    }
    if (ai == NULL)
    {
        goto err_out0;
    }

    do
    {
        impl->sockRecvBufLen = 0;
        impl->sockFd = CdxAsynSocket(ai->ai_family, &impl->sockRecvBufLen);
        if(impl->sockFd < 0)
            continue;

        if(impl->callback)
        {
            int flag = DETAIL_INFO_STREAM_CONNECT_START;
            //logd("RPT: STREAM_EVT_DETAIL_INFO, cb:%08lx, flag:%d", impl->callback, flag); 
            impl->callback(impl->pUserData, STREAM_EVT_DETAIL_INFO, &flag);
        }
        ret = CdxSockAsynConnect(impl->sockFd,
                ai->ai_addr, ai->ai_addrlen, 0,
                &impl->forceStopFlag);
        if(impl->callback)
        {
            int flag = DETAIL_INFO_STREAM_CONNECT_END;
            //logd("RPT: STREAM_EVT_DETAIL_INFO, cb:%08lx, flag:%d", impl->callback, flag);
            impl->callback(impl->pUserData, STREAM_EVT_DETAIL_INFO, &flag);
        }

        if(ret == 0)
        {
            break;
        }
        else if(ret < 0)
        {
            CDX_LOGE("connect failed. error(%d): %s.",
                     errno, strerror(errno));
            lwip_close(impl->sockFd);
            impl->sockFd = -1;
        }

        if(impl->forceStopFlag == 1)
        {
            CDX_LOGV("force stop connect.");
            goto err_out0;
        }
    } while ((ai = ai->ai_next) != NULL);

    if (ai == NULL)
    {
        CDX_LOGE("connect failed.");
        goto err_out0;
    }

#if (!CDX_IOT_DNS_CACHE)
    lwip_freeaddrinfo(ai_backup);
    ai_backup = NULL;
#endif /* (!CDX_IOT_DNS_CACHE) */

#ifdef CEDARX_MBEDTLS
    impl->fd_ctx        = calloc(1, sizeof(mbedtls_net_context));
    impl->ssl_ctx       = calloc(1, sizeof(mbedtls_ssl_context));
    impl->ssl_conf      = calloc(1, sizeof(mbedtls_ssl_config));
    impl->ssl_cacert    = calloc(1, sizeof(mbedtls_x509_crt));
//    impl->ssl_entropy   = calloc(1, sizeof(mbedtls_entropy_context));
//    impl->ssl_ctr_drbg  = calloc(1, sizeof(mbedtls_ctr_drbg_context));
    if (!(impl->ssl_ctx && impl->ssl_conf &&
        impl->ssl_cacert))//  && impl->ssl_entropy&& impl->ssl_ctr_drbg
        goto err_out1;

    mbedtls_ssl_init(impl->ssl_ctx);
    mbedtls_ssl_config_init(impl->ssl_conf);
    mbedtls_x509_crt_init(impl->ssl_cacert);
//    mbedtls_entropy_init(impl->ssl_entropy);
//    mbedtls_ctr_drbg_init(impl->ssl_ctr_drbg);

#if CEDARX_MBEDTLS_DEBUG_ON
    mbedtls_debug_set_threshold(MBEDTLS_DEBUG_LEVEL);
    mbedtls_ssl_conf_dbg(impl->ssl_conf, mbedtls_debug, stdout );
#endif
#if 0	
    int mbedtls_ret = 0;
    mbedtls_ret = mbedtls_ctr_drbg_seed(impl->ssl_ctr_drbg, mbedtls_entropy_func, impl->ssl_entropy, (const unsigned char *)pers, strlen(pers));

    //if (mbedtls_ctr_drbg_seed(impl->ssl_ctr_drbg, mbedtls_entropy_func, impl->ssl_entropy, (const unsigned char *)pers, strlen(pers)) != 0) {
    if (mbedtls_ret != 0) {
        CDX_LOGE("mbedtls_ctr_drbg_seed fail, mbedtls_ret = %d, MBEDTLS_CTR_DRBG_ENTROPY_LEN = %d, MBEDTLS_ENTROPY_BLOCK_SIZE = %d", mbedtls_ret, MBEDTLS_CTR_DRBG_ENTROPY_LEN, MBEDTLS_ENTROPY_BLOCK_SIZE);
        goto err_out2;
    }
#endif
    if (impl->certificate) {

        if (mbedtls_x509_crt_parse(impl->ssl_cacert, (const unsigned char *)impl->certificate, strlen(impl->certificate) + 1)) {
            CDX_LOGE("mbedtls_x509_crt_parse fail\n");
            goto err_out2;
        }
    }

    if (mbedtls_ssl_config_defaults(impl->ssl_conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
        CDX_LOGE("mbedtls_ssl_config_defaults fail");
        goto err_out2;
    }

    if (impl->certificate) {
        mbedtls_ssl_conf_authmode(impl->ssl_conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
        mbedtls_ssl_conf_ca_chain(impl->ssl_conf, impl->ssl_cacert, NULL);
    } else {
        mbedtls_ssl_conf_authmode(impl->ssl_conf, MBEDTLS_SSL_VERIFY_NONE);
    }
    //mbedtls_ssl_conf_rng(impl->ssl_conf, mbedtls_ctr_drbg_random, impl->ssl_ctr_drbg);
    mbedtls_ssl_conf_rng(impl->ssl_conf, tls_example_random, NULL);
    mbedtls_timeout = CDX_SELECT_TIMEOUT / 1000;
    mbedtls_ssl_conf_read_timeout(impl->ssl_conf, mbedtls_timeout);

    if (mbedtls_ssl_setup(impl->ssl_ctx, impl->ssl_conf) != 0) {
        CDX_LOGE("mbedtls_ssl_setup fail");
        goto err_out2;
    }
    impl->fd_ctx->fd = impl->sockFd;
    mbedtls_ssl_set_bio(impl->ssl_ctx, impl->fd_ctx, mbedtls_net_send, mbedtls_net_recv, mbedtls_net_recv_timeout);

#else
    //SSL initial
    SSL_library_init();
    SSL_load_error_strings();
    impl->ctx = SSL_CTX_new(TLSv1_client_method());
    // downward compatibility? SSLv3_client_method()/SSLv23_client_method()
    if(impl->ctx == NULL)
    {
        CDX_LOGE("xxx %s", ERR_error_string(ERR_get_error(), NULL));
        goto err_out;
    }
    impl->ssl = SSL_new(impl->ctx);
    if(impl->ssl == NULL)
    {
        CDX_LOGE("xxx %s", ERR_error_string(ERR_get_error(), NULL));
        goto err_out;
    }

    //inject relevance into socket&SSL
    ret = SSL_set_fd(impl->ssl, impl->sockFd);
    if(ret == 0)
    {
        ERR_print_errors_fp(stderr);
        goto err_out;
    }
#endif
    CdxSockSetBlocking(impl->sockFd, 0);// set socket to non-blocking

    if(impl->callback)
    {
        int flag = DETAIL_INFO_STREAM_HANDSHAKE_START;
        //logd("RPT: STREAM_EVT_DETAIL_INFO, cb:%08lx, flag:%d", impl->callback, flag);
        impl->callback(impl->pUserData, STREAM_EVT_DETAIL_INFO, &flag);
    }
#ifdef CEDARX_MBEDTLS
    ret = CdxSSLConnect(impl->sockFd,impl->ssl_ctx, 0, &impl->forceStopFlag, impl->certificate != NULL);
    free(impl->certificate);
    impl->certificate = NULL;
#else
    ret = CdxSSLConnect(impl->sockFd,impl->ssl, 0, &impl->forceStopFlag);
#endif
    if(impl->callback)
    {
        int flag = DETAIL_INFO_STREAM_HANDSHAKE_END;
        //logd("RPT: STREAM_EVT_DETAIL_INFO, cb:%08lx, flag:%d", impl->callback, flag);
        impl->callback(impl->pUserData, STREAM_EVT_DETAIL_INFO, &flag);
    }

    if(ret < 0)
    {
        CDX_LOGE("ssl connect failed.");
        goto err_out2;
    }
    return 0;

err_out2:
    CdxstreamDeinitTls(impl);
err_out1:
    CdxstreamFreeTls(impl);

    shutdown(impl->sockFd, SHUT_RDWR);
    lwip_close(impl->sockFd);
err_out0:
#if (!CDX_IOT_DNS_CACHE)
    lwip_freeaddrinfo(ai_backup);
    ai_backup = NULL;
#endif
    return -1;
}

static cdx_int32 __CdxSSLStreamConnect(CdxStreamT *stream)
{
    CdxSSLStreamImplT *impl;
    cdx_int32 result;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxSSLStreamImplT, base);

    pthread_mutex_lock(&impl->stateLock);
    if(impl->forceStopFlag)
    {
        pthread_mutex_unlock(&impl->stateLock);
        return -1;
    }
    CdxAtomicSet(&impl->state, SSL_STREAM_CONNECTING);
    CdxAtomicInc(&impl->ref);
    pthread_mutex_unlock(&impl->stateLock);

    result = StartSSLStreamConnect(stream);
    if (result < 0)
    {
        CDX_LOGE("StartSSLStreamConnect failed!");
        pthread_mutex_lock(&impl->stateLock);
        impl->ioState = CDX_IO_STATE_ERROR;
        pthread_mutex_unlock(&impl->stateLock);
    }
    else
    {
        pthread_mutex_lock(&impl->stateLock);
        impl->ioState = CDX_IO_STATE_OK;
        pthread_mutex_unlock(&impl->stateLock);
    }

    pthread_mutex_lock(&impl->stateLock);
    CdxAtomicSet(&impl->state, SSL_STREAM_IDLE);
    CdxSSLStreamDecRef(&impl->base);
    pthread_mutex_unlock(&impl->stateLock);
    return (impl->ioState == CDX_IO_STATE_ERROR) ? -1 : 0;

}

static const struct CdxStreamOpsS CdxSSLStreamOps = {
    .connect    = __CdxSSLStreamConnect,
    .read       = __CdxSSLStreamRead,
    .close      = __CdxSSLStreamClose,
    .getIOState = __CdxSSLStreamGetIOState,
//    .forceStop  = __CdxTcpStreamForceStop,
    .write      = __CdxSSLStreamWrite,
    .control    = __CdxSSLStreamControl
};

static CdxStreamT *__CdxSSLStreamCreate(CdxDataSourceT *source)
{
    CdxSSLStreamImplT *impl = NULL;

    impl = (CdxSSLStreamImplT *)malloc(sizeof(CdxSSLStreamImplT));
    if(NULL == impl)
    {
        CDX_LOGE("malloc failed");
        return NULL;
    }
    memset(impl, 0x00, sizeof(CdxSSLStreamImplT));
    impl->base.ops = &CdxSSLStreamOps;
    impl->ioState = CDX_IO_STATE_INVALID;
    impl->port = *(cdx_int32 *)((CdxHttpSendBufferT *)source->extraData)->size;
    impl->hostname = (char *)((CdxHttpSendBufferT *)source->extraData)->buf;

    CdxAtomicSet(&impl->ref, 1);

    if (source->certificate)
    {
        impl->certificate = strdup(source->certificate);
        if (NULL == impl->certificate)
        {
            CDX_LOGE("malloc failed");
            goto err;
        }
    }

    impl->dnsMutex = (pthread_mutex_t*)calloc(1,sizeof(pthread_mutex_t));
    if (impl->dnsMutex == NULL)
    {
        CDX_LOGE("malloc failed");
        goto err;
    }

    pthread_mutex_init(&impl->stateLock, NULL);
    pthread_mutex_init(impl->dnsMutex, NULL);
    pthread_cond_init(&impl->dnsCond, NULL);
    impl->dnsRet = -1;

    return &impl->base;
err:
    if (impl->dnsMutex)
    {
        free(impl->dnsMutex);
    }
    if (impl->certificate)
    {
        free(impl->certificate);
    }
    if (impl)
    {
        free(impl);
    }
    return NULL;
}

CdxStreamCreatorT sslStreamCtor = {
    .create = __CdxSSLStreamCreate
};
