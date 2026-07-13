
#include "aumixcom.h"
#define BPC 320
#define	FILTERTAP	63 //3  /* must be odd */
//BPCINT >=3
#define BPCINT 3
//#define	bufoldcpy_ //do not open,open error


#define	FIXPOINT
#define	float2int(x) (x==1.0)?0x7fffffff:((x>0.)?(x*0x7fffffff):(x*0x80000000))
//#define	clip(x)  x>0x7fff?0x7fff:x<0x8000?0x8000:x  //  if(x>0x7fff) x=0x7fff; else if(x<0x8000) x=0x8000;

#ifndef ABS
#define ABS(A) (((A)>0) ? (A) : -(A))
#endif
#ifndef Min
#define         Min(A, B)       ((A) < (B) ? (A) : (B))
#endif
#ifndef Max
#define         Max(A, B)       ((A) > (B) ? (A) : (B))
#endif

typedef	struct __ResampleInfo__{
	int resample_ratio;
    short	inbuf_old[2*(FILTERTAP+2)/**4*/];//
    float	blackfilt[2 * BPC + 1][FILTERTAP+2];//[2 * BPC + 1];
#ifdef	FIXPOINT
	int		blackfiltINT[2 * BPC + 1][FILTERTAP+2];//[2 * BPC + 1];
#endif
    int   itime;
	//float   scale;
	int		BLACKSIZE;
	int		filter_l;
	float	fcn,intratio;
	int		bpc;   /* number of convolution functions to pre-compute */
	//int     numin;//sample input 
	//int     numout;//sample output
	int     int_advance;
	int     frac_advance;
	int     last_sample;
	int     samp_frac_num;
	int     den_rate;
	int     num_rate;
	int     bpcint;
	int     fill_buffer_resample_init;
	int     old_Infs;
	int     old_Outfs;


}ResampleInfo;

#define	FLOAT2FIXbit1 12
#define	FLOAT2FIXbit1MASK 0XFFF


int		AudioResample(PcmInfo *Input,PcmInfo *Output, ResampleInfo* RESI);
int		AudioMixFuc(PcmInfo *InputA,PcmInfo *InputB,PcmInfo *Output) ;

