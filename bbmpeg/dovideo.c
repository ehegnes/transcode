/* dovideo.c */
#define MPEGMAIN 1
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include "main.h"
#include "bbencode.h"
#include "consts.h"

#define MAXINT 2147483647

static FILE *vFile;
static char doVBRLimit;
static char ProgressStr[128];

//====================
//== little helpers ==
//====================
static int mmx_probe()
{
  int  mode = MODE_NONE;
  FILE *proc;
  char string[1024];

  if(!(proc = fopen("/proc/cpuinfo", "r")))
  {
    fprintf(stderr, "mmx_probe: failed to open /proc/cpuinfo\n");
    return mode;
  }
  
  while(!feof(proc))
  {
    fgets(string, 1024, proc);

    /*-- Got the flags line --*/
    if(!strncasecmp(string, "flags", 5))
    {
      if (strstr(string, " sse"))
        mode = MODE_SSE;
      else if (strstr(string, " SSE"))
        mode = MODE_SSE;
      else if (strstr(string, " 3dnow"))
        mode = MODE_3DNOW; 
      else if (strstr(string, " 3DNOW"))
        mode = MODE_3DNOW;   
      else if (strstr(string, " mmx"))
        mode = MODE_MMX;
      else if (strstr(string, " MMX")) 
        mode = MODE_MMX;
    
      break;
    }
  }
  fclose(proc);

  return mode;
}


//======================
//== Display-Routines ==
//======================
static int need_new_line = 0;

void DisplayError(char *txt)
{
  fprintf(stderr, "\nERROR: %s", txt);
  need_new_line = 1;
}
void DisplayWarning(char *txt)
{
  fprintf(stderr, "\nWARNING: %s", txt);
  need_new_line = 1;
}
void DisplayInfo(char *txt)
{
  fprintf(stderr, "\nINFO: %s", txt);
  need_new_line = 1;
}

void DisplayProgress(char *txt, int percent)
{
  if (need_new_line) 
  {  
    fprintf(stderr, "\n"); 
    need_new_line = 0;
  }   
  fprintf(stderr, "*** %s    \r", txt);
}

//===========================
//== other helper routines ==
//===========================

static int putMaxBitrate();

static int quant_hfnoise_filt(int orgquant, int qmat_pos, int denoise )
{
	int x = qmat_pos % 8;
	int y = qmat_pos / 8;
	int qboost = 1024;

	if( ! denoise )
	{
		return orgquant;
	}

	/* Maximum 50% quantisation boost for HF components... */
	if( x > 4 )
		qboost += (256*(x-4)/3);
	if( y > 4 )
		qboost += (256*(y-4)/3);

	return (orgquant * qboost + 512)/ 1024;
}

static int ReadQuantMat(int denoise)
{
  int  i,v;
  FILE *fd;

  if (strlen(iqname) == 0)
  {
    /* use default intra matrix */
    load_iquant = 0;
    for (i=0; i<64; i++)
    {
      v = quant_hfnoise_filt(default_intra_quantizer_matrix[i], i, denoise);
      
      if (v<1) v = 1;
      if (v>255) v = 255;
       
      intra_q[i] = (unsigned char)v;
      //s_intra_q[i] = ...
      //i_intra_q[i] = (int)(((double)IQUANT_SCALE) / ((double)v);
    }  
  }
  else
  {
    /* read customized intra matrix */
    load_iquant = 1;
    fd = fopen(iqname,"r");
    if (fd == NULL)
    {
      sprintf(errortext,"Could not open intra quant matrix file %s.",iqname);
      DisplayError(errortext);
      return FALSE;
    }

    for (i=0; i<64; i++)
    {
      fscanf(fd,"%d",&v);
      if (v<1 || v>255)
      {
        sprintf(errortext, "Invalid value in intra quant matrix file %s.", iqname);
        DisplayError(errortext);
        return FALSE;
      }
      intra_q[i] = (unsigned char) v;
    }

    fclose(fd);
  }

  if (strlen(niqname) == 0)
  {
    /* use default non-intra matrix */
    load_niquant = 0;
    for (i=0; i<64; i++)
    {
      v = quant_hfnoise_filt(default_non_intra_quantizer_matrix[i], i, denoise);
      
      if (v<1) v = 1;
      if (v>255) v = 255;
      
      inter_q[i]   = (unsigned char)v;
      s_inter_q[i] = (unsigned short)v;
      i_inter_q[i] = (int)(((double)IQUANT_SCALE) / ((double)v));
    }
  }
  else
  {
    if (!strcmp(niqname, "mpeg default"))
    {
      /* use the MPEG default non-intra matrix */
      load_niquant = 0;
      for (i=0; i<64; i++)
      {
        inter_q[i] = 16;
        s_inter_q[i] = 16;
        i_inter_q[i] = (int)(((double)IQUANT_SCALE) / ((double)16));
      }
    }
    else
    {
      /* read customized non-intra matrix */
      load_niquant = 1;
      fd = fopen(niqname,"r");
      if (fd == NULL)
      {
        sprintf(errortext,"Could not open non-intra quant matrix file %s.",niqname);
        DisplayError(errortext);
        return FALSE;
      }

      for (i=0; i<64; i++)
      {
        fscanf(fd,"%d",&v);
        if (v<1 || v>255)
        {
          sprintf(errortext, "Invalid value in non-intra quant matrix file %s.", niqname);
          DisplayError(errortext);
          return FALSE;
        }
        inter_q[i] = (unsigned char) v;
        s_inter_q[i] = (unsigned short)v;
        i_inter_q[i] = (int)(((double)IQUANT_SCALE) / ((double)v));
      }

      fclose(fd);
    }
  }
  return TRUE;
}

