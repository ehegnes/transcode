/*
 *  import_mp3.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
 *
 *  This file is part of transcode, a linux video stream  processing tool
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

#define MOD_NAME    "import_mp3.so"
#define MOD_VERSION "v0.1.4 (2003-08-04)"
#define MOD_CODEC   "(audio) MPEG"

#include "transcode.h"

static int verbose_flag = TC_QUIET;
static int capability_flag = TC_CAP_PCM;

#define MOD_PRE mp3
#include "import_def.h"

#include "libxio/xio.h"


char import_cmd_buf[TC_BUF_MAX];

static FILE *fd;

static int codec;

static int count=TC_PAD_AUD_FRAMES;
static int decoded_frames=0;
static int offset=0;
static int last_percent=0;

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
    int is_dir=0;
    int sret;

    // audio only
    if(param->flag != TC_AUDIO) return(TC_IMPORT_ERROR);

    sret = scan(vob->audio_in_file);
    if (sret < 0)
      return(TC_IMPORT_ERROR);
    else
      if (sret == 1)
	is_dir = 1;
      else
	is_dir = 0;

    codec = vob->im_a_codec;
    count = 0;
    offset = vob->vob_offset;

	// (offset && vob->nav_seek_file)?("-S %s"):""

    switch(codec) {

    case CODEC_PCM:

	if (offset && vob->nav_seek_file) {
	  sret = snprintf(import_cmd_buf, TC_BUF_MAX, 
	              "tcextract -a %d -i \"%s\" -x %s -d %d -f %s -C %d-%d |"
                      " tcdecode -x %s -d %d -z %d", 
		      vob->a_track, vob->audio_in_file, 
		      (vob->fixme_a_codec==0x50 ? "mp2" : "mp3"), vob->verbose, 
		      vob->nav_seek_file, offset, offset + 1, 
		      (vob->fixme_a_codec==0x50 ? "mp2" : "mp3"), vob->verbose,
		      vob->a_padrate);
          if (tc_test_string(__FILE__, __LINE__, TC_BUF_MAX, sret, errno))
	    return(TC_IMPORT_ERROR);

	} else {
	  if (is_dir) {
	    sret = snprintf(import_cmd_buf, TC_BUF_MAX, 
			"tccat -a -i %s | tcextract -a %d -x %s -d %d |"
                        " tcdecode -x %s -d %d -z %d", 
			vob->audio_in_file, vob->a_track,
			(vob->fixme_a_codec==0x50 ? "mp2" : "mp3"),
			vob->verbose, 
			(vob->fixme_a_codec==0x50 ? "mp2" : "mp3"),
			vob->verbose,
			vob->a_padrate);
	    if (tc_test_string(__FILE__, __LINE__, TC_BUF_MAX, sret, errno))  
	      return(TC_IMPORT_ERROR);

	  } else {
	    sret = snprintf(import_cmd_buf, TC_BUF_MAX, 
			"tcextract -a %d -i \"%s\" -x %s -d %d |"
                        " tcdecode -x %s -d %d -z %d", 
			vob->a_track, vob->audio_in_file, 
			(vob->fixme_a_codec==0x50?"mp2":"mp3"),
			vob->verbose, 
			(vob->fixme_a_codec==0x50?"mp2":"mp3"),
			vob->verbose, vob->a_padrate);
	    if (tc_test_string(__FILE__, __LINE__, TC_BUF_MAX, sret, errno))
	      return(TC_IMPORT_ERROR);

	  }
	}

	if(verbose_flag) printf("[%s] MP3->PCM\n", MOD_NAME);

	break;

    default: 
	fprintf(stderr, "invalid import codec request 0x%x\n", codec);
	return(TC_IMPORT_ERROR);

    }

    // print out
    if(verbose_flag) printf("[%s] %s\n", MOD_NAME, import_cmd_buf);

    // set to NULL if we handle read
    param->fd = NULL;

    // popen
    if((fd = popen(import_cmd_buf, "r"))== NULL) {
	perror("popen pcm stream");
	return(TC_IMPORT_ERROR);
    }

    return(TC_IMPORT_OK);
}

/* ------------------------------------------------------------ 
 *
 * decode stream
 *
 * ------------------------------------------------------------*/

MOD_decode
{

  int ac_bytes=0, ac_off=0; 

  // audio only
  if(param->flag != TC_AUDIO) return(TC_IMPORT_ERROR);

  switch(codec) {
      
  case CODEC_PCM:

    //default:
    ac_off   = 0;
    ac_bytes = param->size;
    break;

  default: 
    fprintf(stderr, "invalid import codec request 0x%x\n",codec);
      return(TC_IMPORT_ERROR);

  }

  // this can be done a lot smarter in tcextract
#if 1
  do {
      int percent=0;
      if (offset) percent = decoded_frames*100/offset+1;

      if (fread(param->buffer+ac_off, ac_bytes, 1, fd) !=1) {
	  return(TC_IMPORT_ERROR);
      }
      if (offset && percent <= 100 && last_percent != percent) {
	  fprintf(stderr, "[%s] skipping to frame %d .. %d%%\r", 
		  MOD_NAME, offset, percent);
	  last_percent = percent;
      }
  } while (decoded_frames++<offset);
#else
      memset(param->buffer+ac_off, 0, ac_bytes);
      if (fread(param->buffer+ac_off, ac_bytes-ac_off, 1, fd) !=1) {
	  // not sure what this hack is for, probably can be removed
	  //--count; if(count<=0) return(TC_IMPORT_ERROR);
	  return(TC_IMPORT_ERROR);
      }
#endif

  return(TC_IMPORT_OK);
}

/* ------------------------------------------------------------ 
 *
 * close stream
 *
 * ------------------------------------------------------------*/

MOD_close
{

  if(param->flag != TC_AUDIO) return(TC_IMPORT_ERROR);

  if(fd != NULL) pclose(fd);
  if(param->fd != NULL) pclose(param->fd);

  fd        = NULL;
  param->fd = NULL;
  decoded_frames = 0;
  last_percent = 0;

  return(TC_IMPORT_OK);
}
