/*
 *  export_mpeg.c
 *
 *  Copyright (C) Gerhard Monzel - December 2001
 *
 *  Thanks to Brent Beyeler (beyeler.@home.com) for the bbmpeg stuff
 *  on windows, which i have ported to linux and modified to use it
 *  as libbbmpeg.a !
 * 
 *  This file is part of transcode, a linux video stream processing tool
 *      
 *  transcode is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *   
 *  transcode is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *   
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "vid_aux.h"

//-- experimental --
//#define HAS_DNR    1
//------------------

#define HAS_FFMPEG 1

#ifdef HAS_DNR
#include "mpeg_dnr.c"
#endif

#include "transcode.h"
#include "../bbmpeg/bbencode.h"

#ifdef HAS_FFMPEG
#include "../ffmpeg/libavcodec/avcodec.h"
#endif

#ifdef HAS_DNR
#define MOD_NAME    "export_mpeg_dnr.so"
#else
#define MOD_NAME    "export_mpeg.so"
#endif
#define MOD_VERSION "v1.2.3 (2003-08-21)"
#ifdef HAS_FFMPEG
#define MOD_CODEC   "(video) MPEG 1/2 | (audio) MPEG 1 Layer II"
#else
#define MOD_CODEC   "(video) MPEG 1/2"
#endif

#define MOD_PRE mpeg
#include "export_def.h"

static T_BBMPEG_CTX *bbmpeg_ctx   = NULL;
static int          bbmpeg_type   = 1;
static int          bbmpeg_dst_w  = 352;
static int          bbmpeg_dst_h  = 288;  
static int          bbmpeg_size_l = 0;
static int          bbmpeg_size_c = 0;
static int          bbmpeg_fnew   = 0;
static int          bbmpeg_fcnt   = -1;
static vob_t        bbmpeg_vob;

#ifdef HAS_FFMPEG
static AVCodec        *mpa_codec = NULL;
static AVCodecContext mpa_ctx;
static FILE*          mpa_out_file = NULL;
static char           *mpa_buf     = NULL;
static int            mpa_buf_ptr  = 0;
static int            mpa_bytes_ps, mpa_bytes_pf;
static ReSampleContext *ReSamplectx=NULL;
#endif

static int verbose_flag=TC_QUIET;
#ifdef HAS_FFMPEG
static int capability_flag=TC_CAP_PCM|TC_CAP_YUV|TC_CAP_RGB;
#else
static int capability_flag=TC_CAP_YUV|TC_CAP_RGB;
#endif


//== cleanup/setup buffer to read source mpeg into ==
//===================================================
static int  setup_buf_done = 0;

static void mpeg_cleanup_pagebuf(T_BBMPEG_CTX *ctx)
{
  if (ctx)
  {
    if (ctx->pY) free (ctx->pY);    
    if (ctx->pU) free (ctx->pU);
    if (ctx->pV) free (ctx->pV);
   
    ctx->pY = NULL;
    ctx->pU = NULL;
    ctx->pV = NULL;
  }
  
  setup_buf_done = 0;
}

static int mpeg_create_pagebuf(T_BBMPEG_CTX *ctx, int dst_w, int dst_h)
{
  //-- run only once ! --
  if (setup_buf_done) 
    return 1;
  else
  {
    setup_buf_done = 1;
    
    ctx->pY = malloc(ctx->pic_size_l * ctx->gop_size_max );
    ctx->pU = malloc(ctx->pic_size_c * ctx->gop_size_max );
    ctx->pV = malloc(ctx->pic_size_c * ctx->gop_size_max );

    if ( !(ctx->pY) || !(ctx->pU)  || !(ctx->pV) )
    {
      mpeg_cleanup_pagebuf(ctx);  
      return 0;
    }
  }
  
  return 1;  
}

//== little helper ==
//===================
static void adjust_ch(char *line, char ch)
{
  char *src = &line[strlen(line)];
  char *dst = line;

  //-- remove blanks from right and left side --
  do { src--; } while ( (src != line) && (*src == ch) );
  *(src+1) = '\0';
  src = line;
  while (*src == ch) src++; 

  if (src == line) return;

  //-- copy rest --
  while (*src)
  {
    *dst = *src;
    src++;
    dst++;
  }
  *dst = '\0';
}


/* ------------------------------------------------------------ 
 *
 * open outputfile
 *
 * ------------------------------------------------------------*/

