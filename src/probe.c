/*
 *  probe.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
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

#include "transcode.h"
#include "probe.h"

#define PMAX_BUF 1024
char probe_cmd_buf[PMAX_BUF];

static char *std_module[] = {"null",
			     "raw", 
			     "dv", 
			     "nuv", 
			     "yuv4mpeg", 
			     "mpeg2", "vob", "dvd",
			     "af6", "avi", "divx", "ffmpeg",
			     "mp3", "ac3",
			     "net",
			     "im",
			     "ogg",
			     "mov",
			     "v4l",
			     "xml",
			     "lav",
			     "lzo",
			     "ts",
			     "vnc",
			     "fraps",
			     "mplayer"
};

enum _std_module {_null_, 
		  _raw_,
		  _dv_, 
		  _nuv_, 
		  _yuv4mpeg_, 
		  _mpeg2_, _vob_, _dvd_,
		  _af6_, _avi_, _divx_, _ffmpeg_,
		  _mp3_, _ac3_,
		  _net_,
		  _im_,
		  _ogg_,
		  _mov_,
		  _v4l_,
		  _xml_,
		  _lav_,
		  _lzo_,
		  _ts_,
		  _vnc_,
		  _fraps_,
		  _theora_
};

static int title, verb;



char *get_audio_module(int f, int flag)
{
  
  if(!flag) return(std_module[_null_]);
  
  switch(f) {
    
  case CODEC_MP2:
  case CODEC_MP3:
    return(std_module[_mp3_]);
    break;
    
  case CODEC_A52:
  case CODEC_AC3:
    return(std_module[_ac3_]);
    break;
    
  case CODEC_PCM:
    //this can only be AVI!
    return(std_module[_avi_]);
    break;

  case CODEC_VORBIS:
    //this can only be OGG!
    return(std_module[_ogg_]);
    break;
    
  case CODEC_NULL:
  default:
    return(std_module[_null_]);
    break;
  }
}

int probe_source_core(probe_info_t *pvob, int range, char *file, char *nav_seek_file)
{
  
  FILE *fd;

  if(nav_seek_file) {
    if((snprintf(probe_cmd_buf, PMAX_BUF, "tcprobe -B -i \"%s\" -T %d -d %d -H %d -f \"%s\"", file, title, verb, range, nav_seek_file)<0)) return(-1);
  } else {
    if((snprintf(probe_cmd_buf, PMAX_BUF, "tcprobe -B -i \"%s\" -T %d -d %d -H %d", file, title, verb, range)<0)) return(-1);
  }

  
  // popen
  if((fd = popen(probe_cmd_buf, "r"))== NULL)  return(-1);

  if (fread(&tc_probe_pid, sizeof(pid_t), 1, fd) !=1) return(-1);
  if (fread(pvob, sizeof(probe_info_t), 1, fd) !=1) return(-1);

  pclose(fd);

  return(0);
}

/*------------------------------------------------------------
 *
 * probe a source, determine:
 *
 *  (I) magic: filetype 
 *
 * (II) codec: audio format and video codec
 *
 *------------------------------------------------------------*/


