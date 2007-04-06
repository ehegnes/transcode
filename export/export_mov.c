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
 *  This file is part of transcode, a video stream processing tool
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
 TC_CAP_YUV|  /* YUV 4:2:0 */
 TC_CAP_YUY2|
 TC_CAP_VID|
 TC_CAP_YUV422; /* YUV 4:2:2 */

#define MOD_PRE mov
#include "export_def.h"

#include <quicktime.h>
#include <colormodels.h>
#include <lqt.h>

#define QT_LIST_AUDIO "audio codec"
#define QT_LIST_VIDEO "video codec"
#define QT_LIST_PARM "parameters"

/* exported quicktime file */
static quicktime_t *qtfile = NULL;

/* row pointer for source frames */
static unsigned char** row_ptr = NULL;

/* temporary buffer*/
static unsigned char *tmp_buf = NULL;

/* toggle for raw frame export */
static int rawVideo = 0;
static int rawAudio = 0;

/* store frame dimension */
static int w = 0,h = 0;

/* audio channels */
static int channels = 0;

/* colormodels */
static int tc_cm = 0;
static int qt_cm = 0;

/* sample size */
static int bits = 0;

/* encode audio buffers */
static int16_t* audbuf0 = NULL;
static int16_t* audbuf1 = NULL;

struct qt_codec_list {
    char *name;
    char *internal_name;
    char *comments;
};

/* special paramters not retrievable from lqt */
struct qt_codec_list qt_param_list[] = {
  {"", "",  ""},
  {"Special Parameters:", "",  ""},
  {"copyright", "",  "Copyright string (no '=' or ',' allowed)"},
  {"name", "",  "Name string (no '=' or ',' allowed) "},
  {"info", "",  "Info string (no '=' or ',' allowed) "},
  {NULL, NULL, NULL}};

/* from libquicktime */
int tc_quicktime_get_timescale(double frame_rate)
{
	int timescale = 600;
	/* Encode the 29.97, 23.976, 59.94 framerates */
	if(frame_rate - (int)frame_rate != 0) 
		timescale = (int)(frame_rate * 1001 + 0.5);
	else
		if((600 / frame_rate) - (int)(600 / frame_rate) != 0) 
			timescale = (int)(frame_rate * 100 + 0.5);
	return timescale;
}


/* print list of things. Shamelessly stolen from export_ffmpeg.c */ 
static int list(char *list_type) 
{
    int cod = 0;
    int i = 0;
    
    lqt_codec_info_t ** qi = NULL;


    if (list_type == QT_LIST_VIDEO) qi = lqt_query_registry(0, 1, 1, 0);
    else if (list_type == QT_LIST_AUDIO) qi = lqt_query_registry(1, 0, 1, 0);
    else {
        qi = lqt_query_registry(1, 1, 1, 0);
    }
    
    fprintf(stderr, "[%s] List of supported %s:\n"
                    "[%s] Name                    comments\n"
                    "[%s] ---------------         ---------------------------"
                    "--------\n", MOD_NAME, list_type, MOD_NAME, MOD_NAME);
    while(qi[cod] != NULL)
        {       
        if (list_type == QT_LIST_PARM) {
            fprintf(stderr, "[%s]\n[%s] %s:\n", MOD_NAME, MOD_NAME,
                qi[cod]->name);
            for(i = 0; i < qi[cod]->num_encoding_parameters; i++) {
                if (qi[cod]->encoding_parameters[i].type != LQT_PARAMETER_SECTION) {
                    fprintf(stderr, "[%s]  %-23s %s\n", MOD_NAME,
                        qi[cod]->encoding_parameters[i].name, 
                        qi[cod]->encoding_parameters[i].real_name);
                }
            }
        }
        else {
            fprintf(stderr, "[%s] %-23s %s\n", MOD_NAME,
                qi[cod]->name, 
                qi[cod]->description);
        }
        cod++;
    }

return 1;
}

