#include <stdio.h>

#define BLACK   0
#define BLACK_C 127

static int   blank = 0;

static int DST_W, DST_H, SRC_W, SRC_H;
static int RW, RH;
static int BORDER_L, BORDER_R, BORDER_T, BORDER_B;
static int CROP_L, CROP_R, CROP_T, CROP_B;
static int CROP_T_OFS, CROP_L_OFS;
static int BORDER_T_OFS, BORDER_B_OFS;
static int LINES_RH, ROW_END_RW, ROWS_RW, LINES_OFS;
static int BORDER_C_L, BORDER_C_R;
static int CROP_C_T_OFS, CROP_C_L_OFS;
static int BORDER_C_T_OFS, BORDER_C_B_OFS;
static int LINES_C_RH, ROW_C_END_RW, ROWS_C_RW, LINES_C_OFS;

//== calc important parameters for the resizing procedure ==
//==========================================================
//static int setup_constants_done = 0;
void bb_resize_setup(int src_w, int src_h, int dst_w, int dst_h, int sh_info)
{
  double  d_rw, fw, fh;
 
  //-- only run once --
//  if (setup_constants_done) return;
//  setup_constants_done = 1;
  
  DST_W = dst_w;
  DST_H = dst_h;
  SRC_W = src_w;
  SRC_H = src_h;

  fw = src_w / (double)dst_w;
  fh = src_h / (double)dst_h;

  RW   = 1;                 // dst == src
  d_rw = 1.0;
  
  if (fw > 1.5)             // dst < 480 
  {
    RW   = 2;
    d_rw = 2.0;
  }  
  else if (fw > 1.24)       // 480 >= dst <= 580 
  {
    RW   = 3;
    d_rw = 1.5;
  };
 
  RH = 1;
  if (fh > 1.49) RH = 2; 
  
  CROP_L = (src_w/d_rw - dst_w) / 2;                // (720/2 - 352)/2 = 4
                                                    // (720/1.5 - 480)/2 = 0
  if (CROP_L < 0) CROP_L = 0;
  CROP_R = CROP_L;

  CROP_T = (src_h/RH - dst_h) / 2;                  // (480/2 - 288)/2 = -24
  if (CROP_T < 0) CROP_T = 0;
  CROP_B = CROP_T;
 
  BORDER_L = (dst_w - src_w/d_rw) / 2;              // (352 - 720/2)/2 = -4
                                                    // (576 - 720/1.5)/2 = 48
  if (BORDER_L < 0) BORDER_L = 0;
  BORDER_R = BORDER_L; 
  
  BORDER_T = (dst_h - src_h/RH) / 2;                // (288 - 480/2)/2 = 24  
  if (BORDER_T < 0) BORDER_T = 0;
  BORDER_B = BORDER_T;   
   
  CROP_T_OFS     = (CROP_T * RH * src_w);
  CROP_L_OFS     = (d_rw * CROP_L);

  BORDER_T_OFS   = (BORDER_T * dst_w);
  BORDER_B_OFS   = (BORDER_B * dst_w);

  LINES_RH       = (src_h/RH - (CROP_T + CROP_B));
  ROW_END_RW     = (d_rw * (dst_w + CROP_L - BORDER_L - BORDER_R));
  ROWS_RW        = ROW_END_RW - CROP_L_OFS;
  LINES_OFS      = (src_w * RH);

  BORDER_C_L     = (BORDER_L / 2);
  BORDER_C_R     = (BORDER_R / 2);

  CROP_C_T_OFS   = (CROP_T_OFS / 4);
  CROP_C_L_OFS   = (CROP_L_OFS / 2);

  BORDER_C_T_OFS = (BORDER_T_OFS / 4);
  BORDER_C_B_OFS = (BORDER_B_OFS / 4);

  LINES_C_RH     = (LINES_RH / 2);
  ROW_C_END_RW   = (ROW_END_RW / 2);
  ROWS_C_RW      = (ROWS_RW / 2);
  LINES_C_OFS    = (LINES_OFS / 2);
  
  if (sh_info)
    fprintf(stderr, "INFO: resize from w/h (%d/%d) to (w/h) (%d/%d)\n", 
            src_w, src_h, dst_w, dst_h);	
}

