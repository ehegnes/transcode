#include "halfpel.h"
 
void interpolate_halfpel_h(uint8_t *src, 
	uint8_t *dstH, 
	int width, 
	int height,
	int iRounding)
{
    int i;

    for(i=0; i<width*height-1; i++)
    {
        *dstH++=((uint32_t)src[0]+(uint32_t)src[1]+1-iRounding)>>1;
        src++;
    }
}

void interpolate_halfpel_v(uint8_t *src, 
	uint8_t *dstH, 
	int width,
	int height,
	int iRounding)
{
    int i;

    for(i=0; i<width*(height-1); i++)
    {
        *dstH++=((uint32_t)src[0]+(uint32_t)src[width]+1-iRounding)>>1;
        src++;
    }
}
	
void interpolate_halfpel_hv(uint8_t *src, 
	uint8_t *dstH, 
	int width,
	int height,
	int iRounding)
{
    int i;

    for(i=0; i<width*(height-1)-1; i++)
    {
	*dstH++=(
	    (uint32_t)src[0]+(uint32_t)src[width]+
	    (uint32_t)src[1]+(uint32_t)src[width+1]+
	    2-iRounding)>>2;
	src++;
    }
}