static void FinishVideo()
{
  int i;

  finish_putbits(&videobs);

  if (vFile)
    fclose(vFile);
  vFile = NULL;
  
  if (orgclp)
  {
    free(orgclp);
    orgclp = NULL;
    clp = NULL;
  }

  for (i=0; i<3; i++)
  {
    if (newrefframe[i])
      free(newrefframe[i]);
    newrefframe[i] = NULL;
    if (oldrefframe[i])
      free(oldrefframe[i]);
    oldrefframe[i] = NULL;
    if (auxframe[i])
      free(auxframe[i]);
    auxframe[i] = NULL;
    if (neworgframe[i])
      free(neworgframe[i]);
    neworgframe[i] = NULL;
    if (oldorgframe[i])
      free(oldorgframe[i]);
    oldorgframe[i] = NULL;
    if (auxorgframe[i])
      free(auxorgframe[i]);
    auxorgframe[i] = NULL;
    if (predframe[i])
      free(predframe[i]);
    predframe[i] = NULL;

    if (unewrefframe[i])
      free(unewrefframe[i]);
    unewrefframe[i] = NULL;
    if (uoldrefframe[i])
      free(uoldrefframe[i]);
    uoldrefframe[i] = NULL;
    if (uneworgframe[i])
      free(uneworgframe[i]);
    uneworgframe[i] = NULL;
    if (uoldorgframe[i])
      free(uoldorgframe[i]);
    uoldorgframe[i] = NULL;
    if (uauxframe[i])
      free(uauxframe[i]);
    uauxframe[i] = NULL;
    if (uauxorgframe[i])
      free(uauxorgframe[i]);
    uauxorgframe[i] = NULL;
    if (upredframe[i])
      free(upredframe[i]);
    upredframe[i] = NULL;
  }

  if (mbinfo)
    free(mbinfo);
  mbinfo = NULL;

  if (umbinfo)
    free(umbinfo);
  umbinfo = NULL;

  if (blocks)
    free(blocks);
  blocks = NULL;

  if (ublocks)
    free(ublocks);
  ublocks = NULL;

  if (ubuffer)
    free(ubuffer);
  ubuffer = NULL;

  if (maxmotion > 7)
    finish_motion_est();
}


static char temp_max_name[]  = "./tmp_XXXXXX";
static int InitVideo(int sh_info)
{
  int i, size;
  static int block_count_tab[3] = {6,8,12};

  vFile = NULL;
  orgclp = NULL;
  clp = NULL;
  for (i = 0; i < 3; i++)
  {
    newrefframe[i] = NULL;
    oldrefframe[i] = NULL;
    auxframe[i] = NULL;
    neworgframe[i] = NULL;
    oldorgframe[i] = NULL;
    auxorgframe[i] = NULL;
    predframe[i] = NULL;
    unewrefframe[i] = NULL;
    uoldrefframe[i] = NULL;
    uauxframe[i] = NULL;
    uneworgframe[i] = NULL;
    uoldorgframe[i] = NULL;
    uauxorgframe[i] = NULL;
    upredframe[i] = NULL;
  }
  mbinfo = NULL;
  umbinfo = NULL;
  blocks = NULL;
  ublocks = NULL;
  ubuffer = NULL;

  /* open output file */
  if (!init_putbits(&videobs, VideoFilename))
    return FALSE;

  /* read quantization matrices */
  if (!ReadQuantMat(bb_denoise))
    return FALSE;

  if ((video_type > MPEG_VCD) && !constant_bitrate && !max_bit_rate)
  {
    mkstemp(temp_max_name);
    vFile = fopen(temp_max_name, "wb");
    if (vFile == NULL)
    {
      DisplayError("Could not create temporary max bitrate output file.");
      return FALSE;
    }
  }

  doVBRLimit = !constant_bitrate && (max_bit_rate || avg_bit_rate || min_bit_rate);

  bb_init_fdct();
  bb_init_idct();

  /* round picture dimensions to nearest multiple of 16 or 32 */
  mb_width = (horizontal_size+15)/16;
  mb_height = prog_seq ? (vertical_size+15)/16 : 2*((vertical_size+31)/32);
  mb_height2 = fieldpic ? mb_height>>1 : mb_height; /* for field pictures */
  width = 16*mb_width;
  height = 16*mb_height;

  chrom_width = (chroma_format==CHROMA444) ? width : width>>1;
  chrom_height = (chroma_format!=CHROMA420) ? height : height>>1;

  height2 = fieldpic ? height>>1 : height;
  width2 = fieldpic ? width<<1 : width;
  chrom_width2 = fieldpic ? chrom_width<<1 : chrom_width;

  block_count = block_count_tab[chroma_format-1];

  vbvOverflows = 0;
  vbvUnderflows = 0;
  paddingSum = 0.0;
  maxPadding = 0;
  headerSum = 0.0;

  /* clip table */
  clp = (unsigned char *)malloc(1024);
  if (clp == NULL)
  {
    DisplayError("Cannot allocate memory for clip table.");
    return FALSE;
  }
  orgclp = clp;
  clp+= 384;
  for (i = -384; i < 640; i++)
    clp[i] = (unsigned char) ((i<0) ? 0 : ((i>255) ? 255 : i));

  for (i=0; i<3; i++)
  {
    size = (i==0) ? width*height : chrom_width*chrom_height;

    newrefframe[i] = (unsigned char *)malloc(size);
    if (newrefframe[i] == NULL)
    {
      DisplayError("Cannot allocate memory for new ref frame.");
      return FALSE;
    }
    oldrefframe[i] = (unsigned char *)malloc(size);
    if (oldrefframe[i] == NULL)
    {
      DisplayError("Cannot allocate memory for old ref frame.");
      return FALSE;
    }
    auxframe[i] = (unsigned char *)malloc(size);
    if (auxframe[i] == NULL)
    {
      DisplayError("Cannot allocate memory for aux frame.");
      return FALSE;
    }
    neworgframe[i] = (unsigned char *)malloc(size);
    if (neworgframe[i] == NULL)
    {
      DisplayError("Cannot allocate memory for new org frame.");
      return FALSE;
    }
    oldorgframe[i] = (unsigned char *)malloc(size);
    if (oldorgframe[i] == NULL)
    {
      DisplayError("Cannot allocate memory for old org frame.");
      return FALSE;
    }
    auxorgframe[i] = (unsigned char *)malloc(size);
    if (auxorgframe[i] == NULL)
    {
      DisplayError("Cannot allocate memory for aux org frame.");
      return FALSE;
    }
    predframe[i] = (unsigned char *)malloc(size);
    if (predframe[i] == NULL)
    {
      DisplayError("Cannot allocate memory for pred frame.");
      return FALSE;
    }

    if (doVBRLimit)
    {
      unewrefframe[i] = (unsigned char *)malloc(size);
      if (unewrefframe[i] == NULL)
      {
        DisplayError("Cannot allocate memory for new ref frame.");
        return FALSE;
      }
      uoldrefframe[i] = (unsigned char *)malloc(size);
      if (uoldrefframe[i] == NULL)
      {
        DisplayError("Cannot allocate memory for old ref frame.");
        return FALSE;
      }
      uauxframe[i] = (unsigned char *)malloc(size);
      if (uauxframe[i] == NULL)
      {
        DisplayError("Cannot allocate memory for aux frame.");
        return FALSE;
      }
      uneworgframe[i] = (unsigned char *)malloc(size);
      if (uneworgframe[i] == NULL)
      {
        DisplayError("Cannot allocate memory for new org frame.");
        return FALSE;
      }
      uoldorgframe[i] = (unsigned char *)malloc(size);
      if (uoldorgframe[i] == NULL)
      {
        DisplayError("Cannot allocate memory for old org frame.");
        return FALSE;
      }
      uauxorgframe[i] = (unsigned char *)malloc(size);
      if (uauxorgframe[i] == NULL)
      {
        DisplayError("Cannot allocate memory for aux org frame.");
        return FALSE;
      }
      upredframe[i] = (unsigned char *)malloc(size);
      if (upredframe[i] == NULL)
      {
        DisplayError("Cannot allocate memory for pred frame.");
        return FALSE;
      }
    }
  }

  mbinfo = (struct mbinfo *)malloc(mb_width*mb_height2*sizeof(struct mbinfo));
  if (!mbinfo)
  {
    DisplayError("Cannot allocate memory for mb info.");
    return FALSE;
  }

  if (doVBRLimit)
  {
    umbinfo = (struct mbinfo *)malloc(mb_width*mb_height2*sizeof(struct mbinfo));
    if (!umbinfo)
    {
      DisplayError("Cannot allocate memory for mb info.");
      return FALSE;
    }
  }

  blocks =
    (short (*)[64])malloc(mb_width*mb_height2*block_count*sizeof(short [64]));
  if (!blocks)
  {
    DisplayError("Cannot allocate memory for blocks.");
    return FALSE;
  }

  if (doVBRLimit)
  {
    ublocks =
      (short (*)[64])malloc(mb_width*mb_height2*block_count*sizeof(short [64]));
    if (!ublocks)
    {
      DisplayError("Cannot allocate memory for blocks.");
      return FALSE;
    }

    ubuffer = (unsigned char*)malloc(BUFFER_SIZE);
    if (!ubuffer)
    {
      DisplayError("Cannot allocate memory for undo file buffer.");
      return FALSE;
    }
  }

  if (maxmotion > 7)
    if (!init_motion_est2(sh_info))
      return FALSE;

  return TRUE;
}

