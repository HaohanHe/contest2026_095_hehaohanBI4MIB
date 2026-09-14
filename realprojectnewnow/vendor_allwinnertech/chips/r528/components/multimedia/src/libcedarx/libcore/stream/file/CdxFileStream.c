/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxFileStream.c
 * Description : File Stream Definition
 * History :
 *
 */

#define LOG_SOURCE_FILE 0

#ifndef _LARGEFILE64_SOURCE
//* defined this macro for using flag O_LARGEFILE in open() method, see 'man 2 open'
#define _LARGEFILE64_SOURCE
#endif
#define _FILE_OFFSET_BITS 64

#include <CdxStream.h>
#include <CdxAtomic.h>
#include <CdxMemory.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <CdxDebug.h>
#include <cdx_log.h>

#define FILE_STREAM_SCHEME "file://"
#define FD_STREAM_SCHEME "fd://"
#define DEFAULT_PROBE_DATA_LEN (1024 * 128)

#include <sys/time.h>
static long long GetNowUs(void)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    return now.tv_sec * 1000000 + now.tv_usec;
}

//extern _off64_t lseek64 (int __fd, _off64_t __offset, int __whence);
#if 0
extern uint32_t vfs_lseek(int32_t fd, int64_t offset, int32_t whence);
extern int vfs_dup(int oldfd);
extern int32_t vfs_read(int32_t fd, void *buf, uint32_t nbytes);
extern int32_t vfs_open(const char *path, int32_t flags);
extern int32_t vfs_close(int32_t fd);
#define cdxfopen64(uri) vfs_open(uri, O_RDONLY) //O_LARGEFILE

#define cdxfseek64(fd, offset, whence) vfs_lseek(fd, offset, whence)

#define cdxftell64(fd) vfs_lseek(fd, 0, SEEK_CUR)

//#define cdxfread64(fd, buf, len) read(fd, buf, len)
#define cdxfread64(fd, buf, len) vfs_read(fd, buf, len)

//#define feof64(fd) vfs_lseek(fd, 0, SEEK_CUR)

#define cdxfclose64(fd) vfs_close(fd)
#else
#if 1
#define cdxfopen64(uri) open(uri, O_RDONLY)

//#define cdxfseek64(fd, offset, whence) lseek64(fd, offset, whence)
#define cdxfseek64(fd, offset, whence) lseek(fd, offset, whence)

//#define cdxftell64(fd) lseek64(fd, 0, SEEK_CUR)
#define cdxftell64(fd) lseek(fd, 0, SEEK_CUR)

#define cdxfread64(fd, buf, len) read(fd, buf, len)

//#define feof64(fd) lseek64(fd, 0, SEEK_CUR)

#define cdxfclose64(fd) close(fd)
#else
#define cdxfopen64(uri) fopen(uri, "wb")

//#define cdxfseek64(fd, offset, whence) lseek64(fd, offset, whence)
#define cdxfseek64(fd, offset, whence) fseek(fd, offset, whence)

//#define cdxftell64(fd) lseek64(fd, 0, SEEK_CUR)
#define cdxftell64(fd) fseek(fd, 0, SEEK_CUR)

#define cdxfread64(fd, buf, len) fread(buf, 1, len, fd)

//#define feof64(fd) lseek64(fd, 0, SEEK_CUR)

#define cdxfclose64(fd) fclose(fd)

#endif
#endif
enum FileStreamStateE
{
    FILE_STREAM_IDLE = 0x00L,
    FILE_STREAM_READING = 0x01L,
    FILE_STREAM_SEEKING = 0x02L,
    FILE_STREAM_CLOSING = 0x03L,
    FILE_STREAM_WRITING = 0x04L,
};

/*fmt: "file://xxx" */
struct CdxFileStreamImplS
{
    CdxStreamT base;
    cdx_atomic_t state;
    CdxStreamProbeDataT probeData;
    cdx_int32 ioErr;

    //int fd;
    FILE *fd;
    cdx_int64 offset;
    cdx_int64 size;

    /*when datasource uri is fd, then will try to get the absolute path like "/mnt/..." */
    char *redriectPath;
    char *filePath;
    cdx_int32 parser_init;
    /*fatfs realization*/
    //FIL file;
};

