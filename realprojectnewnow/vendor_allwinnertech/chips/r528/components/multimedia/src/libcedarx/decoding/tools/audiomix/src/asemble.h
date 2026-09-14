#include "alib_typedef.h"
#include "alib_utils.h"

#if defined WIN32
#pragma warning( disable : 4035 )	/* complains about inline asm not returning a value */

static alib_inline int MULSHIFT32(int x, int y)	
{
    __asm {
		mov		eax, x
	    imul	y
	    mov		eax, edx
	}
}
static alib_inline short STAURATE16(int x)	
{
	signed int value=x;
		if(value>0x7fff)
			value=0x7fff;
		else if(value<(signed int)0xffff8000)
			value=0xffff8000;
//		if(value!=0)
//			value = value;
	return (short)value;
}

#elif (defined ARM_ADS)
#pragma diag_remark 550
static alib_inline int MULSHIFT32(int x, int y)
{
    /* important rules for smull RdLo, RdHi, Rm, Rs:
     *     RdHi and Rm can't be the same register
     *     RdLo and Rm can't be the same register
     *     RdHi and RdLo can't be the same register
     * Note: Rs determines early termination (leading sign bits) so if you want to specify
     *   which operand is Rs, put it in the SECOND argument (y)
	 * For inline assembly, x and y are not assumed to be R0, R1 so it shouldn't matter
	 *   which one is returned. (If this were a function call, returning y (R1) would 
	 *   require an extra "mov r0, r1")
     */
    int zlow;
    __asm {
    	smull zlow,y,x,y
   	}

    return y;
}

static alib_inline short STAURATE16(int x)	
{
	int m = 0x7fff;
	int a;
    __asm {
    	mov	a,x,asr #15
    	cmp a,x,asr #31
    	eorne x,m,x,asr #31
    }
    return (short)x;
}

#else
static int MULSHIFT32(int x, int y)
{
	int  ret = (int)(((alib_int64)x *(alib_int64)y)>>32) ;
	return ret;
}

static alib_inline short STAURATE16(int x)	
{
	signed int value=x;
		if(value>0x7fff)
			value=0x7fff;
		else if(value<(signed int)0xffff8000)
			value=0xffff8000;
//		if(value!=0)
//			value = value;
	return (short)value;
}
#endif




