/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxHttpStream.c
 * Description : Http stream implementation.
 * History :
 *
 */

//#define CONFIG_LOG_LEVEL 3
#define LOG_TAG "httpStream"

#include "cdx_config.h"
#include <stdio.h>
#include <CdxStream.h>
#include <CdxHttpStream.h>
#include <CdxMemory.h>
#include <CdxTypes.h>
#include <sys/time.h>
#include <stdint.h>
#include <CdxTime.h>
#include <debug.h>
#if CDX_BUF_STAT
#include "CdxBufStat.h"
#endif

#define GetNowUs() CdxGetNowUs()

#define IGNORE_DATA_SEEK_THRESHOLD (3*1024)
//#define PROBE_DATA_LEN_DEFAULT (128*1024)
#define PROBE_DATA_LEN_DEFAULT (2*1024)
//#define MAX_STREAM_BUF_SIZE (10*1024*1024)
//#define PROTECT_AREA_SIZE (512*1024)  //should not too big
//#define PROTECT_AREA_SIZE (4*1024)
#define PROTECT_AREA_SIZE (16*1024)
//#define TEMP_HTTP_DATA_BUF (PROBE_DATA_LEN_DEFAULT + 4096)
#define RE_CONNECT_TIME (3600) //* unit: second

#define HTTP_STREAM_BUFFER_SET_ENABLE
#ifdef HTTP_STREAM_BUFFER_SET_ENABLE
extern int g_max_stream_buf_size;
extern int g_cdx_http_threshold;

//#define CDX_HTTP_THRESHOLD g_cdx_http_threshold//(4*1024)
#define CDX_HTTP_THRESHOLD (2*1024)
//#define MAX_STREAM_BUF_SIZE g_max_stream_buf_size//(10*1024)
#define MAX_STREAM_BUF_SIZE (384*1024)//5*1024*1024
#else
#define CDX_HTTP_THRESHOLD (4*1024)
#define MAX_STREAM_BUF_SIZE (10*1024)
#endif

/*
 * CDX_HTTP_READ_SIZE <= MAX_STREAM_BUF_SIZE - PROTECT_AREA_SIZE - PROBE_DATA_LEN_DEFAULT
 * CDX_HTTP_READ_SIZE <= MAX_STREAM_BUF_SIZE - PROTECT_AREA_SIZE - CDX_HTTP_THRESHOLD
 */
//#define CDX_HTTP_READ_SIZE (1*1024)
#define CDX_HTTP_READ_SIZE (2*1024)//32

#define HTTP_FIELD_STR_LEN  (1024)

#define TMALLPLAYER_DEFAULT_PRIORITY 48   //4
#define TMALLPLAYER_NORMAL_STACKSIZE 32768 //32768

#if __SAVE_BITSTREAMS
static int streamIndx = 0;
#endif

#ifndef LOG_TAG
#define LOG_TAG "CdxHttpStream"       //* prefix of the printed messages.
#endif

static int CallbackProcess(void* pUserData, int eMessageId, void* param)
{
    CdxHttpStreamImplT *impl = (CdxHttpStreamImplT *)pUserData;

    switch(eMessageId)
    {
        case STREAM_EVT_DOWNLOAD_DOWNLOAD_ERROR:
        {
            if(param != NULL)
            {
                impl->callback(impl->pUserData, STREAM_EVT_DOWNLOAD_DOWNLOAD_ERROR, param);
            }
            break;
        }
        case STREAM_EVT_DETAIL_INFO:
        {
            //int flag = *(int *)param;
            //loge("EVT: STREAM_EVT_DETAIL_INFO, RPT: STREAM_EVT_DETAIL_INFO, cb:%08lx, flag:%d", impl->callback, flag);
            impl->callback(impl->pUserData, STREAM_EVT_DETAIL_INFO, param);
            break;
        }
        default:
            logw("ignore demux callback message, eMessageId = 0x%x.", eMessageId);
            return -1;
    }
    return 0;
}

static cdx_int32 CdxHttpSendRequest(CdxHttpStreamImplT *impl, cdx_int64 pos)
{
    cdx_int32 ret = -1;
    CdxHttpHeaderT *httpHdr;
    CdxUrlT *serverUrl = impl->url;
    cdx_int32 setRangeFlag = 0;
    CdxDataSourceT tcpSource;
    CdxHttpSendBufferT sendBuf;
    cdx_int32 i;

    cdx_char *str = (cdx_char *)malloc(HTTP_FIELD_STR_LEN);
    if (str == NULL)
    {
        CDX_LOGE("str is NULL.");
        return -1;
    }
    memset(str, 0, HTTP_FIELD_STR_LEN);

    httpHdr = CdxHttpNewHeader();
    if(httpHdr == NULL)
    {
        CDX_LOGE("httpHdr is NULL.");
        free(str);
        return -1;
    }

    CdxHttpSetUri(httpHdr, serverUrl->file);

    if (serverUrl->port && (serverUrl->port != 80))
    {
        snprintf(str, HTTP_FIELD_STR_LEN, "Host: %s:%d", serverUrl->hostname, serverUrl->port);
    }
    else
    {
        snprintf(str, HTTP_FIELD_STR_LEN, "Host: %s", serverUrl->hostname);
    }
    CdxHttpSetField(httpHdr, str);//"Host: hostname" field

    //set extended header field
    if (impl->pHttpHeader)
    {
        for (i=0; i < impl->nHttpHeaderSize; i++)
        {
            if (strcasecmp("User-Agent", impl->pHttpHeader[i].key) == 0)//UA max length?
            {
                continue;
            }
            if(strcasecmp("Range", impl->pHttpHeader[i].key) == 0)//Range: bytes=%lld-%lld
            {
                setRangeFlag = 1;
            }
            snprintf(str, HTTP_FIELD_STR_LEN, "%s: %s",impl->pHttpHeader[i].key,
                                                impl->pHttpHeader[i].val);
            CdxHttpSetField(httpHdr, str);
            /*CDX_LOGV("xxx http header key: %s, val:%s",
            impl->pHttpHeader[i].key,impl->pHttpHeader[i].val);*/
        }
    }
    snprintf(str, HTTP_FIELD_STR_LEN, "User-Agent: %s",impl->ua);//set User-Agent
    CdxHttpSetField(httpHdr, str);

    if(setRangeFlag == 0)//user not set Range
    {
        if (pos >= 0)
            /*pos=0, whence=SEEK_SET, for detecting if a server supports seeking by
            analysing the reply headers.*/
        {
            snprintf(str, HTTP_FIELD_STR_LEN, "Range: bytes=%d-", (int)pos);
            //"Range: bytes=pos-" field. PRId64=lld
            CdxHttpSetField(httpHdr, str);
        }
    }

    //referer pass from app...
    //snprintf(str, sizeof(str), "Referer: %s://%s", serverUrl->protocol, serverUrl->hostname);
    //CdxHttpSetField(httpHdr, str);
    /*if no referer header, for example(http://flv.cntv.wscdns.com/live/flv/channel15.flv),
    response 403.*/
#ifndef HTTP_KEEP_ALIVE
    CdxHttpSetField(httpHdr, "Connection: close");
#else
    CdxHttpSetField(httpHdr, "Connection: Keep-Alive");
#endif

    if (impl->isAuth == 1)
    {
        CdxHttpAddBasicAuthentication(httpHdr, impl->url->username, impl->url->password);
        impl->isAuth = 0;
    }

    httpHdr->httpMinorVersion = 1;
    if(CdxHttpBuildRequest(httpHdr) == NULL) //store in http_hdr->buffer
    {
        CDX_LOGE("CdxHttpBuildRequest");
        goto err_out;
    }

    if(!strcasecmp(serverUrl->protocol, "https"))
    {
        if(serverUrl->port == 0)
        {
            serverUrl->port = 443;
        }
    }
    if (serverUrl->port == 0)
    {
        serverUrl->port = 80;  // Default port for the web server
    }

    //build "tcp://host:port"
    sendBuf.buf = (void *)serverUrl->hostname;
    sendBuf.size = (void *)&serverUrl->port;
    tcpSource.extraData = (void *)&sendBuf;
    if(!strcasecmp(serverUrl->protocol, "http"))
    {
        snprintf(str, HTTP_FIELD_STR_LEN, "tcp://%s:%d", serverUrl->hostname,serverUrl->port);
    }
    else if(!strcasecmp(serverUrl->protocol, "https"))
    {
        snprintf(str, HTTP_FIELD_STR_LEN, "ssl://%s:%d", serverUrl->hostname,serverUrl->port);
    }

    tcpSource.uri = str;
    tcpSource.certificate = impl->certificate;

#if CDX_IOT_CMCC_LOG
    cdx_int64 startConnect, endConnect, connectTimeMs;
    startConnect = GetNowUs();
#endif /* CDX_IOT_CMCC_LOG */

#if CDX_BUF_STAT
    CdxBufStatSetStartStamp(&http_stat.http_req_tcp_open_time);
#endif

    if(impl->callback)
    {
        //* Add Callback for CdxTcpStream
        struct CallBack cb;
        cb.callback = CallbackProcess;
        cb.pUserData = (void *)impl;
        ContorlTask streamContorlTask;
        streamContorlTask.cmd = STREAM_CMD_SET_CALLBACK;
        streamContorlTask.param = (void *)&cb;
        streamContorlTask.next = NULL;

        ret = CdxStreamOpen(&tcpSource, &impl->lock, &impl->forceStopFlag, &impl->tcpStream,
            &streamContorlTask);//__CdxTcpStreamOpen
    }
    else
    {
        ret = CdxStreamOpen(&tcpSource, &impl->lock, &impl->forceStopFlag, &impl->tcpStream, NULL);
        //__CdxTcpStreamOpen
    }

    if (impl->certificate)
    {
        Pfree(impl->pool, impl->certificate);
        impl->certificate = NULL;
    }

#if CDX_BUF_STAT
    CdxBufStatSetEndStamp(&http_stat.http_req_tcp_open_time);
    CdxBufStatIncProcTime(&http_stat.http_req_tcp_open_time);
#endif

    if(ret < 0)
    {
        CDX_LOGE("CdxStreamOpen failed. '%s'", tcpSource.uri);
        if(ret == -2)
        {
            CDX_LOGE("network disconnect! ");
            int flag = 1;
            if(impl->callback)
            {
                impl->callback(impl->pUserData, STREAM_EVT_NET_DISCONNECT, &flag);
            }
        }
        goto err_out;
    }

    int flag = 0;

#if CDX_IOT_CMCC_LOG
    endConnect = GetNowUs();
    connectTimeMs = (endConnect - startConnect) / 1000;
#endif /* CDX_IOT_CMCC_LOG */

    if(impl->callback)
    {
#if CDX_BUF_STAT
        CdxBufStatSetStartStamp(&http_stat.http_req_callback_time);
#endif
        impl->callback(impl->pUserData, STREAM_EVT_NET_DISCONNECT, &flag);
#if CDX_BUF_STAT
        CdxBufStatSetEndStamp(&http_stat.http_req_callback_time);
        CdxBufStatIncProcTime(&http_stat.http_req_callback_time);
#endif

#if CDX_IOT_CMCC_LOG
        //*cmcc 2.1.7.12-m3
        char cmccLog[4096] = "";
        sprintf(cmccLog, "[info][%s %s %d]Connect server OK! spend time: %lldms",
            LOG_TAG, __FUNCTION__, __LINE__, connectTimeMs);
        impl->callback(impl->pUserData, STREAM_EVT_CMCC_LOG_RECORD, (void*)cmccLog);
#endif /* CDX_IOT_CMCC_LOG */
    }

#if CDX_BUF_STAT
    CdxBufStatSetStartStamp(&http_stat.http_req_tcp_write_time);
#endif
    ret = CdxStreamWrite(impl->tcpStream, httpHdr->buffer, httpHdr->bufferSize);
#if CDX_BUF_STAT
    CdxBufStatSetEndStamp(&http_stat.http_req_tcp_write_time);
    CdxBufStatIncProcTime(&http_stat.http_req_tcp_write_time);
#endif
    if (ret < 0)
    {
        CDX_LOGE("send error.");
        goto err_out;
    }

    free(str);
    CdxHttpFree(httpHdr);
    return 0;

err_out:

    free(str);
    CdxHttpFree(httpHdr);
    return -1;
}
#define HTTP_RSP_HEADER_READ_SIZE 512
#define HTTP_RSP_HEADER_MALLOC_SIZE 1024
static CdxHttpHeaderT *CdxHttpReadResponse(CdxHttpStreamImplT *impl)
{
    CdxHttpHeaderT *httpHdr;
    CdxStreamT *tcpStream;
    int i = 0;
    int bufTmpSize = 0;
    char *buf = NULL;

    tcpStream = impl->tcpStream;

    httpHdr = CdxHttpNewHeader();

    if (httpHdr == NULL )
    {
        CDX_LOGE("CdxHttpNewHeader fail.");
        return NULL;
    }
    cdx_int64 start, end;
    start = GetNowUs();
    while(1)
    {
        if(impl->forceStopFlag == 1)
        {
            CDX_LOGW("force stop CdxHttpReadResponse.");
            goto err_out;
        }

        if((int)httpHdr->bufferSize == bufTmpSize)
        {
            if(bufTmpSize >= (1 << 16))
            {
                CDX_LOGE("size too big...");
                goto err_out;
            }
            buf = realloc(httpHdr->buffer, bufTmpSize+1024+1); //* attention, end with '\0'
            if(!buf)
            {
                CDX_LOGE("realloc failed.");
                goto err_out;
            }
            httpHdr->buffer = buf;
            bufTmpSize += 1024;
        }
        unsigned char *pch = httpHdr->buffer+httpHdr->bufferSize;
        i = CdxStreamRead(tcpStream, pch, 1);
        if(i != 1)
        {
            CDX_LOGE("read failed.");
            goto err_out;
        }
        httpHdr->bufferSize++;
        httpHdr->buffer[httpHdr->bufferSize] = 0;
        if( ((*pch)=='\n') && (CdxHttpIsHeaderEntire(httpHdr) > 0 )){
            break;
        }
    }

#if CONFIG_ALI_YUNOS
    impl->downloadFirstTime = GetNowUs();//Ali YUNOS invoke info
    if(impl->callback)
    {
        impl->callback(impl->pUserData, STREAM_EVT_DOWNLOAD_FIRST_TIME, &(impl->downloadFirstTime));
    }
#endif

    end = GetNowUs();
    //CDX_LOGV("xxx get response header cost time: %lld", end-start);
    if (CdxHttpResponseParse(httpHdr) < 0)
    {
        CdxHttpFree(httpHdr);
        return NULL;
    }

#if CONFIG_ALI_YUNOS
    //Ali YUNOS invoke info http respond header
    if(httpHdr->posHdrSep > 0)
    {
        char *tmpBuf = malloc(httpHdr->posHdrSep);
        memset(tmpBuf, 0x00, httpHdr->posHdrSep);
        memcpy(tmpBuf, httpHdr->buffer, httpHdr->posHdrSep);

        if(impl->callback)
            impl->callback(impl->pUserData, STREAM_EVT_DOWNLOAD_RESPONSE_HEADER, tmpBuf);
        free(tmpBuf);
    }
#endif

    return httpHdr;

err_out:
    CdxHttpFree(httpHdr);
    return NULL;
}

