/*
 *  import_ogg.c
 *
 *  Copyright (C) Thomas Östreich - July 2002
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
#include <unistd.h>

#include "transcode.h"

#define MOD_NAME    "import_ogg.so"
#define MOD_VERSION "v0.0.1 (2002-07-30)"
#define MOD_CODEC   "(audio) Ogg Vorbis"

#define MOD_PRE ogg
#include "import_def.h"

#define MAX_BUF 1024
char import_cmd_buf[MAX_BUF];

static FILE *fd;

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_PCM;

/* ------------------------------------------------------------ 
 *
 * open stream
 *
 * ------------------------------------------------------------*/

MOD_open
{

    // audio only
    if(param->flag != TC_AUDIO) return(TC_IMPORT_ERROR);
    
    
    if((snprintf(import_cmd_buf, MAX_BUF, "oggdec --quiet %s --raw=1 --output=-", vob->audio_in_file)<0)) {
      perror("command buffer overflow");
      return(TC_IMPORT_ERROR);
    }
    
    if(verbose_flag) printf("[%s] Ogg Vorbis->PCM\n", MOD_NAME);
    
    // print out
    if(verbose_flag) printf("[%s] %s\n", MOD_NAME, import_cmd_buf);
    
    // popen
    if((fd = popen(import_cmd_buf, "r"))== NULL) {
	perror("popen pcm stream");
	return(TC_IMPORT_ERROR);
    }

    //caller handles read
    param->fd = fd;

    return(0);
}

/* ------------------------------------------------------------ 
 *
 * decode stream
 *
 * ------------------------------------------------------------*/

MOD_decode
{
  //nothing to do
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