static inline cdx_int32 WaitIdleAndSetState(cdx_atomic_t *state, cdx_ssize val)
{
    cdx_int32 timeout = 0xFFFFFFF;

    while (!CdxAtomicCAS(state, FILE_STREAM_IDLE, val) || !(timeout--))
    {
        if (CdxAtomicRead(state) == FILE_STREAM_CLOSING)
        {
            CDX_LOGW("file is closing.");
            return CDX_FAILURE;
        }
        logd("dead loop wait");
    }
    return timeout > 0 ? CDX_SUCCESS : CDX_FAILURE;
}

static CdxStreamProbeDataT *__FileStreamGetProbeData(CdxStreamT *stream)
{
    struct CdxFileStreamImplS *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxFileStreamImplS, base);

    return &impl->probeData;
}

void *test_cpfile(void *arg);

static cdx_int32 __FileStreamRead(CdxStreamT *stream, void *buf, cdx_uint32 len)
{
    struct CdxFileStreamImplS *impl;
    cdx_int32 ret;
    //FRESULT res;
    cdx_int64 nHadReadLen;
    CDX_ENTRY();

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxFileStreamImplS, base);

    //* we must limit the HadReadLen within impl->size,
    //* or in some case will be wrong, such as cts
    //nHadReadLen = f_tell(&impl->file) - impl->offset;
    nHadReadLen = cdxftell64((int)impl->fd) - impl->offset;
    if(nHadReadLen >= impl->size)
    {
        CDX_LOGD("eos, pos(%lld)",impl->size);
        return 0;
    }

    if (WaitIdleAndSetState(&impl->state, FILE_STREAM_READING) != CDX_SUCCESS)
    {
        CDX_LOGE("set state(%d) fail.", CdxAtomicRead(&impl->state));
        return -1;
    }

    if((nHadReadLen + len) > impl->size)
    {
        len = impl->size - nHadReadLen;
    }
    /*
    res = f_read(&impl->file, buf, len, (cdx_uint32 *)&ret);
    if (res != FR_OK ) {
        CDX_LOGE("ret(%d), fresult(%d), cur pos:(%lld), impl->size(%lld)",
                 ret, res, f_tell(&impl->file) - impl->offset, impl->size);
        impl->ioErr = CDX_IO_STATE_ERROR;
        ret = -1;
        goto exit;
    }
    */
#if LOG_SOURCE_FILE
    log_file("0:/music/STREAM.MP3", buf, ret);
#endif
    /*
    if ((cdx_uint32)ret < len)
    {
        if ((f_tell(&impl->file) - impl->offset) == impl->size) //end of file
        {
            CDX_LOGD("eos, ret(%d), pos(%lld)...", ret, impl->size);
            impl->ioErr = CDX_IO_STATE_EOS;
        }
        else
        {
            impl->ioErr = (cdx_int32)res;
            CDX_LOGE("ret(%d), fresult(%d), cur pos:(%lld), impl->size(%lld)",
                    ret, impl->ioErr, f_tell(&impl->file) - impl->offset, impl->size);
        }
    }
    */
      
	int64_t t1 = GetNowUs();
    ret = cdxfread64((int)impl->fd, buf, len);
	int64_t t2 = GetNowUs();

	if(ret >2048)
	{
	logv("wht>>>>>>>>debug, file read timediff = %lld ms, ret =%d", (t2-t1)/1000, ret);
	}

    if (ret < (cdx_int32)len)
    {
        if ((cdxftell64((int)impl->fd) - impl->offset) == impl->size) /*end of file*/
        {
            CDX_LOGD("eos, ret(%d), pos(%lld)...", ret, impl->size);
            impl->ioErr = CDX_IO_STATE_EOS;
        }
        else
        {
            impl->ioErr = errno;
            CDX_LOGE("ret(%d), errno(%d), cur pos:(%lld), impl->size(%lld)",
                    ret, impl->ioErr, cdxftell64((int)impl->fd) - impl->offset, impl->size);
        }
    }
    
exit:
    CdxAtomicSet(&impl->state, FILE_STREAM_IDLE);
    CDX_EXIT(ret);
    return ret;
}