static void ClearHttpExtraDataContainer(CdxHttpStreamImplT *impl)
{
    if(impl->hfsContainer.extraData)
    {
        if(impl->hfsContainer.extraDataType == EXTRA_DATA_HTTP_HEADER)
        {
            CdxHttpHeaderFieldsT *hdr = (CdxHttpHeaderFieldsT *)(impl->hfsContainer.extraData);
            if(hdr->pHttpHeader)
            {
                int i;
                for(i = 0; i < hdr->num; i++)
                {
                    free((void*)(hdr->pHttpHeader + i)->key);
                    free((void*)(hdr->pHttpHeader + i)->val);
                }
                free(hdr->pHttpHeader);
            }
        }
        free(impl->hfsContainer.extraData);
        impl->hfsContainer.extraData = NULL;
    }
    impl->hfsContainer.extraDataType = EXTRA_DATA_UNKNOWN;
    return ;
}

int AnalyseHttpHeader(CdxHttpStreamImplT *impl, int *haveCookie)
{
    int ret = 0, i;
    *haveCookie = 0;
    if(impl->pHttpHeader)
    {
        for(i = 0; i < impl->nHttpHeaderSize; i++)
        {
            if(strcasecmp("Cookie", impl->pHttpHeader[i].key) == 0)
            {
                ret = 1;
                *haveCookie = 1;
                break;
            }
        }
    }
    return ret;
}

void MakeExtraDataContainer(CdxHttpStreamImplT *impl, CdxHttpHeaderT* httpHdr)
{
    int haveCookie = 0, num = 0, i = 0, j = 0, flag = 0;
    ClearHttpExtraDataContainer(impl);
    AnalyseHttpHeader(impl, &haveCookie);

    if(!haveCookie && httpHdr->cookies)
    {
        num = 1;
    }
    if(num + impl->nHttpHeaderSize > 0)
    {
        CdxHttpHeaderFieldsT *extraData =
            (CdxHttpHeaderFieldsT *)malloc(sizeof(CdxHttpHeaderFieldsT));
        CdxHttpHeaderFieldT *pHttpHeader = (CdxHttpHeaderFieldT *)malloc(
            (num + impl->nHttpHeaderSize) * sizeof(CdxHttpHeaderFieldT));
        if(!extraData || !pHttpHeader)
        {
            CDX_LOGE("malloc fail.");
            if (extraData)
                free(extraData);
            if (pHttpHeader)
                free(pHttpHeader);
            return;
        }

        for(i = 0; i < impl->nHttpHeaderSize; i++)
        {
            if(strcasecmp("Cookie", impl->pHttpHeader[i].key) == 0)
            {
                if(!httpHdr->cookies || strstr(impl->pHttpHeader[i].val, httpHdr->cookies))
                {
                    (pHttpHeader + j)->key = strdup(impl->pHttpHeader[i].key);
                    (pHttpHeader + j)->val = strdup(impl->pHttpHeader[i].val);
                }
                else
                {
                    (pHttpHeader + j)->key = strdup(impl->pHttpHeader[i].key);
                    (pHttpHeader + j)->val = strdup(httpHdr->cookies);
                }
                flag = 1;
                j++;
            }
            else
            {
                (pHttpHeader + j)->key = strdup(impl->pHttpHeader[i].key);
                (pHttpHeader + j)->val = strdup(impl->pHttpHeader[i].val);
                j++;
            }
        }

        if(flag == 0 && httpHdr->cookies)
        {
            (pHttpHeader + j)->key = strdup("Cookie");
            (pHttpHeader + j)->val = strdup(httpHdr->cookies);
            j++;
        }

        extraData->num = impl->nHttpHeaderSize + num;
        extraData->pHttpHeader = pHttpHeader;
        impl->hfsContainer.extraDataType = EXTRA_DATA_HTTP_HEADER;
        impl->hfsContainer.extraData = extraData;
    }

    return ;
}

static cdx_int32 ReSetHeaderFields(CdxHttpHeaderFieldsT *pHdrs, CdxHttpStreamImplT *impl)
{
    int i;

    if(pHdrs == NULL || impl == NULL)
    {
        loge("check para");
        return -1;
    }

    if(impl->pHttpHeader)
    {
        for(i = 0; i < impl->nHttpHeaderSize; i++)
        {
            if(impl->pHttpHeader[i].key)
            {
                Pfree(impl->pool, (void *)impl->pHttpHeader[i].key);
            }
            if(impl->pHttpHeader[i].val)
            {
                Pfree(impl->pool, (void *)impl->pHttpHeader[i].val);
            }
        }
        Pfree(impl->pool, impl->pHttpHeader);
        impl->pHttpHeader = NULL;
    }

    impl->nHttpHeaderSize = pHdrs->num;
    impl->pHttpHeader = (CdxHttpHeaderFieldT *)Palloc(impl->pool, pHdrs->num *
        sizeof(CdxHttpHeaderFieldT));
    if(impl->pHttpHeader == NULL)
    {
        loge("malloc failed.");
        return -1;
    }

    for(i = 0; i < impl->nHttpHeaderSize; i++)
    {
        (impl->pHttpHeader + i)->key = (const char *)Pstrdup(impl->pool,
            (pHdrs->pHttpHeader + i)->key);
        (impl->pHttpHeader + i)->val = (const char *)Pstrdup(impl->pool,
            (pHdrs->pHttpHeader + i)->val);

        CDX_LOGV("extraDataContainer %s %s", (impl->pHttpHeader + i)->key,
            (impl->pHttpHeader + i)->val);
    }

    return 0;
}

static cdx_int32 CdxHttpStreamingStart(CdxHttpStreamImplT *impl, cdx_int64 offset)
{
    int redirect = 0;
    const char *acceptRanges;
    int seekable = 1;
    int res = -1;
    int ret = -1;
    int authRetry = 0;
    char* contentLength;
    char* transferEncoding;
    char* contentRange;
    char* contentEncoding;
    char* nextUrl = NULL;
    CdxUrlT* url = NULL;
    CdxHttpHeaderT* httpHdr = NULL;

    url = impl->url;

    do
    {
#if CDX_BUF_STAT
        CdxBufStatIncCount(&http_stat.http_reconn_do_cnt);
#endif
        if (httpHdr)
        {
            CdxHttpFree(httpHdr);
            httpHdr = NULL;
        }

        if(impl->forceStopFlag)
        {
            CDX_LOGD("forcestop");
            goto err_out;
        }
#if CDX_BUF_STAT
        CdxBufStatSetStartStamp(&http_stat.http_reconn_tcpfree_time);
#endif
#if CDX_BUF_STAT
        CdxBufStatSetStartStamp(&http_stat.http_reconn_getlock_time);
#endif
        pthread_mutex_lock(&impl->lock);
#if CDX_BUF_STAT
        CdxBufStatSetEndStamp(&http_stat.http_reconn_getlock_time);
        CdxBufStatIncProcTime(&http_stat.http_reconn_getlock_time);
#endif

        if(impl->tcpStream)
        {
            CdxStreamClose(impl->tcpStream);
            impl->tcpStream = NULL;
        }
        pthread_mutex_unlock(&impl->lock);
#if CDX_BUF_STAT
        CdxBufStatSetEndStamp(&http_stat.http_reconn_tcpfree_time);
        CdxBufStatIncProcTime(&http_stat.http_reconn_tcpfree_time);
#endif
        if (redirect == 1)
            redirect = 0;

        cdx_int64 t0=CdxGetNowUs();
        (void)t0;
#if CDX_BUF_STAT
        CdxBufStatSetStartStamp(&http_stat.http_reconn_sendreq_time);
#endif
        res = CdxHttpSendRequest(impl, offset/*impl->baseOffset*/);
#if CDX_BUF_STAT
        CdxBufStatSetEndStamp(&http_stat.http_reconn_sendreq_time);
        CdxBufStatIncProcTime(&http_stat.http_reconn_sendreq_time);
#endif
        if(res < 0)
        {
            CDX_LOGE("xxx CdxHttpSendRequest failed.");
            impl->ioState = CDX_IO_STATE_ERROR;
            return -1;
        }
#if CDX_BUF_STAT
        CdxBufStatSetStartStamp(&http_stat.http_reconn_recvrsp_time);
#endif
        httpHdr = CdxHttpReadResponse(impl);//--hdr->body has data.
#if CDX_BUF_STAT
        CdxBufStatSetEndStamp(&http_stat.http_reconn_recvrsp_time);
        CdxBufStatIncProcTime(&http_stat.http_reconn_recvrsp_time);
#endif
        if (httpHdr == NULL)
        {
            CDX_LOGE("Read http response failed.");
            impl->ioState = CDX_IO_STATE_ERROR;
            return -1;
        }
        MakeExtraDataContainer(impl, httpHdr);
        logv("lbh request+read response: %d ms", (uint32_t)(CdxGetNowUs()-t0)/1000);

        if(httpHdr->httpMinorVersion == 0)
            //http/1.0 not support range, but some http/1.0 server may not really http/1.0...
        {
            //seekable = 0;
            CDX_LOGD("Http server version: HTTP/1.%u", httpHdr->httpMinorVersion);
        }

#if CDX_IOT_CMCC_LOG
        if(impl->callback)
        {
            char cmccLog[4096] = "";
            sprintf(cmccLog, "[info][%s %s %d]http status code: %d",
                LOG_TAG, __FUNCTION__, __LINE__, httpHdr->statusCode);
            impl->callback(impl->pUserData, STREAM_EVT_CMCC_LOG_RECORD, (void*)cmccLog);
        }
#endif /* CDX_IOT_CMCC_LOG */

        CDX_LOGD("statusCode = %d", httpHdr->statusCode);
        switch(httpHdr->statusCode)
        {
            case 200:
            case 201:
            case 202:
            case 203:
            case 204:
            case 205:
                //seekable init to 1 or 0 is up to if we compatible the servers
                //that not telling accepted range but support range or not support
                //range.
#if CONFIG_HTTP_STREAM_IGNORE_DATA_SEEK
                seekable = 0; //has Range field && response code==200.
#else
                seekable = 1; //has Range field && response code==200.
#endif
            case 206:
            {
#if CDX_BUF_STAT
                CdxBufStatSetStartStamp(&http_stat.http_reconn_proc206_time);
#endif
                contentLength = CdxHttpGetField(httpHdr, "Content-Length");
                if (contentLength)
                {
                    CDX_LOGI("contentLength = %s",contentLength);
                    if (impl->totalSize == 0 || impl->totalSize == -1)
                        impl->totalSize = atoll(contentLength);
                    impl->chunkedFlag = 0;
                }
                else
                {
                    if((contentRange = CdxHttpGetField(httpHdr, "Content-Range")))
                    {
                        char *p = strchr(contentRange,'/');
                        if(p != NULL)
                        {
                            impl->totalSize = atoll(p + 1);
                            CDX_LOGI("Content-Range: %s, totalSize(%d)",
                                contentRange, (cdx_uint32)impl->totalSize);
                        }
                        else
                        {
                            CDX_LOGV("wrong Content-Range str->[%s]", p);
                            impl->totalSize = -1;
                        }
                    }
                    else
                    {
                        impl->totalSize = -1;
                    }

                    if ((transferEncoding = CdxHttpGetField(httpHdr, "Transfer-Encoding")))
                    {
                        CDX_LOGI("transferEncoding = %s", transferEncoding);
                        impl->chunkedFlag = 1;
                        seekable = 0;
                    }
                    else
                    {
                        impl->chunkedFlag = 0;
                    }
                }

                //check if we can make partial content requests and thus seek in http-streams
                if(httpHdr->statusCode >= 200 && httpHdr->statusCode <= 206)
                {
                    acceptRanges = CdxHttpGetField(httpHdr, "Accept-Ranges");
                    if (acceptRanges)
                    {
                        seekable = strncmp(acceptRanges,"none",4) == 0 ? 0 : 1;
                        CDX_LOGI("xxx Accept-Ranges: bytes, seekable=%d", seekable);
                    }
                }

                contentEncoding = CdxHttpGetField(httpHdr, "Content-Encoding");
                if(contentEncoding)
                {
                    if(!strcasecmp(contentEncoding, "gzip") ||
                        !strcasecmp(contentEncoding, "deflate"))
                    {
#if __CONFIG_ZLIB
                        impl->compressed = 1;
                        if(inflateInit2(&impl->inflateStream, 32+15) != Z_OK)
                        {
                            loge("inflateInit2 failed.");
                            goto err_out;
                        }
                        if(zlibCompileFlags() & (1 << 17))
                        {
                            loge("not support, check.");
                            goto err_out;
                        }
                        impl->inflateBuffer = NULL;
#else
                        logw("(%s) need zlib support.",contentEncoding);
#if CDX_BUF_STAT
                        CdxBufStatSetEndStamp(&http_stat.http_reconn_proc206_time);
                        CdxBufStatIncProcTime(&http_stat.http_reconn_proc206_time);
#endif

                        goto err_out;
#endif
                    }
                }

                char *conn = CdxHttpGetField(httpHdr, "Connection");
                if (conn != NULL)
                {
                    if (strcasecmp(conn, "Keep-Alive") == 0)
                    {
                        logd("http keep alive");
                        impl->keepAlive = 1;
                    }
                }
#if CDX_BUF_STAT
                CdxBufStatSetEndStamp(&http_stat.http_reconn_proc206_time);
                CdxBufStatIncProcTime(&http_stat.http_reconn_proc206_time);
#endif
                logv("============ (re)connect ok, url:(%s), offset:(%lld)", url->url, offset);
                goto out;
            }
            // Redirect
            case 301: // Permanently
            case 302: // Temporarily
            case 303: // See Other
            case 307: // Temporarily (since HTTP/1.1)
            {
                //RFC 2616, recommand to detect infinite redirection loops
                nextUrl = CdxHttpGetField(httpHdr, "Location");
                CDX_LOGV("xxx nextUrl:(%s)", nextUrl);

#if CDX_IOT_CMCC_LOG
                if(impl->callback)
                {
                    char cmccLog[4096] = "";
                    sprintf(cmccLog, "[info][%s %s %d]Redirect url: %s",
                        LOG_TAG, __FUNCTION__, __LINE__, nextUrl);
                    impl->callback(impl->pUserData, STREAM_EVT_CMCC_LOG_RECORD, (void*)cmccLog);
                }
#endif /* CDX_IOT_CMCC_LOG */

                if(nextUrl != NULL)
                {
                    nextUrl = RmSpace(nextUrl);
                    //CDX_LOGV("xxx nextUrl:(%s)", nextUrl);
                    impl->url = CdxUrlRedirect(&url, nextUrl);
                    if(strcasecmp(url->protocol, "http") && strcasecmp(url->protocol, "https"))
                    {
                        CDX_LOGE("Unsupported http %d redirect to %s protocol.",
                                      httpHdr->statusCode, url->protocol);
                        goto err_out;
                    }
                    if(impl->sourceUri)
                    {
                        Pfree(impl->pool, impl->sourceUri);
                        impl->sourceUri = NULL;
                    }
                    impl->sourceUri = (cdx_char *)Palloc(impl->pool, strlen(url->url)+1);
                    CDX_CHECK(impl->sourceUri);
                    memset(impl->sourceUri, 0x00, strlen(url->url)+1);
                    memcpy(impl->sourceUri, url->url, strlen(url->url));  //for ParserTypeGuess
                    redirect = 1;//resend request.
#if CDX_BUF_STAT
                    CdxBufStatIncCount(&http_stat.http_reconn_307_cnt);
#endif
                }
                else
                {
                    CDX_LOGW("No redirect uri?");
                    goto err_out;
                }

                //*
                if(httpHdr->cookies)
                {
                    ReSetHeaderFields(impl->hfsContainer.extraData, impl);
                }
                break;
            }
            case 401: // Authentication required
            {

#if defined(CONF_YUNOS)
                if(impl->callback)
                {
                    impl->mYunOSstatusCode = 3401; //Ali YUNOS invoke info
                    impl->callback(impl->pUserData, STREAM_EVT_DOWNLOAD_DOWNLOAD_ERROR,
                        &(impl->mYunOSstatusCode));
                }
#endif
                if(CdxHttpAuthenticate(httpHdr, url, &authRetry)<0)
                {
                    CDX_LOGE("CdxHttpAuthenticate < 0.");
                    goto err_out;
                }
                redirect = 1;
#if CDX_BUF_STAT
                CdxBufStatIncCount(&http_stat.http_reconn_401_cnt);
#endif
                impl->isAuth = 1;
                break;
            }
            case 404:
            case 410:
            case 500:
            {
                CDX_LOGE("something error happened,statusCode(%d)", httpHdr->statusCode);
                if(impl->callback)
                {
                    impl->callback(impl->pUserData, STREAM_EVT_DOWNLOAD_ERROR,
                        &httpHdr->statusCode);

#if defined(CONF_YUNOS)
                    impl->mYunOSstatusCode = 3500;//Ali YUNOS invoke info
                    impl->callback(impl->pUserData, STREAM_EVT_DOWNLOAD_DOWNLOAD_ERROR,
                        &(impl->mYunOSstatusCode));
#endif
                }
                goto err_out;
            }
            default:
            {
                CDX_LOGE("shoud not be here. statusCode(%d)", httpHdr->statusCode);
                if(impl->callback)
                {
                    impl->callback(impl->pUserData, STREAM_EVT_DOWNLOAD_ERROR,
                        &httpHdr->statusCode);
                }
                impl->ioState = CDX_IO_STATE_ERROR;
                ret = -2;
                goto err_out;
            }
        }
    }while(redirect);

err_out:
    impl->ioState = CDX_IO_STATE_ERROR;
    CdxHttpFree(httpHdr);
    return ret;

out:
    impl->seekAble = seekable;
    CdxHttpFree(httpHdr);

    if(impl->ioState != CDX_IO_STATE_EOS)
    {
        impl->ioState = CDX_IO_STATE_OK;
    }

    return 0;
}
size_t Hex2Oct(char* src)
{
    return strtol(src, NULL, 16);
}
cdx_int32 CopyChunkSize(cdx_char *srcBuf, cdx_int32 *pNum)//pNum: length of "len" in "len\r\n".
{
    cdx_char  result[10] = {0};
    cdx_int32 pos = 0;
    cdx_char *tmpSrcBuf = srcBuf;

    while (1)
    {
        cdx_char byte;

        byte = *tmpSrcBuf++;

        if((byte >= '0' && byte <= '9')
            || (byte >= 'a' && byte <= 'f')
            || (byte >= 'A' && byte <= 'F'))
        {
            result[pos++] = byte;
            *pNum = pos;
            continue;
        }
        else if(byte == '\r')
        {
            byte = *tmpSrcBuf++;
            if(byte != '\n')
            {
                CDX_LOGW("No lf after len flag.");
                return -1;
            }
            break;
        }
        else
        {
            CDX_LOGW("check the content.");
            return -2;
        }
    }
    return strtol(result, NULL, 16);
}

