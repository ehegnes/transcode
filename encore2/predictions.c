/**************************************************************************
 *                                                                        *
 * This code is developed by Eugene Kuznetsov.  This software is an       *
 * implementation of a part of one or more MPEG-4 Video tools as          *
 * specified in ISO/IEC 14496-2 standard.  Those intending to use this    *
 * software module in hardware or software products are advised that its  *
 * use may infringe existing patents or copyrights, and any such use      *
 * would be at such party's own risk.  The original developer of this     *
 * software module and his/her company, and subsequent editors and their  *
 * companies (including Project Mayo), will have no liability for use of  *
 * this software or modifications or derivatives thereof.                 *
 *                                                                        *
 * Project Mayo gives users of the Codec a license to this software       *
 * module or modifications thereof for use in hardware or software        *
 * products claiming conformance to the MPEG-4 Video Standard as          *
 * described in the Open DivX license.                                    *
 *                                                                        *
 * The complete Open DivX license can be found at                         *
 * http://www.projectmayo.com/opendivx/license.php .                      *
 *                                                                        *
 **************************************************************************/

/**************************************************************************
 *
 *  predictions.c, AC/DC and motion vector spatial prediction
 *
 *  Copyright (C) 2001  Project Mayo
 *
 *  Eugene Kuznetsov
 *
 *  DivX Advance Research Center <darc@projectmayo.com>
 *
 **************************************************************************/

#include "enctypes.h"
#include "predictions.h"
#include <assert.h>
#include <stdlib.h>
#define _div_div(a, b) (a>0) ? (a+(b>>1))/b : (a-(b>>1))/b
#define MIN(X, Y) ((X)<(Y)?(X):(Y))
#define MAX(X, Y) ((X)>(Y)?(X):(Y))

static const int16_t default_acdc_values[15]=
{1024, 
    0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0};

int16_t enc_find_pmv(Vop* pVop, int x, int y, int block, int comp)
{
// It doesn't work properly!
  int p1, p2, p3;
  int xin1, xin2, xin3;
  int yin1, yin2, yin3;
  int vec1, vec2, vec3;
    int index=x+y*pVop->iMbWcount;
	if ((y == 0) && ((block == 0) || (block == 1)))
	{
		if ((x == 0) && (block == 0))
			return 0;
		else if (block == 1)
		{
		    MotionVector mv=pVop->pMBs[index].mvs[0];
		    return comp?mv.y:mv.x;
		}
		else // block == 0
		{
		    MotionVector mv=pVop->pMBs[index-1].mvs[1];
		    return comp?mv.y:mv.x;
		}
	}
	else
	{
		switch (block)
		{
			case 0: 
				vec1 = 1;	yin1 = y;		xin1 = x-1;
				vec2 = 2;	yin2 = y-1;	xin2 = x;
				vec3 = 2;	yin3 = y-1;	xin3 = x+1;
				break;
			case 1:
				vec1 = 0;	yin1 = y;		xin1 = x;
				vec2 = 3;	yin2 = y-1;	xin2 = x;
				vec3 = 2;	yin3 = y-1;	xin3 = x+1;
				break;
			case 2:
				vec1 = 3;	yin1 = y;		xin1 = x-1;
				vec2 = 0;	yin2 = y;	  xin2 = x;
				vec3 = 1;	yin3 = y;		xin3 = x;
				break;
			default: // case 3
				vec1 = 2;	yin1 = y;		xin1 = x;
				vec2 = 0;	yin2 = y;		xin2 = x;
				vec3 = 1;	yin3 = y;		xin3 = x;
				break;
		}
		if((xin1<0) || (yin1<0) || (xin1>+pVop->iMbWcount))
		    p1=0;
		else
		    p1 = comp 
		    ? pVop->pMBs[xin1+yin1*pVop->iMbWcount].mvs[vec1].y
		    : pVop->pMBs[xin1+yin1*pVop->iMbWcount].mvs[vec1].x;
		if((xin2<0) || (yin2<0) || (xin2>=pVop->iMbWcount))
		    p2=0;
		else
		p2 = comp 
		    ? pVop->pMBs[xin2+yin2*pVop->iMbWcount].mvs[vec2].y
		    : pVop->pMBs[xin2+yin2*pVop->iMbWcount].mvs[vec2].x;
		if((xin3<0) || (yin3<0) || (xin3>=pVop->iMbWcount))
		    p3=0;
		else
		    p3 = comp 
		    ? pVop->pMBs[xin3+yin3*pVop->iMbWcount].mvs[vec3].y
		    : pVop->pMBs[xin3+yin3*pVop->iMbWcount].mvs[vec3].x;
		return MIN(MAX(p1, p2), MIN(MAX(p2, p3), MAX(p1, p3)));
	}
}