void probe_source(int *flag, vob_t *vob, int range, char *vid_file, char *aud_file)
{

  int preset=0,s_vxml=0,s_axml=0;

  int av_fine=0;
  
  long a_magic=0;

  int track;
  int probe_further_for_codec=1;
  
  probe_info_t  *pvob, *paob, *info;

  double pts_diff=0;
  int D_arg=0, D_arg_ms=0;

  paob = (probe_info_t *) malloc(sizeof(probe_info_t));  
  pvob = (probe_info_t *) malloc(sizeof(probe_info_t));

  memset (paob, 0, sizeof(probe_info_t));
  memset (pvob, 0, sizeof(probe_info_t));

  title=vob->dvd_title;
  verb = (verbose > TC_DEBUG) ? verbose : 0;

  //probe

  if(vid_file!=NULL) {

    if(probe_source_core(pvob, range, vid_file, vob->nav_seek_file)<0) {
      *flag = TC_PROBE_ERROR;

      if(verbose & TC_DEBUG) printf("(%s) failed to probe video source\n", __FILE__);	
      
      return;
    }
  } else vob->has_video=0;

  //need to probe with -p argument?

  if(aud_file != NULL) {
    //probe audio file
    if(probe_source_core(paob, range, aud_file, vob->nav_seek_file)<0) {
      *flag = TC_PROBE_ERROR;
      if(verbose & TC_DEBUG) printf("(%s) failed to probe audio source\n", __FILE__);	

      return;
    }   
  }

  info = pvob;

  track=vob->a_track;
  if(vob->a_track < 0 || vob->a_track>=TC_MAX_AUD_TRACKS) track=0;

  //------------------
  //video
  //------------------

  //check basic frame parameter
  if( !(*flag & TC_PROBE_NO_FRAMESIZE)) {
    if(info->width>0) vob->im_v_width  = info->width;
    if(info->height>0) vob->im_v_height = info->height;
  }

  //check frame rate
  if( !(*flag & TC_PROBE_NO_FPS)) {
    if(info->fps>0) vob->fps = info->fps;
  }

  //check for standard encoder profiles
  if( !(*flag & TC_PROBE_NO_IMASR)) {
    if(info->asr>0) vob->im_asr = info->asr;
  }

  if( !(*flag & TC_PROBE_NO_FRC)) {
    if(info->frc>0) { vob->im_frc = info->frc; }
  } else {
    if (vob->fps == MIN_FPS) {vob->fps = frc_table[vob->im_frc];}
  }

  //check for additional attributes
  if(info->attributes>0) vob->attributes = info->attributes;


  // AV sync correction: 
  // calculate PTS difference:
  
  if(info->pts_start>0 && info->track[track].pts_start>0) {
      pts_diff = info->pts_start - info->track[track].pts_start;
    
      //calculate -D option argument:
      D_arg = (int) (vob->fps*pts_diff);
      
      //fine tuning
      D_arg_ms = (int) ((pts_diff - D_arg/vob->fps)*1000);
  } else {
      
      D_arg_ms = 0;
      D_arg = 0;
  }

  if(verbose & TC_INFO) 
      printf("[%s] (probe) suggested AV correction -D %d (%d ms) | AV %d ms | %d ms\n",
	  "transcode", D_arg, (int) ((D_arg*1000)/vob->fps), (int) (1000*pts_diff), D_arg_ms);	
  
  // AV sync correction: case (1)
  //
  // demuxer disabled needs PTS sync mode 

  if( !(*flag & TC_PROBE_NO_DEMUX) && info->attributes & TC_INFO_NO_DEMUX) {
      vob->demuxer=0;
      av_fine=1;
  }

  // AV sync correction: case (2)
  //
  // check if PTS of requested audio track requires video frame dropping
  // vob->demuxer>0 and audio_pts > video_pts:
  
  if( !(*flag & TC_PROBE_NO_DEMUX)  
      && (info->pts_start - info->track[track].pts_start)<0) {
      av_fine=1;
  }
  
  // AV sync correction: case (3)
  //
  // fully PTS based sync modes requested
  
  if(vob->demuxer==3 || vob->demuxer==4) av_fine=1; 
  
  //set parameter 

  if(av_fine) {
      //few demuxer modes allow auto-probing of AV shift parameter
      if( !(*flag & TC_PROBE_NO_AVSHIFT)) vob->sync=D_arg;
      if( !(*flag & TC_PROBE_NO_AV_FINE)) vob->sync_ms=D_arg_ms;
  }
  
  //check for MPEG program stream entry point
  if( !(*flag & TC_PROBE_NO_SEEK)) {
    if(info->unit_cnt>0) vob->ps_unit=info->unit_cnt;
  }
  
  //maybe negative
  if(info->magic) vob->format_flag = info->magic;
  if(info->codec) vob->codec_flag = info->codec;
  
  //no video detected?
  if(info->width==0 || info->height==0) vob->has_video=0;
  
  //inital video PTS info
  vob->pts_start = info->pts_start;
  s_vxml=info->magic-info->magic_xml;

  //------------------
  //audio
  //------------------

  if(aud_file != NULL) info = paob;

  if( !(*flag & TC_PROBE_NO_RATE)) {
    if(info->track[track].samplerate>0) vob->a_rate = info->track[track].samplerate;
  }

  if( !(*flag & TC_PROBE_NO_BITS)) {
    if(info->track[track].bits>0) vob->a_bits = info->track[track].bits;
  }
  
  if( !(*flag & TC_PROBE_NO_CHAN)) {
    if(info->track[track].chan>0) vob->a_chan = info->track[track].chan;
  }

  if(info->track[track].bitrate>0) vob->a_stream_bitrate = info->track[track].bitrate;

  if(info->track[track].padrate>0) vob->a_padrate = info->track[track].padrate;
  
  if( !(*flag & TC_PROBE_NO_ACODEC)) {
    if(info->track[track].format>0) vob->fixme_a_codec = info->track[track].format; 
  }

  if(info->track[track].lang>0) vob->lang_code = info->track[track].lang; 

  if(info->num_tracks==0) vob->has_audio=0; //safety check

  if(info->track[track].format==CODEC_NULL) vob->has_audio_track=0;

  //different audio file format detected?
  a_magic = (aud_file == NULL) ? vob->format_flag:info->magic;
  s_axml=info->magic-info->magic_xml;

  //get infos from audio file, if video file is absent
  if(vid_file==NULL) {
    if(info->magic) vob->format_flag = info->magic;
    if(info->codec) vob->codec_flag = info->codec;
  }

  // preset based on video file magic, usually with fixed codecs 

  if(verbose & TC_DEBUG) {
    printf("(%s) V magic=0x%lx, A magic=0x%lx, V codec=0x%lx, A codec=0x%x\n", __FILE__, vob->format_flag, a_magic, vob->codec_flag, info->track[track].format);	
    printf("(%s) V magic=%s, A magic=%s, V codec=%s, A codec=%s\n", __FILE__, mformat2str(vob->format_flag), mformat2str(a_magic), codec2str(vob->codec_flag), aformat2str(info->track[track].format));	
  }

  free(paob);
  free(pvob);

  //-----------------------------------------------------
  //
  // find suitable import module for both video and audio
  //
  //-----------------------------------------------------


  // no video?
  
  if(!vob->has_video) {
    vob->vmod_probed=std_module[_null_];

    vob->im_v_height=0;
    vob->im_v_width=0;

    preset |= TC_VIDEO;
  }

  // no audio?
  
  if(!vob->has_audio) {
    vob->amod_probed=std_module[_null_];
    
    vob->a_chan=0;
    vob->a_rate=0;

    preset |= TC_AUDIO;
  }

  // avifile import requested and no extra audio import module?
  if(vob->af6_mode) {
    vob->vmod_probed=std_module[_af6_];

    //audio
    vob->amod_probed=get_audio_module(vob->fixme_a_codec, vob->has_audio);

    preset=(TC_AUDIO|TC_VIDEO);
  }

  switch(vob->format_flag) {

  case TC_MAGIC_V4L_VIDEO:
    vob->vmod_probed=std_module[_v4l_];
    preset |= TC_VIDEO;

    if( !(*flag & TC_PROBE_NO_FRAMESIZE) && vob->im_v_codec == CODEC_RGB) {
	vob->im_v_width  = PAL_W/2;
	vob->im_v_height = PAL_H/2;
    }
    if( !(*flag & TC_PROBE_NO_FRAMESIZE) && vob->im_v_codec == CODEC_YUV) {
	vob->im_v_width  = 352;
	vob->im_v_height = 288;
    }

    break;

  case TC_MAGIC_SOCKET:
    vob->vmod_probed=std_module[_net_];
    break;
    
  case TC_MAGIC_YUV4MPEG:
    if(!(preset & TC_VIDEO) && vob->im_v_codec==CODEC_RGB) vob->im_v_codec = CODEC_YUV;
    vob->vmod_probed=std_module[_yuv4mpeg_];
    
    preset=(TC_AUDIO|TC_VIDEO);
    break;
    
  case TC_MAGIC_NUV:
    if(vob->im_v_codec==CODEC_RGB) vob->im_v_codec=CODEC_YUV;
    if(!(preset & TC_VIDEO)) vob->vmod_probed=std_module[_nuv_];
    vob->amod_probed=std_module[_nuv_];
    preset=(TC_AUDIO|TC_VIDEO);
    break;

  case TC_MAGIC_OGG:
    if(!(preset & TC_VIDEO)) vob->vmod_probed=std_module[_ogg_];
    if(!(preset & TC_AUDIO)) vob->amod_probed=std_module[_ogg_];
    preset=(TC_AUDIO|TC_VIDEO);
    probe_further_for_codec = 0;
    break;

  case TC_MAGIC_DVD_NTSC:

    //set default
    if(vob->demuxer == -1 && !(*flag & TC_PROBE_NO_DEMUX)) vob->demuxer=1;

    //1->2 or 3->4
    if(vob->fps < PAL_FPS && !(*flag & TC_PROBE_NO_DEMUX)) 
      if(vob->demuxer ==1 || vob->demuxer ==3) ++vob->demuxer;

  case TC_MAGIC_DVD_PAL:
    if(!(preset & TC_VIDEO)) vob->vmod_probed=std_module[_dvd_];
    vob->amod_probed=std_module[_dvd_];
    preset=(TC_AUDIO|TC_VIDEO);

    break;

  case TC_MAGIC_AVI:
      //pass-through?
      if(!(preset & TC_VIDEO) && vob->pass_flag & TC_VIDEO) {
	  vob->vmod_probed=std_module[_avi_];
	  preset |= TC_VIDEO;
      }
      break;

  case TC_MAGIC_MOV:
    vob->vmod_probed=std_module[_mov_];
    preset |= TC_VIDEO;
    break;

  case TC_MAGIC_TS:
    vob->vmod_probed=std_module[_ts_];
    preset |= TC_VIDEO;
    break;
    
  case TC_MAGIC_TIFF1:
  case TC_MAGIC_TIFF2:
  case TC_MAGIC_JPEG:
  case TC_MAGIC_PPM:
  case TC_MAGIC_PGM:
  case TC_MAGIC_BMP:
  case TC_MAGIC_PNG:
  case TC_MAGIC_GIF:
  case TC_MAGIC_SGI:
      vob->vmod_probed=std_module[_im_];
      preset |= TC_VIDEO;
      break;

  case TC_MAGIC_VNC:
      vob->vmod_probed=std_module[_vnc_];
      preset |= TC_VIDEO;
      break;

  case TC_MAGIC_DV_PAL:
  case TC_MAGIC_DV_NTSC:

      //pass-through?
      if(!(preset & TC_VIDEO) && vob->pass_flag & TC_VIDEO) {
	  vob->vmod_probed=std_module[_dv_];
	  preset |= TC_VIDEO;
      }
      break;
      
  case TC_MAGIC_CDXA:

    if(!(preset & TC_VIDEO)) vob->vmod_probed=std_module[_vob_];
    if(!(preset & TC_AUDIO)) vob->amod_probed=std_module[_vob_];
    preset =(TC_AUDIO|TC_VIDEO);

    break;

  case TC_MAGIC_MP3:
    vob->amod_probed=std_module[_mp3_];
    preset |= TC_AUDIO;
    break;

  case TC_MAGIC_AC3:
    vob->amod_probed=std_module[_ac3_];
    preset |= TC_AUDIO;
    break;

  case TC_MAGIC_LAV:
    vob->vmod_probed=std_module[_lav_];
    vob->amod_probed=std_module[_lav_];
    preset |= (TC_AUDIO|TC_VIDEO);
    break;
  } 

  //audio import file may be different
  
  switch(a_magic) {

  case TC_MAGIC_V4L_AUDIO:
      vob->amod_probed=std_module[_v4l_];
      preset |= TC_AUDIO;
      break;
      
  case TC_MAGIC_WAV:
    // vob->amod_probed=std_module[_yuv4mpeg_];
    vob->amod_probed=std_module[_raw_];
    preset |= TC_AUDIO;
    break;

  case TC_MAGIC_AVI:
    
    //pass-through?
    if(vob->pass_flag & TC_AUDIO) {
      vob->amod_probed=std_module[_avi_];
      preset |= TC_AUDIO;
    }
    break;

  case TC_MAGIC_MOV:
    vob->amod_probed=std_module[_mov_];
    preset |= TC_AUDIO;
    break;

  case TC_MAGIC_TS:
    vob->amod_probed=std_module[_ts_];
    preset |= TC_AUDIO;
    break;

  case TC_MAGIC_MP3:
    vob->amod_probed=std_module[_mp3_];
    preset |= TC_AUDIO;
    break;

  case TC_MAGIC_AC3:
    vob->amod_probed=std_module[_ac3_];
    preset |= TC_AUDIO;
    break;
  }
  
  // select import modules based on stream codec

  switch(vob->codec_flag) {
   
  case TC_CODEC_DV:
    
    //use dv standard module
    if(!(preset & TC_VIDEO)) vob->vmod_probed=std_module[_dv_];
    preset |= TC_VIDEO;

    if(preset & TC_AUDIO) break;
    
    if(vob->format_flag == TC_MAGIC_AVI) {

      //audio
      vob->amod_probed=get_audio_module(vob->fixme_a_codec, vob->has_audio);
      preset |= TC_AUDIO;  
      
    } else vob->amod_probed=std_module[_dv_];
    preset |= TC_AUDIO; 
    break;
    
  case TC_CODEC_MPEG:
  case TC_CODEC_M2V:
  case TC_CODEC_MPEG1:
    if(!(preset & TC_VIDEO)) vob->vmod_probed=std_module[_mpeg2_];
    preset |= TC_VIDEO;

    if(preset & TC_AUDIO) break;

    //audio
    vob->amod_probed=get_audio_module(vob->fixme_a_codec, vob->has_audio);
    preset |= TC_AUDIO;  

    break;

  case TC_CODEC_MPEG2:
    if(!(preset & TC_VIDEO)) vob->vmod_probed=std_module[_vob_];
     preset |= TC_VIDEO;

     //set default
     if(vob->demuxer == -1 && !(*flag & TC_PROBE_NO_DEMUX)) vob->demuxer=1;

     //1->2 or 3->4
     if(vob->fps < PAL_FPS && !(*flag & TC_PROBE_NO_DEMUX)) 
	 if(vob->demuxer == 1 || vob->demuxer == 3) ++vob->demuxer;
     
     if(preset & TC_AUDIO) break;
     
     // audio?
     if(!vob->has_audio) {
       vob->amod_probed=std_module[_null_];
     } else {
       vob->amod_probed=std_module[_vob_];
       preset |= TC_AUDIO;  
     }    
     break;

  case TC_CODEC_MJPG:
  case TC_CODEC_MPG1:
  case TC_CODEC_MP42:
  case TC_CODEC_MP43:
  case TC_CODEC_RV10:
    vob->im_v_codec=CODEC_YUV;

    //overwrite pass-through selection!
    vob->vmod_probed=std_module[_ffmpeg_];
    preset |= TC_VIDEO;
    
    if(preset & TC_AUDIO) break;

    //audio
    vob->amod_probed=get_audio_module(vob->fixme_a_codec, vob->has_audio);
    preset |= TC_AUDIO;  

    break;

  case TC_CODEC_LZO1:
  case TC_CODEC_LZO2:

    vob->im_v_codec=CODEC_YUV;

    //overwrite pass-through selection!
    vob->vmod_probed=std_module[_lzo_];
    preset |= TC_VIDEO;
    
    if(preset & TC_AUDIO) break;

    //audio
    vob->amod_probed=get_audio_module(vob->fixme_a_codec, vob->has_audio);
    preset |= TC_AUDIO;  

    break;

  case TC_CODEC_THEORA:
    if (probe_further_for_codec) {
	vob->vmod_probed=std_module[_theora_];
	preset |= TC_VIDEO;
	if(preset & TC_AUDIO) break;
    }
    vob->amod_probed=get_audio_module(vob->fixme_a_codec, vob->has_audio);
    preset |= TC_AUDIO;
    break;
    
  case TC_CODEC_DIVX3:
  case TC_CODEC_DIVX4:
  case TC_CODEC_DIVX5:
  case TC_CODEC_XVID:

    // if input is ogg, do not probe video
    if (probe_further_for_codec) {
	//overwrite pass-through selection!
	vob->vmod_probed=std_module[_ffmpeg_];
	preset |= TC_VIDEO;
	if(preset & TC_AUDIO) break;
    }
    
    //audio
    vob->amod_probed=get_audio_module(vob->fixme_a_codec, vob->has_audio);
    preset |= TC_AUDIO;  
    
    break;
    
  case TC_CODEC_FRAPS:
    vob->im_v_codec=CODEC_YUV;

    //overwrite pass-through selection!
    vob->vmod_probed=std_module[_fraps_];
    preset |= TC_VIDEO;
    
    if(preset & TC_AUDIO) break;
    
    //audio
    vob->amod_probed=get_audio_module(vob->fixme_a_codec, vob->has_audio);
    preset |= TC_AUDIO;  
    break;

  case TC_CODEC_YV12:
    vob->im_v_codec=CODEC_YUV;

    //overwrite pass-through selection!
    vob->vmod_probed=std_module[_raw_];
    preset |= TC_VIDEO;
    
    if(preset & TC_AUDIO) break;
    
    //audio
    vob->amod_probed=get_audio_module(vob->fixme_a_codec, vob->has_audio);
    preset |= TC_AUDIO;  

    break;
    
  case TC_CODEC_UYVY:
    vob->im_v_codec=CODEC_YUV422;

    //overwrite pass-through selection!
    vob->vmod_probed=std_module[_raw_];
    preset |= TC_VIDEO;
    
    if(preset & TC_AUDIO) break;
    
    //audio
    vob->amod_probed=get_audio_module(vob->fixme_a_codec, vob->has_audio);
    preset |= TC_AUDIO;  

    break;
    
  case TC_CODEC_RGB:

    if(!(preset & TC_VIDEO)) vob->vmod_probed=std_module[_avi_];
    preset |= TC_VIDEO;
    
    if(preset & TC_AUDIO) break;
    
    //audio
    vob->amod_probed=get_audio_module(vob->fixme_a_codec, vob->has_audio);
    preset |= TC_AUDIO;  
    
    break;
  }    
  
  if (s_axml)
    vob->amod_probed_xml=std_module[_xml_];
  else
    vob->amod_probed_xml=vob->amod_probed;
  if (s_vxml)
    vob->vmod_probed_xml=std_module[_xml_];
  else
    vob->vmod_probed_xml=vob->vmod_probed;
  return;
}