//Transfer-Encoding: chunked
//len1\r\n...\r\nlen2\r\n...\r\n......0\r\n\r\n.
//return -2: force stop
//copy data from httpDataBufferChunked to buf.
static int CdxReadChunkedData(CdxHttpStreamImplT *impl,void* buf, int len)
{
    cdx_int32   bufferLen = 0;
    cdx_int32   readLen = 0;
    cdx_int32   sum = 0;
    cdx_int32   size = 0;
    //cdx_int32   ioState = 0;
    cdx_int32   tempLen = len;
    cdx_char    *tempBuf = buf;
    cdx_char    crlfBuf[4];
    cdx_int32   needReadLen = 0;
    cdx_int32   ret;
    cdx_char    chunkedLenChar[10] = {0};
    cdx_int32   chunkedSizeInt = 0;

    while(sum < len)
    {

        if(impl->forceStopFlag == 1)
        {
            if(sum > 0)
                break;
            else
            {
                CDX_LOGW("force stop CdxReadChunkedData.");
                return -2;
            }
        }

        //*******************************************
        //*copy data from httpDataBufferChunked.
        //*******************************************
        if(impl->httpDataBufferChunked)
        {
            bufferLen = impl->httpDataSizeChunked - impl->httpDataBufferPosChunked;
            size = (tempLen < bufferLen) ? tempLen : bufferLen;
            memcpy(tempBuf, impl->httpDataBufferChunked + impl->httpDataBufferPosChunked, size);
            impl->httpDataBufferPosChunked += size;
            sum += size;
            tempLen -= size;
            tempBuf += size;
            if(impl->httpDataBufferPosChunked >= impl->httpDataSizeChunked)
            {
                //Pfree(impl->pool, impl->httpDataBufferChunked);
                free(impl->httpDataBufferChunked);
                impl->httpDataBufferChunked = NULL;
                impl->httpDataBufferPosChunked = 0;
                impl->httpDataSizeChunked = 0;
            }
        }

        //*******************************************
        //*read data to httpDataBufferChunked.
        //*******************************************
        if(sum < len)
        {
            if(impl->restChunkSize > 0)//last chunk not finished.
            {
                needReadLen = impl->restChunkSize;
                CDX_LOGV("needRead len =%d", needReadLen);
            }
            else
            {
                if(impl->dataCRLF != 0) //get rid of CRLF or LF.
                {
                    ret = CdxStreamRead(impl->tcpStream, crlfBuf, impl->dataCRLF);
                    if(ret <= 0)
                    {
                        if(ret == -2)
                        {
                            CDX_LOGW("force stop CdxReadChunkedData while get crlf.");
                            return -2;
                        }

                        CDX_LOGE("Io error.");
                        impl->ioState = CDX_IO_STATE_ERROR;
                        goto err_out;
                    }
                    else if(ret < impl->dataCRLF)
                    {
                        CDX_LOGW("force stop CdxReadChunkedData while get crlf.");
                        impl->dataCRLF -= ret;
                        return -2;
                    }
                    else
                    {
                        //CDX_LOGV("xxx crlfBuf %d %d", crlfBuf[0],crlfBuf[1]);
                        impl->dataCRLF = 0;
                    }
                }
                else if(impl->lenCRLF != 0) //just get rid of LF.
                {
                    ret = CdxStreamRead(impl->tcpStream, crlfBuf, impl->lenCRLF);
                    if(ret <= 0)
                    {
                        if(ret == -2)
                        {
                            CDX_LOGW("force stop CdxReadChunkedData while get crlf.");
                            return -2;
                        }

                        CDX_LOGE("Io error.");
                        impl->ioState = CDX_IO_STATE_ERROR;
                        goto err_out;
                    }
                    else
                    {
                        //CDX_LOGV("xxx crlfBuf %d %d", crlfBuf[0],crlfBuf[1]);
                        impl->lenCRLF = 0;
                    }
                }

                if(impl->chunkedLen) // already has chunked size
                {
                    needReadLen = impl->chunkedLen;
                    impl->chunkedLen = 0;
                }
                else
                {   /*chunked size has been force stop last time, in order to
                    continue read the chunked data this time, need to combine "len".*/
                    if(impl->tmpChunkedSize > 0)
                        /*last read chunked size been force stop,
                        combine "len" this time. should clear when not forcestop.*/
                    {
                        strcpy(chunkedLenChar, impl->tmpChunkedLen);
                        chunkedSizeInt = impl->tmpChunkedSize;
                        memset(impl->tmpChunkedLen, 0, 10);
                        impl->tmpChunkedSize = 0;
                        needReadLen = ReadChunkedSize(impl->tcpStream, impl->tmpChunkedLen,
                            &impl->tmpChunkedSize);
                        if(needReadLen >= 0)
                        {
                            if(impl->tmpChunkedSize > 0)
                            {
                                strcat(chunkedLenChar, impl->tmpChunkedLen);
                            }
                            memset(impl->tmpChunkedLen, 0, 10);
                            impl->tmpChunkedSize = 0;
                            needReadLen = strtol(chunkedLenChar, NULL, 16);
                            if(needReadLen == 0)
                            {
                                CDX_LOGD("stream end.");
                                impl->ioState = CDX_IO_STATE_EOS;
                                break;
                            }
                        }
                        else if(needReadLen < 0) // force stop again...
                        {
                            if(needReadLen == -2)
                            {
                                CDX_LOGW("force stop CdxReadChunkedData while get len.");
                                impl->tmpChunkedSize += chunkedSizeInt;
                                strcat(chunkedLenChar, impl->tmpChunkedLen);
                                strcpy(impl->tmpChunkedLen, chunkedLenChar);
                                return -2;
                            }
                            else if(needReadLen == -3) // need to skip \n in len\r\n next time.
                            {
                                impl->lenCRLF = 1;
                                strcat(chunkedLenChar, impl->tmpChunkedLen);
                                impl->chunkedLen = strtol(chunkedLenChar, NULL, 16);
                                if(impl->chunkedLen == 0)
                                {
                                    CDX_LOGD("stream end.");
                                    impl->ioState = CDX_IO_STATE_EOS;
                                    break;
                                }
                                CDX_LOGW("Forcestop, Next chunk will begin with LF, chunkedLen(%d)",
                                    impl->chunkedLen);
                                return -2;
                            }
                            CDX_LOGE("Io error.");
                            impl->ioState = CDX_IO_STATE_ERROR;
                            goto err_out;
                        }
                        else
                        {
                            impl->tmpChunkedSize = 0;
                        }
                    }
                    else
                    {
                        memset(impl->tmpChunkedLen, 0, 10);
                        impl->tmpChunkedSize = 0;
                        needReadLen = ReadChunkedSize(impl->tcpStream, impl->tmpChunkedLen,
                            &impl->tmpChunkedSize);
                        CDX_LOGV("xxxxxxx chunkSize=%d", needReadLen);
                        if(needReadLen == 0)
                        {
                            CDX_LOGD("stream end.");
                            impl->ioState = CDX_IO_STATE_EOS;
                            break;
                        }
                        else if(needReadLen < 0)
                        {
                            if(needReadLen == -2)
                                // force stop while get xxx\r, next time need to combine...
                            {
                                CDX_LOGW("force stop CdxReadChunkedData while get len.");
                                return -2;
                            }
                            else if(needReadLen == -3) // need to skip \n in len\r\n.
                            {
                                impl->lenCRLF = 1;
                                impl->chunkedLen = strtol(impl->tmpChunkedLen, NULL, 16);
                                if(impl->chunkedLen == 0)
                                {
                                    CDX_LOGD("stream end.");
                                    impl->ioState = CDX_IO_STATE_EOS;
                                    break;
                                }
                                CDX_LOGW("Forcestop, Next chunk will begin with LF, chunkedLen(%d)",
                                    impl->chunkedLen);
                                return -2;
                            }
                            CDX_LOGE("Io error.");
                            impl->ioState = CDX_IO_STATE_ERROR;
                            goto err_out;
                        }
                        else
                        {
                            impl->tmpChunkedSize = 0;
                            //CDX_LOGV("xxx chunkedsize(0x%x)", needReadLen);
                        }

                    }
                }
            }

            impl->httpDataBufferPosChunked = 0;
            impl->restChunkSize = 0;
            impl->httpDataSizeChunked = 0;
            impl->httpDataBufferChunked = malloc(needReadLen);//Palloc(impl->pool, needReadLen);
            CDX_CHECK(impl->httpDataBufferChunked);

            readLen = ReadChunkedData(impl->tcpStream, impl->httpDataBufferChunked, needReadLen);
            if(readLen <= 0)
            {
                if(readLen <= -2)
                {
                    CDX_LOGW("force stop CdxReadChunkedData.");
                    return -2;
                }
                else if(readLen == 0)
                {
                    CDX_LOGD("EOS.");
                    impl->ioState = CDX_IO_STATE_EOS;
                    return 0;
                }
                CDX_LOGE("Io error.");
                impl->ioState = CDX_IO_STATE_ERROR;
                goto err_out;
            }
            else //--if readLen < needReadLen, last read break by forcestop, set restChunkSize
            {
                impl->restChunkSize = needReadLen - readLen;
                impl->httpDataSizeChunked += readLen;
            }
        }
    }

    return sum;

err_out:
    if(impl->httpDataBufferChunked)
    {
        //Pfree(impl->pool, impl->httpDataBufferChunked);
        free(impl->httpDataBufferChunked);
        impl->httpDataBufferChunked= NULL;
    }

    return -1;
}

static cdx_int32 CdxSeekReconnect(CdxStreamT *stream, cdx_int64 offset)
{
    CdxHttpStreamImplT *impl;
    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxHttpStreamImplT, base);
#if CONFIG_HTTP_STREAM_RECONNECT_RELOAD
    int ret = 0;
#endif
#if (!CONFIG_HTTP_STREAM_RECONNECT_RELOAD)
    cdx_int32 ret;
    CdxHttpHeaderT* httpHdr = NULL;
    cdx_int32 redirect = 0;
    char* nextUrl = NULL;
    CdxUrlT* url = impl->url;
#endif

    cdx_int64 backup_offset = offset;
#if CONFIG_HTTP_STREAM_IGNORE_DATA_SEEK
    if(!impl->seekAble)
    {
        CDX_LOGI("try to ignore the data to seek! ignore: %d", (cdx_int32)offset);
        impl->ignoreDataSize = offset;
        offset = 0;
    }
#endif

#if CDX_BUF_STAT
    CdxBufStatIncInterval(&http_stat.http_reconn_invl);
#endif

    impl->bufPos = offset;
    impl->readPos = backup_offset;
    impl->bufReadPtr = impl->buffer;
    impl->bufWritePtr = impl->buffer;
    impl->protectAreaPos = backup_offset;
    impl->protectAreaSize = 0;
    impl->validDataSize = 0;
    memset(impl->buffer, 0, impl->maxBufSize);

    if(offset == impl->totalSize)
    {
        //CDX_LOGV("seek to stream end.");
        impl->ioState = CDX_IO_STATE_EOS;
        return 0;
    }

    if(impl->httpDataBufferChunked)
    {
        //Pfree(impl->pool, impl->httpDataBufferChunked);
        free(impl->httpDataBufferChunked);
        impl->httpDataBufferChunked = NULL;
        impl->httpDataBufferPosChunked= 0;
        impl->httpDataSizeChunked= 0;
    }

#if NO_USE
    if(impl->httpDataBuffer)
    {
        Pfree(impl->pool, impl->httpDataBuffer);
        impl->httpDataBuffer = NULL;
        impl->httpDataSize = 0;
    }
#endif

#if CONFIG_HTTP_STREAM_RECONNECT_RELOAD
#if CDX_BUF_STAT
    CdxBufStatSetStartStamp(&http_stat.http_reconn_time);
#endif
    ret = CdxHttpStreamingStart(impl, offset);
#if CDX_BUF_STAT
    CdxBufStatSetEndStamp(&http_stat.http_reconn_time);
    CdxBufStatIncProcTime(&http_stat.http_reconn_time);
    if (ret != 0)
    {
        CdxBufStatIncCount(&http_stat.http_reconn_fails);
    }
#endif
    return ret;
