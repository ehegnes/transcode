/******* NOTICE: this module is disabled *******/

/*
 *  export_fame.c
 *
 *  Copyright (C) Yannick Vignon - February 2002
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
 *
 */

#include "transcode.h"
#include "avilib.h"
#include "aud_aux.h"

#include <fame.h>

#define MOD_NAME    "export_fame.so"
#define MOD_VERSION "v0.9.1 (2003-07-24)"
#define MOD_CODEC   "(video) MPEG-4 | (audio) MPEG/AC3/PCM"

static int verbose_flag=TC_DEBUG;
static int capability_flag=TC_CAP_PCM|TC_CAP_AC3|TC_CAP_AUD|TC_CAP_YUV;

#define MOD_PRE fame
#include "export_def.h"

static avi_t *avifile=NULL;

//temporary audio/video buffer
static unsigned char *buffer;

static fame_context_t *fame_context;
static fame_parameters_t fame_params = FAME_PARAMETERS_INITIALIZER;

#define CHUNK_SIZE 1024
static int ofile;


int frame =0;
static FILE *logfileout;
static FILE *logfilein;

static int read_stats(fame_frame_statistics_t *stats)
{
  fscanf(logfilein, "Frame: %d coding %c target %d actual %d activity %d quant %d\n",
	 &stats->frame_number,
	 &stats->coding,
	 &stats->target_bits,
	 &stats->actual_bits,
	 &stats->spatial_activity,
	 (int*)&stats->quant_scale);

  return 0;
}



static void print_stats(fame_frame_statistics_t *stats)
{
  fprintf(logfileout, "Frame: %6d coding %c target %7d actual %7d activity %8d quant %2d\n",
	 stats->frame_number,
	 stats->coding,
	 stats->target_bits,
	 stats->actual_bits,
	 stats->spatial_activity,
	 (int)stats->quant_scale);
}



static int split_write(int fd, unsigned char *buffer, unsigned int size)
{
    fd_set set;
    int r, w;

    w = 0;
    while(size > CHUNK_SIZE) {
        r = write(fd, buffer, CHUNK_SIZE);
	if(r < 0) return(r);
	w += r;
        size -= CHUNK_SIZE;
        buffer += CHUNK_SIZE;

        FD_ZERO(&set);
        FD_SET(fd, &set);
        if(select(fd+1, NULL, &set, NULL, NULL) <= 0) break;
    }
    r = write(fd, buffer, size);
    if(r < 0) return(r);
    w += r;
    return(w);
}




/* ------------------------------------------------------------
 *
 * open outputfile
 *
 * ------------------------------------------------------------*/

