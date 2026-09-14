

/*
    20090209  1.修改A通道增益改变时，用outbuf做缓存，以防止改变A buffer原来的值
              2.修正B通道增益值需要复制到temp buffer中的 preamp 

*/
#include "audiomix.h"
#include "alib_typedef.h"
#include "alib_mem.h"
#include "alib_utils.h"
#include "alib_log.h"

#ifdef  LOG_TAG
#undef  LOG_TAG
#define LOG_TAG "Allwinner Audio Resample"
#endif

#ifdef  ALIB_DEBUG
#undef  ALIB_DEBUG
#define ALIB_DEBUG 1
#endif

#include "asemble.h"

int    DebugCount =0;

/*
#define    floorFLOAT2FIXbit1(x_in,y_out)      y_out=x_in>>FLOAT2FIXbit1;    \
                        if(y_out<0)\
                        {\
                            x_in = x_in&FLOAT2FIXbit1MASK;\
                            if(x_in)\
                                y_out--;\
                        }

*/

#define    floorFLOAT2FIXbit1(x_in,y_out)      y_out=x_in>>FLOAT2FIXbit1
#define        MAXDB  20
#define        AMPFLOAT2INTBITS 16
void PreAMP(short *inbuf,short *outbuf,unsigned int PcmLen, int AMPVALUE);
int preampvalue[2*MAXDB+1] = 
{
    0x0000028f,0x00000339,0x0000040e,0x0000051b,0x0000066e,0x00000818,0x00000a31,0x00000cd4,
    0x00001027,0x00001455,0x00001999,0x0000203a,0x00002892,0x00003314,0x0000404d,0x000050f4,
    0x000065ea,0x0000804d,0x0000a186,0x0000cb59,0x00010000,0x00014248,0x000195bb,0x0001fec9,
    0x0002830a,0x0003298b,0x0003fb27,0x0005030a,0x00064f40,0x0007f17a,0x000a0000,0x000c96d9,
    0x000fd953,0x0013f3df,0x00191e6d,0x001f9f6e,0x0027cf8b,0x00321e64,0x003f1882,0x004f6ecd,
    0x00640000
};
void* Init_ResampleInfo(void)
{
    ResampleInfo* ri = NULL;
    ri = (ResampleInfo*)malloc(sizeof(ResampleInfo));
    if(ri == NULL)
        return NULL;
    memset(ri, 0x00, sizeof(ResampleInfo));
    return (void*)ri;
}

void Destroy_ResampleInfo(void * ri)
{
    ResampleInfo* pri = (ResampleInfo*)ri;
    if(pri == NULL)
        return;
    free(pri);
    alib_logd("Destroy_ResampleInfo...");
}