#else
    do
    {
        if(httpHdr)
        {
            CdxHttpFree(httpHdr);
            httpHdr = NULL;
        }

        if(impl->forceStopFlag)
        {
            CDX_LOGD("forcestop");
            goto err_out;
        }

        pthread_mutex_lock(&impl->lock);
        if(impl->tcpStream)
        {
            CdxStreamClose(impl->tcpStream);
            impl->tcpStream = NULL;
        }
        pthread_mutex_unlock(&impl->lock);

        if (redirect == 1)
        {
            redirect = 0;
        }

        ret = CdxHttpSendRequest(impl, offset);
        if(ret != 0)
        {
            CDX_LOGE("Send http request failed.");
            impl->ioState = CDX_IO_STATE_ERROR;
            return -1;
        }

        httpHdr = CdxHttpReadResponse(impl);
        if(httpHdr == NULL)
        {
            CDX_LOGE("Read http response failed.");
            impl->ioState = CDX_IO_STATE_ERROR;
            return -1;
        }

        switch(httpHdr->statusCode)
        {
            case 200:
            case 206:
            {
                logv("============ seek reconnect ok, url:(%s), offset:(%lld)", url->url, offset);
                break;
            }
            case 301: // Permanently
            case 302: // Temporarily
            case 303: // See Other
            case 307: // Temporarily (since HTTP/1.1)
            {
                //RFC 2616, recommand to detect infinite redirection loops
                nextUrl = CdxHttpGetField(httpHdr, "Location");
                CDX_LOGD("xxx nextUrl:(%s)", nextUrl);
                if(nextUrl != NULL)
                {
                    nextUrl = RmSpace(nextUrl);
                    //CDX_LOGV("xxx nextUrl:(%s)", nextUrl);
                    impl->url = CdxUrlRedirect(&url, nextUrl);
                    if(strcasecmp(url->protocol, "http") && strcasecmp(url->protocol, "https"))
                    {
                        CDX_LOGE("Unsupported http %d redirect to %s protocol.",
                                      httpHdr->statusCode, url->protocol);
                        goto err_out;
                    }
                    if(impl->sourceUri)
                    {
                        Pfree(impl->pool, impl->sourceUri);
                        impl->sourceUri = NULL;
                    }
                    impl->sourceUri = (cdx_char *)Palloc(impl->pool, strlen(url->url)+1);
                    CDX_CHECK(impl->sourceUri);
                    memset(impl->sourceUri, 0x00, strlen(url->url)+1);
                    memcpy(impl->sourceUri, url->url, strlen(url->url));  //for ParserTypeGuess
                    redirect = 1;//resend request.
                }
                else
                {
                    CDX_LOGW("No redirect uri?");
                    goto err_out;
                }

                logv("============ seek reconnect redirect, url:(%s), offset:(%lld)",
                    url->url, offset);
                break;
            }
            default:
            {
                CDX_LOGD("status code=%d", httpHdr->statusCode);
                impl->ioState = CDX_IO_STATE_ERROR;
                CdxHttpFree(httpHdr);
                return -2;
            }
        }

    }while(redirect);

    if(impl->ioState != CDX_IO_STATE_EOS)
    {
        impl->ioState = CDX_IO_STATE_OK;
    }
    CdxHttpFree(httpHdr);
    return 0;

err_out:
    impl->ioState = CDX_IO_STATE_ERROR;
    CdxHttpFree(httpHdr);
    return -1;
#endif
}

static void ClearHttpHeaderFields(CdxHttpStreamImplT *impl)
{
    cdx_int32 i;

    if(impl->pHttpHeader)
    {
        for(i = 0; i < impl->nHttpHeaderSize; i++)
        {
            if(impl->pHttpHeader[i].key)
            {
                Pfree(impl->pool, (void *)impl->pHttpHeader[i].key);
            }
            if(impl->pHttpHeader[i].val)
            {
                Pfree(impl->pool, (void *)impl->pHttpHeader[i].val);
            }
        }
        Pfree(impl->pool, impl->pHttpHeader);
        impl->pHttpHeader = NULL;
    }
    impl->nHttpHeaderSize = 0;
}

static void ClearDataSourceFields(CdxHttpStreamImplT *impl)
{
    if(impl->sourceUri)
    {
        Pfree(impl->pool, impl->sourceUri);
        impl->sourceUri = NULL;
    }

    ClearHttpHeaderFields(impl);

    ClearHttpExtraDataContainer(impl);

    return;
}
static cdx_int32 SetDataSourceFields(CdxDataSourceT * source, CdxHttpStreamImplT *impl)
{
    CdxHttpHeaderFieldsT* pHttpHeaders;
    cdx_int32             i;
    cdx_int32             j = 0;
    cdx_int32             num_cacert = 0;

    if(source->uri)
    {
        if (source->probeSize > 0)
        {
            impl->probeData.len = source->probeSize;
        }

        pHttpHeaders = (CdxHttpHeaderFieldsT *)source->extraData;
        impl->sourceUri = Pstrdup(impl->pool, source->uri);
        if(source->extraData)
        {
            for (i = 0; i < pHttpHeaders->num; i++)
            {
                if (0 == strcasecmp(pHttpHeaders->pHttpHeader[i].key, "cacert"))
                {
                    impl->certificate = Pstrdup(impl->pool, pHttpHeaders->pHttpHeader[i].val);
                    if(impl->certificate == NULL)
                    {
                        CDX_LOGE("Palloc failed.");
                        ClearDataSourceFields(impl);
                        return -1;
                    }
                    num_cacert = 1;
                    break;
                }
            }

            impl->nHttpHeaderSize = pHttpHeaders->num - num_cacert;
            if (impl->nHttpHeaderSize == 0)
            {
                return 0;
            }

            impl->pHttpHeader = (CdxHttpHeaderFieldT *)Palloc(impl->pool, impl->nHttpHeaderSize *
                sizeof(CdxHttpHeaderFieldT));
            if(impl->pHttpHeader == NULL)
            {
                CDX_LOGE("Palloc failed.");
                ClearDataSourceFields(impl);
                return -1;
            }
            memset(impl->pHttpHeader, 0x00, impl->nHttpHeaderSize * sizeof(CdxHttpHeaderFieldT));
            for(i = 0; i < impl->nHttpHeaderSize; i++)
            {
                if (0 == strcasecmp(pHttpHeaders->pHttpHeader[j].key, "cacert"))
                {
                    j++;
                }
                if(0 == strcasecmp(pHttpHeaders->pHttpHeader[j].key, "Set-Cookie"))
                {
                    impl->pHttpHeader[i].key = (const char*)Pstrdup(impl->pool, "Cookie");
                }
                else
                {
                    impl->pHttpHeader[i].key = (const char*)Pstrdup(impl->pool,
                        pHttpHeaders->pHttpHeader[j].key);
                }
                if(impl->pHttpHeader[i].key == NULL)
                {
                    CDX_LOGE("dup key failed.");
                    ClearDataSourceFields(impl);
                    return -1;
                }
                impl->pHttpHeader[i].val = (const char*)Pstrdup(impl->pool,
                    pHttpHeaders->pHttpHeader[j].val);
                if(impl->pHttpHeader[i].val == NULL)
                {
                    CDX_LOGE("dup val failed.");
                    ClearDataSourceFields(impl);
                    return -1;
                }
                j++;
                CDX_LOGV("============ impl->pHttpHeader[i].val(%s):%s",
                    impl->pHttpHeader[i].key, impl->pHttpHeader[i].val);
            }
        }
    }
    return 0;
}

static CdxStreamProbeDataT *__CdxHttpStreamGetProbeData(CdxStreamT *stream)
{
    CdxHttpStreamImplT *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxHttpStreamImplT, base);

    return &impl->probeData;
}

static cdx_int32 __CdxHttpStreamRead(CdxStreamT *stream, void *buf, cdx_uint32 len)
{
    CdxHttpStreamImplT *impl;
    cdx_uint32 sendSize = 0;
    cdx_uint32 remainSize = 0;
    cdx_int32 ret = 0;
    cdx_int64 startTime = 0;
    cdx_int64 endTime = 0;
    cdx_int64 totTime = 0;
    cdx_int32 errorSendFlag = 0;

#if CDX_BUF_STAT
    CdxBufStatIncInterval(&http_stat.http_read_invl);
#endif

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxHttpStreamImplT, base);

    pthread_mutex_lock(&impl->lock);
    if(impl->forceStopFlag)
    {
        pthread_mutex_unlock(&impl->lock);
        return -1;
    }
    impl->state = HTTP_STREAM_READING;
    pthread_mutex_unlock(&impl->lock);
#if CDX_BUF_STAT
    CdxBufStatSetStartStamp(&http_stat.http_read_time);
#endif
    sendSize = len;
    //keep validDataSize >= size

    if (len > (impl->maxBufSize - impl->maxProtectAreaSize))
    {
        CDX_LOGE("len is too big!want:%d, max:%d", len, (int)(impl->maxBufSize - impl->maxProtectAreaSize));
        sendSize = -1;
        goto __exit;
    }

    if (impl->validDataSize < len)
    {
        impl->enoughData = CDX_FALSE;
    }

    extern unsigned int XPlayer_Playerback_Timeout_ms;
    cdx_int64 t1, t0 = GetNowUs();

#if CDX_BUF_STAT
    int not_enough_data_cnt = 0;
#endif

    while (impl->enoughData == CDX_FALSE)
    {
#if CDX_BUF_STAT
        not_enough_data_cnt++;
#endif
        if(impl->forceStopFlag == 1)
        {
            CDX_LOGV("http stream read forceStop.");
            sendSize = -1;
            goto __exit;
        }
        if(impl->ioState == CDX_IO_STATE_EOS)
        {
            if (impl->validDataSize >= len)
                break;
            sendSize = impl->validDataSize;
            CDX_LOGD("xxx impl->ioState(%d), sendSize(%u) impl->validDataSize(%d)",
                (cdx_uint32)impl->ioState, (cdx_uint32)sendSize, (cdx_uint32)impl->validDataSize);
            break;
        }
        else if(impl->ioState == CDX_IO_STATE_ERROR)
        {
            if(!impl->seekAble)
            {
                CDX_LOGI("not support seek. trying to ignore data to seek");
#if (!CONFIG_HTTP_STREAM_IGNORE_DATA_SEEK)
                sendSize = -1;
                goto __exit;
#endif
            }
            if(impl->callback)
            {
                cdx_int32 disconnect_state = 1;
                impl->callback(impl->pUserData, STREAM_EVT_NET_DISCONNECT, &disconnect_state);
            }
            while(1)
            {
                impl->ioState = CDX_IO_STATE_INVALID;
                startTime = GetNowUs();
                pthread_mutex_lock(&impl->bufferMutex);
                CDX_LOGD("reconnect at(%d/%d)", (cdx_uint32)impl->readPos, (cdx_uint32)impl->totalSize);

#if CDX_BUF_STAT
                CdxBufStatSetStartStamp(&http_stat.http_read_reconn_time);
#endif
                ret = CdxSeekReconnect(stream, impl->readPos);
#if CDX_BUF_STAT
                CdxBufStatSetEndStamp(&http_stat.http_read_reconn_time);
                CdxBufStatIncProcTime(&http_stat.http_read_reconn_time);
#endif
                if(impl->forceStopFlag == 1)
                {
                    CDX_LOGV("http stream read forceStop.");
                    pthread_mutex_unlock(&impl->bufferMutex);
                    sendSize = -1;
                    goto __exit;
                }

                if(ret == 0)
                {
                    CDX_LOGD("reconnect at(%d/%d)ok, continue read.",
                        (cdx_uint32)impl->readPos, (cdx_uint32)impl->totalSize);
                    if(impl->callback)
                    {
                        cdx_int32 disconnect_state = 0;
                        impl->callback(impl->pUserData, STREAM_EVT_NET_DISCONNECT, &disconnect_state);
                    }
                    pthread_mutex_unlock(&impl->bufferMutex);
                    break;
                }
#if defined(CONF_CMCC)
                else if(ret == -2 || impl->isHls)
                {
                    sendSize = -1;
                    pthread_mutex_unlock(&impl->bufferMutex);
                    goto __exit;
                }
#endif
                else
                {
#if CDX_BUF_STAT
                    CdxBufStatIncCount(&http_stat.http_read_reconn_fails);
#endif
                    CDX_LOGW("reconnect failed, try again...");
                    usleep(200*1000);
                }
                pthread_mutex_unlock(&impl->bufferMutex);
                endTime = GetNowUs();
                totTime += (endTime - startTime);
                if(totTime >= (cdx_int64)RE_CONNECT_TIME * 1000000 && !errorSendFlag)
                {
                    CDX_LOGE("reconnect failed, tried time:%d s, break.", RE_CONNECT_TIME);
                    sendSize = -1;

                    if(impl->callback)
                    {
                        cdx_int32 errCode = 2000;
                        impl->callback(impl->pUserData, STREAM_EVT_DOWNLOAD_ERROR, &errCode);
                        errorSendFlag = 1;
                    }
                    goto __exit;
                }/*RE_CONNECT_TIME > (XPlayer_Playerback_Timeout_ms<<10), so this code doesn't work*/

                t1 = GetNowUs();
                if ((t1 - t0) > (XPlayer_Playerback_Timeout_ms << 10))
                {
                    CDX_LOGW("read timeout:%d us, break.", (int)(t1 - t0));
                    sendSize = -1;
                    goto __exit;
                }
            }
        }

        t1 = GetNowUs();
        if ((t1 - t0) > (XPlayer_Playerback_Timeout_ms << 10))
        {
            CDX_LOGW("read timeout:%d us, break.", (int)(t1 - t0));
            sendSize = -1;
            goto __exit;
        }

#if CDX_BUF_STAT
        CdxBufStatIncCount(&http_stat.http_read_wait_data_cnt);
#endif
        //CDX_LOGV("validDataSize(%lld) < len(%d), usleep(10ms)", impl->validDataSize, len);
        usleep(10000);

        if ((impl->validDataSize >= CDX_HTTP_THRESHOLD) && (impl->validDataSize >= len))
        {
#if CDX_BUF_STAT
            CdxBufStatIncStat(&http_stat.http_read_not_enough_cnts, not_enough_data_cnt);
#endif
            impl->enoughData = CDX_TRUE;
        }
    }

    if(impl->ioState == CDX_IO_STATE_EOS)
    {
        impl->bStreamReadEos = 1;
    }

    pthread_mutex_lock(&impl->bufferMutex);
    remainSize = impl->bufEndPtr - impl->bufReadPtr + 1;
    if(remainSize >= sendSize)
    {
        memcpy((char*)buf, impl->bufReadPtr, sendSize);
    }
    else
    {
        memcpy((char*)buf, impl->bufReadPtr, remainSize);
        memcpy((char*)buf+remainSize, impl->buffer, sendSize-remainSize);
    }

    impl->bufReadPtr += sendSize;
    impl->readPos    += sendSize;
    if(impl->readPos - impl->protectAreaPos > impl->maxProtectAreaSize)
    {
        impl->protectAreaPos = impl->readPos - impl->maxProtectAreaSize;
        impl->protectAreaSize = impl->maxProtectAreaSize;
    }
    else
    {
        impl->protectAreaSize += sendSize;
        if(impl->protectAreaSize > impl->maxProtectAreaSize)
        {
            impl->protectAreaSize = impl->maxProtectAreaSize;
        }
    }

    if(impl->bufReadPtr > impl->bufEndPtr)
    {
        impl->bufReadPtr -= impl->maxBufSize;
    }
    impl->validDataSize -= sendSize;
    pthread_mutex_unlock(&impl->bufferMutex);

__exit:
#if CDX_BUF_STAT
    CdxBufStatSetEndStamp(&http_stat.http_read_time);
    CdxBufStatIncProcTime(&http_stat.http_read_time);