/*------------------------------------------------------------
 *
 * auxiliary conversion routines
 *
 *------------------------------------------------------------*/


char *codec2str(int f)
{
    
  switch(f) {
    
  case TC_CODEC_MPEG2:
    return("MPEG-2");

  case TC_CODEC_MJPG:
    return("MJPG");

  case TC_CODEC_MPG1:
    return("mpg1");

  case TC_CODEC_LZO1:
    return("LZO1");

  case TC_CODEC_RV10:
    return("RV10 Real Video");

  case TC_CODEC_DIVX3:
    return("DivX;-)");

  case TC_CODEC_MP42:
    return("MSMPEG4 V2");

  case TC_CODEC_MP43:
    return("MSMPEG4 V3");
    
  case TC_CODEC_DIVX4:
    return("DivX");

  case TC_CODEC_DIVX5:
    return("DivX5");

  case TC_CODEC_XVID:
    return("XviD");
    
  case TC_CODEC_MPEG1:
    return("MPEG-1");

  case TC_CODEC_MPEG:
    return("MPEG  ");
 
  case TC_CODEC_DV:
    return("Digital Video");

  case TC_CODEC_YV12:
    return("YV12/I420");

  case TC_CODEC_YUV2:
    return("YUV2");

  case TC_CODEC_NUV:
    return("RTjpeg");

  case TC_CODEC_RGB:
    return("RGB/BGR");

  case TC_CODEC_LAV:
    return("LAV");

  case TC_CODEC_PCM:
    return("PCM");

  default:
    return("unknown");
  }
  
  return("unknown");
}

