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
 *  vop.c, various VOP-level utility functions.
 *
 *  Copyright (C) 2001  Project Mayo
 *
 *  Eugene Kuznetsov
 *
 *  DivX Advance Research Center <darc@projectmayo.com>
 *
 **************************************************************************/

#include "enctypes.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "halfpel.h"


const int iEdgeSize=32;
int CreateVop(Vop* pVop, int width, int height)
{
    assert(pVop);
    pVop->iWidth=width;
    pVop->iHeight=height;
    pVop->iMbWcount=(width+15)/16;
    pVop->iMbHcount=(height+15)/16;
//    pVop->iEdgedWidth=width+2*iEdgeSize;
//    pVop->iEdgedHeight=height+2*iEdgeSize;
    pVop->iEdgedWidth=pVop->iMbWcount*16+2*iEdgeSize;
    pVop->iEdgedHeight=pVop->iMbHcount*16+2*iEdgeSize;
    /** allocate a little more memory, so that MMX routines won't run over
    the buffer **/
    pVop->pY=(uint8_t*)malloc(pVop->iEdgedWidth*pVop->iEdgedHeight+64);
    if(pVop->pY==0)
	return -1;
    pVop->pY+=(iEdgeSize+iEdgeSize*pVop->iEdgedWidth);
    pVop->pU=(uint8_t*)malloc(pVop->iEdgedWidth*pVop->iEdgedHeight/4+64);
    if(pVop->pU==0)
    {
	free(pVop->pY-(iEdgeSize+iEdgeSize*pVop->iEdgedWidth));
	return -1;    
    }
    pVop->pU+=(iEdgeSize/2+iEdgeSize/2*pVop->iEdgedWidth/2);
    pVop->pV=(uint8_t*)malloc(pVop->iEdgedWidth*pVop->iEdgedHeight/4+64);
    if(pVop->pV==0)
    {
	free(pVop->pY-(iEdgeSize+iEdgeSize*pVop->iEdgedWidth));
	free(pVop->pU-(iEdgeSize/2+iEdgeSize/2*pVop->iEdgedWidth/2));
	return -1;
    }
    pVop->pV+=(iEdgeSize/2+iEdgeSize/2*pVop->iEdgedWidth/2);
    pVop->pMBs=(Macroblock*)malloc(pVop->iMbWcount*pVop->iMbHcount*sizeof(Macroblock));
    if(pVop->pMBs==0)
    {
	free(pVop->pY-(iEdgeSize+iEdgeSize*pVop->iEdgedWidth));
	free(pVop->pU-(iEdgeSize/2+iEdgeSize/2*pVop->iEdgedWidth/2));
	free(pVop->pV-(iEdgeSize/2+iEdgeSize/2*pVop->iEdgedWidth/2));
	return -1;
    }
    return 0;
}

void FreeVop(Vop* pVop)
{
    uint8_t* pTmp;
    assert(pVop->pY);
    assert(pVop->pU);
    assert(pVop->pV);
    pVop->pY-=(iEdgeSize+iEdgeSize*(pVop->iEdgedWidth));
    pVop->pU-=(iEdgeSize/2+iEdgeSize/2*(pVop->iEdgedWidth)/2);
    pVop->pV-=(iEdgeSize/2+iEdgeSize/2*(pVop->iEdgedWidth)/2);
    pTmp=pVop->pY;
    pVop->pY=0;
    free(pTmp);    
    free(pVop->pU);    
    free(pVop->pV);
    free(pVop->pMBs); 
}

void SwapVops(Vop* pVop1, Vop* pVop2)
{
    uint8_t* tmp;

    assert(pVop1);
    assert(pVop2);
    assert(pVop1->iWidth==pVop2->iWidth);
    assert(pVop1->iHeight==pVop2->iHeight);
    
    tmp=pVop1->pY; pVop1->pY=pVop2->pY; pVop2->pY=tmp;
    tmp=pVop1->pU; pVop1->pU=pVop2->pU; pVop2->pU=tmp;
    tmp=pVop1->pV; pVop1->pV=pVop2->pV; pVop2->pV=tmp;
}