#endif
    pthread_mutex_lock(&impl->lock);
    impl->state = HTTP_STREAM_IDLE;
    pthread_mutex_unlock(&impl->lock);
    pthread_cond_signal(&impl->cond);
    return sendSize;
}
static cdx_int32 CdxHttpStreamForceStop(CdxStreamT *stream)
{
    CdxHttpStreamImplT *impl;
    long ref;

    CDX_UNUSE(ref);

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxHttpStreamImplT, base);

    CDX_LOGV("xxx begin http force stop.");

    pthread_mutex_lock(&impl->lock);

    impl->forceStopFlag = 1;

    while (impl->state != HTTP_STREAM_IDLE)
    {
        if (impl->tcpStream != NULL)
        {
            CdxStreamForceStop(impl->tcpStream);
        }

        pthread_cond_wait(&impl->cond, &impl->lock);
    }

    pthread_mutex_unlock(&impl->lock);

    CDX_LOGV("xxx finish http force stop");
    return 0;
}
static cdx_int32 CdxHttpStreamClrForceStop(CdxStreamT *stream)
{
    CdxHttpStreamImplT *impl;
    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxHttpStreamImplT, base);

    pthread_mutex_lock(&impl->lock);
    impl->forceStopFlag = 0;
    if(impl->tcpStream)
    {
        CdxStreamClrForceStop(impl->tcpStream);
    }
    impl->state = HTTP_STREAM_IDLE;
    pthread_mutex_unlock(&impl->lock);

    return 0;
}
static void ExitGetNetworkData(void *pArg)
{
    CdxHttpStreamImplT *impl;

    impl = (CdxHttpStreamImplT *)pArg;
    if(impl == NULL)
    {
        CDX_LOGV("xxx impl is NULL.");
        return ;
    }

    if(impl->tcpStream)
    {
        CdxStreamForceStop(impl->tcpStream);
    }
    while(impl->getNetworkDataFlag == 1)
    {
        //CDX_LOGV("xxx impl->getNetworkDataFlag == 1");
        usleep(5000);
    }
    //CDX_LOGV("xxxxxxxxxxxxxx ExitGetNetworkData finish");

    return;
}
static cdx_int32 __CdxHttpStreamClose(CdxStreamT *stream)
{
    CdxHttpStreamImplT *impl;
    //long ref;
    //cdx_int32 ret = 0;
    //AwPoolT *pool;
    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxHttpStreamImplT, base);

    //pool = impl->pool;

    CDX_LOGV("xxxx http close begin. stream(%p)", stream);
    impl->exitFlag = 1;

#if __SAVE_BITSTREAMS
    fclose(impl->fp_http);
#endif

    CdxHttpStreamForceStop(stream);

    ExitGetNetworkData(impl);// exit from GetNetworkData

    if(impl->threadId)
        pthread_join(impl->threadId, NULL);

    if(impl->tcpStream)
    {
        if (impl->bStreamReadEos == 1 && impl->keepAlive == 1)
            CdxStreamControl(impl->tcpStream, STREAM_CMD_SET_EOF, NULL);

        pthread_mutex_lock(&impl->lock);
        CdxStreamClose(impl->tcpStream);//close tcp first.
        impl->tcpStream = NULL;
        pthread_mutex_unlock(&impl->lock);
        impl->ioState = CDX_IO_STATE_INVALID;
    }

    if (impl->certificate)
    {
        Pfree(impl->pool, impl->certificate);
        impl->certificate = NULL;
    }

    if(impl->buffer)
    {
        Pfree(impl->pool, impl->buffer);
        impl->buffer = NULL;
    }
    if(impl->url != NULL)
    {
        CdxUrlFree(impl->url);
        impl->url = NULL;
    }
#if NO_USE
    if(impl->httpDataBuffer != NULL)
    {
        Pfree(impl->pool, impl->httpDataBuffer);
        impl->httpDataBuffer = NULL;
    }
#endif

    if(impl->httpDataBufferChunked != NULL)
    {
        //Pfree(impl->pool, impl->httpDataBufferChunked);
        free(impl->httpDataBufferChunked);
        impl->httpDataBufferChunked = NULL;
    }
    if(impl->data != NULL)
    {
        CdxHttpFree((CdxHttpHeaderT *)impl->data);
        impl->data = NULL;
    }
    ClearDataSourceFields(impl);
    if(impl->probeData.buf)
    {
        Pfree(impl->pool, impl->probeData.buf);
        impl->probeData.buf = NULL;
    }
#if __CONFIG_ZLIB
    inflateEnd(&impl->inflateStream);
    free(impl->inflateBuffer);
#endif
    pthread_mutex_destroy(&impl->bufferMutex);
    pthread_cond_destroy(&impl->bufferCond);
    pthread_mutex_destroy(&impl->seekMutex);
    pthread_cond_destroy(&impl->seekCond);
    pthread_mutex_destroy(&impl->pauseReadDataMutex);
    pthread_cond_destroy(&impl->pauseReadDataCond);

#if (CONFIG_HTTP_STREAM_SET_PROTECT_AREA_SIZE || CONFIG_HTTP_STREAM_SUPPORT_FREE_BUFFER)
    pthread_mutex_destroy(&impl->setBufferMutex);
    pthread_cond_destroy(&impl->setBufferCond);
#endif

    pthread_mutex_destroy(&impl->lock);
    pthread_cond_destroy(&impl->cond);

    if (impl->pool != NULL)
    {
	AwPoolDestroy(impl->pool);
    }

    free(impl);
    //AwPoolDestroy(pool);
    
    CDX_LOGV("xxxx http close finish.");
    return 0;
}

static cdx_int32 __CdxHttpStreamGetIOState(CdxStreamT *stream)
{
    CdxHttpStreamImplT *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxHttpStreamImplT, base);

    return impl->ioState;

}

int GetEstimatedBandwidthKbps(CdxHttpStreamImplT *impl, cdx_int32 *kbps)//0424
{
    if(impl->mBandwidthTimes < 2)
    {
        return -1;
    }

    *kbps = ((double)impl->mTotalTransferBytes * 8E3
                                            / impl->mTotalTransferTimeUs);
    return 0;
}

void TransferMeasurement(CdxHttpStreamImplT *impl, cdx_int32 numBytes, cdx_int64 delayUs) //0424
{
#if CDX_IOT_CACHE_SUPPORT
    BandwidthEntryT begin;

    if(impl->index >= BAND_WIDTH_RECORD_NUMBER)
    {
        impl->index = 0;
    }
    begin = impl->bandWidthHistory[impl->index];

    impl->bandWidthHistory[impl->index].mDelayUs = delayUs;
    impl->bandWidthHistory[impl->index].mNumBytes = numBytes;
    impl->mTotalTransferBytes += numBytes;
    impl->mTotalTransferTimeUs += delayUs;
    /*CDX_LOGD("mTotalTransferBytes(%d),mTotalTransferTimeUs(%lld), impl->mBandwidthTimes(%d)",
    impl->mTotalTransferBytes, impl->mTotalTransferTimeUs, impl->mBandwidthTimes);*/

    impl->mBandwidthTimes++;
    if(impl->mBandwidthTimes > BAND_WIDTH_RECORD_NUMBER)// || impl->bufFullFlag == 1)
    {
        impl->mTotalTransferBytes -= begin.mNumBytes;
        impl->mTotalTransferTimeUs -= begin.mDelayUs;
        --impl->mBandwidthTimes;
    }

    impl->index++;
#else

    impl->count++;
    impl->tmpTotalBytes += numBytes;
    impl->tmpTotalTimeUs += delayUs;

    impl->mBandwidthTimes++;
    impl->mTotalTransferBytes += numBytes;
    impl->mTotalTransferTimeUs += delayUs;

    if (impl->count >= BAND_WIDTH_RECORD_NUMBER)
    {
        impl->mTotalTransferBytes = impl->tmpTotalBytes;
        impl->mTotalTransferTimeUs = impl->tmpTotalTimeUs;
        impl->mBandwidthTimes = BAND_WIDTH_RECORD_NUMBER;
        impl->count = 0;
        impl->tmpTotalBytes = 0;
        impl->tmpTotalTimeUs = 0;
    }

#endif
}

static int GetCacheState(struct StreamCacheStateS *cacheState, CdxStreamT *stream)//0424
{
    CdxHttpStreamImplT *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxHttpStreamImplT, base);

    cdx_int64 totSize = impl->totalSize;
    cdx_int64 bufPos = impl->bufPos;
    cdx_int32 percent = 0;
    cdx_int32 kbps = 0;//4000;//512KB/s

    memset(cacheState, 0, sizeof(struct StreamCacheStateS));

    if (totSize > 0)
    {
        percent = 100 * bufPos/totSize;
    }
    else
    {
        percent = 0;
    }

    cacheState->nPercentage = percent;

    if(GetEstimatedBandwidthKbps(impl, &kbps) == 0)
    {
        cacheState->nBandwidthKbps = kbps;
    }
    else
    {
        cacheState->nBandwidthKbps = 300;
    }

    cacheState->nCacheCapacity = impl->maxBufSize;
    cacheState->nCacheSize = impl->validDataSize;

    //CDX_LOGD("nCacheSize:%d, nCacheCapacity %d, nBandwidthKbps:%dkbps, percent:%d%%",
    //        cacheState->nCacheSize, cacheState->nCacheCapacity,
    //        cacheState->nBandwidthKbps, cacheState->nPercentage);
    return 0;
}

//mSec: in unit of millisecond
void PrintCacheState(cdx_int32 mSec, cdx_int64 *lastNotifyTime, CdxStreamT *stream)
{
    struct StreamCacheStateS cacheState;

    if(GetNowUs() > *lastNotifyTime + mSec * 1000)
    {
        GetCacheState(&cacheState, stream);
        CDX_LOGD("xxx nCacheSize:%d, nCacheCapacity %d, nBandwidthKbps:%dkbps, percent:%d%%",
                cacheState.nCacheSize, cacheState.nCacheCapacity,
                cacheState.nBandwidthKbps, cacheState.nPercentage);
        CDX_LOGD("xxx nCacheSize(%d)", cacheState.nCacheSize);
        *lastNotifyTime = GetNowUs();
    }

    return;
}

static cdx_int32 __CdxHttpStreamControl(CdxStreamT *stream, cdx_int32 cmd, void *param)
{
    CdxHttpStreamImplT *impl;
    cdx_int64* value = NULL;
    int tmp_init = 0;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxHttpStreamImplT, base);

    switch(cmd)
    {
        case STREAM_CMD_GET_CACHESTATE:
            return GetCacheState((struct StreamCacheStateS *)param, stream);

        case STREAM_CMD_SET_FORCESTOP:
            //CDX_LOGV("xxx STREAM_CMD_SET_FORCESTOP");
            return CdxHttpStreamForceStop(stream);

        case STREAM_CMD_CLR_FORCESTOP:
            return CdxHttpStreamClrForceStop(stream);
        case STREAM_CMD_SET_CALLBACK:
        {
            struct CallBack *cb = (struct CallBack *)param;
            impl->callback = cb->callback;
            impl->pUserData = cb->pUserData;
            return 0;
        }

        case STREAM_CMD_SET_ISHLS:
        {
            CDX_LOGD("======= set ishls");
            impl->isHls = 1;
            return 0;
        }

        case STREAM_CMD_SET_RELATIVE_START_POS:
        {
            value = (cdx_int64*)param;
            impl->baseOffset = (*value);
            return 0;
        }
        case STREAM_CMD_SET_RELATIVE_FILE_SIZE:
        {
            return 0;
        }
        case STREAM_CMD_NEXT_PROBE_DATA:
        {
            cdx_int32 ret;
            impl->probeData.len = PROBE_DATA_LEN_DEFAULT;
            ret = __CdxHttpStreamRead(stream, impl->probeData.buf, impl->probeData.len);
            /* it could be better if set relative pos and file size. */
            CDX_LOG_CHECK(ret >= 0, "f_read failed in file stream connect, fresult(%d)", ret);
            if ((cdx_uint32)ret < impl->probeData.len)
            {
                CDX_LOGW("io fail, file end");
                   return CDX_FAILURE;
            }
            break;
        }
        case STREAM_CMD_FREE_PROBE_DATA:
        {
            if(impl->probeData.buf)
            {
                Pfree(impl->pool, impl->probeData.buf);
                impl->probeData.buf = NULL;
                impl->probeData.len= 0;
            }
            return 0;
        }
#if CONFIG_HTTP_STREAM_SET_PROTECT_AREA_SIZE
        case STREAM_CMD_SET_PROTECT_SIZE:
        {
            cdx_int32 buffer_size = 0;
            cdx_char *tmp_buffer = NULL;

            struct ProtectAreaInfo *info = (struct ProtectAreaInfo *)param;
            if ((info->protectsize < 0) || (info->protectsize == impl->maxProtectAreaSize)) {
                return 0;
            }

            if (!info->fixbuffer) {
                buffer_size = impl->maxBufSize - impl->maxProtectAreaSize + info->protectsize;
                tmp_buffer = (cdx_char *)malloc(buffer_size);
                if (tmp_buffer == NULL) {
                    return 0;
                }
            }

            impl->setBufferFlag = 1;
            pthread_mutex_lock(&impl->pauseReadDataMutex);
            while(impl->pauseReadDataFlag == -1)
            {
                pthread_cond_wait(&impl->pauseReadDataCond, &impl->pauseReadDataMutex);
            }
            pthread_mutex_unlock(&impl->pauseReadDataMutex);

            if (!info->fixbuffer) {
                if (impl->bufWritePtr >= impl->bufReadPtr) {
                    memcpy(tmp_buffer, impl->bufReadPtr, impl->bufWritePtr - impl->bufReadPtr);
                } else {
                    cdx_uint32 remain_size;
                    remain_size = impl->bufEndPtr - impl->bufReadPtr + 1;
                    memcpy(tmp_buffer, impl->bufReadPtr, remain_size);
                    memcpy(tmp_buffer + remain_size, impl->buffer, impl->bufWritePtr - impl->buffer);
                }
                free(impl->buffer);
                impl->buffer = tmp_buffer;
                impl->bufWritePtr = impl->buffer + impl->validDataSize;
                impl->bufReadPtr = impl->buffer;
                impl->maxBufSize = buffer_size;
                impl->bufEndPtr = impl->buffer + impl->maxBufSize - 1;
            }

            impl->maxProtectAreaSize = info->protectsize;
            impl->protectAreaPos = impl->readPos;
            impl->protectAreaSize = 0;

            pthread_mutex_lock(&impl->setBufferMutex);
            impl->setBufferFlag = 0;
            pthread_cond_signal(&impl->setBufferCond);
            pthread_mutex_unlock(&impl->setBufferMutex);
            return 0;
        }
#endif
#if CONFIG_HTTP_STREAM_SUPPORT_FREE_BUFFER
        case STREAM_CMD_FREE_BUFFER:
        {
            impl->setBufferFlag = 1;
            pthread_mutex_lock(&impl->pauseReadDataMutex);
            while(impl->pauseReadDataFlag == -1)
            {
                pthread_cond_wait(&impl->pauseReadDataCond, &impl->pauseReadDataMutex);
            }
            pthread_mutex_unlock(&impl->pauseReadDataMutex);

            if ((impl->ioState == CDX_IO_STATE_EOS) && (impl->validDataSize == 0)) {
                free(impl->buffer);
                impl->buffer = NULL;
                impl->bufWritePtr = NULL;
                impl->bufReadPtr = NULL;
                impl->maxBufSize = 0;
                impl->bufEndPtr = NULL;
                impl->maxProtectAreaSize = 0;
                impl->protectAreaPos = impl->readPos;
                impl->protectAreaSize = 0;
            }

            pthread_mutex_lock(&impl->setBufferMutex);
            impl->setBufferFlag = 0;
            pthread_cond_signal(&impl->setBufferCond);
            pthread_mutex_unlock(&impl->setBufferMutex);
            return 0;
        }
#endif
        case STREAM_CMD_NO_PROBE_DATA:
        {
            impl->noProbeData = 1;
            return 0;
        }
        case STREAM_CMD_SET_PARSER_INIT:
        {
            pthread_mutex_lock(&impl->lock);
            impl->parser_init = 1;
            pthread_mutex_unlock(&impl->lock);
            return 0;
        }
        case STREAM_CMD_GET_PARSER_INIT:
        {
            pthread_mutex_lock(&impl->lock);
            tmp_init = impl->parser_init;
            pthread_mutex_unlock(&impl->lock);
            return tmp_init;
        }
        default:
            return 0;
    }
    return 0;
}