int GetFCode(int sw)
{
  if (sw < 8)
    return 1;
  if (sw < 16)
    return 2;
  if (sw < 32)
    return 3;
  if (sw < 64)
    return 4;
  if (sw < 128)
    return 5;
  if (sw < 256)
    return 6;
  if (sw < 512)
    return 7;
  if (sw < 1024)
    return 8;
  if (sw < 2048)
    return 9;
  return 1;
}

void DoVarMotion()
{
  char tmpStr[256];

  switch (pict_type)
  {
    case P_TYPE:
      if (Sxf > maxmotion + 5)
      {
        sprintf(tmpStr, "Warning, horz forward motion vector larger than max, vector = %d, max = %d.", Sxf, maxmotion + 5);
        DisplayInfo(tmpStr);
      }
      if (Syf > maxmotion + 5)
      {
        sprintf(tmpStr, "Warning, vert forward motion vector larger than max, vector = %d, max = %d.", Syf, maxmotion + 5);
        DisplayInfo(tmpStr);
      }
      forw_hor_f_code = GetFCode(Sxf);
      forw_vert_f_code = GetFCode(Syf);

      /* keep MPEG-1 horz/vert f_codes the same */
      if ((video_type < MPEG_MPEG2) && (forw_hor_f_code != forw_vert_f_code))
      {
        if (forw_hor_f_code > forw_vert_f_code)
          forw_vert_f_code = forw_hor_f_code;
        else
          forw_hor_f_code = forw_vert_f_code;
      }
//      sprintf(tmpStr, "Sxf,Syf = %d,%d", Sxf, Syf);
//      DisplayInfo(tmpStr);
      break;

    case B_TYPE:
      if (Sxf > maxmotion + 5)
      {
        sprintf(tmpStr, "Warning, horz forward motion vector larger than max, vector = %d, max = %d.", Sxf, maxmotion + 5);
        DisplayInfo(tmpStr);
      }
      if (Syf > maxmotion + 5)
      {
        sprintf(tmpStr, "Warning, vert forward motion vector larger than max, vector = %d, max = %d.", Syf, maxmotion + 5);
        DisplayInfo(tmpStr);
      }
      if (Sxb > maxmotion + 5)
      {
        sprintf(tmpStr, "Warning, horz backward motion vector larger than max, vector = %d, max = %d.", Sxb, maxmotion + 5);
        DisplayInfo(tmpStr);
      }
      if (Syb > maxmotion + 5)
      {
        sprintf(tmpStr, "Warning, vert backward motion vector larger than max, vector = %d, max = %d.", Syb, maxmotion + 5);
        DisplayInfo(tmpStr);
      }

      forw_hor_f_code = GetFCode(Sxf);
      forw_vert_f_code = GetFCode(Syf);
      back_hor_f_code = GetFCode(Sxb);
      back_vert_f_code = GetFCode(Syb);

      /* keep MPEG-1 forw horz/vert f_codes the same */
      if ((video_type < MPEG_MPEG2) && (forw_hor_f_code != forw_vert_f_code))
      {
        if (forw_hor_f_code > forw_vert_f_code)
          forw_vert_f_code = forw_hor_f_code;
        else
          forw_hor_f_code = forw_vert_f_code;
      }

      /* keep MPEG-1 back horz/vert f_codes the same */
      if ((video_type < MPEG_MPEG2) && (back_hor_f_code != back_vert_f_code))
      {
        if (back_hor_f_code > back_vert_f_code)
          back_vert_f_code = back_hor_f_code;
        else
          back_hor_f_code = back_vert_f_code;
      }
//      sprintf(tmpStr, "Sxf,Syf = %d,%d, Sxb,Syb = %d,%d", Sxf, Syf, Sxb, Syb);
//      DisplayInfo(tmpStr);
      break;
  }
}

