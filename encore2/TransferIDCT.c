#include "TransferIDCT.h"

void TransferIDCT_add(uint8_t* pDest, int16_t* pSrc, int stride)
{
    int x, y;
    for(y=0; y<8; y++)
        for(x=0; x<8; x++)
        {
    	    int16_t tmp=pDest[x+y*stride]+pSrc[x+y*8];
	    if(tmp<0) tmp=0;
	    if(tmp>255) tmp=255;
	    pDest[x+y*stride]=(uint8_t)tmp;
        }
}

void TransferIDCT_copy(uint8_t* pDest, int16_t* pSrc, int stride)
{
    int x, y;
    for(y=0; y<8; y++)
        for(x=0; x<8; x++)
        {
    	    int16_t tmp=pSrc[x+y*8];
	    if(tmp<0) tmp=0;
	    if(tmp>255) tmp=255;
	    pDest[x+y*stride]=(uint8_t)tmp;
        }
}

void TransferFDCT_sub(uint8_t* pSrc1, uint8_t* pSrc2, 
    int16_t* pDest, int stride1, int stride2)
{
    int x, y;
    for(y=0; y<8; y++)
	for(x=0; x<8; x++)
	    pDest[x+y*8]=(int16_t)pSrc1[x+y*stride1]-(int16_t)pSrc2[x+y*stride2];
}

void TransferFDCT_copy(uint8_t* pSrc, int16_t* pDest, int stride)
{
    int x, y;
    for(y=0; y<8; y++)
	for(x=0; x<8; x++)
	    pDest[x+y*8]=(int16_t)pSrc[x+y*stride];
}
