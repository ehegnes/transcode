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
 *  mad.c, utility functions that calculate MADs and SADs.
 *
 *  Copyright (C) 2001  Project Mayo
 *
 *  Eugene Kuznetsov
 *
 *  DivX Advance Research Center <darc@projectmayo.com>
 *
 **************************************************************************/

#include "mad.h"
#define ABS(X) (((X)>0)?(X):-(X))
float MAD_Image(const Image* pIm, const Vop* pVop)
{
    int x, y;
    int32_t sum=0;
    int iStride=pVop->iWidth;
    int iStride2=pVop->iEdgedWidth;
    for(y=0; y<pVop->iHeight; y++)
	for(x=0; x<pVop->iWidth; x++)
	    sum+=ABS((int32_t)pVop->pY[x+y*iStride2]-(int32_t)pIm->pY[x+y*iStride]);
    iStride/=2;
    iStride2/=2;	    
    for(y=0; y<pVop->iHeight/2; y++)
	for(x=0; x<pVop->iWidth/2; x++)
	    sum+=ABS((int32_t)pVop->pU[x+y*iStride2]-(int32_t)pIm->pU[x+y*iStride]);
    for(y=0; y<pVop->iHeight/2; y++)
	for(x=0; x<pVop->iWidth/2; x++)
	    sum+=ABS((int32_t)pVop->pV[x+y*iStride2]-(int32_t)pIm->pV[x+y*iStride]);
    return ((float)sum)/(pVop->iWidth*pVop->iHeight*3/2);
}

// x & y in blocks ( 8 pixel units )
// dx & dy in pixels
int32_t SAD_Block(const Image* pIm, const Vop* pVop,
     int x, int y,
     int dx, int dy, 
     int sad_opt,
     int component)
{
    int32_t sum=0;
    int i, j;
    const uint8_t *pRef;
    const uint8_t *pCur;
    int iWidth=pVop->iWidth;
    int iEdgedWidth=pVop->iEdgedWidth;
    switch(component)
    {
    case 0:
	pRef=pIm->pY+x*8+y*8*pVop->iWidth;
	pCur=pVop->pY+(x*8+dx)+(y*8+dy)*pVop->iEdgedWidth;
	break;
    case 1:
	pRef=pIm->pU+x*8+y*8*pVop->iWidth/2;
	pCur=pVop->pU+(x*8+dx)+(y*8+dy)*pVop->iEdgedWidth/2;
	break;
    case 2:
    default:
	pRef=pIm->pV+x*8+y*8*pVop->iWidth/2;
	pCur=pVop->pV+(x*8+dx)+(y*8+dy)*pVop->iEdgedWidth/2;
	break;
    }
    if(component)
    {
	iWidth/=2;
	iEdgedWidth/=2;
    }
    for(i=0; i<8; i++)
    {
	for(j=0; j<8; j++)
	    sum+=ABS((int32_t)pRef[j]-(int32_t)pCur[j]);
	if(sum>sad_opt)
	    return sum;
	pRef+=iWidth;
	pCur+=iEdgedWidth;
    }
    return sum;
}

int32_t SAD_Macroblock(const Image* pIm, 
    const Vop* pVopN, const Vop* pVopH, const Vop* pVopV, const Vop* pVopHV,
    int x, int y, int dx, int dy, int sad_opt, int iQuality)
{
    const Vop* pVop;
    int32_t sum=0;
    int i, j;
    const uint8_t *pRef;
    const uint8_t *pCur;
    int iWidth=pVopN->iWidth;
    int iEdgedWidth=pVopN->iEdgedWidth;
    switch(((dx%2)?2:0)+((dy%2)?1:0))
    {
    case 0:
	pVop=pVopN;
	break;
    case 1:
	pVop=pVopV;
	dy--;
	break;
    case 2:
	pVop=pVopH;
	dx--;
	break;
    case 3:
    default:
	pVop=pVopHV;
	dx--;
	dy--;
	break;
    }
    dx/=2;
    dy/=2;

    pRef=pIm->pY+x*16+y*16*iWidth;
    pCur=pVop->pY+(x*16+dx)+(y*16+dy)*iEdgedWidth;

    switch(iQuality)
    {
    case 1:
	for(i=0; i<16; i+=4)
	{
	    for(j=0; j<16; j+=4)
		sum+=ABS((int32_t)pRef[j]-(int32_t)pCur[j]);
	    if(sum>sad_opt)
		return sum;
	    pRef+=iWidth;
	    pCur+=iEdgedWidth;
	}
        return sum*16;
    case 2:
	for(i=0; i<16; i+=2)
	{
	    for(j=0; j<16; j+=2)
		sum+=ABS((int32_t)pRef[j]-(int32_t)pCur[j]);
	    if(sum>sad_opt)
		return sum;
	    pRef+=iWidth;
	    pCur+=iEdgedWidth;
	}
        return sum*4;
    default:
	for(i=0; i<16; i++)
	{
	    for(j=0; j<16; j++)
		sum+=ABS((int32_t)pRef[j]-(int32_t)pCur[j]);
	    if(sum>sad_opt)
		return sum;
	    pRef+=iWidth;
	    pCur+=iEdgedWidth;
	}
        return sum;
    }
}

int32_t SAD_Deviation_MB(const Image* pIm, int x, int y, int width)
{
    int32_t sum=0, avg=0;
    const uint8_t *pRef;
    int i, j;

    pRef=pIm->pY+x*16+y*16*width;
    for(i=0; i<16; i++)
    {
	for(j=0; j<16; j++)
	    sum+=(int32_t)pRef[j];
	pRef+=width;
    }
    sum/=256;
    pRef=pIm->pY+x*16+y*16*width;
    for(i=0; i<16; i++)
    {
	for(j=0; j<16; j++)
	    avg+=ABS((int32_t)pRef[j]-sum);
	pRef+=width;
    }
    return avg;
}