char *aformat2str(int f)
{
    switch(f) {

    case CODEC_AC3:
      return("AC3");
    case CODEC_A52:
      return("AC3/A52");
    case CODEC_MP3:
      return("MPEG layer-3");
    case CODEC_MP2:
      return("MPEG layer-2");
    case CODEC_PCM:
      return("PCM");
    case CODEC_LPCM:
      return("LPCM");
    case CODEC_VORBIS:
      return("Ogg Vorbis");
    default:
      return("unknown");
    }
    return("unknown");
}


char *mformat2str(int f)
{
    switch(f) {
	
    case TC_MAGIC_PAL:
	return("PAL");
    case TC_MAGIC_NTSC:
	return("NTSC");
    case TC_MAGIC_TS:
	return("MPEG transport stream");
    case TC_MAGIC_YUV4MPEG:
	return("YUV4MPEG");
    case TC_MAGIC_SOCKET:
	return("network stream");
    case TC_MAGIC_NUV:
	return("NuppelVideo");
    case TC_MAGIC_DVD_PAL:
	return("DVD PAL");
    case TC_MAGIC_DVD_NTSC:
	return("DVD NTSC");
    case TC_MAGIC_AVI:
	return("RIFF data, AVI");
    case TC_MAGIC_MOV:
	return("QuickTime");
    case TC_MAGIC_XML:
       return("XML file");
    case TC_MAGIC_LAV:
       return("Edit List");
    case TC_MAGIC_TIFF1:
    case TC_MAGIC_TIFF2:
        return("TIFF image");
    case TC_MAGIC_JPEG:
        return("JPEG image");
    case TC_MAGIC_BMP:
        return("BMP image");
    case TC_MAGIC_PNG:
        return("PNG image");
    case TC_MAGIC_GIF:
        return("GIF image");
    case TC_MAGIC_PPM:
        return("PPM image");
    case TC_MAGIC_PGM:
        return("PGM image");
    case TC_MAGIC_CDXA:
	return("RIFF data, CDXA");
    case TC_MAGIC_AC3:
	return("AC3");
    case TC_MAGIC_MP3:
	return("MP3");
    case TC_MAGIC_MP2:
	return("MP2");
    case TC_MAGIC_OGG:
	return("OGG stream");
    case TC_MAGIC_WAV:
	return ("WAVE");
    case TC_MAGIC_V4L_VIDEO:
    case TC_MAGIC_V4L_AUDIO:
	return("V4L");
    }
    return("");
}

char *asr2str(int c)
{

    switch(c) {

    case 1:
	return("encoded @ 1:1");
	break;
	
    case  2:
    case  8:
    case 12:
	return("encoded @ 4:3");
	break;
	
    case 3:
	return("encoded @ 16:9");
	break;

    case 4:
	return("encoded @ 2.21:1");
	break;

    default:
	return("");
    }
}