static cdx_int32 __FileStreamClose(CdxStreamT *stream)
{
    struct CdxFileStreamImplS *impl;
    cdx_int32 ret;

    CDX_ENTRY();
    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxFileStreamImplS, base);

    ret = WaitIdleAndSetState(&impl->state, FILE_STREAM_CLOSING);

    CDX_FORCE_CHECK(CDX_SUCCESS == ret);

    //* the fd may be invalid when close, such as in TF-card test
    //ret = f_close(&impl->file);
    ret = cdxfclose64((int)impl->fd);
    if(ret != 0)
    {
        logw(" close fd may be not normal, ret = %d, errno = %d",ret,errno);
    }

    if (impl->probeData.buf)
    {
        CdxFree(impl->probeData.buf);
        impl->probeData.buf = NULL;
    }
    if (impl->filePath)
    {
        CdxFree(impl->filePath);
        impl->filePath = NULL;
    }
    if(impl->redriectPath)
    {
        CdxFree(impl->redriectPath);
        impl->redriectPath = NULL;
    }
    CdxFree(impl);
    // TODO: use refence
    CDX_EXIT(CDX_SUCCESS);
    return CDX_SUCCESS;
}

static cdx_int32 __FileStreamGetIoState(CdxStreamT *stream)
{
    struct CdxFileStreamImplS *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxFileStreamImplS, base);

    return impl->ioErr;
}

static cdx_uint32 __FileStreamAttribute(CdxStreamT *stream)
{
    CDX_UNUSE(stream);
    return CDX_STREAM_FLAG_SEEK;
}

static cdx_int32 __FileStreamControl(CdxStreamT *stream, cdx_int32 cmd, void *param)
{

    struct CdxFileStreamImplS *impl;
    cdx_int64* value = NULL;
    CDX_UNUSE(param);
    CDX_UNUSE(impl);

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxFileStreamImplS, base);
    switch (cmd)
    {
        case STREAM_CMD_SET_RELATIVE_START_POS:
            value = (cdx_int64*)param;
            impl->offset = (*value);
            break;
        case STREAM_CMD_SET_RELATIVE_FILE_SIZE:
            value = (cdx_int64*)param;
            impl->size = (cdx_int64)(*value);
            break;
        case STREAM_CMD_NEXT_PROBE_DATA:
            {
                //FRESULT res;
                cdx_uint32 ret;
                impl->probeData.len = DEFAULT_PROBE_DATA_LEN;
                //res = f_read(&impl->file, impl->probeData.buf, impl->probeData.len, (cdx_uint32 *)&ret);
                //CDX_LOG_CHECK(res == FR_OK, "f_read failed in file stream connect, fresult(%d)", res);
                ret = cdxfread64((int)impl->fd, impl->probeData.buf, impl->probeData.len);
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
                    CdxFree(impl->probeData.buf);
                    impl->probeData.buf = NULL;
                    impl->probeData.len= 0;
                }
                return CDX_SUCCESS;
            }
        case STREAM_CMD_SET_PARSER_INIT:
        {
            impl->parser_init = 1;
            return 0;
        }
        case STREAM_CMD_GET_PARSER_INIT:
        {
            return impl->parser_init;
        }
        default :
            break;
    }

    return CDX_SUCCESS;
}