static cdx_int32 __CdxHttpStreamSeek(CdxStreamT *stream, cdx_int64 offset, cdx_int32 whence)
{
    CdxHttpStreamImplT *impl;
    cdx_int64 fileLen = 0;
    cdx_int32 ret = 0;
    cdx_int32 try_times = 0;
    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxHttpStreamImplT, base);

#if CDX_BUF_STAT
    CdxBufStatIncInterval(&http_stat.http_seek_invl);
#endif

    pthread_mutex_lock(&impl->lock);
    if(impl->forceStopFlag)
    {
        pthread_mutex_unlock(&impl->lock);
        //CDX_LOGV("force stop http seek.");
        return -1;
    }
    impl->state = HTTP_STREAM_SEEKING;
    pthread_mutex_unlock(&impl->lock);

    fileLen = impl->totalSize;

    switch(whence)
    {
        case STREAM_SEEK_SET:
        {
            CDX_LOGD("STREAM_SEEK_SET, offset(%lld) += baseOffset(%lld), readPos:%lld, bufPos:%lld, protectAreaPos:%lld, fileLen:%lld", offset, impl->baseOffset, impl->readPos, impl->bufPos, impl->protectAreaPos, fileLen);
            offset += impl->baseOffset;
            break;
        }
        case STREAM_SEEK_CUR:
        {
            CDX_LOGD("STREAM_SEEK_CUR, offset(%lld) += readPos(%lld), baseOffset:%lld, bufPos:%lld, protectAreaPos:%lld, fileLen:%lld", offset, impl->readPos, impl->baseOffset, impl->bufPos, impl->protectAreaPos, fileLen);
            offset += impl->readPos;
            break;
        }
        case STREAM_SEEK_END:
        {
            if(fileLen > 0)
            {
                CDX_LOGD("STREAM_SEEK_END, offset(%lld) += baseOffset(%lld) + fileLen(%lld), readPos:%lld, bufPos:%lld, protectAreaPos:%lld", offset, impl->baseOffset, fileLen, impl->readPos, impl->bufPos, impl->protectAreaPos);
                offset += impl->baseOffset + fileLen;
            }
            else
            {
                CDX_LOGW("bad fileLen, maybe live stream.");
                goto err_out;
            }

            break;
        }
        default:
        {
            CDX_LOGE("should not be here.");
            goto err_out;
        }
    }
    CDX_LOGV("offset(%d), whence(%d)", (cdx_int32)offset, whence);

    if((fileLen > 0 && offset > fileLen) || offset < 0)
    {
        CDX_LOGE("bad offset(%d), fileLen(%d), stream(%p)", (cdx_uint32)offset, (cdx_uint32)fileLen, stream);
        goto err_out;
    }

    if((offset <= impl->bufPos) && (offset >= impl->protectAreaPos))//a. [protectAreaPos, bufPos]
    {
        CDX_LOGD("Seek not reconnect.");
        //CDX_LOGD("xxx offset(%d),validDataSize(%d), [(%d),(%d)],impl->readPos(%d)",
        //          (cdx_uint32)offset, (cdx_uint32)impl->validDataSize,
        //          (cdx_uint32)impl->protectAreaPos, (cdx_uint32)impl->bufPos,
        //          (cdx_uint32)impl->readPos);
        goto seek_in_buffer;
    }
#if CONFIG_HTTP_STREAM_IGNORE_DATA_FAST_SEEK
    else if ((!impl->seekAble && (offset > impl->bufPos)) ||
             (impl->seekAble && (offset >= impl->bufPos) && (offset - impl->bufPos < IGNORE_DATA_SEEK_THRESHOLD)))
    {
        CDX_LOGD("Seek not reconnect. but ignore data.");
        goto seek_out_buffer;
    }
#endif
    else // b. < protectAreaPos || > bufPos
    {
        CDX_LOGD("Seek reconnect.");
        //CDX_LOGD("xxx offset(%d),validDataSize(%d), [(%d),(%d)],impl->readPos(%d)",
        //         (cdx_uint32)offset, (cdx_uint32)impl->validDataSize,
        //         (cdx_uint32)impl->protectAreaPos, (cdx_uint32)impl->bufPos,
        //         (cdx_uint32)impl->readPos);
        goto re_connect_begin;
    }

seek_in_buffer:
#if CDX_BUF_STAT
    CdxBufStatIncCount(&http_stat.http_seek_inbuf_cnt);
#endif
    pthread_mutex_lock(&impl->bufferMutex);
    if(offset > impl->readPos)// seek front
    {
        if(offset - impl->protectAreaPos > impl->maxProtectAreaSize)
        {
            impl->protectAreaPos = offset - impl->maxProtectAreaSize;
            impl->protectAreaSize = impl->maxProtectAreaSize;
        }
        else
        {
            impl->protectAreaSize += (offset - impl->readPos);
            if(impl->protectAreaSize > impl->maxProtectAreaSize)
            {
                impl->protectAreaSize = impl->maxProtectAreaSize;
            }
        }
    }
    else // seek back
    {
        impl->protectAreaSize += (offset - impl->readPos);
        /*CDX_LOGD("xxx protectAreaSize(%lld),offset(%lld),impl->readPos(%lld),impl->bufPos(%lld)",
        impl->protectAreaSize,offset,impl->readPos,impl->bufPos);*/
    }

    impl->bufReadPtr += (offset - impl->readPos);
    impl->validDataSize -= (offset - impl->readPos);
    impl->readPos = offset;

    if(impl->bufReadPtr > impl->bufEndPtr)
    {
        impl->bufReadPtr -= impl->maxBufSize;
    }
    else if(impl->bufReadPtr < impl->buffer)
    {
        impl->bufReadPtr += impl->maxBufSize;
    }
    pthread_mutex_unlock(&impl->bufferMutex);

    pthread_mutex_lock(&impl->lock);
    impl->state = HTTP_STREAM_IDLE;
    pthread_mutex_unlock(&impl->lock);
    pthread_cond_signal(&impl->cond);
    return 0;

#if CONFIG_HTTP_STREAM_IGNORE_DATA_FAST_SEEK
seek_out_buffer:
#if CDX_BUF_STAT
    CdxBufStatIncCount(&http_stat.http_seek_outbuf_cnt);
#endif

    impl->seekFlag = 1;
    pthread_mutex_lock(&impl->pauseReadDataMutex);
    while(impl->pauseReadDataFlag == -1)
    {
        pthread_cond_wait(&impl->pauseReadDataCond, &impl->pauseReadDataMutex);
    }
    pthread_mutex_unlock(&impl->pauseReadDataMutex);
    impl->ioState = CDX_IO_STATE_INVALID;

    pthread_mutex_lock(&impl->bufferMutex);
    if (offset > impl->bufPos)
    {
        logd("readPos: %d, bufPos: %d, offset: %d, bufReadPtr: %d, \n"
            "bufWritePtr: %d, validDataSize: %d, protectAreaSize: %d",
            (uint32_t)impl->readPos, (uint32_t)impl->bufPos, (uint32_t)offset,
            (uint32_t)impl->bufReadPtr, (uint32_t)impl->bufWritePtr,
            (uint32_t)impl->validDataSize, (uint32_t)impl->protectAreaSize);
        CDX_LOGD("try to ignore the data to seek backward! ignore: %d", (cdx_int32)(offset - impl->bufPos));
        impl->ignoreDataSize = offset - impl->bufPos;
        impl->readPos = offset;
        impl->bufReadPtr = impl->buffer;
        impl->bufWritePtr = impl->buffer;
        impl->protectAreaPos = offset;
        impl->protectAreaSize = 0;
        impl->validDataSize = 0;
        memset(impl->buffer, 0, impl->maxBufSize);

        if(offset == impl->totalSize)
        {
            //CDX_LOGV("seek to stream end.");
            impl->ioState = CDX_IO_STATE_EOS;
            pthread_mutex_unlock(&impl->bufferMutex);
            return 0;
        }

        if(impl->httpDataBufferChunked)
        {
            //Pfree(impl->pool, impl->httpDataBufferChunked);
            free(impl->httpDataBufferChunked);
            impl->httpDataBufferChunked = NULL;
            impl->httpDataBufferPosChunked= 0;
            impl->httpDataSizeChunked= 0;
        }
    }
    else
    {
        logw("buffer over offset, turn to buffer seek: offset: %d, bufPos: %d",
             (cdx_int32)offset, (cdx_int32)(impl->bufPos));
        if(offset - impl->protectAreaPos > impl->maxProtectAreaSize)
        {
            impl->protectAreaPos = offset - impl->maxProtectAreaSize;
            impl->protectAreaSize = impl->maxProtectAreaSize;
        }
        else
        {
            impl->protectAreaSize += (offset - impl->readPos);
            if(impl->protectAreaSize > impl->maxProtectAreaSize)
            {
                impl->protectAreaSize = impl->maxProtectAreaSize;
            }
        }

        impl->bufReadPtr += (offset - impl->readPos);
        impl->validDataSize -= (offset - impl->readPos);
        impl->readPos = offset;

        if(impl->bufReadPtr > impl->bufEndPtr)
        {
            impl->bufReadPtr -= impl->maxBufSize;
        }
        else if(impl->bufReadPtr < impl->buffer)
        {
            impl->bufReadPtr += impl->maxBufSize;
        }
    }
    impl->ioState = CDX_IO_STATE_OK;
    pthread_mutex_unlock(&impl->bufferMutex);

    pthread_mutex_lock(&impl->seekMutex);
    impl->seekFlag = 0;
    pthread_cond_signal(&impl->seekCond);
    pthread_mutex_unlock(&impl->seekMutex);

    pthread_mutex_lock(&impl->lock);
    impl->state = HTTP_STREAM_IDLE;
    pthread_mutex_unlock(&impl->lock);
    pthread_cond_signal(&impl->cond);
    return 0;
#endif

re_connect_begin:
#if CDX_BUF_STAT
    CdxBufStatSetStartStamp(&http_stat.http_seek_reconn_time);
    CdxBufStatIncCount(&http_stat.http_seek_reconn_cnt);
#endif

re_connect:
    CDX_LOGV("seek reconnect... offset(%d)", (cdx_uint32)offset);
    try_times++;
    if(!impl->seekAble)
    {
        CDX_LOGD("not seekable.");
#if CONFIG_HTTP_STREAM_IGNORE_DATA_SEEK
        CDX_LOGD("try to ignore the data to seek!");//goto err_out;
#else
        goto err_out;
#endif
    }

    impl->seekFlag = 1;

    pthread_mutex_lock(&impl->lock);
    if (impl->forceStopFlag)
    {
        pthread_mutex_unlock(&impl->lock);
        goto err_out;
    }
    if(impl->tcpStream)
    {
        CdxStreamClrForceStop(impl->tcpStream);
    }
    pthread_mutex_unlock(&impl->lock);

    pthread_mutex_lock(&impl->pauseReadDataMutex);
    while(impl->pauseReadDataFlag == -1)
    {
#if CDX_BUF_STAT
        CdxBufStatSetStartStamp(&http_stat.http_seek_wait_to_reconn);
#endif
        pthread_cond_wait(&impl->pauseReadDataCond, &impl->pauseReadDataMutex);
#if CDX_BUF_STAT
        CdxBufStatSetEndStamp(&http_stat.http_seek_wait_to_reconn);
        CdxBufStatIncProcTime(&http_stat.http_seek_wait_to_reconn);
#endif
    }
    pthread_mutex_unlock(&impl->pauseReadDataMutex);

    if(impl->tcpStream)
    {
        CdxStreamControl(impl->tcpStream, STREAM_CMD_CLR_FORCESTOP, NULL);
    }
    impl->ioState = CDX_IO_STATE_INVALID;

    pthread_mutex_lock(&impl->bufferMutex);
    ret = CdxSeekReconnect(stream, offset);
    if(impl->forceStopFlag == 1)
    {
        pthread_mutex_unlock(&impl->bufferMutex);

#if CDX_BUF_STAT
        CdxBufStatSetEndStamp(&http_stat.http_seek_reconn_time);
        CdxBufStatIncProcTime(&http_stat.http_seek_reconn_time);
#endif

        goto err_out;
    }
    if(ret != 0)
    {

#if CDX_BUF_STAT
        CdxBufStatIncCount(&http_stat.http_seek_reconn_fails);
#endif

        pthread_mutex_unlock(&impl->bufferMutex);
        //goto err_out;
        if(try_times<3){
            CDX_LOGW("Seek reconnect failed, try again.");
            usleep(100*1000);
            goto re_connect;
        }
        else{
            CDX_LOGE("Seek reconnect failed, try %d times.", try_times);
            goto err_out;
        }
    }
    //CDX_LOGV("xxx seekreconnect ok. iostate(%d)", impl->ioState);
    pthread_mutex_unlock(&impl->bufferMutex);

    pthread_mutex_lock(&impl->seekMutex);
    impl->seekFlag = 0;
    pthread_cond_signal(&impl->seekCond);
    pthread_mutex_unlock(&impl->seekMutex);

#if CDX_BUF_STAT
    CdxBufStatSetEndStamp(&http_stat.http_seek_reconn_time);
    CdxBufStatIncProcTime(&http_stat.http_seek_reconn_time);
#endif
    pthread_mutex_lock(&impl->lock);
    impl->state = HTTP_STREAM_IDLE;
    pthread_mutex_unlock(&impl->lock);
    pthread_cond_signal(&impl->cond);
    return 0;

err_out:
    pthread_mutex_lock(&impl->seekMutex);
    impl->seekFlag = 0;
    pthread_cond_signal(&impl->seekCond);
    pthread_mutex_unlock(&impl->seekMutex);

    pthread_mutex_lock(&impl->lock);
    impl->state = HTTP_STREAM_IDLE;
    pthread_mutex_unlock(&impl->lock);
    pthread_cond_signal(&impl->cond);

    return -1;
}

static cdx_int32 __CdxHttpStreamEos(CdxStreamT *stream)
{
    CdxHttpStreamImplT *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxHttpStreamImplT, base);

    return (impl->bStreamReadEos == 1);
}

static cdx_int64 __CdxHttpStreamTell(CdxStreamT *stream)
{
    CdxHttpStreamImplT *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxHttpStreamImplT, base);

    return impl->readPos - impl->baseOffset;
}

static cdx_uint32 __CdxHttpStreamAttribute(CdxStreamT *stream)
{
    CdxHttpStreamImplT *impl;
    CDX_CHECK(stream);
    cdx_uint32 flag = 0;

    impl = CdxContainerOf(stream, CdxHttpStreamImplT, base);

#if CONFIG_HTTP_STREAM_IGNORE_DATA_SEEK
    flag |= CDX_STREAM_FLAG_SEEK;
#else
    if(impl->seekAble)
    {
        flag |= CDX_STREAM_FLAG_SEEK;
    }
#endif

    if (impl->isDTMB)
    {
        flag |= CDX_STREAM_FLAG_DTMB;
    }

    return flag|CDX_STREAM_FLAG_NET;
}
static cdx_int32 __CdxHttpStreamWrite(CdxStreamT *stream, void *buf, cdx_uint32 len)
{//not use now..
    CdxHttpStreamImplT *impl;
    cdx_char *temp;

    CDX_UNUSE(impl);
    CDX_UNUSE(temp);

    temp = (cdx_char *)buf;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxHttpStreamImplT, base);

    return len;
}