//=====================
//== public routines ==
//=====================
typedef struct t_enc_runtime
{ 
  int  i, undoi, f0, gopCount;
  int  sxf, syf, sxb, syb;
  int  availablebr, sparedbitrate, calcbr, originalmquant;
  int  averagebitrate, absmaxbitrate, absminbitrate, maxbr, minbr;
  int  new_mquant, max_done, initial_mq;
  int  hours, min, sec;
  
  unsigned char  *neworg[3], *newref[3], *uneworg[3], *unewref[3];
  unsigned int   percent, tot_t;
  
  time_t  start_t, end_t;
  double  frame_t, lastbitrate, fSize, factor;
  
  struct bitstream  undobs;
  
} t_enc_runtime;

static char          ipb[5] = {' ','I','P','B','D'};
static int           last_nframes = 0;
static t_enc_runtime rt;
static T_BBMPEG_CTX  ictx;

T_BBMPEG_CTX *bb_start(char *fname, int enc_w, int enc_h, int sh_info)
{
  char         tmpStr[255];
  unsigned int byteCount;

  ictx.verbose      = sh_info;
  ictx.pic_size_l   = enc_w * enc_h;
  ictx.pic_size_c   = ictx.pic_size_l/4;
  ictx.progress_str = NULL;

  //-- this variable shouldn be uninitialized -> it's important --
  //-- for MPEG2, but i don't know wich rule has it on MPEG 1   --
  slice_hdr_every_MBrow = 1;

  strcpy(VideoFilename, fname);
  vertical_size         = enc_h;
  horizontal_size       = enc_w;
  input_vertical_size   = enc_h;
  input_horizontal_size = enc_w;
  
  frame0  = 0;
  nframes = 0x7fffffff;  // unlimited frames 

  audioStereo = 1;

  //-- test for mmx/sse capabilities (using libmpeg3-function) --
  //-------------------------------------------------------------
  MMXMode = mmx_probe();

  if (!InitVideo(sh_info))
  {
    FinishVideo();
    return NULL;
  }

  time(&rt.start_t);

  if (maxmotion > 7)
  {
    submotiop = maxmotion >> 2;
    submotiob = (submotiop * 3) >> 2;
  }

  init_motion_est(sh_info);
  init_transform(sh_info);
  init_predict(sh_info);
  
  rc_init_seq(); /* initialize rate control */

  alignbits(&videobs); // moved from puthdr
  if (vFile)
  {
    byteCount = (unsigned int) floor(bitcount(&videobs) / 8.0) + 8;
    if (fwrite(&byteCount, 1, sizeof(byteCount), vFile) != sizeof(byteCount))
    {
      FinishVideo();
      DisplayError("Unable to write to temporary max bitrate file.");
      return NULL;
    }
  }
  /* sequence header, sequence extension and sequence display extension */
  putseqhdr();
  if (video_type > MPEG_VCD)
  {
    putseqext();
    putseqdispext();
  }

  /* optionally output some text data (description, copyright or whatever) */
  //if (strlen(id_string))
  //  putuserdata(id_string);

  if (doVBRLimit)
  {
    prepareundo(&videobs,&rt.undobs);
    rt.undoi = 0;
    rt.lastbitrate = bitcount(&videobs);

    if (max_bit_rate == 0)
      rt.absmaxbitrate = MAXINT;
    else
      rt.absmaxbitrate = (int)floor((max_bit_rate * N) / frame_rate);

    if (min_bit_rate == 0)
      rt.absminbitrate = 0;
    else
      rt.absminbitrate = (int)floor((min_bit_rate * N) / frame_rate);

    rt.originalmquant = mquant_value;

    if ((avg_bit_rate > 0) && (!constant_bitrate))
    {
      rt.sparedbitrate = 40000000;
      rt.averagebitrate = (int)floor((avg_bit_rate * N) / frame_rate);
    }
    else
    {
      rt.sparedbitrate  = 0;
      rt.averagebitrate = MAXINT;
    }
    save_rc_max();
    rt.initial_mq = 0;
    rt.max_done = 0;
    rt.new_mquant = mquant_value;
  }
  
  rt.i        = 0;
  rt.gopCount = 1;
  rt.frame_t  = 0.0;
  rt.hours    = rt.min = rt.sec = 0;
  
  ictx.file_size     = 0;
  ictx.max_file_size = max_file_size;
  ictx.bitrate       = 0;
  
  ictx.check_pending = 0;
  ictx.gop_size      = (N - (M-1));
  ictx.gop_size_max  = N; 
  ictx.gop_size_undo = ictx.gop_size; 
  ictx.ret           = ENCODE_RUN;
  
  return &ictx;
}

static int load_frame(T_BBMPEG_CTX *ctx, unsigned char *frame[], int num)
{
  int idx = num - rt.f0;

  memcpy(frame[0], ctx->pY + idx*ctx->pic_size_l, ctx->pic_size_l);
  memcpy(frame[1], ctx->pU + idx*ctx->pic_size_c, ctx->pic_size_c);
  memcpy(frame[2], ctx->pV + idx*ctx->pic_size_c, ctx->pic_size_c);

  return 1;
}

