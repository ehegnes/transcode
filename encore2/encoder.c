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
 *  encoder.c, video encoder kernel
 *
 *  Copyright (C) 2001  Project Mayo
 *
 *  Eugene Kuznetsov
 *
 *  DivX Advance Research Center <darc@projectmayo.com>
 *
 **************************************************************************/

#include "encoder.h"
#include "motion.h"
#include "block.h"
#include "bitstream.h"
#include "rgb2yuv.h"
#include "dct.h"
#include "mad.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include "timer.h"
#include "rate_ctl.h"
static int EncodeKeyFrame(Encoder* pEnc, Image* im, Bitstream* bs, uint32_t* pBits);
static int EncodeDeltaFrame(Encoder* pEnc, Image* im, Bitstream* bs, uint32_t* pBits);

static __inline uint8_t get_fcode (uint16_t sr)
{
	if (sr<=16) return 1;
	else if (sr<=32) return 2;
	else if (sr<=64) return 3;
	else if (sr<=128) return 4;
	else if (sr<=256) return 5;
	else if (sr<=512) return 6;
	else if (sr<=1024) return 7;
	else return 0;
}

#define ENC_CHECK(X) if(!(X)) return ENC_BAD_FORMAT
int CreateEncoder(ENC_PARAM* pParam)
{      
        Encoder* pEnc;

        pParam->handle=NULL;
        
        ENC_CHECK(pParam);
/* Validate input parameters */        
        ENC_CHECK(pParam->x_dim>0);
        ENC_CHECK(pParam->y_dim>0);
	ENC_CHECK(!(pParam->x_dim % 2));
	ENC_CHECK(!(pParam->y_dim % 2));
        /* these two limits are introduced by limitations of decore */
        ENC_CHECK(pParam->x_dim<=720);
        ENC_CHECK(pParam->y_dim<=576);
/* Set default values for all other parameters if they are out of
    acceptable range */
        if(pParam->framerate<=0)
            pParam->framerate=25.;
        if(pParam->bitrate<=0)
            pParam->bitrate=910000;
        if(pParam->rc_period<=0)
            pParam->rc_period=50;
        if(pParam->rc_reaction_period<=0)
            pParam->rc_reaction_period=10;
        if(pParam->rc_reaction_ratio<=0)
            pParam->rc_reaction_ratio=10;
        if((pParam->min_quantizer<=0) || (pParam->min_quantizer>31))
            pParam->min_quantizer=2;
        if((pParam->max_quantizer<=0) || (pParam->max_quantizer>31))
            pParam->max_quantizer=15;
	if(pParam->max_key_interval==0)
	    pParam->max_key_interval=250; /* 1 keyframe each 10 seconds */
	if(pParam->max_quantizer<pParam->min_quantizer)
	    pParam->max_quantizer=pParam->min_quantizer;
	if((pParam->quality<1) || (pParam->quality>5))
	    pParam->quality=5;
        
        pEnc=(Encoder*)malloc(sizeof(Encoder));
	if(pEnc==0)
	    return ENC_MEMORY;
/* Fill members of Encoder structure */        
        pEnc->iWidth=pParam->x_dim;
        pEnc->iHeight=pParam->y_dim;
	pEnc->iQuality=pParam->quality;
	pEnc->sStat.fMvPrevSigma=-1;
/* Fill rate control parameters */
//        pEnc->sRC.fFramerate=pParam->framerate;
//        pEnc->sRC.iBitrate=pParam->bitrate;
//        pEnc->sRC.iRcPeriod=pParam->rc_period;
//        pEnc->sRC.iRcReactionPeriod=pParam->rc_reaction_period;
//        pEnc->sRC.iRcReactionRatio=pParam->rc_reaction_ratio;
        pEnc->sRC.iMaxQuantizer=pParam->max_quantizer;
        pEnc->sRC.iMinQuantizer=pParam->min_quantizer;

	pEnc->iQuantizer=4;
	if(pEnc->sRC.iMaxQuantizer<pEnc->iQuantizer)
	    pEnc->iQuantizer=pEnc->sRC.iMaxQuantizer;
	if(pEnc->sRC.iMinQuantizer>pEnc->iQuantizer)
	    pEnc->iQuantizer=pEnc->sRC.iMinQuantizer;
	
        pEnc->iFrameNum=0;
	pEnc->iMaxKeyInterval=pParam->max_key_interval;
        if(CreateVop(&(pEnc->sCurrent), pEnc->iWidth, pEnc->iHeight)<0)
	{
	    free(pEnc);
	    return ENC_MEMORY;
	}
        if(CreateVop(&(pEnc->sReference), pEnc->iWidth, pEnc->iHeight)<0)
	{
	    FreeVop(&(pEnc->sCurrent));
	    free(pEnc);
	    return ENC_MEMORY;
	}
        pParam->handle=(void*)pEnc;
	
	RateCtlInit(&(pEnc->sRC), pEnc->iQuantizer,
		pParam->bitrate/pParam->framerate,
		pParam->rc_period,
		pParam->rc_reaction_period,
		pParam->rc_reaction_ratio);
#if !(defined(WIN32) && defined(_MMX_))
	init_fdct_enc();
	init_idct_enc();
#endif
	__init_rgb2yuv();
        return ENC_OK;
}