int do_AuMIX(AudioMix    *AMX)
{
/*
notice: 1.以A采样率为resample
2.Output，TempBuf buffer长度需以A长度为准
3.pcm sample must 16bits
    */    
    int ret = 1;
    switch(AMX->ByPassflag)
    {
    case 0:
        {
            AMX->TempBuf->SampleRate = AMX->InputA->SampleRate;
            AMX->TempBuf->PcmLen = AMX->InputA->PcmLen;
            AudioResample(AMX->InputB,AMX->TempBuf, AMX->RESI);
#ifdef    DebugOutResample
            fwrite(AMX->TempBuf->PCMPtr,2*AMX->TempBuf->Chan,AMX->TempBuf->PcmLen,outputResample);
#endif
#if 1
            ////setp 2: mix pcm
            AMX->MixbufPtr = AMX->Output->PCMPtr;
            AMX->Mixch =AMX->InputA->Chan;
            AMX->MixLen = AudioMixFuc(AMX->InputA,AMX->TempBuf,AMX->Output);
#endif    
            break;
        }
    case 1:
        {    
            if(AMX->InputA->preamp)
            {    
                if(AMX->InputA->preamp>MAXDB)
                    AMX->InputA->preamp = MAXDB;
                if(AMX->InputA->preamp<-MAXDB)
                    AMX->InputA->preamp = -MAXDB;
                PreAMP(AMX->InputA->PCMPtr,AMX->Output->PCMPtr,AMX->InputA->PcmLen*AMX->InputA->Chan,preampvalue[AMX->InputA->preamp+MAXDB]);
            }
            else
            memcpy(AMX->Output->PCMPtr,AMX->InputA->PCMPtr,2*AMX->InputA->Chan*AMX->InputA->PcmLen);
            ////setp 2: mix pcm
            AMX->MixbufPtr = AMX->Output->PCMPtr;
            AMX->Mixch =AMX->InputA->Chan;
            AMX->MixLen = AMX->InputA->PcmLen;
            break;
        }
    case 2://resample only
        {
            AudioResample(AMX->InputB,AMX->InputA,AMX->RESI);
#ifdef    DebugOutResample
            fwrite(AMX->TempBuf->PCMPtr,2*AMX->TempBuf->Chan,AMX->TempBuf->PcmLen,outputResample);
#endif
            AMX->MixbufPtr = AMX->InputA->PCMPtr;
            AMX->Mixch =AMX->InputA->Chan;
            AMX->MixLen =AMX->InputA->PcmLen;
            break;
        }
    default:
        ret = 0;//
    }
    return ret;
}
#if 0
int do_AuMIX(AudioMix    *AMX)
{
/*
notice: 1.以A采样率为resample
2.Output，TempBuf buffer长度需以A长度为准
3.pcm sample must 16bits
    */    
    /////////////////////////////input B for mixx        ///////////
    if(AMX->ByPassflag == 1)  //by pass true
    {
        memcpy(AMX->Output->PCMPtr,AMX->InputA->PCMPtr,2*AMX->InputA->Chan*AMX->InputA->PcmLen);
        return 1;
    }
    /////////////////////////////input B for mixx        ///////////
    ////STEP 1: resampler        
    if(AMX->ByPassflag == 2)  //resample only
    {
        AudioResample(AMX->InputB,AMX->InputA);
#ifdef    DebugOutResample
        fwrite(AMX->TempBuf->PCMPtr,2*AMX->TempBuf->Chan,AMX->TempBuf->PcmLen,outputResample);
#endif
        AMX->MixbufPtr = AMX->InputA;
        AMX->Mixch =AMX->InputA->Chan;
        AMX->MixLen =AMX->InputA->PcmLen;
        return 1;
    }else 
    {
        AMX->TempBuf->SampleRate = AMX->InputA->SampleRate;
        AMX->TempBuf->PcmLen = AMX->InputA->PcmLen;
        AudioResample(AMX->InputB,AMX->TempBuf);
#ifdef    DebugOutResample
        fwrite(AMX->TempBuf->PCMPtr,2*AMX->TempBuf->Chan,AMX->TempBuf->PcmLen,outputResample);
#endif
#if 1
        ////setp 2: mix pcm
        AMX->MixbufPtr = AMX->Output->PCMPtr;
        AMX->Mixch =AMX->InputA->Chan;
        AMX->MixLen = AudioMixFuc(AMX->InputA,AMX->TempBuf,AMX->Output);
#endif        
    }
    ///setp 3: out pcm        
    return 1;
}
#endif
short clip(int x)
{
    signed int value=x;
        if(value>0x7fff)
            value=0x7fff;
        else if(value<(signed int)0xffff8000)
            value=0xffff8000;
//        if(value!=0)
//            value = value;
    return (short)value;

}

void PreAMP(short *inbuf,short *outbuf,unsigned int PcmLen, int AMPVALUE)
{
    unsigned int i;//=PcmLen;
    int temp;
    int temp3;
    short temp2;
    for(i=0;i<PcmLen;i++)
    {    
        temp3 = inbuf[i];
//        if(temp3!=0)
//            temp3 = temp3;
        temp = temp3*AMPVALUE;
        temp2 =  clip(temp >>AMPFLOAT2INTBITS);
        outbuf[i] = temp2;
    }
    
}