void SetEdges(Vop* pVop)
{
    uint8_t* c_ptr;
    unsigned char* src_ptr;

    int i;
    int c_width;
    int _width, _height;
    assert(pVop);
    c_width=pVop->iEdgedWidth;
    _width=pVop->iWidth;
    _height=pVop->iHeight;
// Y
    c_ptr=pVop->pY-(iEdgeSize+iEdgeSize*c_width);
    src_ptr=pVop->pY;
    for(i=0; i<iEdgeSize; i++)
    {
        memset(c_ptr, *src_ptr, iEdgeSize);
        memcpy(c_ptr+iEdgeSize, src_ptr, _width);
        memset(c_ptr+c_width-iEdgeSize, src_ptr[_width-1], iEdgeSize);
        c_ptr+=c_width;
    }	
    for(i=0; i<_height; i++)
    {
        memset(c_ptr, *src_ptr, iEdgeSize);
        memset(c_ptr+c_width-iEdgeSize, src_ptr[_width-1], iEdgeSize);	
        c_ptr+=c_width;
        src_ptr+=c_width;
    }
    src_ptr-=c_width;
    for(i=0; i<iEdgeSize; i++)
    {
	memset(c_ptr, *src_ptr, iEdgeSize);
	memcpy(c_ptr+iEdgeSize, src_ptr, _width);
	memset(c_ptr+c_width-iEdgeSize, src_ptr[_width-1], iEdgeSize);
	c_ptr+=c_width;	    
    }	

//U
    c_ptr=pVop->pU-(iEdgeSize/2+iEdgeSize/2*c_width/2);
    src_ptr=pVop->pU;
	for(i=0; i<iEdgeSize/2; i++)
	{
	    memset(c_ptr, src_ptr[0], iEdgeSize/2);
	    memcpy(c_ptr+iEdgeSize/2, src_ptr, _width/2);
	    memset(c_ptr+c_width/2-iEdgeSize/2, src_ptr[_width/2-1], iEdgeSize/2);
	    c_ptr+=c_width/2;
	}
	for(i=0; i<_height/2; i++)
	{
	    memset(c_ptr, src_ptr[0], iEdgeSize/2);
	    memset(c_ptr+c_width/2-iEdgeSize/2, src_ptr[_width/2-1], iEdgeSize/2);
	    c_ptr+=c_width/2;
	    src_ptr+=c_width/2;
	}
	src_ptr-=c_width/2;
	for(i=0; i<iEdgeSize/2; i++)
	{
	    memset(c_ptr, src_ptr[0], iEdgeSize/2);
	    memcpy(c_ptr+iEdgeSize/2, src_ptr, _width/2);
	    memset(c_ptr+c_width/2-iEdgeSize/2, src_ptr[_width/2-1], iEdgeSize/2);
	    c_ptr+=c_width/2;
	}
// V
    c_ptr=pVop->pV-(iEdgeSize/2+iEdgeSize/2*c_width/2);
    src_ptr=pVop->pV;
	for(i=0; i<iEdgeSize/2; i++)
	{
	    memset(c_ptr, src_ptr[0], iEdgeSize/2);
	    memcpy(c_ptr+iEdgeSize/2, src_ptr, _width/2);
	    memset(c_ptr+c_width/2-iEdgeSize/2, src_ptr[_width/2-1], iEdgeSize/2);
	    c_ptr+=c_width/2;
	}
	for(i=0; i<_height/2; i++)
	{
	    memset(c_ptr, src_ptr[0], iEdgeSize/2);
	    memset(c_ptr+c_width/2-iEdgeSize/2, src_ptr[_width/2-1], iEdgeSize/2);
	    c_ptr+=c_width/2;
	    src_ptr+=c_width/2;
	}
	src_ptr-=c_width/2;
	for(i=0; i<iEdgeSize/2; i++)
	{
	    memset(c_ptr, src_ptr[0], iEdgeSize/2);
	    memcpy(c_ptr+iEdgeSize/2, src_ptr, _width/2);
	    memset(c_ptr+c_width/2-iEdgeSize/2, src_ptr[_width/2-1], iEdgeSize/2);
	    c_ptr+=c_width/2;
	}

}

