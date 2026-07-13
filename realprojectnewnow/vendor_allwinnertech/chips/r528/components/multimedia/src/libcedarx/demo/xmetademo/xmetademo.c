/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : xmetademo.c
 * Description : xmetademo
 * History :
 *
 */


#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <ctype.h>
#include <errno.h>

#include "cdx_config.h"
#include <cdx_log.h>
#include "xmetadataretriever.h"
#include "CdxTypes.h"
#include "CdxTime.h"
#include <debug.h>

//* the main method.
int main(int argc, char** argv)
{
    CEDARX_UNUSE(argc);
    CEDARX_UNUSE(argv);
    int ret;

    _info("\n");
    _info("****************************************************************************\n");
    _info("* This program implements a simple player,");
    _info("* you can type commands to control the player.\n");
    _info("* To show what commands supported, type 'help'.\n");
    _info("* Inplemented by Allwinner ALD-AL3 department.\n");
    _info("***************************************************************************\n");

    if(argc < 2)
    {
        _info("Usage:\n");
        _info("demoretriver filename \n");
        return -1;
    }

    cdx_int64 start = CdxMonoTimeUs();

    XRetriever* demoRetriver;
    demoRetriver = XRetrieverCreate();

    if(NULL == demoRetriver)
    {
        _info("create failed\n");
        return -1;
    }

    ret = XRetrieverSetDataSource(demoRetriver, argv[1], NULL);
    if(ret < 0)
    {
        _info("set datasource failed\n");
        return -1;
    }
    _info("XRetrieverSetDataSource end");

    int width;
    XRetrieverGetMetaData(demoRetriver, METADATA_VIDEO_WIDTH, &width);

    int height;
    XRetrieverGetMetaData(demoRetriver, METADATA_VIDEO_HEIGHT, &height);

      int duration;
    XRetrieverGetMetaData(demoRetriver, METADATA_DURATION, &duration);

    _info("get metadata: w(%d), h(%d), duration(%d)\n", width, height, duration);

    XVideoFrame* videoFrame = NULL;
    videoFrame = XRetrieverGetFrameAtTime(demoRetriver, 0, 0);

    (void)videoFrame;
    XRetrieverDestory(demoRetriver);

    cdx_int64 end = CdxMonoTimeUs();
    _info("total need cost time is %lldms\n",(end - start)/1000);

    _info("\n");
    _info("*************************************************************************\n");
    _info("* Quit the program, goodbye!\n");
    _info("********************************************************************\n");
    _info("\n");

    return 0;
}
