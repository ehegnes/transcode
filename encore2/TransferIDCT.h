#ifndef _ENCORE_TRANSFERIDCT_H
#define _ENCORE_TRANSFERIDCT_H

#include "portab.h"
void TransferIDCT_add(uint8_t* pDest, int16_t* pSrc, int stride);
void TransferIDCT_copy(uint8_t* pDest, int16_t* pSrc, int stride);
void TransferFDCT_sub(uint8_t* pSrc1, uint8_t* pSrc2, 
    int16_t* pDest, int stride, int stride2);
void TransferFDCT_copy(uint8_t* pSrc, int16_t* pDest, int stride);

#endif