MOD_open
{
    
  if(param->flag == TC_VIDEO) 
  {
    int  dst_w, dst_h;
    char out_fname[PATH_MAX];
    
    //-- save vob parameters --
    //-------------------------
    if (bbmpeg_fcnt<0) 
    {
      memcpy(&bbmpeg_vob, vob, sizeof(vob_t));
      bbmpeg_fcnt = 0;
    }
      
    // fprintf(stderr, "[%s] *** open-v *** !\n", MOD_NAME);
    
    //-- adjust w/h-size to resize-mode --
    //------------------------------------
    if (bbmpeg_dst_w != -1)
    {
      dst_w = bbmpeg_dst_w;
      dst_h = bbmpeg_dst_h;
    }
    else
    { 
      dst_w = vob->ex_v_width;
      dst_h = vob->ex_v_height;
    } 
    
    //-- create out-filename depending on mpeg-type --
    //------------------------------------------------
    strcpy(out_fname, vob->video_out_file);
    
    if (bbmpeg_fcnt > 0)
      sprintf(out_fname, "%s-%03d", out_fname, bbmpeg_fcnt);
     
    if (bbmpeg_type == 1)
      strcat(out_fname, ".m1v");    
    else
      strcat(out_fname, ".m2v");  

    //-- now init encoder with this prepared stuff --
    //-----------------------------------------------   
    bbmpeg_ctx = bb_start(out_fname, dst_w, dst_h, (verbose_flag > 0));
    if (bbmpeg_ctx == NULL)  
    {
      fprintf(stderr, "[%s] error on initialization !\n", MOD_NAME);
      return(TC_EXPORT_ERROR); 
    }

    //-- create page-buffer to hold (resized) frames for 1 GOP (N) --
    //---------------------------------------------------------------
    if ( !mpeg_create_pagebuf(bbmpeg_ctx, dst_w, dst_h) )
    {
      fprintf(stderr, "[%s] out of memory while allocting page-buffer\n",
              MOD_NAME);
              
      return(TC_EXPORT_ERROR); 
    }
   
    //fprintf(stderr, "[%s] writing video to [%s]\n",
    //                  MOD_NAME, out_fname);
    
    return(0);
  }
  
#ifdef HAS_FFMPEG  
  if(param->flag == TC_AUDIO) 
  {
    if (mpa_out_file == NULL)
    { 
      char out_fname[256];
      
      //fprintf(stderr, "[%s] *** open-a *** !\n", MOD_NAME); 

      strcpy(out_fname, vob->video_out_file);
      
      if (bbmpeg_fcnt > 0)
        sprintf(out_fname, "%s-%03d", out_fname, bbmpeg_fcnt);

      strcat(out_fname, ".mpa");
      
      //-- open output-file --
      //----------------------
      if( (mpa_out_file = fopen(out_fname, "wb")) == NULL) 
      {
        fprintf(stderr, "[%s] could not open file (%s) !\n", MOD_NAME, out_fname);
        return(TC_EXPORT_ERROR); 
      }
      
      //-- set parameters (bitrate, channels and sample-rate) --
      //--------------------------------------------------------
      memset(&mpa_ctx, 0, sizeof(mpa_ctx));       // default all
      mpa_ctx.bit_rate = vob->mp3bitrate * 1000;  // bitrate dest.
      mpa_ctx.channels = vob->dm_chan;            // channels
      if (!vob->mp3frequency)                     // sample-rate dest.
        mpa_ctx.sample_rate = vob->a_rate;        
      else {
	//ThOe added ffmpeg re-sampling capability
        mpa_ctx.sample_rate = vob->mp3frequency;
	ReSamplectx = audio_resample_init(vob->dm_chan, vob->dm_chan,
					  vob->mp3frequency, vob->a_rate);
      }

      //-- open codec --
      //----------------
      if (avcodec_open(&mpa_ctx, mpa_codec) < 0) 
      {
        fprintf(stderr, "[%s] could not open mpa codec !\n", MOD_NAME);
        return(TC_EXPORT_ERROR); 
      }
    
      //-- bytes per sample and bytes per frame --
      mpa_bytes_ps = mpa_ctx.channels * vob->dm_bits/8;
      mpa_bytes_pf = mpa_ctx.frame_size * mpa_bytes_ps;
    
      //-- create buffer to hold 1 frame --
      mpa_buf     = malloc(mpa_bytes_pf);
      mpa_buf_ptr = 0;

      //fprintf(stderr, "[%s] writing audio to [%s]\n",
      //                MOD_NAME, out_fname);  
    }
    return(0);
  }
#endif

  // invalid flag
  return(TC_EXPORT_ERROR); 
}


