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
 *  block.c, functions that perform compression on the macroblock level.
 *
 *  Copyright (C) 2001  Project Mayo
 *
 *  Eugene Kuznetsov
 *
 *  DivX Advance Research Center <darc@projectmayo.com>
 *
 **************************************************************************/

#include "block.h"
#include "dct.h"
#include "quantize.h"
#include "putvlc.h"
#include "predictions.h"
#include "timer.h"
#include "TransferIDCT.h"
#include <stdio.h>
#include <assert.h>
#define MIN(X, Y) ((X)<(Y)?(X):(Y))
#define MAX(X, Y) ((X)>(Y)?(X):(Y))


int ENCORE_DEBUG=0;
#define Debug if(ENCORE_DEBUG)

static const uint8_t zig_zag_scan[64]=
{
  0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5,
  12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14, 21, 28,
  35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
  58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63
};

// other scan orders
static const uint8_t alternate_horizontal_scan[64]=
{
   0,  1,  2,  3,  8,  9, 16, 17, 
	10, 11,  4,  5,  6,  7, 15, 14,
  13, 12, 19, 18, 24, 25, 32, 33, 
	26, 27, 20, 21, 22, 23, 28, 29,
  30, 31, 34, 35, 40, 41, 48, 49, 
	42, 43, 36, 37, 38, 39, 44, 45,
  46, 47, 50, 51, 56, 57, 58, 59, 
	52, 53, 54, 55, 60, 61, 62, 63
};

static const uint8_t alternate_vertical_scan[64]=
{
   0,  8, 16, 24,  1,  9,  2, 10, 
	17, 25, 32, 40, 48, 56, 57, 49,
  41, 33, 26, 18,  3, 11,  4, 12, 
	19, 27, 34, 42, 50, 58, 35, 43,
  51, 59, 20, 28,  5, 13,  6, 14, 
	21, 29, 36, 44, 52, 60, 37, 45,
  53, 61, 22, 30,  7, 15, 23, 31, 
	38, 46, 54, 62, 39, 47, 55, 63
};

static __inline int8_t get_dc_scaler(int8_t quant, bool lum)
{
    int8_t dc_scaler;
    if (lum) 
    {
    	if (quant > 0 && quant < 5) 
    	    dc_scaler = 8;
    	else if (quant < 9) 
    	    dc_scaler = (2 * quant);
	else if (quant < 25) 
	    dc_scaler = (quant + 8);
	else 
	    dc_scaler = (2 * quant - 16);
    }
    else 
    {
	if (quant > 0 && quant < 5) 
	    dc_scaler = 8;
	else if (quant < 25) 
	    dc_scaler = ((quant + 13) / 2);
	else 
	    dc_scaler = (quant - 6);
    }
    return dc_scaler;
}

static __inline uint8_t calc_cbp_inter(int16_t codes[6][64])
{
    int i, j;
    uint8_t cbp=0;
    for(i=0; i<6; i++)
    {
	for(j=0; j<64; j++)
	    if(codes[i][j]!=0)
	    {
		cbp |= 1<<(5-i);
		break;
	    }
    }
    return cbp;
}
static __inline uint8_t calc_cbp_intra(int16_t codes[6][64], int acpred_directions[], const int16_t* ac_predictions[])
{
    int i, j;
    uint8_t cbp=0;
    for(i=0; i<6; i++)
    {
	for(j=1; j<64; j++)
	{
	    int16_t value=codes[i][j];
	    if((acpred_directions[i]==1) && (j<8))
		value-=ac_predictions[i][j-1];
	    if((acpred_directions[i]==2) && !(j%8))
		value-=ac_predictions[i][j/8-1];	    
	    if(value!=0)
	    {
		cbp |= 1<<(5-i);
		break;
	    }
	}
    }
    return cbp;
}


