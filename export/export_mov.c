/*
 *  export_mov.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
 *  Copyright (C) (2002) Christian Vogelgsang 
 *  <Vogelgsang@informatik.uni-erlangen.de> (extension for all codecs supported
 *  by quicktime4linux
 *  <stefanscheffler@gmx.net> 2004 fixes & features
 *  Copyright (C) ken@hero.com 2001 (initial module author)
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
#include "import/magic.h"
#include "transcode.h"
#include "vid_aux.h"

#define MOD_NAME    "export_mov.so"
#define MOD_VERSION "v0.1.2 (2004-01-19)"
#define MOD_CODEC   "(video) * | (audio) *"
/* verbose flag */
static int verbose_flag=TC_QUIET;

/* set encoder capabilities */
static int capability_flag=
 /* audio formats */
 TC_CAP_PCM| /* pcm audio */
 /* video formats */
 TC_CAP_RGB|  /* rgb frames */
 TC_CAP_YUV|  /* chunky YUV 4:2:0 */
 TC_CAP_YUY2|
 TC_CAP_VID|
 TC_CAP_YUV422; /* chunky YUV 4:2:2 */

#define MOD_PRE mov
#include "export_def.h"

#include <quicktime.h>

/* exported quicktime file */
static quicktime_t *qtfile = NULL;

/* row pointer for source frames */
static unsigned char** row_ptr = NULL;

/* temporary buffer*/
char *tmp_buf;

/* toggle for raw frame export */
static int rawVideo = 0;
static int rawAudio = 0;

/* 
color model to export to
yuv420 is planar == BC_YUV420P  == 7
rgb is packed == BC_RGB888 == 9
yuy422 is packed == BC_YUV422 == 19
*/
static int qt_cm = 0;

/* store frame dimension */
static int w = 0,h = 0;

/* audio channels */
static int channels = 0;

/* sample size */
static int bits = 0;

/* encode audio buffers */
static int16_t* audbuf0 = NULL;
static int16_t* audbuf1 = NULL;

/* defaults for codec parameters */
int fix_bitrate = 0; 
int div3_interlaced = 0;
int div3_bitrate_tolerance = 500000;
int use_float =0;
int divx_use_deblocking = 0;

int vorbis_max_bitrate = 192;        
int vorbis_min_bitrate = 128;

/* dv */
int dv_anamorphic16x9 = 0;
int dv_vlc_encode_passes = 3; 

struct qt_codec_list {
  char *name;
  char *internal_name;
  char *comments;
};

/* lists might be outdated */
/* list of vcodec*/
struct qt_codec_list qt_vcodecs_list[] = {
  {"divx", "DIVX", "MPEG4 compliant video"},
  {"hv60", "HV60", "Non standart MPEG4 variant"},  
  {"jpeg", "jpeg","JPEG Photo"},
  {"mjpa", "mjpa","Motion JPEG-A (default)"},
  {"png", "png ", "PNG Frames (only RGB)"},
  {"dv", "dvc ", "Digital Video"},
  {"rgb", "raw ", "Uncompressed RGB"},
  {"yv12", "yv12", "Planar YUV420"},
  {"yuv2", "yuv2", "Packed YUV422 (requires --uyvy)"},
  {"v410", "v410", "Planar 10bit YUV444  (only RGB)"},  
  {NULL, NULL, NULL}};
  
/* list of acodec*/
struct qt_codec_list qt_acodecs_list[] = {
  {"ima4", "ima4", "IMA4 compressed audio (default)"},
  {"twos", "twos", "uncompressed audio"},  
  {"ogg", "OggS", "OGG Vorbis"},
  {"mp3", ".mp3", "MP3 Compressed audio"},
  {NULL, NULL, NULL}};