int FreeEncoder(Encoder* pEnc)
{
        ENC_CHECK(pEnc);
        ENC_CHECK(pEnc->sCurrent.pY);
        ENC_CHECK(pEnc->sReference.pY);
        FreeVop(&(pEnc->sCurrent));
        FreeVop(&(pEnc->sReference));
        free(pEnc);
        return ENC_OK;
}

int EncodeFrame(Encoder* pEnc, ENC_FRAME* pFrame, ENC_RESULT* pResult)
{
	int result;
	Image im;
	Bitstream bs;
	uint32_t bits;
        ENC_CHECK(pEnc);
	ENC_CHECK(pFrame);
	ENC_CHECK(pFrame->bitstream);
	ENC_CHECK(pFrame->image);
	
	if(pFrame->colorspace==ENC_CSP_RGB24)
	{
	    im.pY=(uint8_t*)malloc(pEnc->iWidth*pEnc->iHeight);
	    im.pU=(uint8_t*)malloc(pEnc->iWidth*pEnc->iHeight/4);
	    im.pV=(uint8_t*)malloc(pEnc->iWidth*pEnc->iHeight/4);
	    start_etimer();
	    __RGB2YUV(pEnc->iWidth, pEnc->iHeight, pFrame->image, im.pY, im.pU, im.pV, 1);
	    stop_conv_etimer();
	}
	else
	{
	    im.pY=(uint8_t*)pFrame->image;
	    im.pU=(uint8_t*)pFrame->image+pEnc->iWidth*pEnc->iHeight;
	    im.pV=(uint8_t*)pFrame->image+pEnc->iWidth*pEnc->iHeight*5/4;
	}
	
	BitstreamInit(&bs, pFrame->bitstream);
	
	if((pEnc->iFrameNum==0) 
	|| ( (pEnc->iMaxKeyInterval>0) && (pEnc->iFrameNum>=pEnc->iMaxKeyInterval) ))
	    result=EncodeKeyFrame(pEnc, &im, &bs, &bits);
	else
	    result=EncodeDeltaFrame(pEnc, &im, &bs, &bits);
	if(pResult)
	    pResult->is_key_frame=result;
	pFrame->length=BitstreamLength(&bs);
        pEnc->iFrameNum++;
	RateCtlUpdate(&(pEnc->sRC), bits);
	SwapVops(&(pEnc->sCurrent), &(pEnc->sReference));
	if(pFrame->colorspace==ENC_CSP_RGB24)
	{
	    free(im.pY);
	    free(im.pU);
	    free(im.pV);
	}
	return ENC_OK;
}

