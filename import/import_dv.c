/*
 *  import_dv.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
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

#define MOD_NAME    "import_dv.so"
#define MOD_VERSION "v0.3.1 (2003-10-14)"
#define MOD_CODEC   "(video) DV | (audio) PCM"

#include "transcode.h"
#include "aclib/imgconvert.h"
#include "xio.h"

static int verbose_flag = TC_QUIET;
static int capability_flag = TC_CAP_RGB | TC_CAP_YUV | TC_CAP_DV |
    TC_CAP_PCM | TC_CAP_VID | TC_CAP_YUV422;

#define MOD_PRE dv
#include "import_def.h"


char import_cmd_buf[TC_BUF_MAX];

static int frame_size=0;
static FILE *fd=NULL;
static uint8_t *tmpbuf = NULL;
static int yuv422_mode = 0, width, height;

static int scan(char *name)
{
  struct stat fbuf;

  if(xio_stat(name, &fbuf)) {
    fprintf(stderr, "(%s) invalid file \"%s\"\n", __FILE__, name);
    return(-1);
  }

  // file or directory?

  if(S_ISDIR(fbuf.st_mode)) return(1);
  return(0);
}

/* ------------------------------------------------------------ 
 *
 * open stream
 *
 * ------------------------------------------------------------*/

MOD_open
{

  char cat_buf[TC_BUF_MAX];
  char yuv_buf[16];
  long sret;

  if(param->flag == TC_VIDEO) {

    //directory mode?
    sret = scan(vob->video_in_file);
    if (sret < 0)
        return(TC_IMPORT_ERROR);
    (sret == 1) ?
        snprintf(cat_buf, TC_BUF_MAX, "tccat") :
        ((vob->im_v_string) ?
            snprintf(cat_buf, TC_BUF_MAX, "tcextract -x dv %s",
                                              vob->im_v_string) :
            snprintf(cat_buf, TC_BUF_MAX, "tcextract -x dv"));

    //yuy2 mode?
    (vob->dv_yuy2_mode) ?
        snprintf(yuv_buf, 16, "-y yuv420p -Y") :
        snprintf(yuv_buf, 16, "-y yuv420p");

    param->fd = NULL;
    yuv422_mode = 0;

    switch(vob->im_v_codec) {

    case CODEC_RGB:

      sret = snprintf(import_cmd_buf, TC_BUF_MAX,
                      "%s -i \"%s\" -d %d | tcdecode -x dv -y rgb -d %d -Q %d",
                      cat_buf, vob->video_in_file, vob->verbose, vob->verbose,
                      vob->quality);
      if (tc_test_string(__FILE__, __LINE__, TC_BUF_MAX, sret, errno))
          return(TC_IMPORT_ERROR);

      // popen
      if((param->fd = popen(import_cmd_buf, "r"))== NULL) {
	return(TC_IMPORT_ERROR);
      }

      break;

    case CODEC_YUV:

      sret = snprintf(import_cmd_buf, TC_BUF_MAX,
                      "%s -i \"%s\" -d %d | tcdecode -x dv %s -d %d -Q %d",
                      cat_buf, vob->video_in_file, vob->verbose, yuv_buf,
                      vob->verbose, vob->quality);
      if (tc_test_string(__FILE__, __LINE__, TC_BUF_MAX, sret, errno))
	return(TC_IMPORT_ERROR);

      // for reading
      frame_size = (vob->im_v_width * vob->im_v_height * 3)/2;

      param->fd = NULL;

      // popen
      if((fd = popen(import_cmd_buf, "r"))== NULL) {
	return(TC_IMPORT_ERROR);
      }

      break;

    case CODEC_YUV422:

      sret = snprintf(import_cmd_buf, TC_BUF_MAX, 
		      "%s -i \"%s\" -d %d |"
                      " tcdecode -x dv -y yuy2 -d %d -Q %d", 
		      cat_buf, vob->video_in_file, vob->verbose, 
		      vob->verbose, vob->quality);
      if (tc_test_string(__FILE__, __LINE__, TC_BUF_MAX, sret, errno))
	return(TC_IMPORT_ERROR);

      // for reading
      frame_size = vob->im_v_width * vob->im_v_height * 2;

      tmpbuf = malloc(frame_size);
      if (!tmpbuf) {
	tc_error("[%s] out of memory", MOD_NAME);
	return(TC_IMPORT_ERROR);
      }

      yuv422_mode = 1;
      width = vob->im_v_width;
      height = vob->im_v_height;

      param->fd = NULL;

      // popen
      if((fd = popen(import_cmd_buf, "r"))== NULL) {
	return(TC_IMPORT_ERROR);
      }

      break;


    case CODEC_RAW:
    case CODEC_RAW_YUV:

      sret = snprintf(import_cmd_buf, TC_BUF_MAX, "%s -i \"%s\" -d %d",
                      cat_buf, vob->video_in_file, vob->verbose);
      if (tc_test_string(__FILE__, __LINE__, TC_BUF_MAX, sret, errno))
	return(TC_IMPORT_ERROR);

      // for reading
      frame_size = (vob->im_v_height==PAL_H) ?
          TC_FRAME_DV_PAL : TC_FRAME_DV_NTSC;

      param->fd = NULL;

      // popen
      if((fd = popen(import_cmd_buf, "r"))== NULL) {
	return(TC_IMPORT_ERROR);
      }

      break;


    default:
      fprintf(stderr, "invalid import codec request 0x%x\n", vob->im_v_codec);
      return(TC_IMPORT_ERROR);

    }

    // print out
    if(verbose_flag) printf("[%s] %s\n", MOD_NAME, import_cmd_buf);

    return(TC_IMPORT_OK);
  }

  if(param->flag == TC_AUDIO) {

    //directory mode?
    (scan(vob->audio_in_file)) ?
        snprintf(cat_buf, TC_BUF_MAX, "tccat") :
        ((vob->im_a_string) ?
            snprintf(cat_buf, TC_BUF_MAX, "tcextract -x dv %s",
                                              vob->im_a_string) :
            snprintf(cat_buf, TC_BUF_MAX, "tcextract -x dv"));

    sret = snprintf(import_cmd_buf, TC_BUF_MAX,
                    "%s -i \"%s\" -d %d | tcdecode -x dv -y pcm -d %d",
                    cat_buf, vob->audio_in_file, vob->verbose, vob->verbose);
    if (tc_test_string(__FILE__, __LINE__, TC_BUF_MAX, sret, errno))
      return(TC_IMPORT_ERROR);

    // print out
    if(verbose_flag) printf("[%s] %s\n", MOD_NAME, import_cmd_buf);

    param->fd = NULL;

    // popen
    if((param->fd = popen(import_cmd_buf, "r"))== NULL) {
	perror("popen PCM stream");
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

MOD_decode
{

    if(param->flag == TC_AUDIO) return(TC_IMPORT_OK);

    // video and YUV only
    if(param->flag == TC_VIDEO && frame_size==0) return(TC_IMPORT_ERROR);

    // return true yuv frame size as physical size of video data
    param->size = frame_size; 

    if (yuv422_mode) {
        uint8_t *planes[3];
        if (fread(tmpbuf, frame_size, 1, fd) !=1) 
            return(TC_IMPORT_ERROR);
        YUV_INIT_PLANES(planes, param->buffer, IMG_YUV422P, width, height);
	ac_imgconvert(&tmpbuf, IMG_YUY2, planes, IMG_YUV422P, width, height);
    } else {
        if (fread(param->buffer, frame_size, 1, fd) !=1) 
            return(TC_IMPORT_ERROR);
    }

    return(TC_IMPORT_OK);
}

/* ------------------------------------------------------------ 
 *
 * close stream
 *
 * ------------------------------------------------------------*/


MOD_close
{
  if(param->fd != NULL) pclose(param->fd);

  if(param->flag == TC_AUDIO) return(TC_IMPORT_OK);

  if(param->flag == TC_VIDEO) {

    if(fd) pclose(fd);
    fd=NULL;

    free(tmpbuf);
    tmpbuf=NULL;

    return(TC_IMPORT_OK);

  }

  return(TC_IMPORT_ERROR);
}