/* list of paramters */
struct qt_codec_list qt_param_list[] = {
  {"divx_bitrate", "","DIVX bitrate (default: -w <b>)"},
  {"divx_rc_period", "","DIVX rate control (def: --divx_rc <p>"},  
  {"divx_rc_reaction_period", "","DIVX rate control (def: --divx_rc ,<rp>"},
  {"divx_rc_reaction_ratio", "", "DIVX rate control (def: --divx_rc ,,<rr>"},
  {"divx_max_key_interval", "", "DIVX max keyframe interval (def: -w ,<k>"},
  {"divx_max_quantizer", "", "DIVX maximum quantizer <0-32>"}, 
  {"", "", "    (default: --divx_quant ,<max>)"},  
  {"divx_min_quantizer", "", "DIVX minimum quantizer <0-32>"},
  {"", "", "    (default: --divx_quant <min>)"},
  {"divx_quality", "", "DIVX encoding quality (def: -Q <n>"},
  {"quantizer", "", "DIVX quant. for vbr (divx_fix_bitrate=0)"},
  {"", "", "    (default: --divx_quant <min>)"},
  {"divx_fix_bitrate", "", "DIVX fixed br. encoding <1/0> (def: 0)"},
  {"divx_use_deblocking", "", "DIVX deblocking <1/0> (def: 0)"},
  {"jpeg_quality", "", "JPEG quality <1-100> (def: -Q * 20)"},
  {"jpeg_usefloat", "", "JPEG use float <1/0> (def: 0)"},
  {"mp3bitrate", "", "MP3 bitrate (default: -b <b>)"},
  {"mp3quality", "", "MP3 quality (default: -b ,,<g>)"},
  {"vorbis_bitrate", "", "Vorbis bitrate (default: -b <b>)"},
  {"vorbis_vbr", "", "Vorbis vbr mode (default: -b ,<v>)"},
  {"vorbis_max_bitrate", "", "Vorbis max. br. for vbr (def: 192)"},
  {"vorbis_min_bitrate", "", "Vorbis min. br. for vbr (def: 128)"},
  {"dv_anamorphic16x9", "", "??? (default: 0)"}, 
  {"dv_vlc_encode_passes", "",  "??? (default: 3)"},
  {"copyright", "",  "Copyright string (no '=' or ',' allowed)"},
  {"name", "",  "Name string (no '=' or ',' allowed) "},
  {NULL, NULL, NULL}};

int list_was_called = 0;
int do_cs_conv = 0;

/* print list of things. Shamelessly stolen from export_ffmpeg.c */ 
static int list(struct  qt_codec_list codec_list[], char *list_type) 
{
      int i =0;      
      if (list_type == NULL ) return 0;
      
      fprintf(stderr, "[%s] List of known and working %s:\n"
              "[%s] Name                    comments\n"
              "[%s] ---------------         ---------------------------"
              "--------\n", MOD_NAME, list_type,  MOD_NAME, MOD_NAME);
      while (codec_list[i].name != NULL) {
        fprintf(stderr, "[%s] %-23s %s\n", MOD_NAME,
                codec_list[i].name, 
                codec_list[i].comments);
        i++;
      }
      fprintf(stderr, "[%s]\n", MOD_NAME); 
      return 1;
}

/* ------------------------------------------------------------ 
 *
 * open outputfile
 *
 * ------------------------------------------------------------*/

MOD_open
{
  if(param->flag == TC_VIDEO) 
    return(0);
  
  if(param->flag == TC_AUDIO) 
    return(0);
  
  return(TC_EXPORT_ERROR);
}

/* ------------------------------------------------------------ 
 *
 * init codec
 *
 * ------------------------------------------------------------*/