static int EncodeKeyFrame(Encoder* pEnc, Image* im, Bitstream* bs, uint32_t* pBits)
{
    int x=0, y=0;
    Vop* pCurrent=&(pEnc->sCurrent);
    pEnc->iFrameNum=0;
    pEnc->iRoundingType=1;
//    pEnc->iQuantizer=RateCtlGetQ(&(pEnc->sRC), MAD_Image(im, pCurrent));
    pEnc->iQuantizer=RateCtlGetQ(&(pEnc->sRC), 0);
    pCurrent->ePredictionType=I_VOP;

    BitstreamWriteHeader(bs, pEnc->iWidth, pEnc->iHeight, I_VOP,
	pEnc->iRoundingType, pEnc->iQuantizer, pEnc->iFcode);
    *pBits=BsPos(bs);
    for(y=0; y<pCurrent->iMbHcount; y++)
      for(x=0; x<pCurrent->iMbWcount; x++)
	{
	  pCurrent->pMBs[x+y*pCurrent->iMbWcount].mode=MODE_INTRA;
	  EncodeMacroblockIntra(pEnc, im, bs, x, y);
	}
#if (defined(LINUX) && defined(_MMX_))
    __asm__ ("emms\n\t");
#elif defined(WIN32)
	__asm emms;
#endif
    *pBits=BsPos(bs)-*pBits;
    pEnc->sStat.fMvPrevSigma=-1;
    pEnc->iFcode=2;
    return 1; // intra
}
#define INTRA_THRESHOLD 0.5

static int EncodeDeltaFrame(Encoder* pEnc, Image* im, Bitstream* bs, uint32_t* pBits)
{
    float fIntraCount, fSigma;
    int x=0, y=0;
    int iSearchRange;
    Vop* pCurrent=&(pEnc->sCurrent);
    Vop* pRef=&(pEnc->sReference);
    SetEdges(pRef);
    pEnc->iRoundingType=1-pEnc->iRoundingType;

    fIntraCount=MotionEstimateCompensate(pEnc, im);
    
    if(fIntraCount>INTRA_THRESHOLD)
	return EncodeKeyFrame(pEnc, im, bs, pBits);
	
//    pEnc->iQuantizer=RateCtlGetQ(&(pEnc->sRC), MAD_Image(im, pCurrent));
    pEnc->iQuantizer=RateCtlGetQ(&(pEnc->sRC), 0);
    pCurrent->ePredictionType=P_VOP;
	
    BitstreamWriteHeader(bs, pEnc->iWidth, pEnc->iHeight, P_VOP,
	pEnc->iRoundingType, pEnc->iQuantizer, pEnc->iFcode);
    *pBits=BsPos(bs);
    pEnc->sStat.iTextBits=0;
    pEnc->sStat.iMvBits=0;
    pEnc->sStat.iMvUpperCount=0;
    pEnc->sStat.iMvLowerCount=0;
    pEnc->sStat.iMvSum=0;
    for(y=0; y<pCurrent->iMbHcount; y++)
      for(x=0; x<pCurrent->iMbWcount; x++)
	EncodeMacroblockInter(pEnc, im, bs, x, y);
#if defined(LINUX) && defined(_MMX_)
    __asm__ ("emms\n\t");
#elif defined(WIN32)
    __asm emms;
#endif
    if(pEnc->sStat.iMvUpperCount+pEnc->sStat.iMvLowerCount==0)
	pEnc->sStat.iMvLowerCount=1;
    fSigma=sqrt((float)pEnc->sStat.iMvSum/(pEnc->sStat.iMvUpperCount+pEnc->sStat.iMvLowerCount));
    //    printf("Texture: %d bits, motion: %d bits, mv sigma: %f\n",
    //pEnc->sStat.iTextBits, pEnc->sStat.iMvBits, fSigma);
    iSearchRange=1<<(3+pEnc->iFcode);
    if((fSigma>iSearchRange/3) && (pEnc->iFcode<=3)) // maximum search range 128
    {
	pEnc->iFcode++;
	iSearchRange*=2;
	//	printf("New search range: %d\n", iSearchRange);
    }
    else
	if((fSigma<iSearchRange/6) 
	    && (pEnc->sStat.fMvPrevSigma>=0)
	    && (pEnc->sStat.fMvPrevSigma<iSearchRange/6)
	    && (pEnc->iFcode>=2)) // minimum search range 16
	    {
    		pEnc->iFcode--;
		iSearchRange/=2;
		//		printf("New search range: %d\n", iSearchRange);
	    }
    pEnc->sStat.fMvPrevSigma=fSigma;

    *pBits=BsPos(bs)-*pBits;
    return 0; //inter
}
