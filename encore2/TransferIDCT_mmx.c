#include "TransferIDCT.h"
 
void TransferIDCT_add(uint8_t* pDest, int16_t* pSrc, int stride)
{
//not scheduled
    __asm__ __volatile__
    (
    "movl $8, %%edi\n"
    "pxor %%mm2, %%mm2\n"
"1:\n"
    "movq (%%edx), %%mm0\n"
    "movq 8(%%edx), %%mm1\n"
    "movq (%%ecx), %%mm3\n"
    "movq %%mm3, %%mm4\n"
    "punpcklbw %%mm2, %%mm3\n"
    "punpckhbw %%mm2, %%mm4\n"
    "paddsw %%mm3, %%mm0\n"
    "paddsw %%mm4, %%mm1\n"
    "packuswb %%mm1, %%mm0\n"
    "movq %%mm0, (%%ecx)\n"
    "addl $16, %%edx\n"
    "addl %%eax, %%ecx\n"
    "decl %%edi\n"
    "jnz 1b\n"
    :
    : "c" (pDest), "d"(pSrc), "a"(stride)
    : "edi"
    );
}

void TransferIDCT_copy(uint8_t* pDest, int16_t* pSrc, int stride)
{
    __asm__ __volatile__
    (
    "movq (%%edx), %%mm0\n"
    "packuswb 8(%%edx), %%mm0\n"
    "movq 16(%%edx), %%mm1\n"
    "packuswb 24(%%edx), %%mm1\n"
    "movq 32(%%edx), %%mm2\n"
    "packuswb 40(%%edx), %%mm2\n"
    "movq 48(%%edx), %%mm3\n"
    "packuswb 56(%%edx), %%mm3\n"

    "movq %%mm0, (%%ecx)\n"
    "addl %%eax, %%ecx\n"
    "movq %%mm1, (%%ecx)\n"
    "addl %%eax, %%ecx\n"
    "movq %%mm2, (%%ecx)\n"
    "addl %%eax, %%ecx\n"
    "movq %%mm3, (%%ecx)\n"
    "addl %%eax, %%ecx\n"
    
    "movq 64(%%edx), %%mm0\n"
    "addl $64, %%edx\n"
    "packuswb 8(%%edx), %%mm0\n"
    "movq 16(%%edx), %%mm1\n"
    "packuswb 24(%%edx), %%mm1\n"
    "movq 32(%%edx), %%mm2\n"
    "packuswb 40(%%edx), %%mm2\n"
    "movq 48(%%edx), %%mm3\n"
    "packuswb 56(%%edx), %%mm3\n"
    
    "movq %%mm0, (%%ecx)\n"
    "addl %%eax, %%ecx\n"
    "movq %%mm1, (%%ecx)\n"
    "addl %%eax, %%ecx\n"
    "movq %%mm2, (%%ecx)\n"
    "addl %%eax, %%ecx\n"
    "movq %%mm3, (%%ecx)\n"

    :
    : "c" (pDest), "d"(pSrc), "a"(stride)
    : "edi"
    );
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
