/*
 *  export_mov.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
 *  Copyright (C) (2002) Christian Vogelgsang 
 *  <Vogelgsang@informatik.uni-erlangen.de> (extension for all codecs supported
 *  by quicktime4linux
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

#include "transcode.h"

#define MOD_NAME    "export_mov.so"
#define MOD_VERSION "v0.1.1 (2003-07-24)"
#define MOD_CODEC   "(video) * | (audio) *"
#define MOD_PRE mov
#include "export_def.h"

#include <quicktime.h>

/* verbose flag */
static int verbose_flag=TC_QUIET;

/* set encoder capabilities */
static int capability_flag=
 /* audio formats */
 TC_CAP_PCM| /* pcm audio */
 /* video formats */
 TC_CAP_RGB|  /* rgb frames */
 TC_CAP_YUV|  /* chunky YUV 4:2:0 */
 TC_CAP_YUY2; /* chunky YUV 4:2:2 */

/* exported quicktime file */
static quicktime_t *qtfile = NULL;

/* row pointer for source frames */
static unsigned char** row_ptr = NULL;

/* toggle for raw frame export */
static int rawVideo = 0;
static int rawAudio = 0;

/* 
color model to export to
yuv420 is planar == BC_YUV420P  == 7
rgb is packed == BC_RGB888 == 9
yuy422 is packed == BC_YUV422 == 19 // not tested
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

// defaults for codec parameters
int fix_bitrate = 0; 
int div3_interlaced = 0;
int div3_bitrate_tolerance = 500000;
int use_float =0;
int divx_use_deblocking = 0;

int v_search_range = 100; // wild guess no default given in qt code

int vorbis_max_bitrate = -1;        
int vorbis_min_bitrate = -1;
int vorbis_vdr = 0;

/* dv related ... no idea what they mean :| */
// int dv_decode_quality = DV_QUALITY_BEST; 
int dv_anamorphic16x9 = 0;
int dv_vlc_encode_passes = 3;
int dv_clamp_luma = 0;
int dv_clamp_chroma = 0;
int dv_add_ntsc_setup = 0;
int dv_rem_ntsc_setup = 0;


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
  /* video setup -------------------------------------------------- */
  if(param->flag == TC_VIDEO) {
    char *qt_codec;
    int divxbitrate;
    
    /* fetch frame size */
    w = vob->ex_v_width;
    h = vob->ex_v_height;

    /* fetch qt codec from -F switch */
    qt_codec = vob->ex_v_fcc;

    
    /* open target file for writing */
    if(NULL == (qtfile = quicktime_open(vob->video_out_file, 0, 1)) ){
      fprintf(stderr,"[%s] error opening qt file '%s'\n",
	      MOD_NAME,vob->video_out_file);
      return(TC_EXPORT_ERROR);
    }

    if(qt_codec == NULL || strlen(qt_codec)==0) {
      //default
      qt_codec = "mjpa";
      fprintf(stderr,"[%s] empty qt codec name - switching to %s\n", MOD_NAME, qt_codec);
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
     
    /* check given qt video compressor */

    if(strcmp(qt_codec,"rawv") == 0){
        /* raw write frames */
        rawVideo = 1;    
    }
    else{
        /* check if we can encode with this codec */    
        if (!quicktime_supported_video(qtfile, 0)){
        fprintf(stderr,"[%s] qt video codec '%s' unsupported\n",
            MOD_NAME,qt_codec);
            return(TC_EXPORT_ERROR);
        }

        switch(vob->im_v_codec) {
        case CODEC_RGB:
              if (strcmp(qt_codec, "raw ")) rawVideo=1;
              //fprintf(stderr," using rgb\n");
              quicktime_set_cmodel(qtfile, 9); qt_cm = 9;
              break;
              
        case CODEC_YUV:
              if (strcmp(qt_codec, "yv12")) rawVideo=1;        
              //fprintf(stderr," using yuv420\n");
              quicktime_set_cmodel(qtfile, 7); qt_cm = 7;
              break;
              
        case CODEC_YUY2:
              if (strcmp(qt_codec, "yuv2")) rawVideo=1;
              //fprintf(stderr," using yuv422\n");
              quicktime_set_cmodel(qtfile, 19); qt_cm = 19;
              break;
                         
        default:
            /* unsupported internal format */
            fprintf(stderr,"[%s] unsupported internal video format %x\n",
                MOD_NAME,vob->ex_v_codec);
            return(TC_EXPORT_ERROR);
            break;
       }

        if (quicktime_writes_cmodel(qtfile, qt_cm ,0) != 1){ 
                fprintf(stderr,"[%s] color space not supported need to cs conversion \n",
                                MOD_NAME);
        }


        /* set codec parameters */
        // if max bitrate > bitrate  use difference for bitrate tolerance
        divx_bitrate = vob->divxbitrate * 1000; // tc uses kb        
        if (vob->video_max_bitrate > vob->divxbitrate) div3_bitrate_tolerance = (vob->video_max_bitrate - vob->divxbitrate) * 1000; //kb again

        /* set codec parameters */
        /* doesn't look pretty maybe it should be redone in a loop or somethin*/ 

        /* divx */
        quicktime_set_parameter(qtfile, "divx_bitrate", &divx_bitrate);
        quicktime_set_parameter(qtfile, "divx_rc_period", &vob->rc_period);
        quicktime_set_parameter(qtfile, "divx_rc_reaction_period", &vob->rc_reaction_period);
        quicktime_set_parameter(qtfile, "divx_rc_reaction_ratio", &vob->rc_reaction_ratio);        
        quicktime_set_parameter(qtfile, "divx_max_key_interval", &vob->divxkeyframes);
        quicktime_set_parameter(qtfile, "divx_max_quantizer", &vob->max_quantizer);
        quicktime_set_parameter(qtfile, "divx_min_quantizer", &vob->min_quantizer);
        quicktime_set_parameter(qtfile, "divx_quality", &vob->quality);
        quicktime_set_parameter(qtfile, "quantizer", &vob->min_quantizer); // if divx_fix_bitrate == 1; quantizer = min_quantizer 
        quicktime_set_parameter(qtfile, "divx_fix_bitrate", &fix_bitrate); // no idea what that is  in tc .. use -F switch
        quicktime_set_parameter(qtfile, "divx_use_deblocking", &divx_use_deblocking); // no idea what that is  in tc .. use -F switch
        
        /* looks like a divx codec */
        quicktime_set_parameter(qtfile, "v_bitrate", &divx_bitrate);
        quicktime_set_parameter(qtfile, "v_rc_period", &vob->rc_period);
        quicktime_set_parameter(qtfile, "v_rc_reaction_period", &vob->rc_reaction_period);
        quicktime_set_parameter(qtfile, "v_rc_reaction_ratio", &vob->rc_reaction_ratio);        
        quicktime_set_parameter(qtfile, "v_max_key_interval", &vob->divxkeyframes);
        quicktime_set_parameter(qtfile, "v_max_quantizer", &vob->max_quantizer);
        quicktime_set_parameter(qtfile, "v_min_quantizer", &vob->min_quantizer);
        quicktime_set_parameter(qtfile, "v_search_range", &v_search_range);
        
        /* DIV3 */ // no workie 
        quicktime_set_parameter(qtfile, "div3_bitrate", &divx_bitrate);
        quicktime_set_parameter(qtfile, "div3_bitrate_tolerance", &div3_bitrate_tolerance);
        quicktime_set_parameter(qtfile, "div3_interlaced", &div3_interlaced); // -F swich
        quicktime_set_parameter(qtfile, "div3_quantizer", &vob->min_quantizer);
        quicktime_set_parameter(qtfile, "div3_gop_size", &vob->divxkeyframes);
        quicktime_set_parameter(qtfile, "div3_fix_bitrate", &fix_bitrate); // -F swich

        /* jpeg */
        /* quicktime quality is 1-100, so 20,40,60,80,100 */
        quicktime_set_jpeg(qtfile, 20 * vob->divxquality, use_float);
        
        /* mp3 */
        quicktime_set_parameter(qtfile, "mp3bitrate", &vob->mp3bitrate);        
        quicktime_set_parameter(qtfile, "mp3quality", &vob->mp3quality);
        
        /* vorbis */
        quicktime_set_parameter(qtfile, "vorbis_bitrate", &vob->mp3bitrate);
        quicktime_set_parameter(qtfile, "vorbis_max_bitrate", &vorbis_max_bitrate);  //-F switch     
        quicktime_set_parameter(qtfile, "vorbis_min_bitrate", &vorbis_min_bitrate); //-F switch
        quicktime_set_parameter(qtfile, "vorbis_vdr", &vorbis_vdr); // -F switch
        
        /* dv */ // all -F
        //quicktime_set_parameter(qtfile, "dv_decode_quality", &dv_decode_quality);  //defined via macro default is probably best  
        quicktime_set_parameter(qtfile, "dv_anamorphic16x9", &dv_anamorphic16x9);
        quicktime_set_parameter(qtfile, "dv_vlc_encode_passes", &dv_vlc_encode_passes);
        quicktime_set_parameter(qtfile, "dv_clamp_luma", &dv_clamp_luma);        
        quicktime_set_parameter(qtfile, "dv_clamp_chroma", &dv_clamp_chroma);        
        quicktime_set_parameter(qtfile, "dv_add_ntsc_setup", &dv_add_ntsc_setup);        
        quicktime_set_parameter(qtfile, "dv_rem_ntsc_setup", &dv_rem_ntsc_setup);    
        
        //set extended parameters
        if (vob->ex_profile_name != NULL) {  // check for extended option
            char  param[strlen(vob->ex_profile_name)], 
                     qtvalue[strlen(vob->ex_profile_name)]; 
            
            int j = 0; int i,k = 0; int qtvalue_i = 0;
            
            for(i=0;i<=strlen(vob->ex_profile_name);i++,j++) {
                //try to catch wrong input        
                if ( vob->ex_profile_name[i] == ',' ){
                  fprintf(stderr,"[%s] malformed -F option found \n", MOD_NAME);
                  fprintf(stderr,"[%s] try something like this: \"-F vc,ac,opt1=val,opt2=val...\"\n",MOD_NAME);
                return(TC_EXPORT_ERROR);
                break;          
                }
                if ( vob->ex_profile_name[i] == '=' ) {
                    param[j] = (char)NULL;
                    j=-1; //set to 0 in for loop above
                        for(i++,k=0;i<=strlen(vob->ex_profile_name), vob->ex_profile_name[i] != (char)NULL ;i++,k++) {
                            //try to catch wrong input        
                            if ( vob->ex_profile_name[i] == '=' ){
                              fprintf(stderr,"[%s] malformed -F option found, aborting ...\n",MOD_NAME);
                              fprintf(stderr,"[%s] try something like this: \"-F vc,ac,opt1=val,opt2=val...\"\n",MOD_NAME);
                            return(TC_EXPORT_ERROR);
                            break;          
                            }
                            if ( vob->ex_profile_name[i] != ',' ) qtvalue[k] = vob->ex_profile_name[i];
                            else break;
                        }
                        
                qtvalue[k] = (char)NULL; 
//              printf("parameter :%s\n",param);
//              printf("value :%s\n",qtvalue);
                qtvalue_i = atoi(qtvalue);
                quicktime_set_parameter(qtfile, param, &qtvalue_i);
               }
                else param[j] = vob->ex_profile_name[i];
            } //forloop
        } //if ex opt == 0


        /* alloc row pointers for frame encoding */
        row_ptr = malloc (sizeof(char *) * h);
        
        /* we need to encode frames */
        rawVideo = 0;
    }

    /* verbose */
    fprintf(stderr,"[%s] video codec='%s' w=%d h=%d fps=%g\n",
	    MOD_NAME,qt_codec,w,h,vob->ex_fps);

    return(0);
  }

  /* audio setup -------------------------------------------------- */
  if(param->flag == TC_AUDIO){
    char *qt_codec;
    
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
      fprintf(stderr,"[%s] empty qt codec name - switching to %s\n",MOD_NAME, qt_codec);
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
      if(quicktime_write_frame(qtfile,param->buffer,param->size,0)<0) {
	fprintf(stderr, "[%s] error writing raw video frame\n",
		MOD_NAME);
	return(TC_EXPORT_ERROR);
      }
    }
    /* encode frame */
    else {
        switch(qt_cm) {
        case 9:{
                /* setup row pointers for RGB: inverse! */
                int iy,sl = w*3;
                char *ptr = param->buffer;
                for(iy=0;iy<h;iy++){
                row_ptr[iy] = ptr;
                ptr += sl;
                }
                break;
                }
        case 7:{
                /* setup row pointers for YUV420P: inverse! */
                char *ptr = param->buffer;
                row_ptr[0] = ptr;  
                ptr = ptr + (h * w );
                row_ptr[1] = ptr;  
                ptr = ptr + (h * w )/4;
                row_ptr[2] = ptr;
                break;            
                }

        case 19:{
                /* setup row pointers for YUV422: inverse! */
                int iy,sl = w*2;
                char *ptr = param->buffer;
                for(iy=0;iy<h;iy++){
                row_ptr[iy] = ptr;
                ptr += sl;
                }
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