int    AudioMixFuc(PcmInfo *InputA,PcmInfo *InputB,PcmInfo *Output) 
{
    int MixLen,i;
    short *Aptr = InputA->PCMPtr;
    short *Bptr = InputB->PCMPtr;
    short *Outptr = Output->PCMPtr;
    int    MixDataOut,temp1,temp2;
    short temp3;
    MixLen=Min(InputA->PcmLen,InputB->PcmLen);
    ///////////////////////////////amp
    if(InputA->preamp)
    {    
        if(InputA->preamp>MAXDB)
            InputA->preamp = MAXDB;
        if(InputA->preamp<-MAXDB)
            InputA->preamp = -MAXDB;

        PreAMP(InputA->PCMPtr,Output->PCMPtr,MixLen*InputA->Chan,preampvalue[InputA->preamp+MAXDB]);
#ifdef    DebugOutAMP
        fwrite(Output->PCMPtr,2*InputA->Chan,MixLen,outampe);
#endif
        Aptr = Output->PCMPtr;
    }
    
    if(InputB->preamp)
    {
        if(InputB->preamp>MAXDB)
            InputB->preamp = MAXDB;
        if(InputB->preamp<-MAXDB)
            InputB->preamp = -MAXDB;
        PreAMP(InputB->PCMPtr,InputB->PCMPtr,MixLen*InputB->Chan,preampvalue[InputB->preamp+MAXDB]);
    }
    
    
    switch(InputA->Chan)
    {
    case 1:
        if(InputB->Chan == 1)
        {
            for(i=0;i<MixLen;i++)
            {
                MixDataOut = *Aptr++ + *Bptr++;
                *Outptr++ =(short)clip(MixDataOut);
            }
        }
        else if(InputB->Chan == 2)
            
        {
            for(i=0;i<MixLen;i++)
            {
                temp1 = *Bptr++;
                temp2 = *Bptr++;
                temp1 = (temp1+temp2)>>1;
                MixDataOut = *Aptr++ + temp1;
                *Outptr++ =(short)clip(MixDataOut);
            }
            
        }
        break;
    case 2:
        if(InputB->Chan == 1)
        {
            for(i=0;i<MixLen;i++)
            {
                temp3 = *Bptr++;
                
                MixDataOut = *Aptr++ + temp3;//*Bptr;
                *Outptr++ =(short)clip(MixDataOut);
                MixDataOut = *Aptr++ + temp3;//*Bptr++;
                *Outptr++ =(short)clip(MixDataOut);
            }
        }
        else if(InputB->Chan == 2)
            
        {
            for(i=0;i<MixLen;i++)
            {
                MixDataOut = *Aptr++ + *Bptr++;
                *Outptr++ =(short)clip(MixDataOut);
                MixDataOut = *Aptr++ + *Bptr++;
                *Outptr++ =(short)clip(MixDataOut);
            }
            
        }
        break;
    default:
        break;
        
    }
    InputA->PcmLen -= MixLen;
    InputB->PcmLen -= MixLen;
    //InputA->PCMPtr = Aptr;
    InputA->PCMPtr += MixLen*InputA->Chan;//20090209
    InputB->PCMPtr = Bptr;
    if(Output->preamp)
    {
        if(Output->preamp>MAXDB)
            Output->preamp = MAXDB;
        if(Output->preamp<-MAXDB)
            Output->preamp = -MAXDB;
        PreAMP(Output->PCMPtr,Output->PCMPtr,MixLen*InputA->Chan,preampvalue[Output->preamp+MAXDB]);
    }
    Output->Chan = InputA->Chan;
    Output->PcmLen = MixLen;
    return MixLen;
}   