static cdx_int64 __CdxHttpStreamSize(CdxStreamT *stream)
{
    CdxHttpStreamImplT *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxHttpStreamImplT, base);

    return impl->totalSize;
}
static cdx_int32 __CdxHttpStreamGetMetaData(CdxStreamT *stream, const cdx_char *key,
                                        void **pVal)
{
    CdxHttpStreamImplT *impl;
    CdxHttpHeaderT *httpHdr = NULL;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxHttpStreamImplT, base);
    httpHdr = (CdxHttpHeaderT *)impl->data;

    if(strcmp(key, "uri") == 0)
    {
        *pVal = impl->sourceUri;
        return 0;
    }
    else if(strcmp(key, "extra-data") == 0)
    {
        if(impl->hfsContainer.extraData)
        {
            *pVal = &impl->hfsContainer;
            return 0;
        }
        else
            return -1;
    }
    else if(strcmp(key, "statusCode") == 0)
    {
        if(httpHdr)
        {
        CDX_LOGD("++++++ statusCode in http: %d", httpHdr->statusCode);
        *pVal = (void*)&httpHdr->statusCode;
        CDX_LOGD("++++++ *pVal = %p", *pVal);
        }
    }
    else
    {
        *pVal = CdxHttpGetField(httpHdr, key);
        if (!(*pVal))
        {
            return -1;
        }
    }
    return 0;
}
static int CdxGetProbeData(CdxHttpStreamImplT *impl)
{
    if (impl->probeData.len <= 0 || impl->probeData.len > PROBE_DATA_LEN_DEFAULT)
    {
        impl->probeData.len = PROBE_DATA_LEN_DEFAULT;
    }
    CDX_LOGD("probe data len %d", impl->probeData.len);
    impl->probeData.buf = (cdx_char *)Palloc(impl->pool, impl->probeData.len);

    while(impl->validDataSize < impl->probeData.len)
    {
        if(impl->forceStopFlag == 1)
        {
            CDX_LOGV("forceStop.");
            goto err_out;
        }
        if(impl->ioState == CDX_IO_STATE_EOS)
        {
            if(impl->validDataSize < impl->probeData.len)
            {
                impl->probeData.len = impl->validDataSize;
            }
            break;
        }
        else if(impl->ioState == CDX_IO_STATE_ERROR)
        {
            CDX_LOGE("io error.");
            goto err_out;
        }

        usleep(5000);
    }

    memcpy(impl->probeData.buf, impl->buffer, impl->probeData.len);
    return 0;

err_out:
    if(impl->probeData.buf)
    {
        Pfree(impl->pool, impl->probeData.buf);
        impl->probeData.buf = NULL;
    }
    return -1;
}

#if __CONFIG_ZLIB
#define DECOMPRESS_BUF_LEN (256*1024)
static cdx_int32 StreamReadCompressed(CdxHttpStreamImplT *impl, cdx_uint8 *buf, int size)
{
    int ret;
    int readSize;

    if(impl->inflateBuffer == NULL)
    {
        impl->inflateBuffer = malloc(DECOMPRESS_BUF_LEN);
        if(impl->inflateBuffer == NULL)
        {
            CDX_LOGE("malloc failed.");
            return -1;
        }
    }

    if(impl->inflateStream.avail_in == 0)
    {
        if(impl->chunkedFlag)
        {
            readSize = CdxReadChunkedData(impl, impl->inflateBuffer, DECOMPRESS_BUF_LEN);
        }
        else
        {
            readSize = CdxStreamRead(impl->tcpStream, impl->inflateBuffer, DECOMPRESS_BUF_LEN);
        }
        if(readSize <= 0)
        {
            CDX_LOGD("==== size(%lld), pos(%lld), readSize(%d)",
                impl->totalSize, impl->bufPos, readSize);
            return readSize;
        }
        impl->inflateStream.next_in = impl->inflateBuffer;
        impl->inflateStream.avail_in = readSize;
    }

    impl->inflateStream.avail_out = size;
    impl->inflateStream.next_out = (cdx_uint8 *)buf;

    ret = inflate(&impl->inflateStream, Z_SYNC_FLUSH);
    if (ret != Z_OK && ret != Z_STREAM_END)
    {
        CDX_LOGD("inflate return: %d, %s; size: %d, %u", ret, impl->inflateStream.msg,
            size, impl->inflateStream.avail_out);
    }

    return size - impl->inflateStream.avail_out;
}
#endif
static cdx_int32 StreamRead(CdxHttpStreamImplT *impl, void *buf, cdx_uint32 len, cdx_bool *isEos)
{
    *isEos = 0;
    cdx_int32 size = 0;
    cdx_int32 wantToReadLen = 0;

    if(impl->chunkedFlag)
    {
#if __CONFIG_ZLIB
        if(impl->compressed)
        {
            size = StreamReadCompressed(impl, buf, len);
            if(size <= 0)
            {
                CDX_LOGD("======== size = %d", size);
                *isEos = 1;
                return 0;
            }

    #if __SAVE_BITSTREAMS
            fwrite((cdx_char *)buf, 1, size, impl->fp_http);
            CDX_LOGD("xxx readLen(%d)",size);
            fsync(fileno(impl->fp_http));
    #endif

            return size;
        }
#endif
        size = CdxReadChunkedData(impl, buf, len);
        return size;
    }
    else
    {

#if __CONFIG_ZLIB
        if(impl->compressed)
        {
            size = StreamReadCompressed(impl, buf, len);
            if(size <= 0)
            {
                CDX_LOGD("======== size = %d", size);
                *isEos = 1;
                return 0;
            }

#if __SAVE_BITSTREAMS
            fwrite((cdx_char *)buf, 1, size, impl->fp_http);
            CDX_LOGD("xxx readLen(%d)",size);
            fsync(fileno(impl->fp_http));
#endif
            return size;
        }
#endif

        if(impl->totalSize > 0)
        {

            CDX_LOGV("wantToReadLen (%u), totalSize(%d), bufPos(%d), len(%u)",
                                    wantToReadLen, (cdx_uint32)impl->totalSize,
                                    (cdx_uint32)impl->bufPos, len);

            if (impl->totalSize == impl->bufPos)
            {
                *isEos = 1;
                return 0;
            }
            else
            {
                wantToReadLen = (len < impl->totalSize - impl->bufPos) ?
                                len : (impl->totalSize - impl->bufPos);
            }

        }
        else
        {
            wantToReadLen = len;
        }

	int64_t stream_read1 = GetNowUs();
        size = CdxStreamRead(impl->tcpStream, (cdx_char *)buf, wantToReadLen);
	CDX_LOGV("wht>>>>>>>>>warning, stream read size = %d, wantToReadLen = %d", size, wantToReadLen);

	int64_t stream_read2 = GetNowUs();
		if(stream_read2 - stream_read1 > 15000)
		{
			logv("wht>>>Http, CdxStreamRead timeout:%lldms, read size = %d\n",(stream_read2 - stream_read1)/1000, size);
		}

        if(size < 0)
        {
            if(size == -2)
            {
                return size;
            }
            CDX_LOGE("xxx CdxStreamRead failed.");
            impl->ioState = CDX_IO_STATE_ERROR;
            goto err_out;
        }
        else if(size == 0)
        {
            CDX_LOGD("xxx readLen=0.");
            if(impl->totalSize != -1)
            {
                CDX_LOGE("xxx readLen=0, totalSize(%d), bufPos(%d).",
                    (int)impl->totalSize, (int)impl->bufPos);
                impl->ioState = CDX_IO_STATE_ERROR;
                goto err_out;
            }
            else
            {
                *isEos = 1;
                return 0;
            }
        }
        else if(size <= wantToReadLen)
        {
            if((impl->totalSize > 0) && (impl->bufPos + size == impl->totalSize))
            {
                CDX_LOGD("xxx EOS, impl->bufPos(%d), readLen(%d), totsize(%d)",
                    (cdx_int32)impl->bufPos, (cdx_int32)size, (cdx_int32)impl->totalSize);
                *isEos = 1;
            }
        }
#if __SAVE_BITSTREAMS
        fwrite((cdx_char *)buf, 1, size, impl->fp_http);
        CDX_LOGD("xxx readLen(%d)",size);
        fsync(fileno(impl->fp_http));
#endif
        return size;
    }

err_out:

#if NO_USE
    if(impl->httpDataBuffer)
    {
        Pfree(impl->pool, impl->httpDataBuffer);
        impl->httpDataBuffer = NULL;
    }
#endif

    return -1;
}

void *GetNetworkData(void *pArg)
{
    CdxHttpStreamImplT *impl;
    cdx_int32 readSize = CDX_HTTP_READ_SIZE;
    cdx_int32 restSize = 0;
    cdx_int32 ret;
    cdx_int64 startTime = 0;
    cdx_int64 endTime = 0;
    cdx_int64 totTime = 0;
    cdx_int64 lastNotifyTime = 0;

    CDX_LOGD("Start get network data.");
    impl = (CdxHttpStreamImplT *)pArg;
    CDX_FORCE_CHECK(impl);
    impl->getNetworkDataFlag = 1;
    impl->pauseReadDataFlag = -1;
    int callbackEndFlag = 0;

    while(1)
    {
        if(impl->exitFlag == 1)
        {
            CDX_LOGV("GetNetWorkData exit.");
            break;
        }

        PrintCacheState(1000, &lastNotifyTime, &impl->base);
        startTime = GetNowUs();

        if(CDX_IO_STATE_EOS == impl->ioState)
            //not exit GetNetworkData, continue work when seek back.
        {
            CDX_LOGV("EOS, usleep.");
            pthread_mutex_lock(&impl->pauseReadDataMutex);
            impl->pauseReadDataFlag = 1;
            pthread_cond_signal(&impl->pauseReadDataCond);
            pthread_mutex_unlock(&impl->pauseReadDataMutex);
#if CDX_BUF_STAT
            CdxBufStatIncCount(&http_stat.http_net_io_eos_cnt);
#endif
            usleep(10*1000);
            continue;
        }
        else if(CDX_IO_STATE_OK != impl->ioState)
        {
#if CDX_BUF_STAT
            CdxBufStatIncCount(&http_stat.http_net_io_not_ok_cnt);
#endif
            CDX_LOGW("impl->ioState(%d)", impl->ioState);
            usleep(10*1000);
        }

        if(impl->seekFlag == 1) //pause while seeking...
        {
            pthread_mutex_lock(&impl->pauseReadDataMutex);
            impl->pauseReadDataFlag = 1;
            pthread_cond_signal(&impl->pauseReadDataCond);
            pthread_mutex_unlock(&impl->pauseReadDataMutex);

            pthread_mutex_lock(&impl->seekMutex);
#if CDX_BUF_STAT
            CdxBufStatSetStartStamp(&http_stat.http_net_wait_seek_time);
#endif
            while(impl->seekFlag == 1)
            {
                pthread_cond_wait(&impl->seekCond, &impl->seekMutex);
            }
#if CDX_BUF_STAT
            CdxBufStatSetEndStamp(&http_stat.http_net_wait_seek_time);
            CdxBufStatIncProcTime(&http_stat.http_net_wait_seek_time);
#endif
            pthread_mutex_unlock(&impl->seekMutex);
            impl->pauseReadDataFlag = -1;
        }
#if (CONFIG_HTTP_STREAM_SET_PROTECT_AREA_SIZE || CONFIG_HTTP_STREAM_SUPPORT_FREE_BUFFER)
        if(impl->setBufferFlag == 1) //pause while changing buffer...
        {

	    int64_t set_buf1 = GetNowUs();

            pthread_mutex_lock(&impl->pauseReadDataMutex);
            impl->pauseReadDataFlag = 1;
            pthread_cond_signal(&impl->pauseReadDataCond);
            pthread_mutex_unlock(&impl->pauseReadDataMutex);

            pthread_mutex_lock(&impl->setBufferMutex);
            while(impl->setBufferFlag == 1)
            {
                pthread_cond_wait(&impl->setBufferCond, &impl->setBufferMutex);
            }
            pthread_mutex_unlock(&impl->setBufferMutex);
            impl->pauseReadDataFlag = -1;
		int64_t set_buf2 = GetNowUs();
		if(set_buf2 - set_buf1 > 15000)
		{
			logw("wht>>>Http, set buffer timeout:%lldms\n",(set_buf2 - set_buf1)/1000);
		}

        }
#endif
        //buffer is too full
        if(impl->maxBufSize - impl->validDataSize - impl->maxProtectAreaSize < CDX_HTTP_READ_SIZE)
        {
            impl->bufFullFlag = 1;
#if CDX_BUF_STAT
            CdxBufStatIncCount(&http_stat.http_net_buftoofull_cnt);
#endif
            usleep(10*1000);
            endTime = GetNowUs();
            totTime = endTime - startTime;
            TransferMeasurement(impl, 0, totTime);
            continue;
        }

        if(impl->bufWritePtr < impl->bufReadPtr)
        {
            restSize = impl->maxBufSize - impl->validDataSize - impl->maxProtectAreaSize;
        }
        else
        {
            restSize = impl->bufEndPtr - impl->bufWritePtr + 1;
        }
        if(restSize >= readSize)
        {
            restSize = readSize;
        }

        if(impl->ioState == CDX_IO_STATE_OK) //after seek may io error or eos.
        {
            cdx_bool isEos = 0;
#if CDX_BUF_STAT
            CdxBufStatSetStartStamp(&http_stat.http_net_read_time);
#endif
            logv("wht>>>>>>debug, go to read data from network");
	int64_t read_time1 = GetNowUs();
            ret = StreamRead(impl, impl->bufWritePtr, restSize, &isEos);//read data from network.

	int64_t read_time2 = GetNowUs();
		if(read_time2 - read_time1 > 15000)
		{
			logv("wht>>>Http, StreamRead timeout:%lldms\n",(read_time2 - read_time1)/1000);
		}

#if CDX_BUF_STAT
            CdxBufStatSetEndStamp(&http_stat.http_net_read_time);
            CdxBufStatIncProcTime(&http_stat.http_net_read_time);
#endif
            if(ret < 0)
            {
#if CDX_BUF_STAT
                CdxBufStatIncCount(&http_stat.http_net_read_fails);
#endif
                if(ret == -2)
                {
                    endTime = GetNowUs();
                    totTime = endTime - startTime;
                    TransferMeasurement(impl, 0, totTime);
                    continue;
                }
                //CDX_LOGW("Break GetNetworkData thread, ret(%d).", ret);
                //break;

                if(impl->ioState == CDX_IO_STATE_ERROR)
                {
                    CDX_LOGW("Io error, wait for reconnecting ok...");
#if CDX_BUF_STAT
                    CdxBufStatSetStartStamp(&http_stat.http_net_io_error_time);
#endif
                    int sleep_time_ms = 0;
                    while(impl->ioState != CDX_IO_STATE_OK)
                    {
                        usleep(10*1000);
                        sleep_time_ms += 10;
                        if(impl->forceStopFlag == 1 || impl->seekFlag == 1) //*
                        {
                            CDX_LOGW("break reconnecting.");
                            CDX_LOGE("break: %d, %d", impl->forceStopFlag, impl->seekFlag);
                            break;
                        }
                        if(sleep_time_ms>(20*1000)){
                            if( (impl->setBufferFlag == 1) && (impl->pauseReadDataFlag==-1) ){
                                CDX_LOGE("dead pthread_cond_wait happened, force break");
                                break;
                            }
                        }
                    }
#if CDX_BUF_STAT
                    CdxBufStatSetEndStamp(&http_stat.http_net_io_error_time);
                    CdxBufStatIncProcTime(&http_stat.http_net_io_error_time);
#endif
                }
                //CDX_LOGD("reconnect ok, now start download data again...");
                endTime = GetNowUs();
                totTime = endTime - startTime;
                TransferMeasurement(impl, 0, totTime);
                continue;
            }

#if CDX_BUF_STAT
            CdxBufStatIncStat(&http_stat.http_net_read_request_size, restSize);
            CdxBufStatIncStat(&http_stat.http_net_read_return_size, ret);
#endif
            endTime = GetNowUs();
            totTime = (endTime - startTime);
            TransferMeasurement(impl, ret, totTime);

            pthread_mutex_lock(&impl->bufferMutex);
#if CONFIG_HTTP_STREAM_IGNORE_DATA_SEEK
            /* for reconnect */
            if (impl->ignoreDataSize > 0)
            {
                if (impl->ignoreDataSize < ret)
                {
                    logd("readPos: %d, bufPos: %d, ignore: %d, bufReadPtr: %d, \n"
                        "bufWritePtr: %d, validDataSize: %d, protectAreaSize: %d\n"
                        "buffer:%d, bufEndPtr: %d",
                        (uint32_t)impl->readPos, (uint32_t)impl->bufPos, (uint32_t)impl->ignoreDataSize,
                        (uint32_t)impl->bufReadPtr, (uint32_t)impl->bufWritePtr,
                        (uint32_t)impl->validDataSize, (uint32_t)impl->protectAreaSize,
                        (uint32_t)impl->buffer, (uint32_t)impl->bufEndPtr);

                    cdx_int64 afterIgnoreSize = ret - impl->ignoreDataSize;
                    impl->validDataSize += afterIgnoreSize;
                    impl->bufWritePtr   += ret;
                    impl->bufReadPtr    += impl->ignoreDataSize;

                    logd("readPos: %d, bufPos: %d, ignore: %d, bufReadPtr: %d, \n"
                        "bufWritePtr: %d, validDataSize: %d, protectAreaSize: %d",
                        (uint32_t)impl->readPos, (uint32_t)(impl->bufPos + ret),
                        (uint32_t)impl->ignoreDataSize,
                        (uint32_t)impl->bufReadPtr, (uint32_t)impl->bufWritePtr,
                        (uint32_t)impl->validDataSize, (uint32_t)impl->protectAreaSize);

                    impl->ignoreDataSize = 0;
                }
                else
                    impl->ignoreDataSize -= ret;
            }
            else
            {
                impl->validDataSize += ret;
                impl->bufWritePtr   += ret;
            }
#else
            impl->validDataSize += ret;
            impl->bufWritePtr   += ret;
#endif
            impl->bufPos        += ret;
            if(isEos)
            {
                impl->ioState = CDX_IO_STATE_EOS;
            }

            if(CDX_IO_STATE_EOS == impl->ioState)
            {
                impl->downloadEnd = GetNowUs();
                impl->downloadTimeMs = (impl->downloadEnd - impl->downloadStart) / 1000;
                if(impl->callback && callbackEndFlag == 0)
                {
                    CDX_LOGV("eos set STREAM_EVT_DOWNLOAD_END impl->downloadTimeMs(%lld ms)",
                        impl->downloadTimeMs);
                    ExtraDataContainerT httpExtradata;
                    httpExtradata.extraData = &(impl->downloadTimeMs);
                    httpExtradata.extraDataType = EXTRA_DATA_HTTP;
                    impl->callback(impl->pUserData, STREAM_EVT_DOWNLOAD_END, &httpExtradata);

#if defined(CONF_YUNOS)
                    impl->downloadLastTime = GetNowUs();//Ali YUNOS invoke info
                    impl->callback(impl->pUserData, STREAM_EVT_DOWNLOAD_END_TIME,
                        &(impl->downloadLastTime));
#endif
                    callbackEndFlag = 1;
                }
            }
            if(impl->bufWritePtr > impl->bufEndPtr)
            {
                impl->bufWritePtr -= impl->maxBufSize;
            }

            impl->bufFullFlag = 0;
            totTime = 0;
            pthread_mutex_unlock(&impl->bufferMutex);
        }
    }

    impl->getNetworkDataFlag = 0;
    pthread_mutex_lock(&impl->pauseReadDataMutex);
    impl->pauseReadDataFlag = 1;
    pthread_cond_signal(&impl->pauseReadDataCond);
    pthread_mutex_unlock(&impl->pauseReadDataMutex);

    return NULL;
}