/* stolen from vid_aux.c */
/* due to a name clash between libvo and lqt, vid_aux can't be used */
void qt_uyvytoyuy2(unsigned char *input, unsigned char *output, int width, int height)
{
  int i;
  
  for (i=0; i<width*height*2; i+=4) {
      /* packed YUV 4:2:2 is Y[i] U[i] Y[i+1] V[i] (YUY2)*/
      /* packed YUV 4:2:2 is U[i] Y[i] V[i] Y[i+1] (UYVY)*/
      output[i] = input[i+1];
      output[i+1] = input[i];
      output[i+2] = input[i+3];
      output[i+3] = input[i+2];
  }
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

  int list_was_called = 0;

  /* for codec parameters */  
  int jpeg_quality = 0;
  int div3_bitrate_tolerance = 500000;
  int vorbis_max_bitrate = 192;        
  int vorbis_min_bitrate = 128;

  /* overwriting empty parameters now saves trouble later */
  if (vob->ex_v_fcc == NULL) vob->ex_v_fcc = "";
  if (vob->ex_a_fcc == NULL) vob->ex_a_fcc = "";
  if (vob->ex_profile_name == NULL) vob->ex_profile_name = "";

  if (!strcasecmp(vob->ex_v_fcc, "list")) list_was_called = list(QT_LIST_VIDEO);
  if (!strcasecmp(vob->ex_a_fcc, "list")) list_was_called = list(QT_LIST_AUDIO);
  if (!strcasecmp(vob->ex_profile_name, "list")) {
    int i;
    
    list_was_called = list(QT_LIST_PARM);

    /* list special paramters at the end */
    for(i = 0; qt_param_list[i].name != NULL; i++) {
        fprintf(stderr, "[%s]  %-23s %s\n", MOD_NAME,
            qt_param_list[i].name, 
            qt_param_list[i].comments);
    }
  }

  if (list_was_called) {
    return(TC_EXPORT_ERROR);
  }
  /* video setup -------------------------------------------------- */
  if(param->flag == TC_VIDEO) {

    lqt_codec_info_t ** qt_codec_info = NULL;

    char *qt_codec;
    int divx_bitrate;

    /* fetch frame size */
    w = vob->ex_v_width;
    h = vob->ex_v_height;

    /* fetch qt codec from -F switch */
    qt_codec =  strdup(vob->ex_v_fcc);
    
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
            if (verbose_flag != TC_QUIET) {
                fprintf(stderr, "[%s] INFO: Empty qt codec name. Switching to %s use '-F list'\n"
                                "[%s]       to get a list of supported codec. \n", 
                    MOD_NAME, qt_codec, MOD_NAME);
            }
        }

        /* check if we can encode with this codec */
        qt_codec_info = lqt_find_video_codec_by_name(qt_codec);
        if (!qt_codec_info) {
        fprintf(stderr,"[%s] qt video codec '%s' not supported!\n",
            MOD_NAME,qt_codec);
            return(TC_EXPORT_ERROR);
        }

        /* set proposed video codec */
        lqt_add_video_track(qtfile, w, h,
		tc_quicktime_get_timescale(vob->ex_fps) / vob->ex_fps+0.5,
		tc_quicktime_get_timescale(vob->ex_fps), qt_codec_info[0]);
    }
    
    /* set color model */
    switch(vob->im_v_codec) {
        case CODEC_RGB:
            qt_cm = BC_RGB888;
            break;
              
        case CODEC_YUV:
            qt_cm = BC_YUV420P;
            break;

        case CODEC_YUV422:    
            qt_cm = BC_YUV422P;
            break;

        case CODEC_YUY2:
            qt_cm = BC_YUV422;
            break;
 
         /* passthrough */
        case CODEC_RAW_RGB:
        case CODEC_RAW_YUV:
        case CODEC_RAW:
            /* set out output codec to input codec */ 
            if(qt_codec == NULL || strlen(qt_codec)==0) {
                switch (vob->codec_flag) {
                    case TC_CODEC_MJPG:
                        quicktime_set_video(qtfile, 1, w, h, vob->ex_fps,"jpeg");
                        break;

                    case TC_CODEC_MPEG:
                        quicktime_set_video(qtfile, 1, w, h, vob->ex_fps,"mpeg");
                        break;

                    case TC_CODEC_DV:
                        quicktime_set_video(qtfile, 1, w, h, vob->ex_fps,"dvc ");
                        break;

                    case TC_CODEC_SVQ1:
                        quicktime_set_video(qtfile, 1, w, h, vob->ex_fps,"SVQ1");
                        break;

                    case TC_CODEC_SVQ3:
                        quicktime_set_video(qtfile, 1, w, h, vob->ex_fps,"SVQ3");
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
                        fprintf(stderr, "[%s] ERROR: codec '%lx' not supported for pass-through\n"
                                        "[%s]        If you really know what you are doing you can force \n"
                                        "[%s]        a codec via -F <vc>, '-F list' returns a list\n", 
                            MOD_NAME, vob->codec_flag, MOD_NAME, MOD_NAME);
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
    
    /* store tc and lqt colormodel */
    tc_cm = vob->im_v_codec;
    
    /* if cs conversation not supported for codec do conversation */
    /* not required for pass through */
    if (vob->im_v_codec != CODEC_RAW && vob->im_v_codec != CODEC_RAW_YUV && vob->im_v_codec != CODEC_RAW_RGB) {
        if (quicktime_writes_cmodel(qtfile, qt_cm, 0) != 1) { 
            if (verbose_flag != TC_QUIET) {
                fprintf(stderr,"[%s] INFO: Colorspace not supported for this codec\n"
                               "[%s]       converting to RGB\n", 
                        MOD_NAME, MOD_NAME);
            }
            
            qt_cm = BC_RGB888;
            lqt_set_cmodel(qtfile, 0, qt_cm);
            yuv2rgb_init(24, MODE_BGR);
        } else {
            lqt_set_cmodel(qtfile, 0, qt_cm);
        }
    }
    
    /* set codec parameters */
    /* tc uses kb */
    divx_bitrate = vob->divxbitrate * 1000; 
    /* if max bitrate > bitrate  use difference for bitrate tolerance */        
    if (vob->video_max_bitrate > vob->divxbitrate) div3_bitrate_tolerance = (vob->video_max_bitrate - vob->divxbitrate) * 1000;

    
    /* Audio */    
    /* must be changed when it's changed in lqt; "bit_rate" conflicts with ffmpeg video codec */
    if (strcmp(qt_codec,"ffmpeg_mp2") == 0||
        strcmp(qt_codec,"ffmpeg_mp3") == 0||
        strcmp(qt_codec,"ffmpeg_ac3") == 0) 
        quicktime_set_parameter(qtfile, "bit_rate", &vob->mp3bitrate);
    
    if (strcmp(qt_codec,"lame") == 0)
        quicktime_set_parameter(qtfile, "mp3_bitrate", &vob->mp3bitrate);
    
    /* have to check this */
    if (strcmp(qt_codec,"vorbis") == 0) {
        quicktime_set_parameter(qtfile, "vorbis_bitrate", &vob->mp3bitrate);
        quicktime_set_parameter(qtfile, "vorbis_max_bitrate", &vorbis_max_bitrate);
        quicktime_set_parameter(qtfile, "vorbis_min_bitrate", &vorbis_min_bitrate);
        quicktime_set_parameter(qtfile, "vorbis_vbr", &vob->a_vbr);
    }
    
    /* Video */    
    /* jpeg_quality == 0-100 ; tc quality setting (-Q) ==  1-5 */
    jpeg_quality = 20 * vob->divxquality;
    
    if (strcmp(qt_codec,"mjpa") == 0||
        strcmp(qt_codec,"jpeg") == 0)
        quicktime_set_parameter(qtfile, "jpeg_quality", &jpeg_quality);
    
    /* looks terrible :/ */ 
    if (strcmp(qt_codec,"ffmpeg_mjpg") == 0|| 
        strcmp(qt_codec,"ffmpeg_h263p") == 0||
        strcmp(qt_codec,"ffmpeg_h263") == 0||
        strcmp(qt_codec,"ffmpeg_msmpeg4v3") == 0||
        strcmp(qt_codec,"ffmpeg_msmpeg4v2") == 0|| 
        strcmp(qt_codec,"ffmpeg_msmpeg4v1") == 0|| 
        strcmp(qt_codec,"ffmpeg_msmpg1") == 0||
        strcmp(qt_codec,"ffmpeg_mpg4") == 0){
    
        int min_bitrate = 0;
    
        quicktime_set_parameter(qtfile, "flags_gray", &vob->decolor); /* set format different for this */
      
        switch (vob->decolor) {
        case 1:
            quicktime_set_parameter(qtfile, "aspect_ratio_info", "Square");
            break;
      
        case 2:
            quicktime_set_parameter(qtfile, "aspect_ratio_info", "4:3");
            break;
            
        case 3:
            quicktime_set_parameter(qtfile, "aspect_ratio_info", "16:9");
            break;
      
        default:
            fprintf(stderr, "[%s] WARNING: Given aspect ratio not supported, using default \n",
                MOD_NAME);
            break;
        }
    
        quicktime_set_parameter(qtfile, "flags_gray", &vob->decolor);  
        quicktime_set_parameter(qtfile, "bit_rate", &vob->divxbitrate);
        quicktime_set_parameter(qtfile, "bit_rate_tolerance", &div3_bitrate_tolerance);

        min_bitrate = vob->divxbitrate - div3_bitrate_tolerance;
        quicktime_set_parameter(qtfile, "rc_max_rate", &vob->video_max_bitrate);        
        quicktime_set_parameter(qtfile, "qmax", &vob->max_quantizer);
        quicktime_set_parameter(qtfile, "qmin", &vob->min_quantizer);

      
        if (!strcmp(qt_codec,"ffmpeg_mjpg"))
            quicktime_set_parameter(qtfile, "gob_size", &vob->divxkeyframes);
    }
    
    if (strcmp(vob->ex_v_fcc,"opendivx") == 0) {
        quicktime_set_parameter(qtfile, "divx_bitrate", &divx_bitrate);
        quicktime_set_parameter(qtfile, "divx_rc_period", &vob->rc_period);
        quicktime_set_parameter(qtfile, "divx_rc_reaction_period", &vob->rc_reaction_period);
        quicktime_set_parameter(qtfile, "divx_rc_reaction_ratio", &vob->rc_reaction_ratio);
        quicktime_set_parameter(qtfile, "divx_max_key_interval", &vob->divxkeyframes);
        quicktime_set_parameter(qtfile, "divx_min_quantizer", &vob->min_quantizer);
        quicktime_set_parameter(qtfile, "divx_max_quantizer", &vob->max_quantizer);
        quicktime_set_parameter(qtfile, "divx_quantizer", &vob->min_quantizer);
        quicktime_set_parameter(qtfile, "divx_quality", &vob->quality);
    }
    
    if (strcmp(qt_codec,"rtjpeg") == 0)
        quicktime_set_parameter(qtfile, "rtjpeg_quality", &jpeg_quality);


    /* set extended parameters */
    if (vob->ex_profile_name != NULL) {  /* check for extended option */
        char param[strlen(vob->ex_profile_name)], 
             qtvalue[strlen(vob->ex_profile_name)]; 
            
        int k = 0;
        int i, j = 0;
        int qtvalue_i = 0; /* for int values */ 
            
        for(i=0;i<=strlen(vob->ex_profile_name);i++,j++) {
            /* try to catch bad input */
            if (vob->ex_profile_name[i] == ','){
                fprintf(stderr, "[%s] bad -F option found \n"
                                "[%s] try something like this: \"-F vc,ac,opt1=val,opt2=val...\"\n",
                    MOD_NAME, MOD_NAME);
                return(TC_EXPORT_ERROR);
                break;
            }
            if (vob->ex_profile_name[i] == '=') {
                param[j] = (char)NULL;
                j=-1; /* set to 0 by for loop above */
                
                for(i++,k=0;i<=strlen(vob->ex_profile_name) && vob->ex_profile_name[i] != (char)NULL;i++,k++) {
                    /* try to catch bad input */        
                    if (vob->ex_profile_name[i] == '=') {
                        fprintf(stderr, "[%s] bad -F option found, aborting ...\n"
                                        "[%s] try something like this: \"-F vc,ac,opt1=val,opt2=val...\"\n",
                            MOD_NAME, MOD_NAME);
                        return(TC_EXPORT_ERROR);
                        break;
                    }
                    if (vob->ex_profile_name[i] != ',') qtvalue[k] = vob->ex_profile_name[i];
                    else break;
                }

                qtvalue[k] = (char)NULL;
                /* exception for copyright, name, info */
                /* everything else is assumed to be of type int */
                if (strcmp(param, "copyright") == 0||
                    strcmp(param, "name") == 0||
                    strcmp(param, "info") == 0) { 
                    if (strcmp(param, "name") == 0) quicktime_set_name(qtfile, qtvalue);
                    if (strcmp(param, "copyright") == 0) quicktime_set_copyright(qtfile, qtvalue);                    
                    if (strcmp(param, "info") == 0) quicktime_set_info(qtfile, qtvalue);
                }
                else {
                    qtvalue_i = atoi(qtvalue);
                    quicktime_set_parameter(qtfile, param, &qtvalue_i);                 
                }
            }
            else param[j] = vob->ex_profile_name[i];
        }
    }

    /* alloc row pointers for frame encoding */
    row_ptr = malloc (sizeof(char *) * h);        

    /* allocate tmp buffer cs conversation */
    tmp_buf = malloc (w * h * 3);

    /* verbose */
    fprintf(stderr,"[%s] video codec='%s' w=%d h=%d fps=%g\n",
	    MOD_NAME,qt_codec,w,h,vob->ex_fps);

    return(0);
  }

  /* audio setup -------------------------------------------------- */
  if(param->flag == TC_AUDIO){
    char *qt_codec;
    lqt_codec_info_t ** qt_codec_info = 0;

    /* no audio setup if we don't have any channels (size == 0 might be better?)*/
    if(vob->dm_chan==0) return 0;

    /* check given audio format */
    if((vob->dm_chan!=1)&&(vob->dm_chan!=2)) {
        fprintf(stderr, "[%s] Only mono or stereo audio supported\n",
            MOD_NAME);
        return(TC_EXPORT_ERROR);
    }
    channels = vob->dm_chan;
    bits = vob->dm_bits;

    /* fetch qt codec from -F switch */
    qt_codec = vob->ex_a_fcc;
    if(qt_codec == NULL || strlen(qt_codec)==0) {
        qt_codec = "ima4";
        if (verbose_flag != TC_QUIET) {
            fprintf(stderr, "[%s] INFO: empty qt codec name - switching to %s use '-F ,list'\n"
                            "[%s]       to get a list of supported codec \n", 
                MOD_NAME, qt_codec, MOD_NAME);
        }
    }
     
     
    /* check encoder mode */
    switch(vob->im_a_codec) {
        case CODEC_PCM:
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

    qt_codec_info = lqt_find_audio_codec_by_name(qt_codec);
    if (!qt_codec_info){
        fprintf(stderr,"[%s] qt audio codec '%s' unsupported\n",
            MOD_NAME,qt_codec);
        return(TC_EXPORT_ERROR);
    }

    /* setup audio parameters */
    lqt_set_audio(qtfile,channels,vob->a_rate,bits,qt_codec_info[0]);

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
    unsigned char *ptr = (unsigned char *)param->buffer;
    
    /* raw mode is easy */
    if(rawVideo) {
        /* add divx keyframes if needed */
        if(quicktime_divx_is_key(ptr, (unsigned char)param->size) == 1)
            quicktime_insert_keyframe(qtfile, (int)tc_get_frames_encoded, 0);
        if(quicktime_write_frame(qtfile, ptr, param->size,0)<0) {
            fprintf(stderr, "[%s] error writing raw video frame\n",
                MOD_NAME);
            return(TC_EXPORT_ERROR);
        }
    }

    /* encode frame */
    else {
        int iy,sl;
	
        switch(qt_cm) {
            case BC_RGB888:
                if (tc_cm == CODEC_YUV) {
                    yuv2rgb((void *)tmp_buf,
                        ptr, ptr + w*h, ptr + (w*h*5)/4,
                        w, h, w*3, w, w/2);
                    ptr = tmp_buf;
                }
		
                /* setup row pointers for RGB: inverse! */
                sl = w*3;
                for(iy=0;iy<h;iy++){
                    row_ptr[iy] = ptr;
                    ptr += sl;
                }
                break;

            case BC_YUV420P:
                /* setup row pointers for YUV420P: inverse! */
                row_ptr[0] = ptr;
                ptr = ptr + (h * w);
                row_ptr[2] = ptr;
                ptr = ptr + (h * w) / 4;
                row_ptr[1] = ptr;
                break;

            case CODEC_YUY2:
            case BC_YUV422:
                /* setup row pointers for YUV422: inverse ?*/
                sl = w*2;                        
                if (qt_cm != CODEC_YUY2) {
                    /* convert uyvy to yuy2 */   /* find out if lqt supports uyvy byteorder */ 
                    qt_uyvytoyuy2(ptr, tmp_buf, w, h);
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
            (unsigned char *)param->buffer,param->size,0)<0) {
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

        /* no audio */
        if (samples == 0) return 0;

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
            } 
            else /* stereo */ {
                for(s=0,t=0;s<samples;s++,t+=2) {
                    audbuf0[s] = ((((int16_t)param->buffer[t]) << 8)-0x8000);
                    audbuf1[s] = ((((int16_t)param->buffer[t+1]) << 8)-0x8000);
                }
            }
        }
        else /* 16 bit */ {
            if(channels==1) {
                aptr[0] = (int16_t *)param->buffer;
            } 
            else /* stereo */ {
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
    if(tmp_buf!=NULL)
        free(tmp_buf);
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
