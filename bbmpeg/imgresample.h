#ifndef _IMGRESAMPLE_H
#define _IMGRESAMPLE_H 1

#include <inttypes.h>

typedef unsigned short UINT16;
typedef signed short INT16;
typedef unsigned char UINT8;
typedef unsigned int UINT32;
typedef unsigned long long UINT64;
typedef signed char INT8;
typedef signed int INT32;
typedef signed long long INT64;

#define NB_COMPONENTS 3

#define PHASE_BITS 4
#define NB_PHASES  (1 << PHASE_BITS)
#define NB_TAPS    4
#define FCENTER    1  /* index of the center of the filter */

#define POS_FRAC_BITS 16
#define POS_FRAC      (1 << POS_FRAC_BITS)

/* 6 bits precision is needed for MMX */
#define FILTER_BITS     8
#define LINE_BUF_HEIGHT (NB_TAPS * 4)

#if defined(HAVE_MMX)
#define __align8 __attribute__ ((aligned (8)))
#else
#define __align8
#endif

typedef struct 
{
  int iwidth, iheight, owidth, oheight;
  int h_incr, v_incr;
  INT16 h_filters[NB_PHASES][NB_TAPS] __align8; /* horizontal filters */
  INT16 v_filters[NB_PHASES][NB_TAPS] __align8; /* vertical filters */
  UINT8 *line_buf;
} ImgReSampleContext;

typedef struct 
{
    UINT8 *data[3];
    int linesize[3];
} AVPicture;


extern ImgReSampleContext *img_resample_init(int owidth, int oheight,
                                            int iwidth, int iheight);
extern void img_resample(ImgReSampleContext *s, 
                         AVPicture *output, AVPicture *input);
extern void img_resample_close(ImgReSampleContext *s);


#endif