void Interpolate(const Vop* pRef, Vop* pInterH, Vop* pInterV, Vop* pInterHV, const int iRounding, int iChromOnly)
{
    int iSize=iEdgeSize;
    int offset;
//Y
    if(!iChromOnly)
    {
	offset=iSize*(pRef->iEdgedWidth+1);
	interpolate_halfpel_h(pRef->pY-offset, pInterH->pY-offset, pRef->iEdgedWidth, pRef->iEdgedHeight, iRounding);
	interpolate_halfpel_v(pRef->pY-offset, pInterV->pY-offset, pRef->iEdgedWidth, pRef->iEdgedHeight, iRounding);
	interpolate_halfpel_hv(pRef->pY-offset, pInterHV->pY-offset, pRef->iEdgedWidth, pRef->iEdgedHeight, iRounding);
    }
// U
    iSize/=2;
    offset=iSize*(pRef->iEdgedWidth/2+1);
    interpolate_halfpel_h(pRef->pU-offset, pInterH->pU-offset, pRef->iEdgedWidth/2, pRef->iEdgedHeight/2, iRounding);
    interpolate_halfpel_v(pRef->pU-offset, pInterV->pU-offset, pRef->iEdgedWidth/2, pRef->iEdgedHeight/2, iRounding);
    interpolate_halfpel_hv(pRef->pU-offset, pInterHV->pU-offset, pRef->iEdgedWidth/2, pRef->iEdgedHeight/2, iRounding);
// V
    interpolate_halfpel_h(pRef->pV-offset, pInterH->pV-offset, pRef->iEdgedWidth/2, pRef->iEdgedHeight/2, iRounding);
    interpolate_halfpel_v(pRef->pV-offset, pInterV->pV-offset, pRef->iEdgedWidth/2, pRef->iEdgedHeight/2, iRounding);
    interpolate_halfpel_hv(pRef->pV-offset, pInterHV->pV-offset, pRef->iEdgedWidth/2, pRef->iEdgedHeight/2, iRounding);
}
/*
void InterpolateH(const Vop* pRef, Vop* pInterpolated, const int iRounding, int iChromOnly)
{
    int i;
    const uint8_t* pSrc;
    uint8_t* pDest;
    int iSize=iEdgeSize;
//Y
    if(!iChromOnly)
    {
	pSrc=pRef->pY-iSize*(pRef->iEdgedWidth+1);
        pDest=pInterpolated->pY-iSize*(pRef->iEdgedWidth+1);
	interpolate_halfpel_h(pSrc, pDest, pRef->iEdgedWidth, pRef->iEdgedHeight, iRounding);
    }
// U
    iSize/=2;

    pSrc=pRef->pU-iSize*(pRef->iEdgedWidth/2+1);
    pDest=pInterpolated->pU-iSize*(pRef->iEdgedWidth/2+1);
    interpolate_halfpel_h(pSrc, pDest, pRef->iEdgedWidth/2, pRef->iEdgedHeight/2, iRounding);
// V
    pSrc=pRef->pV-iSize*(pRef->iEdgedWidth/2+1);
    pDest=pInterpolated->pV-iSize*(pRef->iEdgedWidth/2+1);
    interpolate_halfpel_h(pSrc, pDest, pRef->iEdgedWidth/2, pRef->iEdgedHeight/2, iRounding);
}

void InterpolateV(const Vop* pRef, Vop* pInterpolated, const int iRounding, int iChromOnly)
{
    int i;
    const uint8_t* pSrc;
    uint8_t* pDest;
    int iSize=iEdgeSize;
    int iStride=pRef->iEdgedWidth;
//Y
    if(!iChromOnly)
    {
	pSrc=pRef->pY-iSize*(iStride+1);
	pDest=pInterpolated->pY-iSize*(iStride+1);
	interpolate_halfpel_v(pSrc, pDest, pRef->iEdgedWidth, pRef->iEdgedHeight, iRounding);
    }
// U
    iSize/=2;
    iStride/=2;

    pSrc=pRef->pU-iSize*(iStride+1);
    pDest=pInterpolated->pU-iSize*(iStride+1);
    interpolate_halfpel_v(pSrc, pDest, pRef->iEdgedWidth/2, pRef->iEdgedHeight/2, iRounding);
// V
    pSrc=pRef->pV-iSize*(iStride+1);
    pDest=pInterpolated->pV-iSize*(iStride+1);
    interpolate_halfpel_v(pSrc, pDest, pRef->iEdgedWidth/2, pRef->iEdgedHeight/2, iRounding);
}

void InterpolateHV(const Vop* pRef, Vop* pInterpolated, const int iRounding, int iChromOnly)
{
    int i;
    const uint8_t* pSrc;
    uint8_t* pDest;
    int iSize=iEdgeSize;
    int iStride=pRef->iEdgedWidth;
//Y
    if(!iChromOnly)
    {
	pSrc=pRef->pY-iSize*(iStride+1);
	pDest=pInterpolated->pY-iSize*(iStride+1);
	interpolate_halfpel_hv(pSrc, pDest, pRef->iEdgedWidth, pRef->iEdgedHeight, iRounding);
    }
// U
    iSize/=2;
    iStride/=2;

    pSrc=pRef->pU-iSize*(iStride+1);
    pDest=pInterpolated->pU-iSize*(iStride+1);
    interpolate_halfpel_hv(pSrc, pDest, pRef->iEdgedWidth/2, pRef->iEdgedHeight/2, iRounding);
// V
    pSrc=pRef->pV-iSize*(iStride+1);
    pDest=pInterpolated->pV-iSize*(iStride+1);
    interpolate_halfpel_hv(pSrc, pDest, pRef->iEdgedWidth/2, pRef->iEdgedHeight/2, iRounding);
}

*/
