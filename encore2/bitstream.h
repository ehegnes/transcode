#ifndef _ENCORE_BITSTREAM_H
#define _ENCORE_BITSTREAM_H
#include "enctypes.h"
#include <assert.h>

/* Reset the bitstream */
void BitstreamInit(Bitstream* bs, void* pointer);

/* Put 'size' bits from 'value' into bitstream, most significant bit first */
static void __inline BitstreamPutBits(Bitstream* bs, uint32_t value, uint32_t size);
/* Special case of size=1 */
static void __inline BitstreamPutBit(Bitstream* bs, uint8_t bit);

/* Write a fake MPEG-4 frame header into the bitstream */
void BitstreamWriteHeader(Bitstream* bs, 
    int iWidth, int iHeight, VOP_TYPE ePredictionType, int iRoundingType, 
    uint8_t iQuant, uint8_t iFcode);

/* Append zero bits to the end of bitstream to byte boundary ( needed in 
VOP header initialization ) */
void BitstreamPad(Bitstream* bs);

/* Return length of bitstream in bytes, rounded up */
uint32_t BitstreamLength(const Bitstream* bs);

/* Return exact length of bitstream in bits */
static uint32_t __inline BsPos(const Bitstream* bs);

/************
    Implementation
************/
#if defined(LINUX) && defined(__i386__)
#define _SWAP(a,b) b=*(int*)a; \
	__asm__ ( "bswapl %0\n" : "=r" (b) : "0" (b) )
#define _SWAP2(a) \
	__asm__ ( "bswapl %0\n" : "=r" (a) : "0" (a) )
	
#elif defined(WIN32)
#define _SWAP(a,b) \
	b=*(int*)a; __asm mov eax,b __asm bswap eax __asm mov b, eax
#define _SWAP2(a) \
	__asm mov eax,a __asm bswap eax __asm mov a, eax
#else
#define _SWAP(a,b) (b=((a[0] << 24) | (a[1] << 16) | (a[2] << 8) | a[3]))
#define _SWAP2(a)	\
	{	\
		char tmp[4];	\
		tmp[0]=(a >> 24); tmp[1]=(int8_t)(a >> 16); 	\
		tmp[2]=(int8_t)(a >> 8); tmp[3]=(int8_t)a;		\
		a=*(int*)tmp;	\
	}	
#endif

static void __inline BitstreamSkip(Bitstream* bs, uint32_t bits)
{
    bs->iPos+=bits;
    if(bs->iPos>=8)
    {
	bs->pBuffer+=bs->iPos/8;
	bs->iPos%=8;
	*(int32_t*)(bs->pBuffer+1)=0;
    }
	if(bs->iPos==0)
		bs->pBuffer[0]=0;
}

static void __inline BitstreamPutBit(Bitstream* bs, uint8_t bit)
{
    if(bit)
	bs->pBuffer[0] |= (0x80>>bs->iPos);
    BitstreamSkip(bs, 1);
}

static void __inline BitstreamPutBits(Bitstream* bs, uint32_t value, uint32_t size)
{
/*
    while(size)
    {
        if(bs->iPos==0)
            bs->pBuffer[0]=0;
        if(value & (1<<(size-1)))
            bs->pBuffer[0] |= 1<<(7-bs->iPos);
        bs->iPos++;
        size--;
        if(bs->iPos==8)
        {
            bs->iPos=0;
            bs->pBuffer++;
        }
    }
*/
    unsigned int b;
	uint8_t* p=bs->pBuffer;
    assert(size<=32);
    _SWAP(p, b);
    b |= value << (32-bs->iPos-size);
    _SWAP2(b);
	*(int*)p=b;
    BitstreamSkip(bs, size);
}

static uint32_t __inline BsPos(const Bitstream* bs)
{
    return 8*((uint32_t)bs->pBuffer-(uint32_t)bs->pStart)+bs->iPos;
}

#endif