int bb_encode(T_BBMPEG_CTX *ctx, int check)
{
  int          f, j, k, n, np, nb, size, ipflag;
  unsigned int byteCount;
  char         tmpStr[255];

  if (rt.i < nframes)
  {
    last_nframes  = rt.i;
    ctx->gop_size = N;
    //fprintf(stderr,"\n *** (%d / %d) ***\n", N, ctx->gop_size);

    if ((rt.i >= 10) && (rt.i % 10 == 0))
    {
      time(&rt.end_t);
      rt.tot_t = (unsigned int) rt.end_t - rt.start_t;
      rt.frame_t = (double) rt.i / (double) rt.tot_t; // frame_t = fps
    }
    ctx->file_size = bitcount(&videobs) / 8388608.0;
    ctx->bitrate   = rt.i? ctx->file_size * 204800/(double)rt.i:0.0;
    sprintf(ProgressStr, 
            "frame (%d), (%.1f)fps, filesize (%.2f)MB, bitrate (%1.0f)Kbps ",
             rt.i, rt.frame_t, ctx->file_size, ctx->bitrate);
    //DisplayProgress( ProgressStr, 0);
    ctx->progress_str = ProgressStr;
     
    /* f0: lowest frame number in current GOP
     *
     * first GOP contains N-(M-1) frames,
     * all other GOPs contain N frames
     */
    rt.f0 = N*((rt.i+(M-1))/N) - (M-1);
    if (rt.f0<0) rt.f0=0;

    //-- VBR - Limiter starts --
    //--------------------------
    if (doVBRLimit && (!ctx->check_pending) && (rt.i==rt.f0) && (rt.i!=0)) 
    {
      int gop_undo = 0;
      
      rt.availablebr = (int)floor((rt.sparedbitrate / ((10*frame_rate)/N)) + rt.averagebitrate); // 10 second buffer
      rt.calcbr = (int)(bitcount(&videobs) - rt.lastbitrate);

      if (bb_verbose)
      {
        if (!rt.initial_mq)
        {
          sprintf(tmpStr, "  GOP #%5d, bitrate = %u, act mquant = %d", rt.gopCount, (int)(rt.calcbr * frame_rate / N), mquant_value);
          DisplayInfo(tmpStr);
          rt.initial_mq = 1;
        }
        else
        {
          sprintf(tmpStr, "              bitrate = %u, new mquant = %d", (int)(rt.calcbr * frame_rate / N), mquant_value);
          DisplayInfo(tmpStr);
        }
      }
      
      //-- test for undo --
      //-------------------
      if ((((rt.calcbr > rt.absmaxbitrate) || (rt.calcbr > rt.availablebr)) && (mquant_value < 31)) ||
           ((rt.calcbr < rt.absminbitrate) && !rt.max_done && (mquant_value > 1)))
      {
        gop_undo = 1;
        
        restore_rc_max();
        undochanges(&videobs,&rt.undobs);
        rt.i  = rt.undoi;
        rt.f0 = N*((rt.i+(M-1))/N) - (M-1);

        if (rt.f0<0) rt.f0=0;

        if (rt.calcbr < rt.absminbitrate)
        {
          rt.minbr = rt.availablebr;
          if (rt.minbr > rt.absminbitrate)
            rt.minbr = rt.absminbitrate;

          rt.factor = (double)rt.calcbr / (double)rt.minbr;
          rt.new_mquant = (int)floor((double)mquant_value * rt.factor + 0.5) + 1;
          if (rt.new_mquant >= mquant_value)
            mquant_value--;
          else
            mquant_value = rt.new_mquant;
        }
        else
        {
          rt.max_done = 1;
          rt.maxbr = rt.availablebr;
          if (rt.maxbr > rt.absmaxbitrate)
            rt.maxbr = rt.absmaxbitrate;

          rt.factor = (double)rt.calcbr / (double)rt.maxbr;
          rt.new_mquant = (int)floor((double)mquant_value * rt.factor + 0.5); // + 1;
          if (rt.new_mquant <= mquant_value)
            mquant_value++;
          else
            mquant_value = rt.new_mquant;
        }

        if (mquant_value < 1)
          mquant_value = 1;
        if (mquant_value > 31)
          mquant_value = 31;

        memcpy(mbinfo, umbinfo, mb_width*mb_height2*sizeof(struct mbinfo));
        memcpy(blocks,ublocks,mb_width*mb_height2*block_count*sizeof(short [64]));
        for (j=0; j<MAXM; j++)
          motion_data[j] = umotion_data[j];
        for (j=0; j<3; j++)
        {
          size = (j==0) ? width*height : chrom_width*chrom_height;

          memcpy(oldorgframe[j],uoldorgframe[j],size);
          memcpy(oldrefframe[j],uoldrefframe[j],size);
          memcpy(auxframe[j],uauxframe[j],size);
          memcpy(neworgframe[j],uneworgframe[j],size);
          memcpy(newrefframe[j],unewrefframe[j],size);
          memcpy(auxorgframe[j],uauxorgframe[j],size);
          memcpy(predframe[j],upredframe[j],size);
          rt.neworg[j] = rt.uneworg[j];
          rt.newref[j] = rt.unewref[j];
          dc_dct_pred[j] = udc_dct_pred[j];
        }

      }
      //-- everything ok with last GOP, no undo --
      //------------------------------------------
      else 
      {
        rt.gopCount++;
        rt.initial_mq = 0;
        rt.max_done = 0;
        save_rc_max();
        if ((avg_bit_rate > 0) && (!constant_bitrate))
          rt.sparedbitrate += (rt.averagebitrate - rt.calcbr);

        prepareundo(&videobs,&rt.undobs);
        rt.undoi = rt.i;

        rt.lastbitrate = bitcount(&videobs);
        mquant_value = rt.originalmquant;

        memcpy(umbinfo, mbinfo, mb_width*mb_height2*sizeof(struct mbinfo));
        memcpy(ublocks,blocks,mb_width*mb_height2*block_count*sizeof(short [64]));
        for (j=0; j<MAXM; j++)
          umotion_data[j] = motion_data[j];
        for (j=0; j<3; j++)
        {
          size = (j==0) ? width*height : chrom_width*chrom_height;

          memcpy(uoldorgframe[j],oldorgframe[j],size);
          memcpy(uoldrefframe[j],oldrefframe[j],size);
          memcpy(uauxframe[j],auxframe[j],size);
          memcpy(uneworgframe[j],neworgframe[j],size);
          memcpy(unewrefframe[j],newrefframe[j],size);
          memcpy(uauxorgframe[j],auxorgframe[j],size);
          memcpy(upredframe[j],predframe[j],size);
          rt.uneworg[j] = rt.neworg[j];
          rt.unewref[j] = rt.newref[j];
          udc_dct_pred[j] = dc_dct_pred[j];
        }
        

      }
      
      if (gop_undo) 
      {
        ctx->check_pending = 1;
        ctx->gop_size      = ctx->gop_size_undo;
        ctx->ret           = ENCODE_UNDO;
        
        return ENCODE_UNDO;
      } 
      else 
      {
        ctx->check_pending   = 1;
        ctx->gop_size_undo   = ctx->gop_size;
        ctx->ret             = ENCODE_NEWGOP;
 
        return ENCODE_NEWGOP; 
      }
    }
    ctx->check_pending = 0;
    //-- VBR-Limiter ends --

    if (check) 
    {
      ctx->ret = ENCODE_RUN;
      return ENCODE_RUN;
    }
    
    if (rt.i==0 || (rt.i-1)%M==0)
    {
      /* I or P frame */
      for (j=0; j<3; j++)
      {
        /* shuffle reference frames */
        rt.neworg[j] = oldorgframe[j];
        rt.newref[j] = oldrefframe[j];
        oldorgframe[j] = neworgframe[j];
        oldrefframe[j] = newrefframe[j];
        neworgframe[j] = rt.neworg[j];
        newrefframe[j] = rt.newref[j];
      }

      /* f: frame number in display order */
      f = (rt.i==0) ? 0 : rt.i+M-1;
      if (f>=nframes) f = nframes - 1;

      if (rt.i==rt.f0) /* first displayed frame in GOP is I */
      {
        /* I frame */

        pict_type = I_TYPE;
        forw_hor_f_code = forw_vert_f_code = 15;
        back_hor_f_code = back_vert_f_code = 15;

        /* n: number of frames in current GOP
         *
         * first GOP contains (M-1) less (B) frames
         */
        n = (rt.i==0) ? N-(M-1): N;

        /* last GOP may contain less frames */
        if (n > nframes-rt.f0) n = nframes-rt.f0;

        /* number of P frames */
        if (rt.i==0)
          np = (n + 2*(M-1))/M - 1; /* first GOP */
        else
          np = (n + (M-1))/M - 1;

        /* number of B frames */
        nb = n - np - 1;

        if (constant_bitrate)
          rc_init_GOP(np,nb);
        if (rt.i)
        {
          alignbits(&videobs); // moved from puthdr
          if (vFile)
          {
            byteCount = (unsigned int) floor(bitcount(&videobs) / 8.0) + 8;
            if (fwrite(&byteCount, 1, sizeof(byteCount), vFile) != sizeof(byteCount))
            {
              FinishVideo();
              DisplayError("Unable to write to temporary max bitrate file.");
              ctx->ret = ENCODE_ERR;
              return ENCODE_ERR;
            }
          }
          putseqhdr();
          if (video_type > MPEG_VCD)
          {
            putseqext();
            putseqdispext();
          }
        }
        
        //-- inform reader for new or repeated GOP --  
        if (!doVBRLimit) rt.gopCount++;
        
        putgophdr(rt.f0,rt.i==0); /* set closed_GOP in first GOP only */
      }
      else
      {
        /* P frame */
        pict_type = P_TYPE;
        forw_hor_f_code = motion_data[0].forw_hor_f_code;
        forw_vert_f_code = motion_data[0].forw_vert_f_code;
        back_hor_f_code = back_vert_f_code = 15;
        rt.sxf = motion_data[0].sxf;
        rt.syf = motion_data[0].syf;
      }
    }
    else
    {
      /* B frame */
      for (j=0; j<3; j++)
      {
        rt.neworg[j] = auxorgframe[j];
        rt.newref[j] = auxframe[j];
      }

      /* f: frame number in display order */
      f = rt.i - 1;
      pict_type = B_TYPE;
      n = (rt.i-2)%M + 1; /* first B: n=1, second B: n=2, ... */
      forw_hor_f_code = motion_data[n].forw_hor_f_code;
      forw_vert_f_code = motion_data[n].forw_vert_f_code;
      back_hor_f_code = motion_data[n].back_hor_f_code;
      back_vert_f_code = motion_data[n].back_vert_f_code;
      rt.sxf = motion_data[n].sxf;
      rt.syf = motion_data[n].syf;
      rt.sxb = motion_data[n].sxb;
      rt.syb = motion_data[n].syb;
    }

    if (maxmotion > 7)
      Sxf = Syf = Sxb = Syb = 0;

    temp_ref = f - rt.f0;
    frame_pred_dct = frame_pred_dct_tab[pict_type-1];
    q_scale_type = qscale_tab[pict_type-1];
    intravlc = intravlc_tab[pict_type-1];
    altscan = altscan_tab[pict_type-1];
    if (OutputStats)
    {
      fprintf(statfile,"\nFrame %d (#%d in display order):\n",rt.i,f);
      fprintf(statfile," picture_type=%c\n",ipb[pict_type]);
      fprintf(statfile," temporal_reference=%d\n",temp_ref);
      fprintf(statfile," frame_pred_frame_dct=%d\n",frame_pred_dct);
      fprintf(statfile," q_scale_type=%d\n",q_scale_type);
      fprintf(statfile," intra_vlc_format=%d\n",intravlc);
      fprintf(statfile," alternate_scan=%d\n",altscan);

      if (pict_type!=I_TYPE)
      {
        fprintf(statfile," forward search window: %d...%d / %d...%d\n",
          -rt.sxf,rt.sxf,-rt.syf,rt.syf);
        fprintf(statfile," forward vector range: %d...%d.5 / %d...%d.5\n",
          -(4<<forw_hor_f_code),(4<<forw_hor_f_code)-1,
          -(4<<forw_vert_f_code),(4<<forw_vert_f_code)-1);
      }

      if (pict_type==B_TYPE)
      {
        fprintf(statfile," backward search window: %d...%d / %d...%d\n",
          -rt.sxb,rt.sxb,-rt.syb,rt.syb);
        fprintf(statfile," backward vector range: %d...%d.5 / %d...%d.5\n",
          -(4<<back_hor_f_code),(4<<back_hor_f_code)-1,
          -(4<<back_vert_f_code),(4<<back_vert_f_code)-1);
      }
    }
 
    currentFrame = f + frame0;
    if (!load_frame(ctx, rt.neworg, currentFrame))
    {
      if (!rt.i)
      {
        FinishVideo();
        ctx->ret = ENCODE_ERR;
        return ENCODE_ERR;
      }
      ctx->ret = ENCODE_STOP;
      return ENCODE_STOP;
    }

    if (fieldpic)
    {
      pict_struct = topfirst ? TOP_FIELD : BOTTOM_FIELD;
      if (!motion_estimation(oldorgframe[0],neworgframe[0],
                        oldrefframe[0],newrefframe[0],
                        rt.neworg[0],rt.newref[0],
                        rt.sxf,rt.syf,rt.sxb,rt.syb,mbinfo,0,0))
      {
        FinishVideo();
        ctx->ret = ENCODE_ERR;
        return ENCODE_ERR;
      }

      if (maxmotion > 7)
        DoVarMotion();

     
      predict(oldrefframe,newrefframe,predframe,0,mbinfo);
      dct_type_estimation(predframe[0],rt.neworg[0],mbinfo);
      transform(predframe,rt.neworg,mbinfo,blocks);
      if (!putpict(rt.neworg[0]))
      {
        FinishVideo();
        ctx->ret = ENCODE_ERR;
        return ENCODE_ERR;
      }

      for (k=0; k<mb_height2*mb_width; k++)
      {
        if (mbinfo[k].mb_type & MB_INTRA)
          for (j=0; j<block_count; j++)
            iquant_intra(blocks[k*block_count+j],blocks[k*block_count+j],
                         dc_prec,intra_q,mbinfo[k].mquant);
        else
          for (j=0;j<block_count;j++)
            iquant_non_intra(blocks[k*block_count+j],blocks[k*block_count+j],
                             inter_q,mbinfo[k].mquant);
      }

      itransform(predframe,rt.newref,mbinfo,blocks);
      if (OutputStats)
      {
        calcSNR(rt.neworg,rt.newref);
        stats();
      }

      if (maxmotion > 7)
        Sxf = Syf = Sxb = Syb = 0;

      pict_struct = topfirst ? BOTTOM_FIELD : TOP_FIELD;

      ipflag = (pict_type==I_TYPE);
      if (ipflag)
      {
        /* first field = I, second field = P */
        pict_type = P_TYPE;
        forw_hor_f_code = motion_data[0].forw_hor_f_code;
        forw_vert_f_code = motion_data[0].forw_vert_f_code;
        back_hor_f_code = back_vert_f_code = 15;
        rt.sxf = motion_data[0].sxf;
        rt.syf = motion_data[0].syf;
      }

      if (!motion_estimation(oldorgframe[0],neworgframe[0],
                        oldrefframe[0],newrefframe[0],
                        rt.neworg[0],rt.newref[0],
                        rt.sxf,rt.syf,rt.sxb,rt.syb,mbinfo,1,ipflag))
      {
        FinishVideo();
        ctx->ret = ENCODE_ERR;
        return ENCODE_ERR;
      }

      if (maxmotion > 7)
        DoVarMotion();

      predict(oldrefframe,newrefframe,predframe,1,mbinfo);
      dct_type_estimation(predframe[0],rt.neworg[0],mbinfo);
      transform(predframe,rt.neworg,mbinfo,blocks);
      if (!putpict(rt.neworg[0]))
      {
        FinishVideo();
        ctx->ret = ENCODE_ERR;
        return ENCODE_ERR;
      }

      for (k=0; k<mb_height2*mb_width; k++)
      {
        if (mbinfo[k].mb_type & MB_INTRA)
          for (j=0; j<block_count; j++)
            iquant_intra(blocks[k*block_count+j],blocks[k*block_count+j],
                         dc_prec,intra_q,mbinfo[k].mquant);
        else
          for (j=0;j<block_count;j++)
            iquant_non_intra(blocks[k*block_count+j],blocks[k*block_count+j],
                             inter_q,mbinfo[k].mquant);
      }

      itransform(predframe,rt.newref,mbinfo,blocks);
      if (OutputStats)
      {
        calcSNR(rt.neworg,rt.newref);
        stats();
      }
    }
    else
    {
      pict_struct = FRAME_PICTURE;
      if ((video_pulldown_flag != PULLDOWN_NONE) &&
          (video_pulldown_flag != PULLDOWN_AUTO))
      {
	tmp_prog_frame = 1;
        if (video_pulldown_flag == PULLDOWN_32)
        {
          switch (currentFrame % 4)
          {
            case 0:
              tmp_topfirst = 1;
              tmp_repeatfirst = 1;
              break;
            case 1:
              tmp_topfirst = 0;
              tmp_repeatfirst = 0;
              break;
            case 2:
              tmp_topfirst = 0;
              tmp_repeatfirst = 1;
              break;
            case 3:
              tmp_topfirst = 1;
              tmp_repeatfirst = 0;
              break;
          }
        }
        else
          switch (currentFrame % 4)
          {
            case 0:
                tmp_topfirst = 1;
                tmp_repeatfirst = 0;
                break;
            case 1:
                tmp_topfirst = 1;
                tmp_repeatfirst = 1;
                break;
            case 2:
                tmp_topfirst = 0;
                tmp_repeatfirst = 0;
                break;
            case 3:
                tmp_topfirst = 0;
                tmp_repeatfirst = 1;
                break;
          }
      }
	  else
	  {
	    tmp_topfirst = topfirst;
		tmp_repeatfirst = repeatfirst;
		tmp_prog_frame = prog_frame;
	  }


      /* do motion_estimation
       *
       * uses source frames (...orgframe) for full pel search
       * and reconstructed frames (...refframe) for half pel search
       */
      if (!motion_estimation(oldorgframe[0],neworgframe[0],
                        oldrefframe[0],newrefframe[0],
                        rt.neworg[0],rt.newref[0],
                        rt.sxf,rt.syf,rt.sxb,rt.syb,mbinfo,0,0))
      {
        FinishVideo();
        ctx->ret = ENCODE_ERR;
        return ENCODE_ERR;
      }
      if (maxmotion > 7)
        DoVarMotion();

      predict(oldrefframe,newrefframe,predframe,0,mbinfo);
      dct_type_estimation(predframe[0],rt.neworg[0],mbinfo);
      transform(predframe,rt.neworg,mbinfo,blocks);
      if (!putpict(rt.neworg[0]))
      {
        FinishVideo();
        ctx->ret = ENCODE_ERR;
        return ENCODE_ERR;
      }

      for (k=0; k<mb_height*mb_width; k++)
      {
        if (mbinfo[k].mb_type & MB_INTRA)
          for (j=0; j<block_count; j++)
            iquant_intra(blocks[k*block_count+j],blocks[k*block_count+j],
                         dc_prec,intra_q,mbinfo[k].mquant);
        else
          for (j=0;j<block_count;j++)
            iquant_non_intra(blocks[k*block_count+j],blocks[k*block_count+j],
                             inter_q,mbinfo[k].mquant);
      }

      itransform(predframe,rt.newref,mbinfo,blocks);
      if (OutputStats)
      {
        calcSNR(rt.neworg,rt.newref);
        stats();
      }
    }
    
    //fprintf(stderr, "\n*** 3 ***\n");
    rt.i++;
    
    ctx->ret = ENCODE_RUN;
    return ENCODE_RUN; // next frame may follow 
 
  } // if (rt.i < nframes)
  
  ctx->ret = ENCODE_STOP;
  return ENCODE_STOP; // no more frames available 
}    
  