static cdx_int32 __FileStreamSeek(CdxStreamT *stream, cdx_int64 offset, cdx_int32 whence)
{
    struct CdxFileStreamImplS *impl;
    cdx_int64 ret = 0;

    CDX_ENTRY();

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxFileStreamImplS, base);

    if (WaitIdleAndSetState(&impl->state, FILE_STREAM_SEEKING) != CDX_SUCCESS)
    {
        CDX_LOGE("set state(%d) fail.", CdxAtomicRead(&impl->state));
        impl->ioErr = CDX_IO_STATE_INVALID;
        return -1;
    }
    switch (whence)
    {
    case STREAM_SEEK_SET:
    {
        if (offset < 0 || offset > impl->size)
        {
            CDX_LOGE("invalid arguments, offset(%lld), size(%lld)", offset, impl->size);
            //CdxDumpThreadStack((pthread_t)gettid());
            CdxAtomicSet(&impl->state, FILE_STREAM_IDLE);
            return -1;
        }
        //ret = f_lseek(&impl->file, impl->offset + offset);
        ret = cdxfseek64((int)impl->fd, impl->offset + offset, SEEK_SET);
        break;
    }
    case STREAM_SEEK_CUR:
    {
        //cdx_int64 curPos = f_tell(&impl->file) - impl->offset;
        cdx_int64 curPos = cdxftell64((int)impl->fd) - impl->offset;
        if (curPos + offset < 0 || curPos + offset > impl->size)
        {
            CDX_LOGE("invalid arguments, offset(%lld), size(%lld), curPos(%lld)",
                     offset, impl->size, curPos);
            //CdxDumpThreadStack((pthread_t)gettid());
            CdxAtomicSet(&impl->state, FILE_STREAM_IDLE);
            return -1;
        }
        //ret = f_lseek(&impl->file, offset + f_tell(&impl->file));
        ret = cdxfseek64((int)impl->fd, offset, SEEK_CUR);
        break;
    }
    case STREAM_SEEK_END:
    {
        cdx_int64 absOffset = impl->offset + impl->size + offset;
        if (absOffset < impl->offset || absOffset > impl->offset + impl->size)
        {
            CDX_LOGE("invalid arguments, offset(%lld), size(%lld)",
                     absOffset, impl->offset + impl->size);
            //CdxDumpThreadStack((pthread_t)gettid());
            CdxAtomicSet(&impl->state, FILE_STREAM_IDLE);
            return -1;
        }
        //ret = f_lseek(&impl->file, absOffset);
        ret = cdxfseek64((int)impl->fd, absOffset, SEEK_SET);
        break;
    }
    default :
        CDX_CHECK(0);
        break;
    }

    if (ret < 0)
    {
        impl->ioErr = errno;
        CDX_LOGE("seek failure, io error(%d); 'whence(%d), base-offset(%lld), offset(%lld)' ",
                 impl->ioErr, whence, impl->offset, offset);
    }

    CdxAtomicSet(&impl->state, FILE_STREAM_IDLE);
    CDX_EXIT((ret >= 0 ? 0 : -1));
    return (ret >= 0 ? 0 : -1);
}

static cdx_int64 __FileStreamTell(CdxStreamT *stream)
{
    struct CdxFileStreamImplS *impl;
    cdx_int64 pos;

    CDX_ENTRY();

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxFileStreamImplS, base);
    //pos = f_tell(&impl->file) - impl->offset;
    pos = cdxftell64((int)impl->fd) - impl->offset;
    if (-1 == pos)
    {
        impl->ioErr = errno;
        CDX_LOGE("ftello failure, io error(%d)", impl->ioErr);
    }

    CDX_EXIT(pos);
    return pos;
}

static cdx_bool __FileStreamEos(CdxStreamT *stream)
{
    struct CdxFileStreamImplS *impl;
    cdx_int64 pos = -1;

    CDX_ENTRY();

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxFileStreamImplS, base);
    //pos = f_tell(&impl->file) - impl->offset;
    pos = cdxftell64((int)impl->fd) - impl->offset;
    CDX_LOGD("(%lld / %lld / %lld)", pos, impl->offset, impl->size);

    CDX_EXIT(pos == impl->size);
    return (pos == impl->size);
}

static cdx_int64 __FileStreamSize(CdxStreamT *stream)
{
    struct CdxFileStreamImplS *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxFileStreamImplS, base);

    return impl->size;
}

static cdx_int32 __FileStreamGetMetaData(CdxStreamT *stream, const cdx_char *key, void **pVal)
{
    struct CdxFileStreamImplS *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxFileStreamImplS, base);

    if (strcmp(key, "uri") == 0)
    {
        *pVal = impl->filePath;
        return 0;
    }

    CDX_LOGW("key(%s) not found...", key);
    return -1;
}

/*
void test_seg_read_file_speed(FIL *file, unsigned int segSize)
{
    FRESULT res;
    unsigned int rdSize;
    long long start;
    unsigned int size = 0;
    unsigned int tick1, tick2, time;
    unsigned char *buf;

    (void)time;

    if((buf = malloc(segSize)) == NULL)
        return;

    start = f_tell(file);

    tick1 = OS_GetTicks();

    do {
        if ((res = f_read(file, buf, segSize, &rdSize)) != FR_OK) {
            loge("read file failed: %d", res);
            goto out;
        }
        size += rdSize;
        log_file("0:/music/TEST.MP3", buf, rdSize);
        msleep(20);
        printf("test addr: %d\n", f_tell(file));
    } while (rdSize == segSize);

    tick2 = OS_GetTicks();
    time = OS_TicksToMSecs(tick2 - tick1);

    loge("rdTime: %d, rdSize: %d, speed: %d bytes/ms", time, size, size / time);
out:
    free(buf);

    if ((res = (cdx_int32)f_lseek(file, start)) != FR_OK) {
        loge("seek file failed: %d", res);
        return;
    }
}

void *test_cpfile(void *arg)
{
    FIL file;
    FRESULT res;

    res = f_open(&file, "0:/music/HeyMa.mp3", FA_READ | FA_OPEN_EXISTING);
    if (res != FR_OK) {
        CDX_LOGE("open file failure, fresult(%d)", res);
        return NULL;
    }

    test_seg_read_file_speed(&file, 1024);

    f_close(&file);
    return NULL;
}

*/