MOD_init
{

  int bla = 0;

  /* overwriting empty parameters now saves trouble later */
  if (vob->ex_v_fcc == NULL) vob->ex_v_fcc = "";
  if (vob->ex_a_fcc == NULL) vob->ex_a_fcc = "";
  if (vob->ex_profile_name == NULL) vob->ex_profile_name = "";

  if (!strcasecmp(vob->ex_v_fcc, "list")) list_was_called = list(qt_vcodecs_list, "video codec");
  if (!strcasecmp(vob->ex_a_fcc, "list")) list_was_called = list(qt_acodecs_list, "audio codec");
  if (!strcasecmp(vob->ex_profile_name, "list")) list_was_called = list(qt_param_list, "parameters");

  if (list_was_called) {
    fprintf(stderr ,"[%s] Warning: The list is based on Quicktime4linux 2.01\n", 
        MOD_NAME);
    fprintf(stderr ,"[%s] it might be incorrect for other libraries/versions\n", 
        MOD_NAME);
    return(TC_EXPORT_ERROR);
  }
  /* video setup -------------------------------------------------- */
  if(param->flag == TC_VIDEO) {
    char *qt_codec;
    int divx_bitrate;
    
    /* fetch frame size */
    w = vob->ex_v_width;
    h = vob->ex_v_height;

    /* fetch qt codec from -F switch */
    qt_codec = vob->ex_v_fcc;
    
    /* change codec to internal name and make  case insensitive */
    while (qt_vcodecs_list[bla].name != NULL) {
        if (!strcasecmp(vob->ex_v_fcc, qt_vcodecs_list[bla].name)) strcpy(vob->ex_v_fcc, qt_vcodecs_list[bla].internal_name);
    bla++;
    }    
    
    /* open target file for writing */
    if(NULL == (qtfile = quicktime_open(vob->video_out_file, 0, 1)) ){
      fprintf(stderr,"[%s] error opening qt file '%s'\n",
	      MOD_NAME,vob->video_out_file);
      return(TC_EXPORT_ERROR);
    }

    /* set qt codecs */
    /* not needed for passthrough*/
    if (vob->im_v_codec != CODEC_RAW && vob->im_v_codec != CODEC_RAW_YUV && vob->im_v_codec != CODEC_RAW_RGB) {
        if(qt_codec == NULL || strlen(qt_codec)==0) {
          /* default */     
          qt_codec = "mjpa";
          if (verbose_flag != TC_QUIET) fprintf(stderr,"[%s] INFO: empty qt codec name - switching to %s use '-F list'\n[%s]       to get a list of supported codec \n", MOD_NAME, qt_codec, MOD_NAME);
        }
        
        /* check frame format */            
        if (!strcmp(qt_codec, "cvid")){
                if( ((w%16)!=0) || ((h%16)!=0) ) {
                    fprintf(stderr,"[%s] width/height must be multiples of 16\n",
                            MOD_NAME);
                    fprintf(stderr,"[%s] Try the -j option\n",
        	            MOD_NAME);
                    return(TC_EXPORT_ERROR);
                }
        }
        
        /* set proposed video codec */
        quicktime_set_video(qtfile, 1, w, h, vob->ex_fps,qt_codec); 
    
        /* check if we can encode with this codec */    
        if (!quicktime_supported_video(qtfile, 0)){
        fprintf(stderr,"[%s] qt video codec '%s' unsupported\n",
            MOD_NAME,qt_codec);
            return(TC_EXPORT_ERROR);
        }
    }

    /* set color model */
    switch(vob->im_v_codec) {
        case CODEC_RGB:
              /* use raw mode when possible */                      /* not working */
              /*if (strcmp(qt_codec, "raw ")) rawVideo=1; */
              quicktime_set_cmodel(qtfile, 9); qt_cm = 9;
              break;
              
        case CODEC_YUV:
              /* use raw mode when possible */                      /* not working*/
              /* if (strcmp(qt_codec, "yv12")) rawVideo=1; */       
              /*fprintf(stderr," using yuv420\n");*/
              quicktime_set_cmodel(qtfile, 7); qt_cm = 7;        
              break;

        case CODEC_YUV422:
              /*fprintf(stderr," using yuv422\n"); */                 
              quicktime_set_cmodel(qtfile, 19); qt_cm = 19;
              break;
                         
        case CODEC_YUY2:
              /*fprintf(stderr," using yuy2\n");*/
              quicktime_set_cmodel(qtfile, 19); qt_cm = CODEC_YUY2;
              break;                         
         
         /* passthrough */
         case CODEC_RAW_RGB:
         case CODEC_RAW_YUV:
         case CODEC_RAW:
                /* set out output codec to input codec */ 
                if(qt_codec == NULL || strlen(qt_codec)==0) {         
                    switch (vob->codec_flag){
                        case TC_CODEC_MJPG:
                            quicktime_set_video(qtfile, 1, w, h, vob->ex_fps,"jpeg"); 
                            break;
                        
                        case TC_CODEC_DV:
                            quicktime_set_video(qtfile, 1, w, h, vob->ex_fps,"dvc "); 
                            break;
                        
                        case TC_CODEC_YV12:
                            quicktime_set_video(qtfile, 1, w, h, vob->ex_fps,"yv12"); 
                            break;
                            
                        case TC_CODEC_RGB:
                            quicktime_set_video(qtfile, 1, w, h, vob->ex_fps,"raw "); 
                            break;
                            
                        case TC_CODEC_YUV2: /* test this */
                            quicktime_set_video(qtfile, 1, w, h, vob->ex_fps,"yuv2"); 
                            break;
                            
                        case TC_CODEC_DIVX3:                        
                        case TC_CODEC_DIVX4:                        
                        case TC_CODEC_DIVX5:
                            quicktime_set_video(qtfile, 1, w, h, vob->ex_fps,"DIVX"); 
                            break;
                        
                        default:
                            fprintf(stderr,"[%s] ERROR: codec not supported for pass-through\n",
                                MOD_NAME);
                            fprintf(stderr,"[%s]        If you really know what you are doing you can force \n[%s]        a codec via -F <vc>, '-F list' gives you a list\n", MOD_NAME,MOD_NAME);                            
                            return(TC_EXPORT_ERROR);
                            break;
                    }
                }
                else {
                        fprintf(stderr,"[%s] WARNING: Overriding the output codec is almost never a good idea\n",
                            MOD_NAME);
                        quicktime_set_video(qtfile, 1, w, h, vob->ex_fps,qt_codec);                        
                }
              /*fprintf(stderr," using vid\n"); */ 
              rawVideo = 1;
              break;
              
        default:
            /* unsupported internal format */
            fprintf(stderr,"[%s] unsupported internal video format %x\n",
                MOD_NAME,vob->ex_v_codec);
            return(TC_EXPORT_ERROR);
            break;
       }
    
        /* Inform user about qt's internal cs conversation */
        /* not required for pass through */
        if (vob->im_v_codec != CODEC_RAW && vob->im_v_codec != CODEC_RAW_YUV && vob->im_v_codec != CODEC_RAW_RGB) {        
            if (quicktime_writes_cmodel(qtfile, qt_cm ,0) != 1){ 
                    if (verbose_flag != TC_QUIET) fprintf(stderr,"[%s] INFO: Colorspace conversation required you may want to try\n[%s]       a different mode (rgb, yuv, uyvy) to speed up encoding\n", MOD_NAME, MOD_NAME);
            }
        }
        
        /* set codec parameters */
        divx_bitrate = vob->divxbitrate * 1000; /* tc uses kb */

        /* if max bitrate > bitrate  use difference for bitrate tolerance */        
        if (vob->video_max_bitrate > vob->divxbitrate) div3_bitrate_tolerance = (vob->video_max_bitrate - vob->divxbitrate) * 1000;

        dv_anamorphic16x9 = ((vob->ex_asr<0) ? vob->im_asr:vob->ex_asr) == 3;
        
        /* divx */
        quicktime_set_parameter(qtfile, "divx_bitrate", &divx_bitrate);
        quicktime_set_parameter(qtfile, "divx_rc_period", &vob->rc_period);
        quicktime_set_parameter(qtfile, "divx_rc_reaction_period", &vob->rc_reaction_period);
        quicktime_set_parameter(qtfile, "divx_rc_reaction_ratio", &vob->rc_reaction_ratio);        
        quicktime_set_parameter(qtfile, "divx_max_key_interval", &vob->divxkeyframes);
        quicktime_set_parameter(qtfile, "divx_max_quantizer", &vob->max_quantizer);
        quicktime_set_parameter(qtfile, "divx_min_quantizer", &vob->min_quantizer);
        quicktime_set_parameter(qtfile, "divx_quality", &vob->quality);
        quicktime_set_parameter(qtfile, "quantizer", &vob->min_quantizer); /* if divx_fix_bitrate == 1; quantizer = min_quantizer */ 
        quicktime_set_parameter(qtfile, "divx_fix_bitrate", &fix_bitrate); 
        quicktime_set_parameter(qtfile, "divx_use_deblocking", &divx_use_deblocking); 
        
        /* DIV3 */ /* no workie */ 
        quicktime_set_parameter(qtfile, "div3_bitrate", &divx_bitrate);
        quicktime_set_parameter(qtfile, "div3_bitrate_tolerance", &div3_bitrate_tolerance);
        quicktime_set_parameter(qtfile, "div3_interlaced", &div3_interlaced); 
        quicktime_set_parameter(qtfile, "div3_quantizer", &vob->min_quantizer);
        quicktime_set_parameter(qtfile, "div3_gop_size", &vob->divxkeyframes);
        quicktime_set_parameter(qtfile, "div3_fix_bitrate", &fix_bitrate);

        /* jpeg */
        /* quicktime quality is 1-100, so 20,40,60,80,100 */
        quicktime_set_jpeg(qtfile, 20 * vob->divxquality, use_float);
        
        /* mp3 */
        quicktime_set_parameter(qtfile, "mp3bitrate", &vob->mp3bitrate);        
        quicktime_set_parameter(qtfile, "mp3quality", &vob->mp3quality);
        
        /* vorbis */
        quicktime_set_parameter(qtfile, "vorbis_bitrate", &vob->mp3bitrate);
        quicktime_set_parameter(qtfile, "vorbis_max_bitrate", &vorbis_max_bitrate);     
        quicktime_set_parameter(qtfile, "vorbis_min_bitrate", &vorbis_min_bitrate); 
        quicktime_set_parameter(qtfile, "vorbis_vbr", &vob->a_vbr);
        
        /* dv */
        quicktime_set_parameter(qtfile, "dv_anamorphic16x9", &dv_anamorphic16x9);
        quicktime_set_parameter(qtfile, "dv_vlc_encode_passes", &dv_vlc_encode_passes);
        
        /* set extended parameters */
        if (vob->ex_profile_name != NULL) {  /* check for extended option */
            char  param[strlen(vob->ex_profile_name)], 
                     qtvalue[strlen(vob->ex_profile_name)]; 
            
            int j = 0; int i,k = 0; int qtvalue_i = 0;
            
            for(i=0;i<=strlen(vob->ex_profile_name);i++,j++) {
                /* try to catch bad input */
                if ( vob->ex_profile_name[i] == ',' ){
                  fprintf(stderr,"[%s] bad -F option found \n", MOD_NAME);
                  fprintf(stderr,"[%s] try something like this: \"-F vc,ac,opt1=val,opt2=val...\"\n",MOD_NAME);
                return(TC_EXPORT_ERROR);
                break;          
                }
                if ( vob->ex_profile_name[i] == '=' ) {
                    param[j] = (char)NULL;
                    j=-1; /* set to 0 by for loop above */
                        for(i++,k=0;i<=strlen(vob->ex_profile_name), vob->ex_profile_name[i] != (char)NULL ;i++,k++) {
                            /* try to catch bad input */        
                            if ( vob->ex_profile_name[i] == '=' ){
                              fprintf(stderr,"[%s] bad -F option found, aborting ...\n",MOD_NAME);
                              fprintf(stderr,"[%s] try something like this: \"-F vc,ac,opt1=val,opt2=val...\"\n",MOD_NAME);
                            return(TC_EXPORT_ERROR);
                            break;
                            }
                            if ( vob->ex_profile_name[i] != ',' ) qtvalue[k] = vob->ex_profile_name[i];
                            else break;
                        }
                        
                    qtvalue[k] = (char)NULL;
                    
                    /* exception for copyright, name, info */
                    if (!strcmp(param, "copyright") && !strcmp(param, "name")) { /* everything else is assumed to be int */
                        qtvalue_i = atoi(qtvalue);
                        quicktime_set_parameter(qtfile, param, &qtvalue_i);
                    }
                    else {
                        /* odd ... copyright and name are switched and info overwrites everything  "scratches head" */
                        if (strcmp(param, "name")) quicktime_set_copyright(qtfile, qtvalue);
                        if (strcmp(param, "copyright")) quicktime_set_name(qtfile, qtvalue);                    
                        /*if (strcmp(param, "info")) quicktime_set_info(qtfile, qtvalue);*/                    
                    }
                    
                }
                else param[j] = vob->ex_profile_name[i];
            } /* for */
        } /* if ex opt == 0*/

    /* alloc row pointers for frame encoding */
    row_ptr = malloc (sizeof(char *) * h);        

    /* same for temp buffer*/
    tmp_buf = malloc (w*h*2);
        
    /* verbose */
    fprintf(stderr,"[%s] video codec='%s' w=%d h=%d fps=%g\n",
	    MOD_NAME,qt_codec,w,h,vob->ex_fps);

    return(0);
  }

  /* audio setup -------------------------------------------------- */
  if(param->flag == TC_AUDIO){
    char *qt_codec;
    bla = 0;
    /* change codec to internal name and make  case insensitive*/
    while (qt_acodecs_list[bla].name != NULL) {
        if (!strcasecmp(vob->ex_a_fcc, qt_acodecs_list[bla].name)) strcpy(vob->ex_a_fcc, qt_acodecs_list[bla].internal_name);
    bla++;
    }
    
   
    /* check given audio format */
    if((vob->dm_chan!=1)&&(vob->dm_chan!=2)) {
      fprintf(stderr,"[%s] Only mono or stereo audio supported\n",
	      MOD_NAME);
      return(TC_EXPORT_ERROR);
    }
    channels = vob->dm_chan;
    bits = vob->dm_bits;

    /* fetch qt codec from -F switch */
    qt_codec = vob->ex_a_fcc;
    if(qt_codec == NULL || strlen(qt_codec)==0) {
      qt_codec = "ima4";
      if (verbose_flag != TC_QUIET) fprintf(stderr,"[%s] INFO: empty qt codec name - switching to %s use '-F ,list'\n[%s]       to get a list of supported codec \n", MOD_NAME, qt_codec, MOD_NAME);
    }
     
     
    /* setup audio parameters */
    quicktime_set_audio(qtfile,channels,vob->a_rate,bits,qt_codec);

    /* check encoder mode */
    switch(vob->im_a_codec) {
    case CODEC_PCM:
      /* check if we can encode with this codec */
      if (!quicktime_supported_audio(qtfile, 0)){
	fprintf(stderr,"[%s] qt audio codec '%s' unsupported\n",
		MOD_NAME,qt_codec);
	return(TC_EXPORT_ERROR);
      }

      /* allocate sample buffers */
      audbuf0 = (int16_t*)malloc(vob->ex_a_size);
      audbuf1 = (int16_t*)malloc(vob->ex_a_size);

      /* need to encode audio */
      rawAudio = 0;
      break;

    default:
      /* unsupported internal format */
      fprintf(stderr,"[%s] unsupported internal audio format %x\n",
	      MOD_NAME,vob->ex_v_codec);
      return(TC_EXPORT_ERROR);
      break;
    }

    /* verbose */
    fprintf(stderr,"[%s] audio codec='%s' bits=%d chan=%d rate=%d\n",
	    MOD_NAME,qt_codec,bits,channels,vob->a_rate);
    return(0);
  }
  
  return(TC_EXPORT_ERROR);
}

