/*
 *  export_sampeg.c
 *
 *  Copyright (C) Thomas Östreich - December 2002
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
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "transcode.h"
#include "avilib.h"

#define MOD_NAME    "export_sampeg.so"
#define MOD_VERSION "v0.0.0 (2002-12-19)"
#define MOD_CODEC   "(video) MPEG-2"

#define MOD_PRE sampeg
#include "export_def.h"

static FILE* 			pFile 		= NULL;
static int 			verbose_flag	= TC_QUIET;
static int 			capability_flag	= TC_CAP_PCM;
static struct wave_header 	rtf;

/* ------------------------------------------------------------ 
 *
 * Pipe write helper function 
 *
 * ------------------------------------------------------------*/


static int p_write (char *buf, size_t len)
{
    size_t n  = 0;
    size_t r  = 0;
    int    fd = fileno (pFile);

    while (r < len) 
    {
        if ((n = write (fd, buf + r, len - r)) < 0)
	    return n;
      
        r += n;
    }
   
    return r;
}


/* ------------------------------------------------------------ 
 *
 * open outputfile
 *
 * ------------------------------------------------------------*/

MOD_open
{
    int verb;
  
    if (param->flag == TC_VIDEO) 
    {
        char buf [PATH_MAX];
	int srate, brate;
	char *chan;

        verb = (verbose & TC_DEBUG) ? 2:0;

	srate = (vob->mp3frequency != 0) ? vob->mp3frequency : vob->a_rate;
	brate = vob->mp3bitrate;
	chan = (vob->a_chan==2) ? "-s": "-m";
	
	if(((unsigned)snprintf(buf, PATH_MAX, "mp2enc -v %d -r %d -b %d %s -o \"%s\".mpa", verb, srate, brate, chan, vob->audio_out_file)>=PATH_MAX)) {
	  perror("cmd buffer overflow");
	  return(TC_EXPORT_ERROR);
	} 
	
        if(verbose & TC_INFO) printf("[%s] (%d/%d) cmd=%s\n", MOD_NAME, strlen(buf), PATH_MAX, buf);
	
        if((pFile = popen (buf, "w")) == NULL)
	  return(TC_EXPORT_ERROR);
	
        if (p_write ((char*) &rtf, sizeof(rtf)) != sizeof(rtf)) 
	  {    
      	    perror("write wave header");
      	    return(TC_EXPORT_ERROR);
        }     
   
        return(0);
    }
  
    if (param->flag == TC_AUDIO) return(0);
    
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
  if (param->flag == TC_VIDEO) return(0);
  if (param->flag == TC_AUDIO) return(0);  
  
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
  if(param->flag == TC_VIDEO)
    {
      if (p_write (param->buffer, param->size) != param->size) 
        {    
	  perror("write video frame");
	  return(TC_EXPORT_ERROR);
        }      
      return (0); 
    }
  
  if (param->flag == TC_AUDIO) return(0);
  
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
  if (param->flag == TC_VIDEO) 
    return (0);
  
  if (param->flag == TC_AUDIO) 
    return (0);
  
  return(TC_EXPORT_ERROR);     
}

/* ------------------------------------------------------------ 
 *
 * close codec
 *
 * ------------------------------------------------------------*/

MOD_close
{  
    if (param->flag == TC_AUDIO) 
      return (0);
    
    if (param->flag == TC_VIDEO) 
      {
        if (pFile) 
	  pclose (pFile);
	
	pFile = NULL;
	
        return(0);
      }
    
    return (TC_EXPORT_ERROR); 
}