/*
cdx_int32 __FileStreamConnect(CdxStreamT *stream)
{
    cdx_int32 ret = 0;
    FRESULT res;
    struct CdxFileStreamImplS *impl;

    CDX_ENTRY();

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxFileStreamImplS, base);

    if (strncmp(impl->filePath, FILE_STREAM_SCHEME, 7) == 0) //file://... 
    {
        // open a file
        res = f_open(&impl->file, impl->filePath + 7, FA_READ | FA_OPEN_EXISTING);
        if (res != FR_OK) {
            CDX_LOGE("open file failure, fresult(%d)", res);
            ret = -1;
            goto failure;
        }
        logd("f_open success");

        // cal file size
#if !ID3_IOT_IMPLEMENT
        impl->offset = 0;
#endif
        impl->size = f_size(&impl->file) - impl->offset;
        logd("    *************impl->size=%d",     (int)impl->size);
    }
    else
    {
        CDX_LOG_CHECK(0, "uri(%s) not file stream...", impl->filePath);
    }

    CdxAtomicSet(&impl->state, FILE_STREAM_IDLE);
    impl->probeData.buf = CdxMalloc(DEFAULT_PROBE_DATA_LEN);    // BUFFER too large, but if set too little i think
    impl->probeData.len = DEFAULT_PROBE_DATA_LEN;                // the parser will go wrong. notice the parser probe.
    impl->ioErr = CDX_SUCCESS;
    CDX_LOG_CHECK(impl->probeData.buf != 0, "CdxMalloc failed, buf = 0x%x", (uint32_t)impl->probeData.buf);

    // if data not enough, only probe 'size' data 
    if (impl->size > 0 && impl->size < DEFAULT_PROBE_DATA_LEN)
    {
        CDX_LOGW("File too small, size(%lld), will read all for probe...", impl->size);
        impl->probeData.len = impl->size;
    }

    logd("seek to %d\n", (int)impl->offset);
    res = (cdx_int32)f_lseek(&impl->file, impl->offset);
    if (res != FR_OK)
    {
        CDX_LOGW("io fail fresult(%d)", res);
        ret = -1;
        goto failure;
    }

    res = f_read(&impl->file, impl->probeData.buf, impl->probeData.len, (cdx_uint32 *)&ret);
    CDX_LOG_CHECK(res == FR_OK, "f_read failed in file stream connect, fresult(%d)", res);
    if ((cdx_uint32)ret < impl->probeData.len)
    {
        CDX_LOGW("io fail, file end");
        ret = -1;
        goto failure;
    }
    ret = 0;

    res = (cdx_int32)f_lseek(&impl->file, impl->offset);
    if (res != FR_OK)
    {
        CDX_LOGW("io fail fresult(%d)", res);
        ret = -1;
        goto failure;
    }

    impl->ioErr = 0;

    CDX_EXIT(ret);
    return ret;

failure:
logd("failed! exit return %d", ret);
    return ret;
}
*/
cdx_int32 __FileStreamConnect(CdxStreamT *stream)
{
    cdx_int32 ret = 0;
    struct CdxFileStreamImplS *impl;

    CDX_CHECK(stream);
    impl = CdxContainerOf(stream, struct CdxFileStreamImplS, base);
#if 1
    if (strncmp(impl->filePath, FILE_STREAM_SCHEME, 7) == 0) /*file://... */
    {
        impl->fd = (FILE*)cdxfopen64(impl->filePath+7);
        logd("open %s file finish,fd = %d",impl->filePath+7,(int)impl->fd);
        if (impl->fd <= 0)
        {
            CDX_LOGE("open file failure, errno(%d)", errno);
            ret = -1;
            goto failure;
        }

        impl->offset = 0;
        impl->size = cdxfseek64((int)impl->fd, 0, SEEK_END);
        logd("    *************impl->size=%lld",     impl->size);
        ret = (cdx_int32)cdxfseek64((int)impl->fd, 0, SEEK_SET);
        CDX_LOG_CHECK(ret == 0, "errno(%d)", errno);
        if(impl->filePath)
        {
            free(impl->filePath);
            impl->filePath = NULL;
        }
        cdx_char  newPath[5120] = {0};
        cdx_int64 fileOffset = 0;
        ret = sprintf(newPath, "fd://%d?offset=%lld&length=%lld", (int)impl->fd, fileOffset, impl->size);
        impl->filePath = strdup(newPath);
        logd("newPath=%s", impl->filePath);

    }
    else
    {
        CDX_LOG_CHECK(0, "uri(%s) not file stream...", impl->filePath);
    }
#else

        //impl->fd = cdxfopen64(impl->filePath+7);
        impl->fd = fopen("/data/test_an.avi", "wb");

	logd("open %s file finish,fd = %d",impl->filePath, impl->fd);
        if (impl->fd <= 0)
        {
            CDX_LOGE("open file failure, errno(%d)", errno);
            ret = -1;
            goto failure;
        }

	impl->offset = 0;
        //impl->size = cdxfseek64(impl->fd, 0, SEEK_END);
        impl->size = fseek(impl->fd, 0, SEEK_END);
        logd("    *************impl->size=%lld",     impl->size);
        //ret = (cdx_int32)cdxfseek64(impl->fd, 0, SEEK_SET);
        ret = (cdx_int32)fseek(impl->fd, 0, SEEK_SET);
#endif
    CdxAtomicSet(&impl->state, FILE_STREAM_IDLE);
    impl->probeData.buf = CdxMalloc(DEFAULT_PROBE_DATA_LEN);
    impl->probeData.len = DEFAULT_PROBE_DATA_LEN;
    impl->ioErr = CDX_SUCCESS;

    /* if data not enough, only probe 'size' data */
    if (impl->size > 0 && impl->size < DEFAULT_PROBE_DATA_LEN)
    {
        CDX_LOGW("File too small, size(%lld), will read all for probe...", impl->size);
        impl->probeData.len = impl->size;
    }
    ret = cdxfread64((int)impl->fd, impl->probeData.buf, impl->probeData.len);
    if (ret < (int)impl->probeData.len)
    {
        CDX_LOGW("io fail, errno=%d", errno);
        ret = -1;
        goto failure;
    }

    //CDX_BUF_DUMP(impl->probeData.buf, 16);

    ret = (cdx_int32)cdxfseek64((int)impl->fd, impl->offset, SEEK_SET);
    if (-1 == ret)
    {
        CDX_LOGW("io fail errno(%d)", errno);
        ret = -1;
        goto failure;
    }

    impl->ioErr = 0;
    return ret;

failure:
    logd("failed! exit return %d", ret);
    return ret;

}


