/***********************************************************
 * YUVDENOISER for the mjpegtools                          *
 * ------------------------------------------------------- *
 * (C) 2001,2002 Stefan Fendt                              *
 *                                                         *
 * Licensed and protected by the GNU-General-Public-       *
 * License version 2 or if you prefer any later version of *
 * that license). See the file LICENSE for detailed infor- *
 * mation.                                                 *
 ***********************************************************/

#ifndef __DENOISER_GLOBAL_H__
#define __DENOISER_GLOBAL_H__

#define Yy (0)
#define Cr (1)
#define Cb (2)

#define Y_LO_LIMIT 16
#define Y_HI_LIMIT 235
#define C_LO_LIMIT 16
#define C_HI_LIMIT 235

#define W  (denoiser.frame.w)
#define H  (denoiser.frame.h)
#define W2 (denoiser.frame.w/2)
#define H2 (denoiser.frame.h/2)

/* does not compile with gcc3 and produces weird results */
#undef HAVE_ASM_MMX

struct DNSR_GLOBAL
  {
    /* denoiser mode */
    /* 0 progressive */
    /* 1 interlaced */
    /* 2 PASS II only */
    uint32_t   mode;
    
    /* Search radius */    
    uint32_t   radius;

    /* Copy threshold */
    uint32_t   threshold;
    uint32_t   pp_threshold;
    
    /* Time-average-delay */
    uint32_t   delay;
    
    /* Deinterlacer to be turned on? */    
    uint32_t   deinterlace;

    /* Postprocessing */
    uint32_t   postprocess;
    uint32_t   luma_contrast;
    uint32_t   chroma_contrast;
    uint32_t   sharpen;

    /* scene change detection */
    uint32_t   do_reset;
    uint32_t   reset;
    uint32_t   block_thres;
    uint32_t   scene_thres;
    
    /* Frame information */
    struct 
    {
      int32_t  w;
      int32_t  h;

      uint8_t *io[3];
      uint8_t *ref[3];
      uint8_t *avg[3];
      uint8_t *dif[3];
      uint8_t *dif2[3];
      uint8_t *avg2[3];
      uint8_t *tmp[3];
      uint8_t *sub2ref[3];
      uint8_t *sub2avg[3];
      uint8_t *sub4ref[3];
      uint8_t *sub4avg[3];
    } frame;
    
    /* Border information */
    struct 
    {
      uint32_t  x;
      uint32_t  y;
      uint32_t  w;
      uint32_t  h;      
    } border;
    
  };
  
struct DNSR_VECTOR
{
  int32_t  x;
  int32_t  y;
  uint32_t SAD;
};

#endif
