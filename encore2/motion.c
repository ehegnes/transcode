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
 *  motion.c, motion estimation/compensation module
 *
 *  Copyright (C) 2001  Project Mayo
 *
 *  Eugene Kuznetsov
 *
 *  DivX Advance Research Center <darc@projectmayo.com>
 *
 **************************************************************************/

#include "motion.h"
#include "mad.h"
#include "predictions.h"
#include "vop.h"
#include "timer.h"
#include <assert.h>
#include <stdio.h>
#include "vlc_codes.h"
const int32_t MV_MAX_ERROR=4096*256; // very large value

/* Threshold for making a decision whether to make a macroblock inter or intra */
const int32_t iFavorInter=432;

/* Parameters which control inter/inter4v decision */
int32_t iMv16x16=5;
int32_t mv_size_parameter=4;

/* Vector map smoother parameters */
int32_t neigh_tend_16x16=2;
int32_t neigh_tend_8x8=2;

#define MIN(X, Y) ((X)<(Y)?(X):(Y))
#define MAX(X, Y) ((X)>(Y)?(X):(Y))
#define ABS(X) (((X)>0)?(X):-(X))
#define SIGN(X) (((X)>0)?1:-1)

//#define FULL_SEARCH_16    
//#define FULL_SEARCH_8

static __inline int32_t mv_bits(int16_t component, uint8_t iFcode)
{
//    int32_t scale=0;
    if(component<0)
	component=-component;
    if(component==0)
        return 1;
    if(iFcode==1)
    {
	if(component>32)
	    component=32;
	return mvtab[component].len+1;
    }
    component+=(1<<(iFcode-1))-1;
    component >>= (iFcode-1);
    if(component>32)
        component=32;
    return mvtab[component].len+1+iFcode-1;
}
static __inline int32_t calc_delta_16(int16_t dx, int16_t dy, uint8_t iFcode)
{
    return neigh_tend_16x16*(mv_bits(dx, iFcode)+mv_bits(dy, iFcode));
}
static __inline int32_t calc_delta_8(int16_t dx, int16_t dy, uint8_t iFcode)
{
    return neigh_tend_8x8*(mv_bits(dx, iFcode)+mv_bits(dy, iFcode));
}
static __inline int32_t get_mv_size(int16_t dx, int16_t dy, uint8_t iFcode)
{
    return mv_size_parameter*(mv_bits(dx, iFcode)+mv_bits(dy, iFcode));
}
/**
    Calculate allowed limits for motion search, depending on
    search range and macroblock position
**/
static void ObtainRange16(int x, int y, int x_dim, int y_dim,
    int16_t pred_x, int16_t pred_y, 
    int16_t* min_x_v, int16_t* max_x_v, 
    int16_t* min_y_v, int16_t* max_y_v,
    int iFcode, int iEdgeSize)
{
    int scale_fac = 1 << (iFcode - 1);
    int search_range = 16*scale_fac;
    int high = search_range - 3;
    int low = -search_range + 2;

    pred_x/=2;
    pred_y/=2;
    
    *max_x_v=MIN(MIN(search_range, (x_dim+iEdgeSize/2-2)-(x*16+pred_x)), high-pred_x);
    *max_y_v=MIN(MIN(search_range, (y_dim+iEdgeSize/2-2)-(y*16+pred_y)), high-pred_y);
    *min_x_v=MAX(MAX(-search_range, (-iEdgeSize/2+2)-(x*16+pred_x)), low-pred_x);
    *min_y_v=MAX(MAX(-search_range, (-iEdgeSize/2+2)-(y*16+pred_y)), low-pred_y);
}

static __inline int32_t SAD_Block_local(Image* pImage, Vop* pRef, Vop* pRefH, Vop* pRefV, Vop* pRefHV,
    int x, int y, int x_v, int y_v, int32_t sad_opt)
{
    int32_t sad;
    switch(((x_v%2)?2:0)+((y_v%2)?1:0))
    {
    case 0:
	sad=SAD_Block(pImage, pRef, x, y,
	    x_v/2, y_v/2, sad_opt, 0);
	break;
    case 1:	
	sad=SAD_Block(pImage, pRefV, x, y,
	    x_v/2, (y_v-1)/2, sad_opt, 0);
	break;
    case 2:
	sad=SAD_Block(pImage, pRefH, x, y,
	    (x_v-1)/2, y_v/2, sad_opt, 0);
        break;
    case 3:
    default:
	sad=SAD_Block(pImage, pRefHV, x, y,
    	    (x_v-1)/2, (y_v-1)/2, sad_opt, 0);
	break;
    }
    return sad;
}