int32_t calc_acdc_prediction(Vop* pVop, int x, int y, int block, 
    int* acpred_direction, const int16_t* dct_codes, 
    int16_t* dc_pred, const int16_t** ac_pred,
    uint8_t iDcScaler)
{
    const int ZZZ=15;
    int16_t* left=0;
    int16_t* top=0;
    int16_t* diag=0;
    int16_t* current;
    
    const int16_t* pLeft=default_acdc_values;
    const int16_t* pTop=default_acdc_values;
    const int16_t* pDiag=default_acdc_values;
    int16_t* pCurrent;
    int32_t S1=0, S2=0;
    int i;
    int index=x+y*pVop->iMbWcount;

    if(x && (pVop->pMBs[index-1].mode==MODE_INTRA || pVop->pMBs[index-1].mode==MODE_INTRA_Q))
	left=pVop->pMBs[index-1].pred_values[0];
    if(y && (pVop->pMBs[index-pVop->iMbWcount].mode==MODE_INTRA || pVop->pMBs[index-pVop->iMbWcount].mode==MODE_INTRA_Q))
	top=pVop->pMBs[index-pVop->iMbWcount].pred_values[0];
    if(x && y && (pVop->pMBs[index-1-pVop->iMbWcount].mode==MODE_INTRA || pVop->pMBs[index-1-pVop->iMbWcount].mode==MODE_INTRA_Q))
	diag=pVop->pMBs[index-1-pVop->iMbWcount].pred_values[0];
    current=pVop->pMBs[x+y*pVop->iMbWcount].pred_values[0];

    pCurrent=current+block*ZZZ;
    switch(block)
    {
    case 0:
	    if(left) pLeft=left+ZZZ;
	    if(top) pTop=top+2*ZZZ;
	    if(diag) pDiag=diag+3*ZZZ;
	    break;
    case 1:
	    pLeft=current;
	    if(top) pTop=top+3*ZZZ;
	    if(top) pDiag=top+2*ZZZ;
	    break;
    case 2:
	    if(left) pLeft=left+3*ZZZ;
	    pTop=current;
	    if(left) pDiag=left+ZZZ;
	    break;
    case 3:
	    pLeft=current+2*ZZZ;
	    pTop=current+ZZZ;
	    pDiag=current;
	    break;
    case 4:
	    if(left) pLeft=left+4*ZZZ;
	    if(top) pTop=top+4*ZZZ;
	    if(diag) pDiag=diag+4*ZZZ;
	    break;
    case 5:
	    if(left) pLeft=left+5*ZZZ;
	    if(top) pTop=top+5*ZZZ;
	    if(diag) pDiag=diag+5*ZZZ;
	    break;	
    }
    if(abs(pLeft[0]-pDiag[0])<abs(pTop[0]-pDiag[0]))
    {
	*acpred_direction=1; // vertical
	*dc_pred=_div_div(pTop[0], iDcScaler);
    }    
    else
    {
	*acpred_direction=2; // horizontal
	*dc_pred=_div_div(pLeft[0], iDcScaler);
    }
    pCurrent[0]=dct_codes[0]*iDcScaler;
    for(i=1; i<8; i++)
    {
	if(*acpred_direction==1)
	{
	    assert(dct_codes[i]<=256);
	    assert(dct_codes[i]>=-256);
	    assert(pTop[i]>=-256);
	    assert(pTop[i]<=256);
	    S1+=abs(pTop[i]-dct_codes[i]);
	    S2+=abs(dct_codes[i]);
	}
	else
	{
	    assert(dct_codes[i*8]<=256);
	    assert(dct_codes[i*8]>=-256);
	    assert(pLeft[i+7]>=-256);
	    assert(pLeft[i+7]<=256);
	    S1+=abs(pLeft[i+7]-dct_codes[i*8]);
	    S2+=abs(dct_codes[i*8]);
	}
    }
    /*
    if(S1>S2)
    {
	*acpred_direction=0;
	*ac_pred=0;
    }
    else
    */
    {
	if(*acpred_direction==1)
	    *ac_pred=pTop+1;
	else
	    *ac_pred=pLeft+8;
    }
    for(i=1; i<8; i++)
    {
	pCurrent[i]=dct_codes[i];
	assert(pCurrent[i]>=-256);
	assert(pCurrent[i]<=256);
	pCurrent[i+7]=dct_codes[i*8];
	assert(pCurrent[i+7]>=-256);
	assert(pCurrent[i+7]<=256);
    }    
    return S2-S1;
}

