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
 *  bitstream.c, bit-level memory access functions
 *
 *  Copyright (C) 2001  Project Mayo
 *
 *  Eugene Kuznetsov
 *
 *  DivX Advance Research Center <darc@projectmayo.com>
 *
 **************************************************************************/

#include "bitstream.h"
#include <assert.h>
void BitstreamInit(Bitstream* bs, void* pointer)
{
    bs->pStart=bs->pBuffer=(uint8_t*)pointer;
    bs->iPos=0;
    *(int32_t*)(bs->pBuffer)=0;
}

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
 
uint32_t BitstreamLength(const Bitstream* bs)
{
    uint32_t iLength=(uint32_t)bs->pBuffer-(uint32_t)bs->pStart;
    if(bs->iPos)
        iLength++;
    return iLength;
}

void BitstreamPad(Bitstream* bs)
{
    if(bs->iPos)
    {
	bs->iPos=0;
	bs->pBuffer++;
	*(int32_t*)bs->pBuffer=0;
    }
}

#define VO_START_CODE		0x8
#define VOL_START_CODE	0x12
#define VOP_START_CODE	0x1b6

#define I_VOP		0
#define P_VOP		1
#define B_VOP		2

#define RECTANGULAR				0
#define BINARY						1
#define BINARY_SHAPE_ONLY 2 
#define GREY_SCALE				3

void BitstreamWriteHeader(Bitstream* bs, 
    int iWidth, int iHeight,
    VOP_TYPE ePredictionType, int iRoundingType, 
    uint8_t iQuant, uint8_t iFcode)
{
	if(ePredictionType!=I_VOP)
	{
		assert(iFcode>=1);
		assert(iFcode<=4);
	}
/* Here goes volhdr */
    BitstreamPutBits(bs, VO_START_CODE, 27);
    BitstreamPutBits(bs, 0, 5); //vo_id
    BitstreamPutBits(bs, VOL_START_CODE, 28);
    BitstreamPutBits(bs, 0, 4); //vol_id
    BitstreamPutBits(bs, 0, 1); //random_accessible_vol
    BitstreamPutBits(bs, 0, 8); //type_indication
    BitstreamPutBits(bs, 0, 1); //is_object_layer_identifier
    BitstreamPutBits(bs, 0, 4); //aspect_ratio_info
    BitstreamPutBits(bs, 0, 1); //vol_control_parameters
    BitstreamPutBits(bs, RECTANGULAR, 2); //shape
    BitstreamPutBits(bs, 1, 1); //marker
    BitstreamPutBits(bs, 1, 16); //time_increment_resolution
    BitstreamPutBits(bs, 1, 1); //marker
    BitstreamPutBits(bs, 0, 1); //fixed_vop_rate
    
    BitstreamPutBits(bs, 1, 1); //marker
    BitstreamPutBits(bs, iWidth, 13);
    BitstreamPutBits(bs, 1, 1); //marker
    BitstreamPutBits(bs, iHeight, 13);
    BitstreamPutBits(bs, 1, 1); //marker
    BitstreamPutBits(bs, 0, 1); //interlaced
    BitstreamPutBits(bs, 0, 1); //obmc_disable
    BitstreamPutBits(bs, 0, 1); //sprite_usage
    BitstreamPutBits(bs, 0, 1); //code
    BitstreamPutBits(bs, 0, 1); //H.263 quant_type
    BitstreamPutBits(bs, 1, 1); //complexity_estimation_disable
    BitstreamPutBits(bs, 0, 1); //error_res_disable
    BitstreamPutBits(bs, 0, 1); //data_partitioning
    BitstreamPutBits(bs, 0, 1); //scalability
/* Here goes vophdr */
    BitstreamPad(bs);
// some strange stuff with byte alignment here!    
    BitstreamPutBits(bs, VOP_START_CODE,32);
    BitstreamPutBits(bs, ePredictionType,2);
    BitstreamPutBits(bs, 0, 1); // time_base = 0
    BitstreamPutBits(bs, 1, 1); //marker
    BitstreamPutBits(bs, 0, 1); // time_inc = 0
    BitstreamPutBits(bs, 1, 1); //marker
    BitstreamPutBits(bs, 1, 1); //coded
    if(ePredictionType!=I_VOP)
	BitstreamPutBits(bs, iRoundingType, 1);
    //index = GetVolConfigModTimeBase(bs, vol_config, 1);
//    BitstreamPutBits(bs, 1,1);
//    BitstreamPutBits(bs, 0,1);
    BitstreamPutBits(bs, 0,3);//intra_dc_vlc_threshold
    BitstreamPutBits(bs, iQuant, 5);
    if(ePredictionType!=I_VOP)
	BitstreamPutBits(bs, iFcode, 3);
}