/* ------------------------------------------------------------ 
 *
 * init codec
 *
 * ------------------------------------------------------------*/
MOD_init
{
  if(param->flag == TC_VIDEO) 
  {
    int tv_type;
    
    int asr=0, frc=0;
    int bitrate=0, max_bitrate=0;
    
    char base_profile = '1'; // default MPEG1
    char *p1 = NULL;
    char *p2 = NULL;
    char *p3 = NULL;

    //ThOe added RGB2YUV cap
    if(vob->im_v_codec == CODEC_RGB) {
	if(tc_rgb2yuv_init(vob->ex_v_width, vob->ex_v_height)<0) {
	    fprintf(stderr, "[%s] rgb2yuv init failed\n", MOD_NAME);
	    return(TC_EXPORT_ERROR); 
	}
    }
    
    //fprintf(stderr, "[%s] *** init-v *** !\n", MOD_NAME); 
    
    //-- transcode's "-F" parameter is the right one to tell -- 
    //-- mpeg-encoder it's own parameters, with this syntax: --
    //-- -F "<base-profile>[,<resizer-mode>[,user-profile]]" --
    //---------------------------------------------------------

    if(vob->ex_v_fcc != NULL && strlen(vob->ex_v_fcc) != 0) {
      p1 = vob->ex_v_fcc;
      adjust_ch(p1, ' ');//-- parameter 1 (base profile) --
    }
    
    if(vob->ex_a_fcc != NULL && strlen(vob->ex_a_fcc) != 0) {
      p2 = vob->ex_a_fcc;
      adjust_ch(p2, ' ');//-- parameter 2 (resizer-mode) --
    }
    
    if(vob->ex_profile_name != NULL && strlen(vob->ex_profile_name) != 0) {
      p3 = vob->ex_profile_name;
      adjust_ch(p3, ' ');//-- parameter 3 (user profile-name) --
    }

    if(verbose_flag & TC_DEBUG) fprintf(stderr, "P1=%s, P2=%s, P3=%s\n", p1, p2, p3);

    //-- adjust frame rate stuff --
    //-----------------------------
    if ((int)(vob->ex_fps*100.0 + 0.01) == (int)(29.97*100.0)) {
      frc=4;
      tv_type = ENCODE_NTSC;
    } else if ((int)(vob->ex_fps*100.0 + 0.01) == (int)(23.97*100.0)) {
      frc=1;
      tv_type = ENCODE_NTSC;
    } else if ((int)(vob->ex_fps*100.0 + 0.01) == (int)(24.00*100.0)) {
      frc=2;
      tv_type = ENCODE_NTSC;
    } else {
      frc=3;
      tv_type = ENCODE_PAL;
    }
    
    //ThOe pulldown?
    if(vob->pulldown) {
      if(frc==1) frc=4;
      if(frc==2) frc=5;
    }

    //ThOe overwrite selection with user export frame rate code 
    if(vob->ex_frc) frc=vob->ex_frc;

    //-- check parameter ("-F <p>,?,?") for base profile. --
    //-- available profiles (1 char string):              --
    //--  '1' = MPEG 1 (default)                          --
    //--  'b' = big MPEG 1 (experimental)                 --
    //--  'v' = VCD                                       --
    //--  's' = SVCD                                      --
    //--  '2' = MPEG2                                     --
    //--  'd' = DVD compliant
    //------------------------------------------------------


    if (vob->divxbitrate != VBITRATE)
	bitrate = vob->divxbitrate;

    if (vob->video_max_bitrate != 0)
	max_bitrate = vob->video_max_bitrate;

    if (p1 && strlen(p1))
    {  
      base_profile = tolower(*p1);
      if (!strchr("1bvs2d", base_profile)) base_profile = '1'; // MPEG1 is default 
    
      //-- eventually force frame-rate (n=NTSC, rest=PAL) --
      if (strlen(p1) > 1)
      {
        if (tolower(p1[1]) == 'n') 
          tv_type = ENCODE_NTSC;
        else 
          tv_type = ENCODE_PAL;  
      }
      if (strchr("d", base_profile) && vob->divxbitrate==VBITRATE) {
	  bitrate=6000;
	  if(vob->video_max_bitrate == 0)
	      max_bitrate = 9800;
      }
    }

    if(max_bitrate < bitrate) {
	//tc_warn("Maximum bitrate is smaller than average bitrate, fixing.");
	max_bitrate = bitrate;
    }
    
    //-- parameter ("-F ?,?,<"user profile">") will be used as --
    //-- user profile with paramters to overload base profile. --
    //-----------------------------------------------------------

    asr = (vob->ex_asr<0) ? vob->im_asr:vob->ex_asr;
    
    if (p3 && strlen(p3))
      bb_set_profile(p3, base_profile, tv_type, asr, frc, vob->pulldown, verbose_flag, bitrate, max_bitrate);  
    else
      bb_set_profile(NULL, base_profile, tv_type, asr, frc, vob->pulldown, verbose_flag, bitrate, max_bitrate);
      
    //-- store type of mpeg (for later use) --
    //----------------------------------------
    if (strchr("1bv", base_profile)) 
      bbmpeg_type = 1; // is mpeg1
    else
      bbmpeg_type = 2; // is mpeg2 
   
    //-- check parameter ("-F ?,<r>,?") for resizer --
    //-- and setup w/h values of destination frame  --
    //-- ( = encoder input). available values are:  --
    //--   0 = disable resizer (default)            --
    //--   1 = 352x288                              --
    //--   2 = 480x480                              --
    //--   3 = 480x576                              --
    //--   4 = 352x240                              --
    //------------------------------------------------ 
    bbmpeg_dst_w = -1; // default: resizer disabled
    bbmpeg_dst_w = -1;
    
    if (p2 && strlen(p2) ) switch (*p2)
    {
      //-- resize to VCD-NTSC --
      case '4':
        bbmpeg_dst_w = 352;
        bbmpeg_dst_h = 240;
        break;
      
      //-- resize to SVCD-PAL --  
      case '3':
        bbmpeg_dst_w = 480;
        bbmpeg_dst_h = 576;
        break;
      
      //-- resize to SVCD-NTSC --
      case '2':
        bbmpeg_dst_w = 480;
        bbmpeg_dst_h = 480;
        break;
      
      //-- resize to VCD-PAL --
      case '1':
        bbmpeg_dst_w = 352;
        bbmpeg_dst_h = 288;
        break;
        
      default:
        break;  
    } 
    
    //-- on resize request setup resizer --
    //-------------------------------------
    if (bbmpeg_dst_w != -1) 
    {
      bb_resize_setup(vob->ex_v_width, vob->ex_v_height,
                      bbmpeg_dst_w, bbmpeg_dst_h, verbose_flag);
    }
    else if ( ((vob->ex_v_width/16) * 16) != vob->ex_v_width)
    {
      fprintf(stderr, "[%s] error: picture width (%d) isn't a multiple of 16\n",
              MOD_NAME, vob->ex_v_width);
    }
      
    //-- set size-values (luma/chroma) of source buffer for later use --
    //------------------------------------------------------------------
    bbmpeg_size_l = vob->ex_v_width * vob->ex_v_height;
    bbmpeg_size_c = bbmpeg_size_l/4;

#ifdef HAS_DNR
    if (bbmpeg_dst_w != -1)
      my_fctx = dnr_init( bbmpeg_dst_w, bbmpeg_dst_h, 1); 
    else 
      my_fctx = dnr_init( vob->ex_v_width, vob->ex_v_height, 1); 
    if (!my_fctx) return (TC_EXPORT_ERROR);
#endif
    
    return(0);
  }

#ifdef HAS_FFMPEG
  if(param->flag == TC_AUDIO) 
  {
    //fprintf(stderr, "[%s] *** init-a *** !\n", MOD_NAME); 

    //-- initialization of ffmpeg stuff:          --
    //-- only mpeg1 layer II audio encoder needed --
    //----------------------------------------------
    avcodec_init();
    register_avcodec(&mp2_encoder);
    
    //-- get it --
    mpa_codec = avcodec_find_encoder(CODEC_ID_MP2);
    if (!mpa_codec) 
    {
      fprintf(stderr, "[%s] mpa codec not found !\n", MOD_NAME);
      return(TC_EXPORT_ERROR); 
    }
    return(0);  
  }
#endif

  // invalid flag
  return(TC_EXPORT_ERROR); 
}