void bb_resize_frame(unsigned char *py_src,
                     unsigned char *pu_src,
                     unsigned char *pv_src, 
                     unsigned char *py_dst, 
                     unsigned char *pu_dst, 
                     unsigned char *pv_dst)
{
  register unsigned char r1, r2;
  unsigned char *src;
  unsigned char *dst;
  int i, j, pg_num;

  //=======
  //== Y ==
  //=======
  
  //-- draw top border --
  if (blank = (*pu_dst != BLACK_C)) memset(py_dst, BLACK, BORDER_T_OFS);

  //-- setup source-page and destination pointer --
  //-- with border and crop offsets.             --  
  src = py_src + CROP_T_OFS;
  dst = py_dst + BORDER_T_OFS;
  
  //-- over all (reduced) lines of source frame --
  for (i = 0; i < LINES_RH; i++)
  {
    //-- draw left border --  
    for (j = 0; j < BORDER_L; j++) { *dst = BLACK; dst++; }

    //-- no horizontal scaling, only cropping --
    //------------------------------------------
    if (RW==1)
    {
      memcpy(dst, &src[CROP_L_OFS], ROWS_RW);
      dst += ROWS_RW;
    }
    //-- horizontal scaling (2:1) with cropping --
    //-------------------------------------------- 
    else if (RW==2)
    {
      //-- draw visible part of destination frame --
      r2 = BLACK;
      for (j = CROP_L_OFS; j < ROW_END_RW; j += RW)
      {
        r1   = r2;
        r2   = src[j+1];
        *dst = (r1 + (src[j]<<1) + r2) >> 2;
        dst++;
      }
    }
    //-- horizontal scaling (3:2) with cropping --
    //-------------------------------------------- 
    else
    {
      r2 = BLACK_C;
      for (j = CROP_L_OFS; j < ROW_END_RW; j += RW)
      {
        r1   = r2;       
        r2   = src[j+2];
        
        *dst = (r1 + (src[j]<<1)) / 3;
        dst++;
        *dst = (r2 + (src[j+1]<<1)) / 3;
        dst++;
      }
    }
    
    //-- draw right border --
    for (j = 0; j < BORDER_R; j++) { *dst = BLACK; dst++; }
    
    //-- adjust source pointer to next (reduced) line --
    src += LINES_OFS;
  }
  
  //-- draw bottom border --
  if (blank) memset(dst, 0, BORDER_B_OFS);

  //========
  //== Cb ==
  //========
  
  //-- draw top border --
  if (blank) memset(pu_dst, BLACK_C, BORDER_C_T_OFS);
  
  src = pu_src + CROP_C_T_OFS;
  dst = pu_dst + BORDER_C_T_OFS;
  
  for (i = 0; i < LINES_C_RH; i++)
  {
    //-- draw left border --  
    for (j = 0; j < BORDER_C_L; j++) { *dst = BLACK_C; dst++; }
  
    if (RW==1)
    {
      memcpy(dst, &src[CROP_C_L_OFS], ROWS_C_RW);
      dst += ROWS_C_RW;
    }
    else if (RW==2)
    {
      //-- draw visible part of destination frame --
      r2 = BLACK_C;
      for (j = CROP_C_L_OFS; j < ROW_C_END_RW; j += RW)
      {
        r1   = r2;
        r2   = src[j+1];
        *dst = (r1 + (src[j]<<1) + r2) >> 2;
        dst++;
      }
    }
    else
    {
      //-- draw visible part of destination frame --
      for (j = CROP_C_L_OFS; j < ROW_C_END_RW; j += 3)
      {
        *dst = src[j];
        dst++;
        *dst = (src[j+1] + src[j+2]) / 2;
        dst++;
      }
    }
    
    //-- draw right border --
    for (j=0; j < BORDER_C_R; j++) { *dst = BLACK_C; dst++; }
    
    src += LINES_C_OFS;
  }
  
  //-- draw bottom border --
  if (blank) memset(dst, BLACK_C, BORDER_C_B_OFS);
  
  //========
  //== Cr ==
  //========
  
  //-- draw top border --
  if (blank) memset(pv_dst, BLACK_C, BORDER_C_T_OFS);
  
  src = pv_src + CROP_C_T_OFS;
  dst = pv_dst + BORDER_C_T_OFS;
  
  for (i = 0; i < LINES_C_RH; i++)
  {
    //-- draw left border --  
    for (j = 0; j < BORDER_C_L-3; j++) { *dst = BLACK_C; dst++; }

    if (RW==1)
    {
      memcpy(dst, &src[CROP_C_L_OFS], ROWS_C_RW);
      dst += ROWS_C_RW;
    }
    else if (RW==2)
    {
      //-- draw visible part of destination frame --
      r2 = BLACK_C;
      for (j = CROP_C_L_OFS; j < ROW_C_END_RW; j += RW)
      {
        r1   = r2;
        r2   = src[j+1];
        *dst = (r1 + (src[j]<<1) + r2) >> 2;
        dst++;
      }
    }
    else
    {
      //-- draw visible part of destination frame --
      for (j = CROP_C_L_OFS; j < ROW_C_END_RW; j += 3)
      {
        *dst = src[j];
        dst++;
        *dst = (src[j+1] + src[j+2]) / 2;
        dst++;
      }
    }  
    
    //-- draw right border --
    for (j = 0; j < BORDER_C_R; j++) { *dst = BLACK_C; dst++; }
    
    src += LINES_C_OFS;
  }
  
  //-- draw bottom border --
  if (blank)
  {
    memset(dst, BLACK_C, BORDER_C_B_OFS);
    blank=0;
  } 
    
}
