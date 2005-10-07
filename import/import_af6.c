/*
 *  import_af6.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
 *
 *  This file is part of transcode, a video stream  processing tool
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

#define MOD_NAME    "import_af6.so"
#define MOD_VERSION "v0.0.2 (2001-12-16)"
#define MOD_CODEC   "(video) Win32 dll | (audio) PCM"

#include "transcode.h"

static int verbose_flag = TC_QUIET;
static int capability_flag = TC_CAP_RGB | TC_CAP_YUV | TC_CAP_PCM;

#define MOD_PRE af6
#include "import_def.h"


char import_cmd_buf[TC_BUF_MAX];

static int codec;

FILE *vfd, *afd;

/* ------------------------------------------------------------ 
 *
 * open stream
 *
 * ------------------------------------------------------------*/

MOD_open
{
    long sret;

    if(param->flag == TC_AUDIO) {

      sret = snprintf(import_cmd_buf, TC_BUF_MAX,
                      "tcdecode -i \"%s\" -x af6audio -y pcm -d %d",
                      vob->audio_in_file, vob->verbose);
      if (tc_test_string(__FILE__, __LINE__, TC_BUF_MAX, sret, errno))
	  return(TC_IMPORT_ERROR);

      // print out
      if(verbose_flag) printf("[%s] %s\n", MOD_NAME, import_cmd_buf);

      param->fd = NULL;

      // popen
      if((afd = popen(import_cmd_buf, "r"))== NULL) {
	perror("popen audio stream");
	return(TC_IMPORT_ERROR);
      }

      return(TC_IMPORT_OK);
    }

    if(param->flag == TC_VIDEO) {

      codec=vob->im_v_codec;

      switch(codec) {

      case CODEC_RGB:

        sret = snprintf(import_cmd_buf, TC_BUF_MAX,
                        "tcdecode -i \"%s\" -x af6video -y rgb -d %d",
                        vob->video_in_file, vob->verbose);
        if (tc_test_string(__FILE__, __LINE__, TC_BUF_MAX, sret, errno))
	    return(TC_IMPORT_ERROR);

	break;

      case CODEC_YUV:

	sret = snprintf(import_cmd_buf, TC_BUF_MAX,
                        "tcdecode -i \"%s\" -x af6video -y yuv420p -d %d",
                        vob->video_in_file, vob->verbose);
        if (tc_test_string(__FILE__, __LINE__, TC_BUF_MAX, sret, errno))
	    return(TC_IMPORT_ERROR);

	break;
      }

      // print out
      if(verbose_flag) printf("[%s] %s\n", MOD_NAME, import_cmd_buf);

      param->fd = NULL;

      // popen
      if((vfd = popen(import_cmd_buf, "r"))== NULL) {
	perror("popen video stream");
	return(TC_IMPORT_ERROR);
      }

      return(TC_IMPORT_OK);
    }

    return(TC_IMPORT_ERROR);
}


/* ------------------------------------------------------------ 
 *
 * decode  stream
 *
 * ------------------------------------------------------------*/

MOD_decode {

  int k=0;

  int bytes=0;

  static int vsync=0, async=0;

  if(param->flag == TC_VIDEO) {

    if(vsync) goto read;

    for(;;) {

      if (fread(param->buffer, 1, 1, vfd) !=1) {
	return(TC_IMPORT_ERROR);
      }

      if(param->buffer[0] != 'T') goto skip;

      if (fread(param->buffer, 1, 1, vfd) !=1) {
	return(TC_IMPORT_ERROR);
      }

      if(param->buffer[0] != 'a') goto skip;

      if (fread(param->buffer, 1, 1, vfd) !=1) {
	return(TC_IMPORT_ERROR);
      }

      if(param->buffer[0] != 'f') goto skip;

      if (fread(param->buffer, 1, 1, vfd) !=1) {
	return(TC_IMPORT_ERROR);
      }

      if(param->buffer[0] != '6') goto skip;

      vsync=1;
      break;

    skip:

      ++k;

      if(k>(1<<20)) {
	fprintf(stderr, "no sync string found within 1024 kB of stream\n");
	return(TC_IMPORT_ERROR);
      }
    }

  read:

    if (fread(param->buffer, param->size, 1, vfd) !=1) 
      return(TC_IMPORT_ERROR);

    return(TC_IMPORT_OK);
  }

  if(param->flag == TC_AUDIO) {

    if(async) goto read2;

    for(;;) {

      if (fread(param->buffer, 1, 1, afd) !=1) {
	return(TC_IMPORT_ERROR);
      }

      if(param->buffer[0] != 'T') goto skip2;

      if (fread(param->buffer, 1, 1, afd) !=1) {
	return(TC_IMPORT_ERROR);
      }

      if(param->buffer[0] != 'a') goto skip2;

      if (fread(param->buffer, 1, 1, afd) !=1) {
	return(TC_IMPORT_ERROR);
      }

      if(param->buffer[0] != 'f') goto skip2;

      if (fread(param->buffer, 1, 1, afd) !=1) {
	return(TC_IMPORT_ERROR);
      }

      if(param->buffer[0] != '6') goto skip2;

      async=1;
      break;

    skip2:

      ++k;

      if(k>(1<<20)) {
	fprintf(stderr, "no sync string found within 1024 kB of stream\n");
	return(TC_IMPORT_ERROR);
      }
    }

  read2:

    if ((bytes=fread(param->buffer, param->size, 1, afd)) !=1) {
      if(verbose_flag & TC_DEBUG)
        printf("[%s] audio read error %d/%d\n", MOD_NAME, bytes, param->size);

      return(TC_IMPORT_ERROR);
    }    
    return(TC_IMPORT_OK);
  }

    return(TC_IMPORT_ERROR);
}



/* ------------------------------------------------------------ 
 *
 * close stream
 *
 * ------------------------------------------------------------*/

MOD_close
{  

  if(param->fd != NULL) pclose(param->fd);

  return(TC_IMPORT_OK);
}