static void EncodeBlockInter(Bitstream* bs, Statistics* pStat, const int16_t* dct_codes)
{
    int run;//, level;
    int prev_run, prev_level;
    const uint8_t* zigzag=zig_zag_scan;
    int pos;
    int bits;
    bits=BsPos(bs);
    run=0;
    prev_level=0;
    for(pos=0; pos<64; pos++)
    {
	int index=zigzag[pos];
	int value=dct_codes[index];
	if(value==0)
	{
	    run++;
	    continue;
	}
	if(prev_level)
	    PutCoeff(bs, prev_run, prev_level, 0, MODE_INTER);
	prev_run=run;
	prev_level=value;
	run=0;
//	level=value;
    }
    PutCoeff(bs, prev_run, prev_level, 1, MODE_INTER);
    bits=BsPos(bs)-bits;
    pStat->iTextBits+=bits;
}

static void EncodeBlockIntra(Bitstream* bs, const int16_t* dct_codes, const int16_t* ac_predictions, int acpred_direction)
{
    int run;//, level;
    int prev_run, prev_level;
    const uint8_t* zigzag=zig_zag_scan;
    int pos;
// not sure here !!!    
    if(acpred_direction==1)
	zigzag=alternate_horizontal_scan;
    if(acpred_direction==2)
	zigzag=alternate_vertical_scan;
    run=0;
    prev_level=0;
    for(pos=1; pos<64; pos++)
    {
	int index=zigzag[pos];
	int value=dct_codes[index];
	if((acpred_direction==1) && (index<8))
	    value-=ac_predictions[index-1];
	if((acpred_direction==2) && !(index%8))
	    value-=ac_predictions[index/8-1];
	if(value==0)
	{
	    run++;
	    continue;
	}
	if(prev_level)
	    PutCoeff(bs, prev_run, prev_level, 0, MODE_INTRA);
	prev_run=run;
	prev_level=value;
	run=0;
//	level=value;
    }
    PutCoeff(bs, prev_run, prev_level, 1, MODE_INTRA);
}