int bb_stop(T_BBMPEG_CTX *ctx)
{
  char tmpStr[255];
  
  putseqend();
  FinishVideo();

  if (constant_bitrate)
  {
    if (!bb_verbose && vbvOverflows)
    {
      sprintf(tmpStr, "   VBV delay overflows = %d", vbvOverflows);
      DisplayInfo(tmpStr);
    }
    if (!bb_verbose && vbvUnderflows)
    {
      sprintf(tmpStr, "   VBV delay underflows = %d", vbvUnderflows);
      DisplayInfo(tmpStr);
    }
  }

  if (ctx->verbose)  
  {
    sprintf(tmpStr, "   Min bitrate of any one frame = %u bits", min_frame_bitrate);
    DisplayInfo(tmpStr);
    sprintf(tmpStr, "   Max bitrate of any one frame = %u bits", max_frame_bitrate);
    DisplayInfo(tmpStr);
    sprintf(tmpStr, "   Min bitrate over any one second = %u bps", min_bitrate);
    DisplayInfo(tmpStr);
    sprintf(tmpStr, "   Avg bitrate over any one second = %.0f bps", bitcount(&videobs) / (last_nframes / frame_rate));
    DisplayInfo(tmpStr);
    sprintf(tmpStr, "   Max bitrate over any one second = %u bps", max_bitrate);
    DisplayInfo(tmpStr);
    if (constant_bitrate)
    {
      if (maxPadding)
      {
        sprintf(tmpStr, "   Avg padding bits over any one second = %.0f", frame_rate * paddingSum/(double) last_nframes);
        DisplayInfo(tmpStr);
        sprintf(tmpStr, "   Max padding in any one frame = %u bits", maxPadding);
        DisplayInfo(tmpStr);
      }

      sprintf(tmpStr, "   Avg header bits over any one second = %.0f", frame_rate * headerSum/(double) last_nframes);
      DisplayInfo(tmpStr);
 
      sprintf(tmpStr, "   Min mquant = %u", min_mquant);
      DisplayInfo(tmpStr);
      sprintf(tmpStr, "   Avg mquant = %.3f", avg_mquant);
      DisplayInfo(tmpStr);
      sprintf(tmpStr, "   Max mquant = %u", max_mquant);
      DisplayInfo(tmpStr);
    }
  }
    
  time(&rt.end_t);
  rt.tot_t = (unsigned int) rt.end_t - rt.start_t;
  rt.frame_t = (double) rt.tot_t / (double) last_nframes;
  rt.sec = rt.tot_t % 60;
  rt.min = rt.tot_t / 60;
  rt.hours = rt.min / 60;
  rt.min = rt.min % 60;
  
  if (ctx->verbose)
  {
    sprintf(tmpStr, 
            "   Total time: %d seconds (%02d:%02d:%02d), %.2f frames/sec, %.3f sec/frame.\n", 
            rt.tot_t, rt.hours, rt.min, rt.sec, 1.0 / rt.frame_t, rt.frame_t);
    DisplayInfo(tmpStr);
  }

  if ((video_type > MPEG_VCD) && !constant_bitrate && !max_bit_rate)
  {
    int mbr;
    mbr = putMaxBitrate();
    unlink(temp_max_name);
    return !mbr;
  }

  return 1;
}