MOD_open
{

tc_log_error(MOD_NAME, "****************** NOTICE ******************");
tc_log_error(MOD_NAME, "This module is disabled, probably because it");
tc_log_error(MOD_NAME, "is considered obsolete or redundant.");
tc_log_error(MOD_NAME, "Try using a different module, such as "
                       "ffmpeg or xvid.");
tc_log_error(MOD_NAME, "If you still need this module, please");
tc_log_error(MOD_NAME, "contact the transcode-users mailing list.");
return TC_IMPORT_ERROR;

  // open file
  if(vob->avifile_out==NULL)
    if(NULL == (vob->avifile_out = AVI_open_output_file(vob->video_out_file))) {
      AVI_print_error("avi open error");
      return(TC_EXPORT_ERROR);
    }

  /* save locally */
  avifile = vob->avifile_out;

  if(param->flag == TC_VIDEO) {
    // video

    ofile = open("/tmp/test.mp4", O_WRONLY | O_CREAT | O_TRUNC, 0666);

    AVI_set_video(vob->avifile_out, vob->ex_v_width, vob->ex_v_height,
		  vob->ex_fps, "DIVX");

    if (vob->avi_comment_fd>0)
	AVI_set_comment_fd(vob->avifile_out, vob->avi_comment_fd);

    return(0);
  }

  if(param->flag == TC_AUDIO) return(audio_open(vob, vob->avifile_out));

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

  int ch;
  fame_object_t *fame_object;

  if(param->flag == TC_VIDEO) {

    //check for odd frame parameter:

    if((ch = vob->ex_v_width - ((vob->ex_v_width>>3)<<3)) != 0) {
      printf("[%s] frame width %d (no multiple of 8)\n", MOD_NAME, vob->ex_v_width);
      printf("[%s] encoder may not work correctly or crash\n", MOD_NAME);

      if(ch & 1) {
	printf("[%s] invalid frame width\n", MOD_NAME);
	return(TC_EXPORT_ERROR);
      }
    }

    if((ch = vob->ex_v_height - ((vob->ex_v_height>>3)<<3)) != 0) {
      printf("[%s] invalid frame height %d (no multiple of 8)\n", MOD_NAME, vob->ex_v_height);
      printf("[%s] encoder may not work correctly or crash\n", MOD_NAME);
      return(TC_EXPORT_ERROR);
    }

    if ((buffer = malloc(vob->ex_v_height*vob->ex_v_width*3))==NULL) {
      perror("out of memory");
      return(TC_EXPORT_ERROR);
    } else
      memset(buffer, 0, vob->ex_v_height*vob->ex_v_width*3);


    fame_params.width = vob->ex_v_width;
    fame_params.height = vob->ex_v_height;
    fame_params.coding = "A";
    fame_params.quality = vob->divxquality;
    fame_params.bitrate = vob->divxbitrate*1000;
    fame_params.frame_rate_num = vob->ex_fps;
    fame_params.frame_rate_den = 1;
    fame_params.verbose = 0;

    fame_context = fame_open();
    fame_object = fame_get_object(fame_context, "profile/mpeg4");
    fame_register(fame_context, "profile", fame_object);

    if (vob->divxmultipass == 2)
      {
	//	fame_object = fame_get_object(fame_context, "rate/2pass");
	//if(fame_object) fame_register(fame_context, "rate", fame_object);
	logfilein = fopen("fame.log", "r");
	fscanf(logfilein, "Frames: %7d\n", &(fame_params.total_frames));
	fame_params.retrieve_cb = read_stats;
	logfileout = fopen("fame_2pass.log", "w");
      }
    else
      logfileout = fopen("fame.log", "w");
    fprintf(logfileout, "Frames: %7d\n", 0);

    fame_init(fame_context, &fame_params, buffer, vob->ex_v_height*vob->ex_v_width*3);


    if(verbose_flag & TC_DEBUG)
    {
	tc_log_info(MOD_NAME, "                quality: %d", fame_params.quality);
	tc_log_info(MOD_NAME, "      bitrate [kBits/s]: %d", fame_params.bitrate/1000);
	tc_log_info(MOD_NAME, "              crispness: %d", vob->divxcrispness);
	tc_log_info(MOD_NAME, "             frame rate: %.2f", vob->ex_fps);
	tc_log_info(MOD_NAME, "            color space: %s", (vob->im_v_codec==CODEC_RGB) ? "RGB24" : "YUV420P");
    }

    return(0);
  }

  if(param->flag == TC_AUDIO) return(audio_init(vob, verbose_flag));

  // invalid flag
  return(TC_EXPORT_ERROR);
}

/* ------------------------------------------------------------
 *
 * encode and export frame
 *
 * ------------------------------------------------------------*/


MOD_encode
{
  int keyframe;
  fame_yuv_t yuv;
  static fame_frame_statistics_t *current_stats=NULL;
  int size;

  if(param->flag == TC_VIDEO) {

    if (!current_stats)
      current_stats = malloc (sizeof (current_stats));
    memset (current_stats, 0, sizeof (current_stats));

    // encode video
    yuv.w = fame_params.width;
    yuv.h = fame_params.height;
    yuv.p = fame_params.width;
    yuv.y = param->buffer;
    yuv.u = yuv.y + yuv.w*yuv.h;
    yuv.v = yuv.u + (yuv.w/2)*(yuv.h/2);

    fame_start_frame(fame_context, &yuv, NULL);
    // segfaults here
    while( (size = fame_encode_slice(fame_context)) ) {
	split_write(ofile, buffer, size);
    }
    fame_end_frame(fame_context, current_stats);

    frame++;

    // write stats
    print_stats(current_stats);

    if (current_stats->coding == 'I' )
      keyframe = 1;
    else
      keyframe =0;


    // write video

    if(AVI_write_frame(avifile, buffer, size, keyframe)<0) {
      printf("avi video write error");
      return(TC_EXPORT_ERROR);
    }
    //split_write(ofile, buffer, current_stats->actual_bits/8);

    return(0);
  }

  if(param->flag == TC_AUDIO) return(audio_encode(param->buffer, param->size, avifile));

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
  if(param->flag == TC_VIDEO) {
    if(fame_close(fame_context) > 0) {
      printf("fame close error");
    }

    if(buffer!=NULL) {
      free(buffer);
      buffer=NULL;
    }


if (logfileout)
    {
      rewind(logfileout);
      fprintf(logfileout, "Frames: %7d\n", frame);
      fclose(logfileout);
    }

  close(ofile);

    return(0);
  }

  if(param->flag == TC_AUDIO) return(audio_stop());

  return(TC_EXPORT_ERROR);
}

/* ------------------------------------------------------------
 *
 * close codec
 *
 * ------------------------------------------------------------*/

MOD_close
{

  vob_t *vob = tc_get_vob();
  if(param->flag == TC_AUDIO) return(audio_close());

  if(vob->avifile_out!=NULL) {
    AVI_close(vob->avifile_out);
    vob->avifile_out=NULL;
  }

  if(param->flag == TC_VIDEO) return(0);

  return(TC_EXPORT_ERROR);
}