void EncodeMacroblockIntra(Encoder* pEnc, Image* im, Bitstream* bs, int x, int y)
{
    int16_t dct_codes[6][64];
    int dx, dy, i;
    uint8_t cbp;
    uint8_t *pY_Cur, *pY_Ref, *pU_Cur, *pU_Ref, *pV_Cur, *pV_Ref; 
    Vop* pCurrent=&(pEnc->sCurrent);
    Macroblock* pMB=pCurrent->pMBs+x+y*pCurrent->iMbWcount;
    int stride=pCurrent->iWidth;
    int stride2=pCurrent->iEdgedWidth;
    int acpred_directions[6];
    uint8_t iQuant=pEnc->iQuantizer;
    int16_t dc_predictions[6];
    const int16_t* ac_predictions[6];
    int32_t S=0;
    
    if(pMB->mode == MODE_INTRA_Q)
	iQuant+=pMB->dquant;
    pY_Ref=im->pY+16*y*stride+16*x;
    pU_Ref=im->pU+8*y*stride/2+8*x;
    pV_Ref=im->pV+8*y*stride/2+8*x;
    pY_Cur=pCurrent->pY+16*y*stride2+16*x;
    pU_Cur=pCurrent->pU+8*y*stride2/2+8*x;
    pV_Cur=pCurrent->pV+8*y*stride2/2+8*x;
    start_etimer();
    TransferFDCT_copy(pY_Ref, dct_codes[0], stride);
    TransferFDCT_copy(pY_Ref+8, dct_codes[1], stride);
    TransferFDCT_copy(pY_Ref+8*stride, dct_codes[2], stride);
    TransferFDCT_copy(pY_Ref+8*stride+8, dct_codes[3], stride);
    TransferFDCT_copy(pU_Ref, dct_codes[4], stride/2);
    TransferFDCT_copy(pV_Ref, dct_codes[5], stride/2);
    stop_transfer_etimer();

    for(i=0; i<6; i++)
    {
	uint8_t iDcScaler=get_dc_scaler(iQuant, (i<4)?1:0);
	start_etimer();
#if (defined(_MMX_) && defined(WIN32))
	fdct_mm32(dct_codes[i]);
#else
	fdct_enc_fast(dct_codes[i]);
#endif
	stop_dct_etimer();
	quantize_intra(dct_codes[i], iQuant, iDcScaler);
	S+=calc_acdc_prediction(pCurrent, x, y, i, &acpred_directions[i],
	    dct_codes[i], &dc_predictions[i], &ac_predictions[i], iDcScaler);
    }	
    if(S<0)
	for(i=0; i<6; i++)
	    acpred_directions[i]=0;
    cbp=calc_cbp_intra(dct_codes, acpred_directions, ac_predictions);
    if(pCurrent->ePredictionType==I_VOP)
	PutMCBPC_intra(bs, cbp & 3, pMB->mode);
    else
    {
	PutMCBPC_inter(bs, cbp & 3, pMB->mode);
	Debug printf(" %d: CBPC 0x%x\n", BsPos(bs), cbp & 3);
    }
    if(acpred_directions[0])
	BitstreamPutBits(bs,1,1);
    else    
	BitstreamPutBits(bs,0,1);
    PutCBPY(bs, cbp >> 2, 1);
    if(pCurrent->ePredictionType==P_VOP)
    {
	Debug printf(" %d: CBPY 0x%x\n", BsPos(bs), cbp >> 2);
    }
    for(i=0; i<6; i++)
	{
	    uint8_t iDcScaler=get_dc_scaler(iQuant, (i<4)?1:0);
	    Debug if(pCurrent->ePredictionType==P_VOP)
		printf("%d: start of data for block %d\n",
		    BsPos(bs), i);
	    PutIntraDC(bs, dct_codes[i][0]-dc_predictions[i], (i<4)?1:0);
	    if(cbp & (1<<(5-i)))
		EncodeBlockIntra(bs, dct_codes[i], ac_predictions[i], acpred_directions[i]);
	    dequantize_intra(dct_codes[i], iQuant, iDcScaler);
	    start_etimer();
#if (defined(_MMX_) && defined(WIN32))
	    Fast_IDCT(dct_codes[i]);
#else
	    idct_enc(dct_codes[i]);
#endif
	    stop_idct_etimer();
	    start_etimer();
	    switch(i)
	    {
	    case 0:
	        TransferIDCT_copy(pY_Cur, dct_codes[0], stride2);
		break;
	    case 1:
	        TransferIDCT_copy(pY_Cur+8, dct_codes[1], stride2);
		break;
	    case 2:
	        TransferIDCT_copy(pY_Cur+8*stride2, dct_codes[2], stride2);
		break;
	    case 3:
	        TransferIDCT_copy(pY_Cur+8+8*stride2, dct_codes[3], stride2);
		break;
	    case 4:
	        TransferIDCT_copy(pU_Cur, dct_codes[4], stride2/2);
		break;
	    case 5:
	        TransferIDCT_copy(pV_Cur, dct_codes[5], stride2/2);
		break;
	    }
	    stop_transfer_etimer();
	}    
}
static __inline void putMVData(Bitstream *bs, Statistics* pStat, int16_t fcode, int16_t value)
{
    int scale_fac = 1 << (fcode - 1);
    int high = (32 * scale_fac) - 1;
    int low = ((-32) * scale_fac);
    int range = (64 * scale_fac);
    int bits;
    if(value < low)
	value += range;
    if(value > high)
        value -= range;
	
    pStat->iMvSum+=value*value;
    if(abs(value)>=(high/2))
	pStat->iMvUpperCount++;
    else
	pStat->iMvLowerCount++;
    
    bits=BsPos(bs);
    if ((scale_fac == 1) || (value == 0))
    {
    	if (value < 0)
	    value = value + 65;
        PutMV(bs, value);
    }
    else
    {
        int mv_res, mv_data;
        int mv_sign;
        if(value<0)
        {
    	    value=-value;
	    mv_sign=-1;
	}
	else mv_sign=1;
	mv_res = (value-1) % scale_fac;	    
	mv_data = (value-1-mv_res) / scale_fac + 1;
	if(mv_sign==-1) mv_data=-mv_data+65;
	PutMV(bs, mv_data);
	BitstreamPutBits(bs, mv_res, fcode-1);
    }
    bits=BsPos(bs)-bits;
    pStat->iMvBits+=bits;
}

