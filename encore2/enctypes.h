#ifndef _ENCORE_ENCTYPES_H
#define _ENCORE_ENCTYPES_H

#include "portab.h"

typedef int8_t bool;

typedef struct Image_s
{
    uint8_t *pY;
    uint8_t *pU;
    uint8_t *pV;
} Image;

typedef enum
{
/* Inter-coded macroblock, 1 motion vector */    
   MODE_INTER=0,
/* Not implemented */ 
   MODE_INTER_Q=1,
/* Inter-coded macroblock, 4 motion vectors */
   MODE_INTER4V=2,
/* Intra-coded macroblock */
   MODE_INTRA=3,
/* Not implemented */ 
   MODE_INTRA_Q=4
} MBMODE;

typedef enum
{
    I_VOP=0,
    P_VOP=1
} VOP_TYPE;

typedef struct MotionVector_s
{
    int16_t x;
    int16_t y;    
} MotionVector;

typedef struct Bitstream_s
{
    uint8_t* pBuffer;
    uint8_t* pStart;
    uint8_t iPos;
} Bitstream;

typedef struct
{
    /* Motion vectors for this macroblock. Initialized in MotionEstimate() ( mot_est.c ).
    When mode is MODE_INTER or MODE_INTER_Q, all four vectors are equal. */
    MotionVector mvs[4];

    /* Values used for AC/DC prediction. Initialized in calc_acdc_prediction(), predictions.c */
    int16_t pred_values[6][15];
    
    /* Macroblock mode. Initialized for P-VOP's in MotionEstimate() ( mot_est.c ),
    for I-VOP's in EncodeKeyFrame() ( encoder.c ) */
    MBMODE mode; 

    /* only meaningful when mode=MODE_INTRA_Q or mode=MODE_INTER_Q ( i.e. never ) */
    uint8_t dquant; 
} Macroblock;

typedef struct Vop_s
{
/* Various kinds of dimensions */
    int iWidth;
    int iHeight;
    int iEdgedWidth;
    int iEdgedHeight;
    int iMbWcount;
    int iMbHcount;
/* Pointers to (0,0) pixels */
    uint8_t *pY;
    uint8_t *pU;
    uint8_t *pV;
    VOP_TYPE ePredictionType;
    Macroblock* pMBs;
} Vop;

typedef struct
{
    int iMaxQuantizer;
    int iMinQuantizer;
    
    double quant;
    int rc_period;
    double target_rate;
    double average_rate;
    double reaction_rate;
    double average_delta;
    double reaction_delta;
    double reaction_ratio;
} RateControl;

typedef struct
{
    int iTextBits;
    int iMvBits;
    float fMvPrevSigma;
    int iMvSum;
    int iMvUpperCount;
    int iMvLowerCount;
} Statistics;
typedef struct Encoder_s
{
    int iWidth;
    int iHeight;	
    
    /* Quantizer used for current frame 
	1<=iQuantizer<=31
	Adjusted by rate control methods */
    uint8_t iQuantizer;
    
    /* Motion estimation parameter 
     1<=iFcode<=4;  motion vector search range is +/- 1<<(iFcode+3) pixels
     Automatically adjusted using motion vector statistics inside
     EncodeDeltaFrame() ( encoder.c ) */
    uint8_t iFcode;
    
    /* Rounding type for image interpolation
     Switched 0->1 and back after each interframe
     Used in motion compensation, methods Interpolate*, vop.c */
    uint8_t iRoundingType;
    
    /* Motion estimation quality/performance balance 
     Supplied by user
     1<=iQuality<=9
     5: highest quality, slowest encoding ( everything turned on )
     4: faster 16x16 vector search method
        disabled half-pel search
	( + ~10% bitrate, +~40% fps )
     3: disabled search for 8x8 vectors
        ( + ~20% bitrate, +80-90% fps )
     2: lower quality of SAD calculation for 16x16 vectors
        ( + 30-50% bitrate, +120% fps )
     1: fastest encoding 
        ( + 60-70% bitrate, +150% fps )
	all bitrates & fps are relative to quality 9, performance is for
	non-MMX version.
     */    
    uint8_t iQuality;
    
    int iFrameNum;
    int iMaxKeyInterval;
        
    Vop sCurrent;
    Vop sReference;

    RateControl sRC;
    Statistics sStat;
} Encoder;

#endif