static const struct CdxStreamOpsS fileStreamOps =
{
    .connect = __FileStreamConnect,
    .getProbeData = __FileStreamGetProbeData,
    .read = __FileStreamRead,
    .write = NULL,
    .close = __FileStreamClose,
    .getIOState = __FileStreamGetIoState,
    .attribute = __FileStreamAttribute,
    .control = __FileStreamControl,
    .getMetaData = __FileStreamGetMetaData,
    .seek = __FileStreamSeek,
    .seekToTime = NULL,
    .eos = __FileStreamEos,
    .tell = __FileStreamTell,
    .size = __FileStreamSize,
};

static CdxStreamT *__FileStreamCreate(CdxDataSourceT *source)
{
    struct CdxFileStreamImplS *impl;

    impl = CdxMalloc(sizeof(*impl));

    CDX_FORCE_CHECK(impl);
    memset(impl, 0x00, sizeof(*impl));

    impl->base.ops = &fileStreamOps;
    impl->filePath = CdxStrdup(source->uri);
    impl->ioErr = -1;
#if ID3_IOT_IMPLEMENT
    /* for id3 implement */
    impl->offset = source->offset;
    logd("source->offset: %d", (int)source->offset);
#endif
    CDX_LOGD("local file '%s'", source->uri);

    return &impl->base;
}

const CdxStreamCreatorT fileStreamCtor =
{
    .create = __FileStreamCreate
};