/* ------------------------------------------------------------ 
 *
 * encode and export frame
 *
 * ------------------------------------------------------------*/


MOD_encode
{
  /* video -------------------------------------------------------- */
  if(param->flag == TC_VIDEO) {
    /* raw mode is easy */
    if(rawVideo) {
        /* add divx keyframes if needed */
        if(quicktime_mpeg4_is_key(param->buffer, param->size, "DIVX") == 1) quicktime_insert_keyframe(qtfile, (int)tc_get_frames_encoded, 0);
        if(quicktime_write_frame(qtfile,param->buffer,param->size,0)<0) {
            fprintf(stderr, "[%s] error writing raw video frame\n",
		      MOD_NAME);
            return(TC_EXPORT_ERROR);
        }
    }
    /* encode frame */
    else {
        char *ptr = param->buffer;
        int iy,sl;      

        switch(qt_cm) {
        case 9:
                /* setup row pointers for RGB: inverse! */
                sl = w*3; 
                for(iy=0;iy<h;iy++){
                    row_ptr[iy] = ptr;
                    ptr += sl;
                }
                break;
                
        case 7: {
                /* setup row pointers for YUV420P: inverse! */
                row_ptr[0] = ptr;
                ptr = ptr + (h * w);
                row_ptr[1] = ptr;  
                ptr = ptr + (h * w )/4;
                row_ptr[2] = ptr;
                break;
                }
              
        case CODEC_YUY2:
        case 19:
                /* setup row pointers for YUV422: inverse */
                sl = w*2;                        
                if (qt_cm != CODEC_YUY2){
                    /* convert uyvy to yuy2 */   /* nts find out if qt supports uyvy byteorder */ 
                    uyvytoyuy2(ptr, tmp_buf, w, h);
                    ptr = tmp_buf;
                }
                for(iy=0;iy<h;iy++){
                    row_ptr[iy] = ptr;
                    ptr += sl;
                }
                break;
        }

  if(quicktime_encode_video(qtfile, row_ptr, 0)<0) {
	fprintf(stderr, "[%s] error encoding video frame\n",MOD_NAME);
	return(TC_EXPORT_ERROR);
      }
    }
    return(0);
  }  
  /* audio -------------------------------------------------------- */
  if(param->flag == TC_AUDIO){
    /* raw mode is easy */
    if(rawAudio) {
      if(quicktime_write_frame(qtfile,
			       param->buffer,param->size,0)<0) {
	fprintf(stderr, "[%s] error writing raw audio frame\n",
		MOD_NAME);
	return(TC_EXPORT_ERROR);
      }
    }
    /* encode audio */
    else {
      int s,t;
      int16_t *aptr[2] = { audbuf0, audbuf1 };

      /* calc number of samples */
      int samples = param->size;
      if(bits==16)
	samples >>= 1;
      if(channels==2)
	samples >>= 1;

      /* fill audio buffer */
      if(bits==8) {
	/* UNTESTED: please verify :) */
	if(channels==1) {
	  for(s=0;s<samples;s++) 
	    audbuf0[s] = ((((int16_t)param->buffer[s]) << 8)-0x8000);
	} else /* stereo */ {
	  for(s=0,t=0;s<samples;s++,t+=2) {
	    audbuf0[s] = ((((int16_t)param->buffer[t]) << 8)-0x8000);
	    audbuf1[s] = ((((int16_t)param->buffer[t+1]) << 8)-0x8000);
	  }
	}
      }
      else /* 16 bit */ {
	if(channels==1) {
	  aptr[0] = (int16_t *)param->buffer;
	} else /* stereo */ {
	  /* decouple channels */
	  for(s=0,t=0;s<samples;s++,t+=2) {
	    audbuf0[s] = ((int16_t *)param->buffer)[t];
	    audbuf1[s] = ((int16_t *)param->buffer)[t+1];
	  }
	}
      }

      /* encode audio samples */
      if ((quicktime_encode_audio(qtfile, aptr, NULL, samples)<0)){
	fprintf(stderr,"[%s] error encoding audio frame\n",MOD_NAME);
	return(TC_EXPORT_ERROR);
      }
    }
        
    return(0);
  }
  
  return(TC_EXPORT_ERROR);
}


/* ------------------------------------------------------------ 
 *
 * stop encoder
 *
 * ------------------------------------------------------------*/
  
MOD_stop
{  
  
  if(param->flag == TC_VIDEO) {
    /* free row pointers */
    if(row_ptr!=NULL)
      free(row_ptr);
    return(0);
  }
  
  if(param->flag == TC_AUDIO) {
    /* free audio buffers */
    if(audbuf0!=NULL)
      free(audbuf0);
    if(audbuf1!=NULL)
      free(audbuf1);
    return(0);
  }
  
  return(TC_EXPORT_ERROR);
}

/* ------------------------------------------------------------ 
 *
 * close outputfile
 *
 * ------------------------------------------------------------*/

MOD_close
{  
  if(param->flag == TC_VIDEO){
    /* close quicktime file */
    quicktime_close(qtfile);
    qtfile=NULL;
    return(0);
    }
  
  if(param->flag == TC_AUDIO) {
    return(0);
  }
  
  return(TC_EXPORT_ERROR);
  
}