/**
    Search for a single motion vector corresponding to macroblock (x,y),
    starting from motion vector <pred_x, pred_y> ( in half-pixels ),
    with search window determined by search_range & iFcode.
    Store the result in '*pmv' and return optimal SAD.
     pRef - reconstructed image
     pRefH - reconstructed image, interpolated along H axis.
     pRefV, pRefHV - same as above
**/        
static int32_t MotionSearch16(Vop* pRef, Vop* pRefH, Vop* pRefV, Vop* pRefHV,
    Image* pImage, int x, int y,
    int pred_x, int pred_y, int iFcode, int iQuant,
    int iQuality, MotionVector* pmv)
{
    int16_t min_x_v, max_x_v;
    int16_t min_y_v, max_y_v;
    int32_t sad_opt=MV_MAX_ERROR, sad_opt2=MV_MAX_ERROR; 
    int16_t x_v, y_v;
    int16_t x_opt, y_opt;
    int16_t x_opt2, y_opt2;
    int32_t sad;
    int delta=0;
    int bestdelta=0, bestdelta2=0;
    int nullflag=0;
    int first_pass=1;
    typedef struct
    {
    	int16_t x;
    	int16_t y;
    	int16_t start_nmbr;
    } DPoint;

    typedef struct
    {
    	DPoint point[8];
    } Diamond;

    int16_t d_type;
    int16_t stop_flag;
    int16_t pt_nmbr;
    int16_t total_check_pts;
    int16_t mot_dirn;
    int16_t d_centre_x;
    int16_t d_centre_y;
    int16_t check_pts;
    int16_t check_pt_x,check_pt_y;
    int16_t iEdgeSize;
    Diamond diamond[2]=
    {
    	{
    		{	{0,1,0},	{1,0,0},	{0,-1,0},	{-1,0,0}	}
    	}
	,
	{
		{
			{0,2,6},	{1,1,0},	{2,0,0},	{1,-1,2},
			{0,-2,2},	{-1,-1,4},	{-2,0,4},	{-1,1,6}	}
	}
    };
    if(iQuality<=4) first_pass=0;
    iEdgeSize=pRef->iEdgedWidth-pRef->iWidth;
    if(pRef->iWidth % 16)
	iEdgeSize+=(pRef->iWidth % 16)-16;
    ObtainRange16(x, y, pRef->iWidth, pRef->iHeight, 
	pred_x, pred_y, 
	&min_x_v, &max_x_v,
	&min_y_v, &max_y_v,
	iFcode, iEdgeSize);

    x_opt = -pred_x/2;
    y_opt = -pred_y/2;

#ifndef FULL_SEARCH_16   
    d_centre_x=-pred_x/2;
    d_centre_y=-pred_y/2;
start:
    d_type=1;
    stop_flag=0;
    pt_nmbr=0;
    total_check_pts=8;
    mot_dirn=0;
    do
    {
    	check_pts=total_check_pts;

    	do
	{
	    check_pt_x = diamond[d_type].point[pt_nmbr].x + d_centre_x;
	    check_pt_y = diamond[d_type].point[pt_nmbr].y + d_centre_y;

	    if(!(check_pt_x || check_pt_y))
		first_pass=0;

	    if ( check_pt_x < min_x_v || check_pt_x > max_x_v || check_pt_y < min_y_v || check_pt_y > max_y_v)
	    {
		sad = MV_MAX_ERROR;
	    }
	    else
	    {
		sad = SAD_Macroblock(pImage, pRef, pRefH, pRefV, pRefHV, x, y, 
		    pred_x+2*check_pt_x, pred_y+2*check_pt_y, sad_opt+bestdelta, iQuality);
		delta = calc_delta_16(2*check_pt_x, 2*check_pt_y, iFcode)*iQuant;
	    }
	    if ((sad+delta)<(sad_opt+bestdelta))
	    {
		sad_opt=sad;
		bestdelta=delta;
	        x_opt=check_pt_x;
	        y_opt=check_pt_y;
	        mot_dirn=pt_nmbr;
	    }

	    pt_nmbr+=1;
	    if((pt_nmbr)>= 8) pt_nmbr-=8;
	    check_pts-=1;
	}
	while(check_pts>0);

	if( d_type == 0)
        {
	    stop_flag = 1;
	}
	else
	{
	    if( (x_opt == d_centre_x) && (y_opt == d_centre_y) )
	    {
	        d_type=0;
	        pt_nmbr=0;
	        total_check_pts = 4;
	    }
	    else
	    {
	        if((x_opt==d_centre_x) ||(y_opt==d_centre_y))
	    	    total_check_pts=5;
	        else
	    	    total_check_pts=3;
	    	pt_nmbr=diamond[d_type].point[mot_dirn].start_nmbr;
		d_centre_x = x_opt;
		d_centre_y = y_opt;
	    }
	}
    }
    while(stop_flag!=1);
    if(first_pass)
    {
	first_pass=0;
	d_centre_x=0;
	d_centre_y=0; 
	sad_opt2=sad_opt;
	bestdelta2=bestdelta;
	sad_opt=MV_MAX_ERROR;
	x_opt2=x_opt;
	y_opt2=y_opt;
	goto start;
    }
    else
    {
	if((sad_opt2+bestdelta2)<(sad_opt+bestdelta))
	{
	    sad_opt=sad_opt2;
	    bestdelta=bestdelta2;
	    x_opt=x_opt2;
	    y_opt=y_opt2;
	}
    }

    sad = SAD_Macroblock(pImage, pRef, pRefH, pRefV, pRefHV, x, y, 
	0, 0, MV_MAX_ERROR, iQuality);
    if(sad<=iQuant*96)
	sad -= (128+1);
    delta=calc_delta_16(pred_x, pred_y, iFcode)*iQuant;
    if((sad+delta)<(sad_opt+bestdelta))
    {
	sad_opt=sad;
	nullflag=1;
	x_opt=-pred_x/2;
	y_opt=-pred_y/2;
	x_opt2=-pred_x;
	y_opt2=-pred_y;
    }
    else
    {
	x_opt2=2*x_opt;
	y_opt2=2*y_opt;
    }

    if(iQuality>3)
    for(x_v=2*x_opt-1; x_v<=2*x_opt+1; x_v++)
    {
	for(y_v=2*y_opt-1; y_v<=2*y_opt+1; y_v++)
#else
    for(check_pt_x=min_x_v; check_pt_x<max_x_v; check_pt_x++)
	for(check_pt_y=min_y_v; check_pt_y<max_y_v; check_pt_y++)
	{
	    sad = SAD_Macroblock(pImage, pRef, pRefH, pRefV, pRefHV, x, y, 
		    pred_x+2*check_pt_x, pred_y+2*check_pt_y, sad_opt+bestdelta, iQuality);
	    delta = calc_delta_16(2*check_pt_x, 2*check_pt_y, iFcode)*iQuant;
	    if ((sad+delta)<(sad_opt+bestdelta))
	    {
		sad_opt=sad;
		bestdelta=delta;
	        x_opt=2*check_pt_x;
	        y_opt=2*check_pt_y;
		nullflag=0;
	        mot_dirn=pt_nmbr;
	    }
	}
    if(nullflag)
    {
	pmv->x=0;
        pmv->y=0;    
	x_opt=-pred_x;
	y_opt=-pred_y;
    }
    x_opt2=x_opt;
    y_opt2=y_opt;
    for(x_v=x_opt-1; x_v<=x_opt+1; x_v++)
    {
	for(y_v=y_opt-1; y_v<=y_opt+1; y_v++)
#endif
	{
	    int32_t sad;
	    if(!x_v && !y_v) continue;
	    sad = SAD_Macroblock(pImage, pRef, pRefH, pRefV, pRefHV, 
		x, y, pred_x+x_v, pred_y+y_v, sad_opt+bestdelta, iQuality);
	    delta = calc_delta_16(x_v, y_v, iFcode)*iQuant;
	    if((sad+delta)<(sad_opt+bestdelta))
	    {
		x_opt2=x_v;
		y_opt2=y_v;
		sad_opt=sad;
		bestdelta=delta;
		nullflag=0;
	    }
	}
    }
    if(!nullflag)
    {
	pmv->x=pred_x+x_opt2;
	pmv->y=pred_y+y_opt2;    
    }
    else
    {
	pmv->x=pmv->y=0;
    }
    return sad_opt;
}
static int32_t MotionSearch8(Vop* pRef, Vop* pRefH, Vop* pRefV, Vop* pRefHV,
    Image* pImage, int x, int y,
    int16_t pred_x, int16_t pred_y, 
    int16_t start_x, int16_t start_y,
    int iFcode, int iQuant, MotionVector* pmv)
{
    int16_t min_x_v, max_x_v;
    int16_t min_y_v, max_y_v;
    int32_t sad_opt=MV_MAX_ERROR; 
    int16_t x_v, y_v;
    int16_t x_opt, y_opt;
    int16_t x_opt2, y_opt2;
    int32_t sad;
    int delta=0;
    int bestdelta=0;
    int nullflag=0;
    int first_pass=1;
    typedef struct
    {
    	int16_t x;
    	int16_t y;
    	int16_t start_nmbr;
    } DPoint;

    typedef struct
    {
    	DPoint point[8];
    } Diamond;

    int16_t d_type;
    int16_t stop_flag;
    int16_t pt_nmbr;
    int16_t total_check_pts;
    int16_t mot_dirn;
    int16_t d_centre_x;
    int16_t d_centre_y;
    int16_t check_pts;
    int16_t check_pt_x,check_pt_y;
    Diamond diamond[2]=
    {
    	{
    		{	{0,1,0},	{1,0,0},	{0,-1,0},	{-1,0,0}	}
    	}
	,
	{
		{
			{0,2,6},	{1,1,0},	{2,0,0},	{1,-1,2},
			{0,-2,2},	{-1,-1,4},	{-2,0,4},	{-1,1,6}	}
	}
    };
    
    ObtainRange16(x, y, pRef->iWidth, pRef->iHeight, 
	pred_x, pred_y, 
	&min_x_v, &max_x_v,
	&min_y_v, &max_y_v,
	iFcode, pRef->iEdgedWidth-pRef->iWidth);

    x_opt = -pred_x/2;
    y_opt = -pred_y/2;
#ifndef FULL_SEARCH_8
    d_centre_x=(start_x-pred_x)/2;
    d_centre_y=(start_y-pred_y)/2;    
    d_type=1;
    stop_flag=0;
    pt_nmbr=0;
    total_check_pts=8;
    mot_dirn=0;
    do
    {
    	check_pts=total_check_pts;

    	do
	{
	    check_pt_x = diamond[d_type].point[pt_nmbr].x + d_centre_x;
	    check_pt_y = diamond[d_type].point[pt_nmbr].y + d_centre_y;

	    if ( check_pt_x < min_x_v || check_pt_x > max_x_v || check_pt_y < min_y_v || check_pt_y > max_y_v)
	    {
		sad = MV_MAX_ERROR;
	    }
	    else
	    {
		sad = SAD_Block_local(pImage, pRef, pRefH, pRefV, pRefHV, x, y, 
		    pred_x+2*check_pt_x, 
		    pred_y+2*check_pt_y,
		    sad_opt+bestdelta);
		delta = calc_delta_8(2*check_pt_x, 2*check_pt_y, iFcode)*iQuant;
	    }
	    if ((sad+delta)<(sad_opt+bestdelta))
	    {
		sad_opt=sad;
		bestdelta=delta;
	        x_opt=check_pt_x;
	        y_opt=check_pt_y;
	        mot_dirn=pt_nmbr;
	    }

	    pt_nmbr+=1;
	    if((pt_nmbr)>= 8) pt_nmbr-=8;
	    check_pts-=1;
	}
	while(check_pts>0);

	if( d_type == 0)
        {
	    stop_flag = 1;
	}
	else
	{
	    if( (x_opt == d_centre_x) && (y_opt == d_centre_y) )
	    {
	        d_type=0;
	        pt_nmbr=0;
	        total_check_pts = 4;
	    }
	    else
	    {
	        if((x_opt==d_centre_x) ||(y_opt==d_centre_y))
	    	    total_check_pts=5;
	        else
	    	    total_check_pts=3;
	    	pt_nmbr=diamond[d_type].point[mot_dirn].start_nmbr;
		d_centre_x = x_opt;
		d_centre_y = y_opt;
	    }
	}
    }
    while(stop_flag!=1);

    x_opt2=2*x_opt;
    y_opt2=2*y_opt;

    for(x_v=2*x_opt-1; x_v<=2*x_opt+1; x_v++)
    {
	for(y_v=2*y_opt-1; y_v<=2*y_opt+1; y_v++)
#else
    for(check_pt_x=min_x_v; check_pt_x<max_x_v; check_pt_x++)
	for(check_pt_y=min_y_v; check_pt_y<max_y_v; check_pt_y++)
	{
	    sad = SAD_Block_local(pImage, pRef, pRefH, pRefV, pRefHV, x, y, 
		    pred_x+2*check_pt_x, pred_y+2*check_pt_y, sad_opt+bestdelta);
	    delta = calc_delta_8(2*check_pt_x, 2*check_pt_y, iFcode)*iQuant;
	    if ((sad+delta)<(sad_opt+bestdelta))
	    {
		sad_opt=sad;
		bestdelta=delta;
	        x_opt=2*check_pt_x;
	        y_opt=2*check_pt_y;
	    }
	}
    x_opt2=x_opt;
    y_opt2=y_opt;
    if(iQuality>4)
    for(x_v=x_opt-1; x_v<=x_opt+1; x_v++)
    {
	for(y_v=y_opt-1; y_v<=y_opt+1; y_v++)
#endif
	{
	    int32_t sad;
	    if(!x_v && !y_v) continue;
	    sad = SAD_Block_local(pImage, pRef, pRefH, pRefV, pRefHV, 
		x, y, pred_x+x_v, pred_y+y_v, sad_opt+bestdelta);
	    delta = calc_delta_8(x_v, y_v, iFcode)*iQuant;
	    if((sad+delta)<(sad_opt+bestdelta))
	    {
		x_opt2=x_v;
		y_opt2=y_v;
		sad_opt=sad;
		bestdelta=delta;
		nullflag=0;
	    }
	}
    }
    pmv->x=pred_x+x_opt2;
    pmv->y=pred_y+y_opt2;    
    return sad_opt;
}

static __inline void CompensateBlock(Vop* pVcur, 
	const Vop* pRefN, const Vop* pRefH, const Vop* pRefV, const Vop* pRefHV,
	int x, int y, int comp, const int dx, const int dy)
{
    int stride=pVcur->iEdgedWidth/(comp?2:1);
    uint8_t *pCur;
    const uint8_t *pRef;
    int i;
    const Vop* pVref;
    switch(((dx%2)?2:0) + ((dy%2)?1:0))
    {
    case 0:
	pVref=pRefN;
	break;
    case 1:
	pVref=pRefV;
	break;
    case 2:
	pVref=pRefH;
	break;
    case 3:
    default:
	pVref=pRefHV;
	break;	
    }
    switch(comp)
    {	
    case 0:
	pCur=pVcur->pY;
	pRef=pVref->pY;
	break;
    case 1:
	pCur=pVcur->pU;
	pRef=pVref->pU;
	break;
    case 2:
    default:
	pCur=pVcur->pV;
	pRef=pVref->pV;
	break;
    }
    x*=8;
    y*=8;
    pCur+=y*stride+x;
    if(dx%2)   
        x+=(dx-1)/2;
    else
        x+=dx/2;
    if(dy%2)   
        y+=(dy-1)/2;
    else
        y+=dy/2;
    pRef+=y*stride+x;
    for (i = 0; i < 8; i++) 
    {
	((uint32_t*)pCur)[0] = ((uint32_t*)pRef)[0];
	((uint32_t*)pCur)[1] = ((uint32_t*)pRef)[1];
	pRef += stride;
	pCur += stride;
    }
}


float MotionEstimateCompensate(Encoder* pEnc, Image* pImage)
{
    static const int roundtab[16] = {0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2};
    Vop* pCurrent=&(pEnc->sCurrent);
    Vop* pRef=&(pEnc->sReference);
    int32_t iIntra=0, iInter4v=0, iInter=0;
    const int iWcount=pCurrent->iMbWcount;
    const int iHcount=pCurrent->iMbHcount;
    int i,j;
    
    Vop vInterH, vInterV, vInterHV;
    start_etimer();
    CreateVop(&vInterH, pEnc->iWidth, pEnc->iHeight);
    CreateVop(&vInterV, pEnc->iWidth, pEnc->iHeight);
    CreateVop(&vInterHV, pEnc->iWidth, pEnc->iHeight);
    Interpolate(pRef, &vInterH, &vInterV, &vInterHV, 
	pEnc->iRoundingType, (pEnc->iQuality<=4));
    stop_inter_etimer();

    start_etimer();
    for(i=0; i<iHcount; i++)
	for(j=0; j<iWcount; j++)
	{
	    Macroblock* pMB=pCurrent->pMBs+j+i*pCurrent->iMbWcount;
	    int16_t pred_x=enc_find_pmv(pCurrent, j, i, 0, 0);
	    int16_t pred_y=enc_find_pmv(pCurrent, j, i, 0, 1);
	    MotionVector mv16;
	    int32_t sad8;
	    int32_t deviation;
	    int32_t mv_size=0;
	    int32_t sad16;
	    sad16=MotionSearch16(pRef, &vInterH, &vInterV, &vInterHV,
		pImage, j, i, pred_x, pred_y, 
		pEnc->iFcode, pEnc->iQuantizer, pEnc->iQuality, &mv16);
	    mv_size=-get_mv_size(mv16.x-pred_x, mv16.y-pred_y, pEnc->iFcode);
	    if(pEnc->iQuality>3)
	    {
	    sad8=MotionSearch8(pRef, &vInterH, &vInterV, &vInterHV, 
		pImage, 2*j, 2*i, pred_x, pred_y, mv16.x, mv16.y, pEnc->iFcode, pEnc->iQuantizer,  &pMB->mvs[0]);
	    mv_size+=get_mv_size(pMB->mvs[0].x-pred_x, pMB->mvs[0].y-pred_y, pEnc->iFcode);
	    pred_x=enc_find_pmv(pCurrent, j, i, 1, 0);
	    pred_y=enc_find_pmv(pCurrent, j, i, 1, 1);
	    sad8+=MotionSearch8(pRef, &vInterH, &vInterV, &vInterHV, 
		pImage, 2*j+1, 2*i, pred_x, pred_y, mv16.x, mv16.y, pEnc->iFcode, pEnc->iQuantizer,  &pMB->mvs[1]);
	    mv_size+=get_mv_size(pMB->mvs[1].x-pred_x, pMB->mvs[1].y-pred_y, pEnc->iFcode);
	    pred_x=enc_find_pmv(pCurrent, j, i, 2, 0);
	    pred_y=enc_find_pmv(pCurrent, j, i, 2, 1);
	    sad8+=MotionSearch8(pRef, &vInterH, &vInterV, &vInterHV, 
		pImage, 2*j, 2*i+1, pred_x, pred_y, mv16.x, mv16.y, pEnc->iFcode, pEnc->iQuantizer, &pMB->mvs[2]);
	    mv_size+=get_mv_size(pMB->mvs[2].x-pred_x, pMB->mvs[2].y-pred_y, pEnc->iFcode);
	    pred_x=enc_find_pmv(pCurrent, j, i, 3, 0);
	    pred_y=enc_find_pmv(pCurrent, j, i, 3, 1);
	    sad8+=MotionSearch8(pRef, &vInterH, &vInterV, &vInterHV,
		pImage, 2*j+1, 2*i+1, pred_x, pred_y, mv16.x, mv16.y, pEnc->iFcode, pEnc->iQuantizer, &pMB->mvs[3]);
	    mv_size+=get_mv_size(pMB->mvs[3].x-pred_x, pMB->mvs[3].y-pred_y, pEnc->iFcode);
	    }
	    deviation=SAD_Deviation_MB(pImage, j, i, pCurrent->iWidth);


	    if((pEnc->iQuality<=3)
		|| (sad16<(sad8+(iMv16x16*pEnc->iQuantizer+mv_size*pEnc->iQuantizer/4))))
	    {
		sad8=sad16;
		pMB->mode=MODE_INTER;
		pMB->mvs[0].x=pMB->mvs[1].x=
		pMB->mvs[2].x=pMB->mvs[3].x=
		    mv16.x;
		pMB->mvs[0].y=pMB->mvs[1].y=
		pMB->mvs[2].y=pMB->mvs[3].y=
		    mv16.y;
	    }
	    else	    
		pMB->mode=MODE_INTER4V;

	    if(deviation<(sad8-iFavorInter))
	    {
		pMB->mode=MODE_INTRA;
		pMB->mvs[0].x=pMB->mvs[1].x=
		pMB->mvs[2].x=pMB->mvs[3].x=
		    0;
		pMB->mvs[0].y=pMB->mvs[1].y=
		pMB->mvs[2].y=pMB->mvs[3].y=
		    0;
		iIntra++;
	    }

	    switch(pMB->mode)
            {
            case MODE_INTRA:
            case MODE_INTRA_Q:
                continue;
            case MODE_INTER:
            case MODE_INTER_Q:
                {
                    int16_t dx=pMB->mvs[0].x;
                    int16_t dy=pMB->mvs[0].y;
		    if(pEnc->iQuality<=3)
		    {
			assert(!(dx%2));
			assert(!(dy%2));
		    }
                    CompensateBlock(pCurrent,
			pRef, &vInterH, &vInterV, &vInterHV, 
			2*j, 2*i, 0, dx, dy);
                    CompensateBlock(pCurrent, 
			pRef, &vInterH, &vInterV, &vInterHV, 
			2*j, 2*i+1, 0, dx, dy);
                    CompensateBlock(pCurrent, 
			pRef, &vInterH, &vInterV, &vInterHV, 
			2*j+1, 2*i, 0, dx, dy);
                    CompensateBlock(pCurrent, 
			pRef, &vInterH, &vInterV, &vInterHV, 
			2*j+1, 2*i+1, 0, dx, dy);
                    if(!(dx%4))
                        dx/=2;
                    else
                        dx=(dx>>1)|1;
                    if(!(dy%4))
                        dy/=2;
                    else
                        dy=(dy>>1)|1;
                    CompensateBlock(pCurrent,
			pRef, &vInterH, &vInterV, &vInterHV, 
			j, i, 1, dx, dy);
                    CompensateBlock(pCurrent, 
			pRef, &vInterH, &vInterV, &vInterHV, 
			j, i, 2, dx, dy);
                }
		iInter++;
                break;
            case MODE_INTER4V:
		assert(pEnc->iQuality>=4);
                {
                    int16_t sum, dx, dy;
                    CompensateBlock(pCurrent, 
			pRef, &vInterH, &vInterV, &vInterHV, 
			2*j, 2*i, 0, pMB->mvs[0].x, pMB->mvs[0].y);
                    CompensateBlock(pCurrent, 
			pRef, &vInterH, &vInterV, &vInterHV,  
			2*j+1, 2*i, 0, pMB->mvs[1].x, pMB->mvs[1].y);
                    CompensateBlock(pCurrent, 
			pRef, &vInterH, &vInterV, &vInterHV,  
			2*j, 2*i+1, 0, pMB->mvs[2].x, pMB->mvs[2].y);
                    CompensateBlock(pCurrent, 
			pRef, &vInterH, &vInterV, &vInterHV,  
			2*j+1, 2*i+1, 0, pMB->mvs[3].x, pMB->mvs[3].y);
                    sum=pMB->mvs[0].x+pMB->mvs[1].x+pMB->mvs[2].x+pMB->mvs[3].x;
                    if (sum==0) 
			dx=0;
	    	    else
			dx=SIGN(sum)*(roundtab[ABS(sum)%16]+(ABS(sum)/16)*2);
                    sum=pMB->mvs[0].y+pMB->mvs[1].y+pMB->mvs[2].y+pMB->mvs[3].y;
                    if (sum==0) 
			dy=0;
	    	    else
			dy=SIGN(sum)*(roundtab[ABS(sum)%16]+(ABS(sum)/16)*2);
                    CompensateBlock(pCurrent, 
			pRef, &vInterH, &vInterV, &vInterHV,  
			j, i, 1, dx, dy);
                    CompensateBlock(pCurrent, 
    			pRef, &vInterH, &vInterV, &vInterHV,  
			j, i, 2, dx, dy);
                }
		iInter4v++;
                break;
            }//switch
	}
#if (defined(LINUX) && defined(_MMX_))
    __asm__ ("emms\n\t");
#elif defined(WIN32)
    __asm emms;
#endif
    FreeVop(&vInterH);
    FreeVop(&vInterV);
    FreeVop(&vInterHV);
    stop_motest_etimer();
    //    printf("Intra: %d Inter: %d Inter4v: %d\n",
    //	iIntra, iInter, iInter4v);
    return ((float)iIntra)/(pCurrent->iMbHcount*pCurrent->iMbWcount);
}