void EncodeMacroblockInter(Encoder* pEnc, Image* im, Bitstream* bs, int x, int y)
{
    Vop* pCurrent=&(pEnc->sCurrent);
    Macroblock* pMB=pCurrent->pMBs+x+y*pCurrent->iMbWcount;
    int16_t dct_codes[6][64];
    int dx, dy, i;
    uint8_t cbp;
    uint8_t *pY_Cur, *pY_Ref, *pU_Cur, *pU_Ref, *pV_Cur, *pV_Ref; 
    int stride=pCurrent->iWidth;
    int stride2=pCurrent->iEdgedWidth;    
    uint8_t iQuant=pEnc->iQuantizer;
    Debug printf("%d: MB (%d, %d): ", BsPos(bs), x, y);
    if(pMB->mode == MODE_INTER_Q)
    {
	Debug printf("INTER_Q\n");
	iQuant+=pMB->dquant;
    }
    if(pMB->mode==MODE_INTRA)
    {
	Debug printf("INTRA\n");
    	BitstreamPutBits(bs,0,1); // present
	return EncodeMacroblockIntra(pEnc, im, bs, x, y);
    }
    pY_Ref=im->pY+16*y*stride+16*x;
    pU_Ref=im->pU+8*y*stride/2+8*x;
    pV_Ref=im->pV+8*y*stride/2+8*x;

    pY_Cur=pCurrent->pY+16*y*stride2+16*x;
    pU_Cur=pCurrent->pU+8*y*stride2/2+8*x;
    pV_Cur=pCurrent->pV+8*y*stride2/2+8*x;

    start_etimer();
    TransferFDCT_sub(pY_Ref, pY_Cur, dct_codes[0], stride, stride2);
    TransferFDCT_sub(pY_Ref+8, pY_Cur+8, dct_codes[1], stride, stride2);
    TransferFDCT_sub(pY_Ref+8*stride, pY_Cur+8*stride2, dct_codes[2], stride, stride2);
    TransferFDCT_sub(pY_Ref+8*stride+8, pY_Cur+8*stride2+8, dct_codes[3], stride, stride2);
    TransferFDCT_sub(pU_Ref, pU_Cur, dct_codes[4], stride/2, stride2/2);
    TransferFDCT_sub(pV_Ref, pV_Cur, dct_codes[5], stride/2, stride2/2);
    stop_transfer_etimer();

    for(i=0; i<6; i++)
    {
	start_etimer();
#if (defined(_MMX_) && defined(WIN32))
	fdct_mm32(dct_codes[i]);
#else
	fdct_enc_fast(dct_codes[i]);
#endif
	stop_dct_etimer();
	
	quantize_inter(dct_codes[i], iQuant);
    }
    cbp=calc_cbp_inter(dct_codes);
    Debug if(pMB->mode==MODE_INTER4V)
	printf("INTER4V ( mvs <%dx%d>, <%dx%d>, <%dx%d>, <%dx%d> )",
	    pMB->mvs[0].x, pMB->mvs[0].y,
	    pMB->mvs[1].x, pMB->mvs[1].y,
	    pMB->mvs[2].x, pMB->mvs[2].y,
	    pMB->mvs[3].x, pMB->mvs[3].y
	    );
    else
	printf("INTER ( mv <%dx%d> )", pMB->mvs[0].x, pMB->mvs[0].y);
    Debug printf(", CBP 0x%x", cbp);
    if((!cbp) && (pMB->mode==MODE_INTER) && (pMB->mvs[0].x==0) && (pMB->mvs[0].y==0))
    { // not coded
	Debug printf(", skipped\n");
	BitstreamPutBits(bs,1,1);
	return;	
    }
    else
	BitstreamPutBits(bs,0,1);
    Debug printf("\n");
    PutMCBPC_inter(bs, cbp & 3, pMB->mode);
    PutCBPY(bs, cbp >> 2, 0);
    if(pMB->mode == MODE_INTER_Q) // this mode is not implemented
	BitstreamPutBits(bs,pMB->dquant,2);
    if(pMB->mode == MODE_INTER4V)
    {
	int pred_x=pMB->mvs[0].x-enc_find_pmv(pCurrent, x, y, 0, 0);
	int pred_y=pMB->mvs[0].y-enc_find_pmv(pCurrent, x, y, 0, 1);
	putMVData(bs, &pEnc->sStat, pEnc->iFcode, pred_x);
	putMVData(bs, &pEnc->sStat, pEnc->iFcode, pred_y);
	pred_x=pMB->mvs[1].x-enc_find_pmv(pCurrent, x, y, 1, 0);
	pred_y=pMB->mvs[1].y-enc_find_pmv(pCurrent, x, y, 1, 1);
	putMVData(bs, &pEnc->sStat, pEnc->iFcode, pred_x);
	putMVData(bs, &pEnc->sStat, pEnc->iFcode, pred_y);
	pred_x=pMB->mvs[2].x-enc_find_pmv(pCurrent, x, y, 2, 0);
	pred_y=pMB->mvs[2].y-enc_find_pmv(pCurrent, x, y, 2, 1);
	putMVData(bs, &pEnc->sStat, pEnc->iFcode, pred_x);
	putMVData(bs, &pEnc->sStat, pEnc->iFcode, pred_y);
	pred_x=pMB->mvs[3].x-enc_find_pmv(pCurrent, x, y, 3, 0);
	pred_y=pMB->mvs[3].y-enc_find_pmv(pCurrent, x, y, 3, 1);
	putMVData(bs, &pEnc->sStat, pEnc->iFcode, pred_x);
	putMVData(bs, &pEnc->sStat, pEnc->iFcode, pred_y);
    }
    else
    {
	int pred_x=pMB->mvs[0].x-enc_find_pmv(pCurrent, x, y, 0, 0);
	int pred_y=pMB->mvs[0].y-enc_find_pmv(pCurrent, x, y, 0, 1);
	putMVData(bs, &pEnc->sStat, pEnc->iFcode, pred_x);
	putMVData(bs, &pEnc->sStat, pEnc->iFcode, pred_y);
    }
    for(i=0; i<6; i++)
	if(cbp & (1<<(5-i)))
	{
	    EncodeBlockInter(bs, &pEnc->sStat, dct_codes[i]);
	    dequantize_inter(dct_codes[i], iQuant);
	    start_etimer();
#if (defined(_MMX_) && defined(WIN32))
	    Fast_IDCT(dct_codes[i]);
#else
	    idct_enc(dct_codes[i]);
#endif
	    stop_idct_etimer();
	    start_etimer();
	    switch(i)
	    {
	    case 0:
	        TransferIDCT_add(pY_Cur, dct_codes[0], stride2);
		break;
	    case 1:
	        TransferIDCT_add(pY_Cur+8, dct_codes[1], stride2);
		break;
	    case 2:
	        TransferIDCT_add(pY_Cur+8*stride2, dct_codes[2], stride2);
		break;
	    case 3:
	        TransferIDCT_add(pY_Cur+8+8*stride2, dct_codes[3], stride2);
		break;
	    case 4:
	        TransferIDCT_add(pU_Cur, dct_codes[4], stride2/2);
		break;
	    case 5:
	        TransferIDCT_add(pV_Cur, dct_codes[5], stride2/2);
		break;
	    }
	    stop_transfer_etimer();
	}
}