static void *StartGetNetworkDataThread(void *pArg)
{
    CdxHttpStreamImplT *impl;

    impl = (CdxHttpStreamImplT *)pArg;
    CDX_FORCE_CHECK(impl);

    pthread_mutex_lock(&impl->lock);
    impl->ioState = CDX_IO_STATE_OK;
    pthread_cond_signal(&impl->cond);
    pthread_mutex_unlock(&impl->lock);
    //start get network data.
    GetNetworkData(impl);

#if CDX_THREAD_STACK_SIZE_DEBUG
    _info("StartGetNetworkDataThread stack left: %d\n", OS_ThreadGetStackMinFreeSize(NULL));
#endif

    return NULL;
}

static cdx_int32 __CdxHttpStreamConnect(CdxStreamT *stream)
{
    CdxHttpStreamImplT *impl;
    cdx_int32 result;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, CdxHttpStreamImplT, base);

    pthread_mutex_lock(&impl->lock);
    if(impl->forceStopFlag)
    {
        pthread_mutex_unlock(&impl->lock);
        return -1;
    }
    impl->state = HTTP_STREAM_CONNECTING;
    pthread_mutex_unlock(&impl->lock);


#if defined(CONF_YUNOS)
    impl->downloadStart = GetNowUs();
    if(impl->callback)
    {
        //Ali YUNOS invoke info
        impl->callback(impl->pUserData, STREAM_EVT_DOWNLOAD_START_TIME, &(impl->downloadStart));
    }
#endif

    result = CdxHttpStreamingStart(impl, impl->baseOffset);
    if(result < 0)
    {
        CDX_LOGE("Start http stream failed.");
        impl->ioState = CDX_IO_STATE_ERROR;
        goto __exit;
    }

    impl->downloadStart = GetNowUs();
    if(impl->callback)
    {
        logv("========================= http callback STREAM_EVT_DOWNLOAD_START");
        impl->callback(impl->pUserData, STREAM_EVT_DOWNLOAD_START, NULL);
    }

#if defined(CONF_YUNOS)
    if(impl->tcpStream)
    {
        //Ali YUNOS invoke info
        CdxStreamControl(impl->tcpStream, STREAM_CMD_GET_IP, (void*)(impl->tcpIp));
        if(impl->callback)
        {
            impl->callback(impl->pUserData, STREAM_EVT_DOWNLOAD_GET_TCP_IP, impl->tcpIp);
        }
    }
#endif

extern int g_http_thread_stack_size;
    pthread_attr_t attr;
    //attr.stack_size = g_http_thread_stack_size;
    //attr.ulpthreadAttrStorage = ((4096)<<16);
    //printf("attr.ulpthreadAttrStorage = %u\n",attr.ulpthreadAttrStorage);
    //result = pthread_create(&impl->threadId, &attr, StartGetNetworkDataThread, (void *)impl);
    pthread_attr_init(&attr);
    struct sched_param sched;
    sched.sched_priority = TMALLPLAYER_DEFAULT_PRIORITY;
    pthread_attr_setschedparam(&attr,&sched);
    pthread_attr_setstacksize(&attr,TMALLPLAYER_NORMAL_STACKSIZE);
    result = pthread_create(&impl->threadId, &attr, StartGetNetworkDataThread, (void *)impl);
    if (result || !impl->threadId)
    {
        CDX_LOGE("create thread error!");
        impl->ioState = CDX_IO_STATE_ERROR;
        goto __exit;
    }

    pthread_setname_np(impl->threadId, "Http");

    if (impl->noProbeData) {
        impl->probeData.buf = NULL;
        impl->probeData.len = 0;
    } else {
        result = CdxGetProbeData(impl);
        if(result < 0)
        {
            CDX_LOGE("get probe data failed.");
            goto __exit;
        }
        if(impl->callback){
            int flag = DETAIL_INFO_STREAM_GET_PROBE_DATA;
            //loge("RPT: STREAM_EVT_DETAIL_INFO, cb:%08lx, flag:%d", impl->callback, flag);
            impl->callback(impl->pUserData, STREAM_EVT_DETAIL_INFO, &flag);
        }
    }

__exit:
    pthread_mutex_lock(&impl->lock);
    impl->state = HTTP_STREAM_IDLE;
    pthread_mutex_unlock(&impl->lock);
    pthread_cond_signal(&impl->cond);
    return (impl->ioState == CDX_IO_STATE_ERROR || impl->forceStopFlag == 1) ? -1 : 0;
}

static const struct CdxStreamOpsS CdxHttpStreamOps =
{
    .connect      = __CdxHttpStreamConnect,
    .getProbeData = __CdxHttpStreamGetProbeData,
    .read         = __CdxHttpStreamRead,
    .write        = __CdxHttpStreamWrite,
    .close        = __CdxHttpStreamClose,
    .getIOState   = __CdxHttpStreamGetIOState,
    .attribute    = __CdxHttpStreamAttribute,
    .control      = __CdxHttpStreamControl,

    .getMetaData  = __CdxHttpStreamGetMetaData,
    .seek         = __CdxHttpStreamSeek,
    .seekToTime   = NULL,
    .eos          = __CdxHttpStreamEos,
    .tell         = __CdxHttpStreamTell,
    .size         = __CdxHttpStreamSize,
//    .forceStop    = __CdxHttpStreamForceStop

};

static CdxHttpStreamImplT *CreateHttpStreamImpl(void)
{
    CdxHttpStreamImplT *impl = NULL;

    impl = (CdxHttpStreamImplT *)malloc(sizeof(CdxHttpStreamImplT));
    if(!impl)
    {
        CDX_LOGE("malloc failed, size(%u)", (unsigned int)sizeof(CdxHttpStreamImplT));
        return NULL;
    }
    memset(impl, 0x00, sizeof(CdxHttpStreamImplT));
    impl->base.ops = &CdxHttpStreamOps;

    return impl;
}
static void DestroyHttpStreamImpl(CdxHttpStreamImplT *impl)
{
    if(impl)
    {
        free(impl);
    }
    return;
}

CdxStreamT *__CdxHttpStreamCreate(CdxDataSourceT *source)
{
    CdxHttpStreamImplT *impl = NULL;
    CdxUrlT* url = NULL;
    cdx_int32 result;
    AwPoolT *pool;

    CDX_LOGD("source uri:(%s)", source->uri);

    impl = CreateHttpStreamImpl();
    if(NULL == impl)
    {
        CDX_LOGE("CreateHttpStreamImpl failed.");
        return NULL;
    }

#if __SAVE_BITSTREAMS
    sprintf(impl->location, "/data/camera/http_stream_%d.es", streamIndx++);
    impl->fp_http = fopen(impl->location,"wb");
#endif

    pool = AwPoolCreate(NULL);
    if(pool == NULL)
    {
        CDX_LOGE("pool is NULL.");
        DestroyHttpStreamImpl(impl);
        return NULL;
    }

    impl->pool = pool;

    url = CdxUrlNew(source->uri);
    if(url == NULL)
    {
        CDX_LOGE("CdxUrlNew failed.");
        goto err_out;
    }
    impl->url = url;

    result = SetDataSourceFields(source, impl);
    if(result < 0)
    {
        CDX_LOGE("Set datasource failed.");
        goto err_out;
    }

    impl->baseOffset = source->offset > 0 ? source->offset : 0;
    impl->ua = GetUA(impl->nHttpHeaderSize, impl->pHttpHeader);//for ua
    impl->ioState = CDX_IO_STATE_INVALID;

#if 1
    impl->maxBufSize = source->bufferSize;
    impl->maxProtectAreaSize = source->protectSize;
    if( (source->bufferSize < (CDX_HTTP_READ_SIZE + source->protectSize + PROBE_DATA_LEN_DEFAULT) )
      ||(source->bufferSize < (CDX_HTTP_READ_SIZE + source->protectSize + CDX_HTTP_THRESHOLD) )
    ){
        impl->maxBufSize = MAX_STREAM_BUF_SIZE;
        impl->maxProtectAreaSize = PROTECT_AREA_SIZE;
    }
    CDX_LOGD("impl->maxBufSize = %lld, impl->maxProtectAreaSize = %d, source->bufferSize = %lld, source->protectSize = %d", impl->maxBufSize, impl->maxProtectAreaSize, source->bufferSize, source->protectSize);
#else
    if ((source->bufferSize < CDX_HTTP_THRESHOLD) || (source->protectSize < 0)) {
        impl->maxBufSize = MAX_STREAM_BUF_SIZE;
        impl->maxProtectAreaSize = PROTECT_AREA_SIZE;
    } else {
        impl->maxBufSize = source->bufferSize;
        impl->maxProtectAreaSize = source->protectSize;
    }
    CDX_LOGD("source->bufferSize = %d", source->bufferSize);
#endif

#ifdef HTTP_STREAM_BUFFER_SET_ENABLE
    CDX_LOGE("!!!!! MAX_STREAM_BUF_SIZE: %d, CDX_HTTP_THRESHOLD: %d !!!!\n", (int)impl->maxBufSize, CDX_HTTP_THRESHOLD);
#endif
    impl->buffer = (char*)Palloc(impl->pool, (int)impl->maxBufSize);
    if(impl->buffer == NULL)
    {
        CDX_LOGE("Palloc buffer failed.");
        goto err_out;
    }

    impl->bufPos    = 0;
    impl->bufEndPtr = impl->buffer + impl->maxBufSize -1;
    impl->bufWritePtr = impl->buffer;
    impl->bufReadPtr  = impl->buffer;
    impl->validDataSize = 0;
    impl->protectAreaSize = 0;
    impl->protectAreaPos = 0;
    impl->isDTMB = CDX_FALSE;
    impl->enoughData = CDX_FALSE;

    if (strstr(url->file,"aw_dtmb_http.ts"))
    {
        impl->isDTMB = CDX_TRUE;
        CDX_LOGD("It is a dtmb stream!");
    }

    pthread_mutex_init(&impl->bufferMutex, NULL);
    pthread_cond_init(&impl->bufferCond, NULL);
    pthread_mutex_init(&impl->seekMutex, NULL);
    pthread_cond_init(&impl->seekCond, NULL);
    pthread_mutex_init(&impl->pauseReadDataMutex, NULL);
    pthread_cond_init(&impl->pauseReadDataCond, NULL);

#if (CONFIG_HTTP_STREAM_SET_PROTECT_AREA_SIZE || CONFIG_HTTP_STREAM_SUPPORT_FREE_BUFFER)
    pthread_mutex_init(&impl->setBufferMutex, NULL);
    pthread_cond_init(&impl->setBufferCond, NULL);
#endif

    pthread_mutex_init(&impl->lock, NULL);
    pthread_cond_init(&impl->cond, NULL);
    impl->state = HTTP_STREAM_IDLE;

    CDX_LOGV("http stream open.");
    return &impl->base;

err_out:
    if(impl != NULL)
    {
        if (impl->certificate)
        {
            Pfree(impl->pool, impl->certificate);
            impl->certificate = NULL;
        }
        if(impl->url)
        {
            CdxUrlFree(impl->url);
            impl->url = NULL;
        }
        if(impl->buffer)
        {
            Pfree(impl->pool, impl->buffer);
            impl->buffer = NULL;
        }

        ClearDataSourceFields(impl);

        free(impl);
        AwPoolDestroy(pool);
    }
    return NULL;
}

const CdxStreamCreatorT httpStreamCtor =
{
    .create = __CdxHttpStreamCreate
};

