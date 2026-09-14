#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <debug.h>
#include "doResample.h"

static int do_resample(Resampler * res)
{
	int tempsamp = 0,temptotalsamp = 0;

	while(res->AMX.InputB->PcmLen)
	{
		do_AuMIX(&res->AMX);
		tempsamp = res->AMX.MixLen;// * 2 * res->AMX.Mixch;
		temptotalsamp += tempsamp;
		res->AMX.InputA->PCMPtr += res->AMX.MixLen * res->AMX.Mixch;
	}
	return temptotalsamp;
}

static int do_prepare(Resampler * res, ResCfg* cfg)
{
	res->AMX.InputB->SampleRate = cfg->insrt;
	res->AMX.InputB->Chan       = cfg->inch;
	res->AMX.InputB->PCMPtr     = (short*)cfg->inbuf;
	res->AMX.InputB->PcmLen     = cfg->samples;
	res->AMX.InputA->PCMPtr = (short*)res->tempResampleBuffer;//(pAudioDecoder->ADCedarCtx.pTempResampleBuffer + (inch==1?TEMP_RESAMPLE_BUFFER_SIZE/2:0));
	res->AMX.InputA->SampleRate = cfg->outsrt;
	res->AMX.InputA->Chan = res->AMX.InputB->Chan;
	res->AMX.InputA->PcmLen = (cfg->outsrt * res->AMX.InputB->PcmLen)/cfg->insrt + 1;
	res->AMX.ByPassflag = 2;
	return 0;
}

static int do_getData(Resampler * res, void* outbuf, unsigned int bufsize)
{
	memcpy(outbuf, res->tempResampleBuffer, bufsize);
	return 0;
}

Resampler* Creat_Resampler()
{
	Resampler * res = NULL;
	res = (Resampler*)malloc(sizeof(Resampler));
	if(!res)
	{
		_err("Error no mem for Reasampler constructing!\n");
		return NULL;
	}
	memset(res, 0x00, sizeof(Resampler));
	res->AMX.InputA = (PcmInfo*)malloc(sizeof(PcmInfo));
	if(!res->AMX.InputA)
	{
		_err("Error no mem for Reasampler InputA constructing!\n");
		if(res){
			free(res);
			res = NULL;
		}
		return NULL;
	}
	memset(res->AMX.InputA, 0x00, sizeof(PcmInfo));
	res->AMX.InputB = (PcmInfo*)malloc(sizeof(PcmInfo));
	if(!res->AMX.InputB)
	{
		_err("Error no mem for Reasampler InputB constructing!\n");
		if(res->AMX.InputA)
		{
			free(res->AMX.InputA);
			res->AMX.InputA = NULL;
		}
		if(res){
			free(res);
			res = NULL;
		}
		return NULL;
	}
	memset(res->AMX.InputB, 0x00, sizeof(PcmInfo));
	res->AMX.Output = (PcmInfo*)malloc(sizeof(PcmInfo));
	if(!res->AMX.Output)
	{
		_err("Error no mem for Reasampler Output constructing!\n");
		if(res->AMX.InputA)
		{
			free(res->AMX.InputA);
			res->AMX.InputA = NULL;
		}
		if(res->AMX.InputB)
		{
			free(res->AMX.InputB);
			res->AMX.InputB = NULL;
		}
		if(res){
			free(res);
			res = NULL;
		}
		return NULL;
	}
	memset(res->AMX.Output, 0x00, sizeof(PcmInfo));
	res->tempResampleBuffer = malloc(TEMP_RESAMPLE_BUFFER_SIZE);
	if(!res->tempResampleBuffer)
	{
		_err("Error no mem for Reasampler Output constructing!\n");
		if(res->AMX.InputA)
		{
			free(res->AMX.InputA);
			res->AMX.InputA = NULL;
		}
		if(res->AMX.InputB)
		{
			free(res->AMX.InputB);
			res->AMX.InputB = NULL;
		}
		if(res->AMX.Output)
		{
			free(res->AMX.Output);
			res->AMX.Output = NULL;
		}
		if(res){
			free(res);
			res = NULL;
		}
		return NULL;
	}
	res->AMX.RESI = Init_ResampleInfo();
	if(!res->AMX.RESI)
	{
		_err("Error no mem for Init_ResampleInfo!\n");
		if(res->AMX.InputA)
		{
			free(res->AMX.InputA);
			res->AMX.InputA = NULL;
		}
		if(res->AMX.InputB)
		{
			free(res->AMX.InputB);
			res->AMX.InputB = NULL;
		}
		if(res->AMX.Output)
		{
			free(res->AMX.Output);
			res->AMX.Output = NULL;
		}
		if(res->tempResampleBuffer)
		{
			free(res->tempResampleBuffer);
			res->tempResampleBuffer = NULL;
		}
		if(res){
			free(res);
			res = NULL;
		}
		return NULL;
	}
	res->prepare  = do_prepare;
	res->process  = do_resample;
	res->getData  = do_getData;
    return res;
}

int Destroy_Resampler(Resampler * res)
{
    if(!res)
    {
        _err("Destroy_Resampler fail!\n");
    	return -1;
    }
	if(res->AMX.InputA)
	{
		free(res->AMX.InputA);
		res->AMX.InputA = NULL;
	}
	if(res->AMX.InputB)
	{
		free(res->AMX.InputB);
		res->AMX.InputB = NULL;
	}
	if(res->AMX.Output)
	{
		free(res->AMX.Output);
		res->AMX.Output = NULL;
	}
	Destroy_ResampleInfo(res);
	if(res->tempResampleBuffer)
	{
		free(res->tempResampleBuffer);
		res->tempResampleBuffer = NULL;
	}
	if(res){
		free(res);
		res = NULL;
	}
	return 0;
}
