/*
 *  import_nvrec.c
 *
 *  Copyright (C) Tilmann Bitterberg - May 2002
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

#define MOD_NAME    "import_nvrec.so"
#define MOD_VERSION "v0.1.4 (2002-10-17)"
#define MOD_CODEC   "(video) nvrec | (audio) nvrec"

#include "transcode.h"

static int verbose_flag = TC_QUIET;
static int capability_flag = TC_CAP_YUV | TC_CAP_PCM;

#define MOD_PRE nvrec
#include "import_def.h"

#include <sys/types.h>


#define MAX_DISPLAY_PTS 25

#define MAX_BUF 1024
char import_cmd_buf[MAX_BUF];

static char afile[MAX_BUF];
static char prgname[MAX_BUF];

/* ------------------------------------------------------------
 *
 * open stream
 *
 * ------------------------------------------------------------*/

MOD_open
{
  
  int n;
  FILE *f;
  char buffer[MAX_BUF], *offset;
  unsigned int nv_version = 0;
  int ret;


  if(param->flag == TC_AUDIO) {
	  param->fd = NULL;
	  return(0);
  }

  if (vob->out_flag) {
      strlcpy(afile, vob->audio_out_file, sizeof(afile));
      vob->out_flag = 0;  /* XXX */
  } else
      strlcpy(afile, "audio.avi", sizeof(afile));

  strlcpy(prgname, "DIVX4rec", sizeof(prgname));

  /* this is ugly */
  ret = system("DIVX4rec -h >/dev/null 2>&1");
  if (ret == 0 || ret == 65280)
      strlcpy(prgname, "DIVX4rec", sizeof(prgname));

  ret = system("divx4rec -h >/dev/null 2>&1");
  if (ret == 0 || ret == 65280)
      strlcpy(prgname, "divx4rec", sizeof(prgname));

  /* make this even more ugly. Add another check for prgname */
  if (tc_test_program(prgname) != 0) return (TC_EXPORT_ERROR);
  
  if(param->flag == TC_VIDEO) {

#define APPEND(fmt,rest...) do { int res = tc_snprintf(import_cmd_buf+n, MAX_BUF-n, fmt , ## rest); if (res < 0) {fprintf(stderr, "cmd buffer overflow\n"); return(TC_IMPORT_ERROR);} n += res; } while (0)
    n = 0;

    APPEND("%s -o raw://%s -w %u -h %u", prgname, afile, vob->im_v_width, vob->im_v_height);

    if(vob->a_chan == 2)
	APPEND(" -s");

    APPEND(" -b %d",    vob->a_bits);
    APPEND(" -r %d",    vob->a_rate);
    APPEND(" -ab %d",   vob->mp3bitrate);
    APPEND(" -aq %d",   (int)vob->mp3quality);
    APPEND(" -vr %.3f", vob->fps);

    if (strncmp(vob->video_in_file, "/dev/zero", 9) == 0) {
	fprintf (stderr, "[%s] Warning: Input v4l1/2 device assumed to be %s\n", MOD_NAME, "/dev/video");
	APPEND(" -v %s", "/dev/video");
    } else {
	APPEND(" -v %s", vob->video_in_file);
    }

    if (strncmp(vob->audio_in_file, "/dev/zero", 9) != 0) {
	APPEND(" -d %s", vob->audio_in_file);
    }

    // new since 0.1.3
    if(vob->im_v_string != NULL) {
	APPEND(" %s", vob->im_v_string);
    }

    /* Check NVrec features */
    memset (buffer, 0, sizeof(buffer));
    tc_snprintf (buffer, sizeof(buffer), "%s -h 2>&1", prgname);
    f = popen (buffer, "r");
    memset (buffer, 0, sizeof(buffer));

    while ( fgets (buffer, MAX_BUF, f) ) {
	offset = strstr(buffer, ", version ");
	if (offset) {
	    nv_version = atoi(offset+10);
	    break;
	}
    }
    if (f) pclose(f);

    if (nv_version == 0) { 
	fprintf( stderr, "Unable to detect NVrec version, trying to continue...\n");
    } else if (0 < nv_version && nv_version < 20020513) {
	fprintf( stderr, "Seems your NVrec doesn't support the -o raw:// option\n");
	return(TC_IMPORT_ERROR);
    } else if (nv_version < 20020524) {
	/* make nvrec silent the hard way */
	APPEND(" 2>/dev/null");
    } else if (nv_version >= 20020524) {
	/* we support the -Q quiet option */
	APPEND(" -Q");
    }


    if (n<0) {
      perror("command buffer overflow");
      return(TC_IMPORT_ERROR);
    }


    
    // print out
    if(verbose_flag) printf("[%s] %s\n", MOD_NAME, import_cmd_buf);

  
    param->fd = NULL;

    // popen video
    /* if param->fd != NULL then transcode will do read */
    if((param->fd = popen(import_cmd_buf, "r"))== NULL) {
	perror("popen stream");
	return(TC_IMPORT_ERROR);
    }
  
  }

  return(0);
}

/* ------------------------------------------------------------ 
 *
 * decode  stream
 *
 * ------------------------------------------------------------*/

MOD_decode
{
    return(0);
}

/* ------------------------------------------------------------ 
 *
 * close stream
 *
 * ------------------------------------------------------------*/

MOD_close
{  

    if(param->fd != NULL) pclose(param->fd);

    return(0);
}


/* vim: sw=4
 */