int putMaxBitrate()
{
  unsigned int byteCount, totalPatches, patchCount, percent;
  unsigned int i, j;
  unsigned char patchVal[3];
  FILE *vidStream;
  struct stat statbuf;
  char tmpStr[132];

  vidStream = fopen(VideoFilename, "r+b");
  if (vidStream == NULL)
  {
    DisplayError("Unable to open video stream.");
    return 1;
  }

  vFile = fopen(temp_max_name, "rb");
  if (vFile == NULL)
  {
    DisplayError("Unable to open temporary max bitrate file.");
    fclose(vidStream);
    return 1;
  }

  if (stat(temp_max_name, &statbuf))
  {
    DisplayError("Unable to get size of temporary max bitrate file.");
    fclose(vidStream);
    fclose(vFile);
    return 1;
  }

  totalPatches = statbuf.st_size / sizeof(byteCount);
  patchCount = 0;
  DisplayInfo(" ");
  DisplayInfo("Embedding max bitrate value in sequence headers ...");

  /* note, this only works when the max_bit_rate is < 104857600 bps */
  byteCount = (unsigned int)ceil(max_bitrate / 400.0);
  patchVal[0] = (unsigned char)((byteCount & 0x3FC00) >> 10);
  patchVal[1] = (unsigned char)((byteCount & 0x03FC) >> 2);
  patchVal[2] = (unsigned char)(((byteCount & 0x3) << 6) | 0x20 | ((vbv_buffer_size & 0x03E0) >> 5));

  while (patchCount < totalPatches)
  {
    percent = (int)floor(((double) (patchCount + 1)) / ((double) totalPatches) * 100.0);
    sprintf(tmpStr, "Embedding max bitrate values: %d%% - %d of %d.", percent, patchCount + 1, totalPatches);
    DisplayProgress(tmpStr, percent);
    if (fread(&byteCount, 1, sizeof(byteCount), vFile) != sizeof(byteCount))
    {
      DisplayError("Unable to read from temporary max bitrate file.");
      fclose(vFile);
      fclose(vidStream);
      return 1;
    }
    fseek(vidStream, (long)byteCount, SEEK_SET);
    if (ferror(vidStream))
    {
      sprintf(tmpStr, "Unable to seek in video stream, offset = %u.", byteCount);
      DisplayError(tmpStr);
      fclose(vFile);
      fclose(vidStream);
      return 1;
    }

    fwrite(patchVal, 3, 1, vidStream);
    if (ferror(vidStream))
    {
      DisplayError("Unable to write to video stream");
      fclose(vFile);
      fclose(vidStream);
      return 1;
    }
    patchCount++;
  }
  fclose(vFile);
  fclose(vidStream);
  
  return 0;
}