/* ------------------------------------------------------------ 
 *
 * encode and export frame
 *
 * ------------------------------------------------------------*/

#define OUTBUF_SIZE 8192

static char out_buf[OUTBUF_SIZE];
static char tmp_buf[OUTBUF_SIZE*4];
static int page_buf_cnt = 0;

MOD_encode
{
  if(param->flag == TC_VIDEO && (bbmpeg_ctx != NULL) ) 
  { 
      int i, size_l, size_c;
      unsigned char *py_src, *pu_src, *pv_src;
      unsigned char *py_dst, *pu_dst, *pv_dst;

      //ThOe 
      if(tc_rgb2yuv_core(param->buffer)<0) {
	  fprintf(stderr, "[%s] rgb2yuv conversion failed\n", MOD_NAME);
	  return(TC_EXPORT_ERROR);
      }
      
      //fprintf(stderr, "\n[%s] *** encode-v *** !\n", MOD_NAME); 

      //-- settup pointers to source and destination YUV-Pages --
      //---------------------------------------------------------      
      size_l = bbmpeg_ctx->pic_size_l;
      size_c = bbmpeg_ctx->pic_size_c;
      
      py_dst = bbmpeg_ctx->pY + page_buf_cnt * size_l;
      pu_dst = bbmpeg_ctx->pU + page_buf_cnt * size_c;
      pv_dst = bbmpeg_ctx->pV + page_buf_cnt * size_c;
      
      py_src = param->buffer;
      pv_src = py_src + bbmpeg_size_l;
      pu_src = pv_src + bbmpeg_size_c;

      //-- write buffer page with or without resizing --
      //------------------------------------------------
      if (bbmpeg_dst_w != -1)
      {
        bb_resize_frame(py_src, pu_src, pv_src, py_dst, pu_dst, pv_dst);  
      }
      else
      {
        memcpy(py_dst, py_src, size_l);
        memcpy(pu_dst, pu_src, size_c);
        memcpy(pv_dst, pv_src, size_c);
      }

#ifdef HAS_DNR      
      dnr_run(my_fctx, py_dst);
      if (my_fctx->undo) memcpy(py_dst, my_fctx->undo_data, my_fctx->img_size);
#endif

      //-- return on incomplete buffer fill --
      //--------------------------------------
      page_buf_cnt++;
      if (page_buf_cnt < bbmpeg_ctx->gop_size) 
      {
        tc_progress(""); // supress any progress !
        return (0);
      } 

      //-- encode all frames from page-buffer.  --
      //-- On undo request repeat encoding loop --
      //-- (VBR bitrate limiter (if enabled)    --
      //-- will produce this, if bitrate of     --
      //-- encoded GOP runs out of boundaries)  --  
      //------------------------------------------
      do
      {
        for (i=0; i<page_buf_cnt; i++)
        {
          //-- break on error (or undo) --
          if (bb_encode(bbmpeg_ctx, 0) != ENCODE_RUN) break;
  
          if (verbose_flag & TC_DEBUG)        
            fprintf(stderr, "Video: %s  \r", bbmpeg_ctx->progress_str); 
        } 

      } while ( bb_encode(bbmpeg_ctx, 1) == ENCODE_UNDO );
      
      
      if (verbose_flag) tc_progress("");
        
      //-- urgency stop --
      //------------------
      if ((bbmpeg_ctx->ret) == ENCODE_STOP)
      {  
        bb_stop(bbmpeg_ctx);
        
        mpeg_cleanup_pagebuf(bbmpeg_ctx);
        bbmpeg_ctx = NULL;
        
        return(TC_EXPORT_ERROR);
      }
      //-- severe encoder error --
      //--------------------------
      else if (bbmpeg_ctx->ret == ENCODE_ERR)
      {
        mpeg_cleanup_pagebuf(bbmpeg_ctx);
        bbmpeg_ctx = NULL;
        
        return(TC_EXPORT_ERROR);
      }
      
      page_buf_cnt = 0;   

      //-- check file-size --
      //---------------------
      if (bbmpeg_ctx->max_file_size)
      {
        if (bbmpeg_ctx->file_size > bbmpeg_ctx->max_file_size)
        {
          bbmpeg_fcnt++;
          bbmpeg_fnew = 1;       // request new audio-file 
          MOD_PRE_close(param);
          MOD_PRE_open(param, &bbmpeg_vob);
        }
      }

      return(0);
  }

#ifdef HAS_FFMPEG  
  if(param->flag == TC_AUDIO) 
  {
    int  in_size, out_size, new_size;
    char *in_buf;
   
    //fprintf(stderr, "\n[%s] *** encode-a *** !\n", MOD_NAME); 
    
    //-- handle new file request --
    if (bbmpeg_fnew)
    {
      bbmpeg_fnew = 0;
      MOD_PRE_close(param);
      MOD_PRE_open(param, &bbmpeg_vob);
    }        

    //-- input buffer and amount of bytes -- 
    in_size = param->size;
    in_buf  = param->buffer;

    
    // ThOe -- do the resampling first --
    if(ReSamplectx!=NULL) {
      
      new_size = audio_resample(ReSamplectx, (short *) tmp_buf, (short *) in_buf, in_size/mpa_bytes_ps);
      
      in_size = new_size*mpa_bytes_ps;
      in_buf = tmp_buf;
    }
    
    //-- any byte in mpa-buffer left from past call ? --
    //-------------------------------------------------- 
    if (mpa_buf_ptr > 0) {
      
      int bytes_needed, bytes_avail;
      
      bytes_needed = mpa_bytes_pf - mpa_buf_ptr;
      bytes_avail  = in_size; 
      
      //-- complete frame -> encode --
      //------------------------------
      if ( bytes_avail >= bytes_needed ) {
	
	memcpy(&mpa_buf[mpa_buf_ptr], in_buf, bytes_needed);
	
	out_size = avcodec_encode_audio(&mpa_ctx, (unsigned char *)out_buf, 
					OUTBUF_SIZE, (short *)mpa_buf);
        fwrite(out_buf, 1, out_size, mpa_out_file);
	
        in_size -= bytes_needed; 
        in_buf  += bytes_needed;
        
        mpa_buf_ptr = 0;
      }
      
      //-- incomplete frame -> append bytes to mpa-buffer and return --
      //--------------------------------------------------------------- 
      else {
	
	memcpy(&mpa_buf[mpa_buf_ptr], param->buffer, bytes_avail);
        mpa_buf_ptr += bytes_avail;
        return (0);
      }
    } //bytes availabe from last call?
    

    //-- encode only as much "full" frames as available --
    //---------------------------------------------------- 
    
    while (in_size >= mpa_bytes_pf) {
      
      out_size = avcodec_encode_audio(&mpa_ctx, (unsigned char *)out_buf, 
				      OUTBUF_SIZE, (short *)in_buf);
      
      fwrite(out_buf, 1, out_size, mpa_out_file);
      
      in_size -= mpa_bytes_pf; 
      in_buf  += mpa_bytes_pf;
    }
    
    //-- hold rest of bytes in mpa-buffer --   
    //--------------------------------------
    if (in_size > 0) {
      mpa_buf_ptr = in_size; 
      memcpy(mpa_buf, in_buf, mpa_buf_ptr);
    }
    
    return(0);
  }
#endif
  
  // invalid flag
  return(TC_EXPORT_ERROR); 
}

