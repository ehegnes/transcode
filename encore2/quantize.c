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
 *  quantize.c, H.263 quantization/dequantization module
 *
 *  Copyright (C) 2001  Project Mayo
 *
 *  Eugene Kuznetsov
 *
 *  DivX Advance Research Center <darc@projectmayo.com>
 *
 **************************************************************************/

#include "quantize.h"
#include "enctypes.h"
#include "timer.h"
/** 
    Quantization of v=n*2*iQuant-1:
      v -> (n*2*iQuant-1) * ( floor( (1<<17) / (2*iQuant)) + 1 )
      We require that 
        (n*2*iQuant-1) * ( floor( (1<<17) / (2*iQuant) ) + 1 ) < n * (1<<17)
	(n*2*iQuant-1) * ( (1<<17) / (2*iQuant) + err ) < n*(1<<17), where 0 < err <= 1
	n*(1<<17) - (1<<17)/(2*iQuant) + err*(n*2*iQuant-1) < n*(1<<17)
	1 < (1<<17)/(2*iQuant)/v
	v*2*iQuant < (1<<17)
    Since 2*iQuant<(1<<6), v<(1<<11), this value will always be quantized correctly.
    
    On the other hand, v=n*2*iQuant will always be quantized into n, because
    n*2*iQuant * ( floor( (1<<17) / (2*iQuant) ) + 1 ) >= n*2*iQuant*(1<<17)/(2*iQuant)=n*(1<<17).
    
    Popular CPUs perform multiplication+shift faster than division. Performance gain of
    this trick depends on used processor and varies from 100% ( 100 ms instead of 200 ms )
    for Pentium-60 to 30% for Celeron-700.
 **/
#define SCALE (1<<17)
static const int32_t multipliers[32]=
{
    SCALE, SCALE/2+1, SCALE/4+1, SCALE/6+1, SCALE/8+1, SCALE/10+1, SCALE/12+1, SCALE/14+1, 
    SCALE/16+1, SCALE/18+1, SCALE/20+1, SCALE/22+1, SCALE/24+1, SCALE/26+1, SCALE/28+1, SCALE/30+1, 
    SCALE/32+1, SCALE/34+1, SCALE/36+1, SCALE/38+1, SCALE/40+1, SCALE/42+1, SCALE/44+1, SCALE/46+1, 
    SCALE/48+1, SCALE/50+1, SCALE/52+1, SCALE/54+1, SCALE/56+1, SCALE/58+1, SCALE/60+1, SCALE/62+1
};

void quantize_intra(int16_t* psBlock, uint8_t iQuant, uint8_t iDcScaler)
{
    int i;
//    int32_t mult=(1<<17)/(2*iQuant)+1;
    int32_t mult;
    start_etimer();
    mult=multipliers[iQuant];
    psBlock[0]=(psBlock[0]+iDcScaler/2)/iDcScaler;
    if(psBlock[0]<0)psBlock[0]=0;
    if(psBlock[0]>255)psBlock[0]=255;
    for(i=1; i<64; i++)
    {
	int16_t sLevel=(abs(psBlock[i])*mult)>>17; ///(2*iQuant);
	if(psBlock[i]<0)sLevel=-sLevel;
	if(sLevel<-2048) sLevel=-2048;
	if(sLevel>2047) sLevel=2047;
	psBlock[i]=sLevel;
    }
    stop_quant_etimer();
}
void quantize_inter(int16_t* psBlock, uint8_t iQuant)
{
    int i;
    int32_t mult;//=(1<<17)/(2*iQuant)+1;
    start_etimer();
    mult=multipliers[iQuant];
    for(i=0; i<64; i++)
    {
	int16_t sLevel=abs(psBlock[i])-iQuant/2;
	if(sLevel<2*iQuant)
	{
	    psBlock[i]=0;
	    continue;
	}
	else
	    sLevel=(sLevel*mult)>>17;
	if(psBlock[i]<0)sLevel=-sLevel;
	if(sLevel<-2048) sLevel=-2048;
	if(sLevel>2047) sLevel=2047;
	psBlock[i]=sLevel;
    }
    stop_quant_etimer();
}
void dequantize_intra(int16_t* psBlock, uint8_t iQuant, uint8_t iDcScaler)
{
    int i;
    start_etimer();
    psBlock[0] *= iDcScaler;
    for(i=1; i<64; i++)
    {
	int iSign;
	if(!psBlock[i])
	    continue;
	if(psBlock[i]<0)
	{
	    iSign=-1;
	    psBlock[i]=-psBlock[i];
	}
	else
	    iSign=1;
	psBlock[i]=iQuant*(2*psBlock[i]+1);
	if(!(iQuant % 2))
	    psBlock[i]--;
	psBlock[i]*=iSign;	    
    }
    stop_iquant_etimer();
}
void dequantize_inter(int16_t* psBlock, uint8_t iQuant)
{
    int i;
    start_etimer();
    for(i=0; i<64; i++)
    {
	int iSign;
	if(!psBlock[i])
	    continue;
	if(psBlock[i]<0)
	{
	    iSign=-1;
	    psBlock[i]=-psBlock[i];
	}
	else
	    iSign=1;
	psBlock[i]=iQuant*(2*psBlock[i]+1);
	if(!(iQuant % 2))
	    psBlock[i]--;
	psBlock[i]*=iSign;	    
    }
    stop_iquant_etimer();
}