#include "math.h"
#define       PI                      3.14159265358979323846
/* resampling via FIR filter, blackman window */
float blackman(float x,float fcn,int l)
{
  /* This algorithm from:
SIGNAL PROCESSING ALGORITHMS IN FORTRAN AND C
S.D. Stearns and R.A. David, Prentice-Hall, 1992
  */
  float bkwn,x2;
  float wcn = (float)(PI * fcn);
  
  x /= l;
  if (x<0) x=0;
  if (x>1) x=1;
  x2 = (float)(x - 0.5);

  bkwn = (float)(0.42 - 0.5*cos(2*x*PI)  + 0.08*cos(4*x*PI));
  if (fabs(x2)<1e-9) return (float)(wcn/PI);
  else 
    return  (float)(  bkwn*sin(l*wcn*x2)  / (PI*l*x2)  );


}

/* gcd - greatest common divisor */
/* Joint work of Euclid and M. Hendry */

int gcd ( int i, int j )
{
    return j ? gcd(j, i % j) : i;
}


/* copy in new samples from in_buffer into mfbuf, with resampling
   if necessary.  n_in = number of samples from the input buffer that
   were used.  n_out = number of samples copied into mfbuf  */
int AudioResample(PcmInfo *Input,PcmInfo *Output, ResampleInfo* RESI)
{
    float offset,resample_ratiotemp;
#ifdef    FIXPOINT
    int xvalue,xvalue2;
#else
    float xvalue,xvalue2;
#endif
    int  offsetINT;
    int i,j=0,k,m;
    int    desired_len = 6*(int)Input->PcmLen+10;
    int BLACKSIZE;
    int filter_l;
    //float fcn,intratio;
    int bpc;   /* number of convolution functions to pre-compute */
    
    short *inbuf_old;
    int num_used;
    int samp_frac_num = 0;
    int int_advance = 0;
    int frac_advance = 0;
    int den_rate =0;
//    int    Tlent;
    desired_len = Output->PcmLen; 

    if(Output->SampleRate==Input->SampleRate)
    {
        num_used = Min((int)Input->PcmLen,desired_len);
        memcpy(Output->PCMPtr,Input->PCMPtr,2*Input->Chan*num_used);
        Input->PcmLen -= num_used;
        Input->PCMPtr += num_used*Input->Chan;//
        Output->PcmLen = num_used;
        Output->Chan = Input->Chan;
        Output->preamp = Input->preamp;
        return num_used; 
        
    }
    
    if (RESI->fill_buffer_resample_init == 1 ) 
    {
        if((RESI->old_Infs != Input->SampleRate) ||(RESI->old_Outfs != Output->SampleRate))
            RESI->fill_buffer_resample_init = 0;
    }
    
    if ( RESI->fill_buffer_resample_init == 0 ) {
        
        RESI->old_Infs = Input->SampleRate;
        RESI->old_Outfs = Output->SampleRate;
        
        RESI->bpc = Output->SampleRate/gcd(Output->SampleRate,Input->SampleRate);
        RESI->den_rate  = RESI->bpc;
        RESI->num_rate  = Input->SampleRate/gcd(Output->SampleRate,Input->SampleRate);
        if (RESI->bpc>BPC) RESI->bpc = BPC;
        resample_ratiotemp = (float)Input->SampleRate/Output->SampleRate;
        RESI->resample_ratio =(int) ((alib_int64)Input->SampleRate*pow(2,FLOAT2FIXbit1)/Output->SampleRate); //MAX IS 6 20.12
        RESI->intratio=( fabs(resample_ratiotemp - floor(.5+resample_ratiotemp)) < .0001 );
        RESI->fcn = 1.00/resample_ratiotemp;
        if (RESI->fcn>1.00) RESI->fcn=1.00;
        RESI->filter_l = FILTERTAP;// 7;//31; //filter_l = gfp->quality < 7 ? 31 : 7;
        if (0==RESI->filter_l % 2 ) --RESI->filter_l;/* must be odd */
        RESI->filter_l += RESI->intratio;            /* unless resample_ratio=int, it must be even */
        RESI->BLACKSIZE = RESI->filter_l+1;  /* size of data needed for FIR */
        memset(RESI->inbuf_old,0,sizeof(RESI->inbuf_old));
        //RESI.numin  = 0;
        //RESI.numout = 0;
        RESI->int_advance = Input->SampleRate/Output->SampleRate;
        RESI->frac_advance = RESI->num_rate%RESI->den_rate;
        RESI->last_sample = 0;
        RESI->samp_frac_num = 0;
        RESI->bpcint = 2.0*RESI->bpc*(1<<(32-BPCINT))/RESI->den_rate;//2*(0-1)
        


        //RESI.itime=0;
        //RESI.scale =0.95;
        /* precompute blackman filter coefficients */
        for ( j = 0; j <= 2*RESI->bpc; j++ ) {
            float sum = 0.; 
            offset = (j-RESI->bpc) / (2.*RESI->bpc);
            for ( i = 0; i <= RESI->filter_l; i++ ) 
                sum += RESI->blackfilt[j][i]  = blackman(i-offset,RESI->fcn,RESI->filter_l);
            for ( i = 0; i <= RESI->filter_l; i++ ) 
            {
                RESI->blackfilt[j][i] /= sum;
#ifdef    FIXPOINT
                if(RESI->blackfilt[j][i] == 1.0)
                    RESI->blackfiltINT[j][i] = (int)(0x7fffffff);
                else if(RESI->blackfilt[j][i]<=-1.0)
                    RESI->blackfiltINT[j][i] = 0x80000000;
                //if(RESI.blackfilt[j][i] >0)
                    //RESI.blackfiltINT[j][i] = RESI.blackfilt[j][i]*0x7fffffff;
                else
                    RESI->blackfiltINT[j][i] = RESI->blackfilt[j][i]*2*(0x1<<30);

                
                if(RESI->blackfilt[j][i]>1)
                    RESI->blackfilt[j][i] = RESI->blackfilt[j][i];
                //RESI.blackfiltINT[j][i] = float2int(RESI.blackfilt[j][i]);
#endif
            }
        }
        RESI->fill_buffer_resample_init = 1;
    }
    samp_frac_num = RESI->samp_frac_num;
    int_advance = RESI->int_advance;
    frac_advance = RESI->frac_advance;
    den_rate = RESI->den_rate;
    
    bpc = RESI->bpc;   /* number of convolution functions to pre-compute */
    BLACKSIZE = RESI->BLACKSIZE;
    filter_l =RESI->filter_l;
    
    inbuf_old=RESI->inbuf_old;
/*    
    if(RESI.scale!=0&&RESI.scale!=1)
    {
        Tlent = Input->PcmLen*Input->Chan;
        for(i=0;i<Tlent;i++)
        {    
            Input->PCMPtr[i] *=    RESI.scale;
        }
    }
*/    
#ifdef    bufoldcpy_
    {
        short *cpyptr;
            cpyptr=Input->PCMPtr-BLACKSIZE*Input->Chan;
            for (i=0;i<BLACKSIZE*Input->Chan;i++)
                *cpyptr++ = inbuf_old[i];
    }
#endif

    /* time of j'th element in inbuf = itime + j/ifreq; */
    /* time of k'th element in outbuf   =  j/ofreq */
    for (k=0,j=RESI->last_sample;k<desired_len;k++,j+=int_advance) { //max is 8k to 48k 6
        //int time0;
        int joff;
        
        //time0 = k*RESI.resample_ratio;     //20.12  /* time of k'th output sample */
        //j=floor(time0 -RESI.itime);
        //m = time0 -RESI.itime;
        //floorFLOAT2FIXbit1(m,j);
/*
        j=m>>FLOAT2FIXbit1;
        if(j<0)
        {
            m = m&FLOAT2FIXbit1MASK;
            if(m)
                j--;
        }
*/        
        /* check if we need more input data */
        if ((filter_l + j - filter_l/2) >= (int)Input->PcmLen) break;
        
        /* blackman filter.  by default, window centered at j+.5(filter_l%2) */
        /* but we want a window centered at time0.   */
        //offset = ( time0 -RESI.itime - (j + .5*(filter_l%2)));
        //offsetINT = ( time0 -RESI.itime - ((j<<FLOAT2FIXbit1) + ((1<<(FLOAT2FIXbit1-1))*(filter_l&0x1))));
        
        /* find the closest precomputed window for this offset: */
        //joff = floor((offset*2*bpc) + bpc +.5);

        //m = (offsetINT*2*bpc)+ (bpc<<FLOAT2FIXbit1) +(1<<(FLOAT2FIXbit1-1));
        //floorFLOAT2FIXbit1(m,joff);
        joff = MULSHIFT32(samp_frac_num<<BPCINT,RESI->bpcint);

#ifdef    bufoldcpy_
        if(Input->Chan == 1)
        {
            
            int j2 = j-filter_l/2;
            xvalue = 0.;
            for (i=0 ; i<=filter_l ; ++i) {
                short y;
                y = Input->PCMPtr[j2++];
#ifdef    FIXPOINT
        #ifdef    asm
                xvalue += MULSHIFT32(y,RESI->blackfiltINT[joff][i])<<1;
        #else
                xvalue += ((alib_int64)y*RESI->blackfiltINT[joff][i])>>31;
        #endif
#else
                xvalue += y*RESI->blackfilt[joff][i];
#endif
            }
            Output->PCMPtr[k]=(short)STAURATE16((int)xvalue);
        }else
        {
            int j2 = j-filter_l/2;
            xvalue = 0.;
            xvalue2 = 0.;
            for (i=0 ; i<=filter_l ; ++i) {
                short y,y2;
                y = Input->PCMPtr[2*j2];
                y2 = Input->PCMPtr[2*j2+1];
#ifdef    FIXPOINT
        #ifdef    asm
                xvalue += MULSHIFT32(y,RESI->blackfiltINT[joff][i])<<1;
                xvalue2 += MULSHIFT32(y2,RESI->blackfiltINT[joff][i])<<1;
        #else
                xvalue += ((alib_int64)y*RESI->blackfiltINT[joff][i])>>31;
                xvalue2 += ((alib_int64)y2*RESI->blackfiltINT[joff][i])>>31;
        #endif
#else
                xvalue += y*RESI->blackfilt[joff][i];
                xvalue2 += y2*RESI->blackfilt[joff][i];
#endif
            }
            Output->PCMPtr[2*k]=(short)STAURATE16((int)xvalue);
            Output->PCMPtr[2*k+1]=(short)STAURATE16((int)xvalue2);

#else

        if(Input->Chan == 1)
        {
            
            int j2 = j-filter_l/2;
            xvalue = 0;

            for (i=0 ; i<=filter_l ; ++i,j2++) {
                
                int y;
                y = (j2<0) ? inbuf_old[BLACKSIZE+j2] : Input->PCMPtr[j2];
#ifdef    FIXPOINT
        #ifdef    asm
                xvalue += MULSHIFT32(y<<9,RESI->blackfiltINT[joff][i]);
        #else
                xvalue += ((alib_int64)y*RESI->blackfiltINT[joff][i])>>23;
        #endif
#else
                xvalue += y*RESI->blackfilt[joff][i];
#endif
            }
            Output->PCMPtr[k]=(short)STAURATE16((int)xvalue/(1<<8));
        }else
        {
            int j2 = j-filter_l/2;
            xvalue = 0;
            xvalue2 = 0;
            for (i=0 ; i<=filter_l ; ++i,j2++) {
                
                int y,y2;
                y = (j2<0) ? inbuf_old[2*(BLACKSIZE+j2)] : Input->PCMPtr[2*j2];
                y2 = (j2<0) ? inbuf_old[2*(BLACKSIZE+j2)+1] : Input->PCMPtr[2*j2+1];
#ifdef    FIXPOINT
        #ifdef    asm
                xvalue += MULSHIFT32(y<<9,RESI->blackfiltINT[joff][i]);
                xvalue2 += MULSHIFT32(y2<<9,RESI->blackfiltINT[joff][i]);
        #else
                xvalue += ((alib_int64)y*RESI->blackfiltINT[joff][i])>>23;
                xvalue2 += ((alib_int64)y2*RESI->blackfiltINT[joff][i])>>23;
        #endif
#else
                xvalue += y*RESI->blackfilt[joff][i];
                xvalue2 += y2*RESI->blackfilt[joff][i];
#endif
            }
            Output->PCMPtr[2*k]=(short)STAURATE16((int)xvalue/(1<<8));
            Output->PCMPtr[2*k+1]=(short)STAURATE16((int)xvalue2/(1<<8));
#endif //#ifdef    bufoldcpy_
          
#if 0            
            DebugCount +=4;
            if(DebugCount==0x990-8)
                DebugCount =DebugCount;
#endif
        }
        samp_frac_num += frac_advance;
        if (samp_frac_num >= den_rate)
        {
           samp_frac_num -= den_rate;
           j++;
        }
    }
    
    
    /* k = number of samples added to outbuf */
    /* last k sample used data from [j-filter_l/2,j+filter_l-filter_l/2]  */
    
    /* how many samples of input data were used:  */
    num_used = Min((int)Input->PcmLen,filter_l+j-filter_l/2);
    
    /* adjust our input time counter.  Incriment by the number of samples used,
    * then normalize so that next output sample is at time 0, next
    * input buffer is at time itime[ch] */
    //RESI.itime += (num_used<<FLOAT2FIXbit1) - k*RESI.resample_ratio;
    
    RESI->last_sample = j - num_used;
    RESI->samp_frac_num = samp_frac_num;
    /* save the last BLACKSIZE samples into the inbuf_old buffer */
    if (num_used >= BLACKSIZE) {
        if(Input->Chan ==1)
        {
            for (i=0;i<BLACKSIZE;i++)
                inbuf_old[i]=Input->PCMPtr[num_used + i -BLACKSIZE];
        }else
        {
            for (i=0;i<BLACKSIZE;i++)
            {
                inbuf_old[2*i]=Input->PCMPtr[2*(num_used + i -BLACKSIZE)];
                inbuf_old[2*i+1]=Input->PCMPtr[2*(num_used + i -BLACKSIZE)+1];
                
            }
            
        }
    }else{
        /* shift in *num_used samples into inbuf_old  */
        int n_shift = BLACKSIZE-num_used;  /* number of samples to shift */
        
        if(Input->Chan ==1)
        {
            for (i=0; i<n_shift; ++i ) 
                inbuf_old[i] = inbuf_old[i+ num_used];
            
            for (j=0; i<BLACKSIZE; ++i, ++j ) 
                inbuf_old[i] = Input->PCMPtr[j];
        }else
        {
            for (i=0; i<n_shift; ++i )
            {
                inbuf_old[2*i] = inbuf_old[2*(i+ num_used)];
                inbuf_old[2*i+1] = inbuf_old[2*(i+ num_used)+1];
            }
            
            for (j=0; i<BLACKSIZE; ++i, ++j )
            {
                inbuf_old[2*i] = Input->PCMPtr[2*j];
                inbuf_old[2*i+1] = Input->PCMPtr[2*j+1];
            }
            
        }
        
    }
    Input->PcmLen -= num_used;
    Input->PCMPtr += num_used*Input->Chan;//
/*
    if(Input->PcmLen!=0)
    {
        memcpy(Input->PCMPtr, Input->PCMPtr+Input->Chan*num_used,Input->PcmLen*2*Input->Chan);
    }
*/
    Output->PcmLen = k;
    Output->Chan = Input->Chan;
    Output->preamp = Input->preamp;
    return k;  /* return the number samples created at the new samplerate */
}