/* ------------------------------------------------------------ 
 *
 * stop encoder
 *
 * ------------------------------------------------------------*/

MOD_stop
{  
  
  if(param->flag == TC_VIDEO) 
  { 
#ifdef HAS_DNR
    if (my_fctx) dnr_cleanup(my_fctx);
    my_fctx = NULL;
#endif      

    //ThOe
    tc_rgb2yuv_close();

    return(0);
  }

#ifdef HAS_FFMPEG  
  if(param->flag == TC_AUDIO) 
  {
    //-- cleanup encoder --
    mpa_codec = NULL;
    
    return (0);
  }  
#endif
  
  return(TC_EXPORT_ERROR);     
}

/* ------------------------------------------------------------ 
 *
 * close codec
 *
 * ------------------------------------------------------------*/

MOD_close
{  

#ifdef HAS_FFMPEG
  if(param->flag == TC_AUDIO) 
  {
    //-- release encoder --
    if (mpa_codec) avcodec_close(&mpa_ctx);

    //ThOe cleanup re-sampling data
    if(ReSamplectx!=NULL) audio_resample_close(ReSamplectx);
    
    //-- cleanup buffer resources --
    if (mpa_buf) free(mpa_buf);
    mpa_buf     = NULL;
    mpa_buf_ptr = 0;

    //-- close output-file --
    if (mpa_out_file) fclose(mpa_out_file);
    mpa_out_file = NULL;
    
    return (0);
  } 
#endif
    
  if(param->flag == TC_VIDEO) 
  {
    //fprintf(stderr, "[%s] *** close *** !\n", MOD_NAME);
    
    if (bbmpeg_ctx == NULL) return (0);
    
    if (bbmpeg_ctx->ret != ENCODE_ERR)
    { 
      int i;
      for (i=0; i<page_buf_cnt; i++) 
      {
        bb_encode(bbmpeg_ctx, 0);
      
        if (verbose_flag & TC_DEBUG)
          fprintf(stderr, "Video: %s  \r", bbmpeg_ctx->progress_str);
      }
      if (verbose_flag)
        tc_progress(bbmpeg_ctx->progress_str);
    }  
    bb_stop(bbmpeg_ctx);
    
    page_buf_cnt = 0;
    mpeg_cleanup_pagebuf(bbmpeg_ctx);
    bbmpeg_ctx = NULL;

    return(0);
  }

  return(TC_EXPORT_ERROR); 
}

