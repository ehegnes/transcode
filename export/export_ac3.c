/*
 *  export_ac3.c
 *
 *  Copyright (C) Daniel Pittman, 2003, based on export_ogg.c which was:
 *  Copyright (C) Tilmann Bitterberg, July 2002
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
#include <unistd.h>

#include "transcode.h"

#define MOD_NAME    "export_ac3.so"
#define MOD_VERSION "v0.1 (2003-02-26)"
#define MOD_CODEC   "(video) null | (audio) ac3"

static int   verbose_flag=TC_QUIET;
static int   capability_flag=TC_CAP_PCM;

#define MOD_PRE ac3
#include "export_def.h"
static FILE *pFile = NULL;

static inline int p_write (char *buf, size_t len)
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
 * open codec
 *
 * ------------------------------------------------------------*/

MOD_open
{
    int result;
    if (param->flag == TC_AUDIO) {
	char buf [PATH_MAX];
        char out_fname [PATH_MAX];

        strcpy(out_fname, vob->audio_out_file);
        strcat(out_fname, ".ac3");

	if (vob->mp3bitrate == 0) {
            fprintf (stderr, "[%s] Audio bitrate 0 is not valid, cannot cope.\n", MOD_NAME);
            return(TC_EXPORT_ERROR);
        }

        result = snprintf (buf, PATH_MAX,
                           "ffmpeg -y -f s%dle -ac %d -ar %d -i - -ab %d -acodec ac3 %s%s",
                           vob->dm_bits,
                           vob->dm_chan,
                           vob->mp3frequency,
                           vob->mp3bitrate,
                           out_fname,
                           vob->verbose > 1 ? "" : " >&/dev/null");
	if (result < 0) {
	    perror("command buffer overflow");
	    return(TC_EXPORT_ERROR); 
	}

        if (verbose > 0)
	    fprintf (stderr, "[%s] %s\n", MOD_NAME, buf);
        
	if ((pFile = popen (buf, "w")) == NULL)
	    return(TC_EXPORT_ERROR);

	return(0);
		
    }
    if (param->flag == TC_VIDEO) 
	return(0);

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
    if(param->flag == TC_AUDIO) 
    {
        return(0);
    }
  
    if (param->flag == TC_VIDEO) 
	return(0);  
  
    // invalid flag
    return(TC_EXPORT_ERROR); 
}


/* ------------------------------------------------------------ 
 *
 * encode and export
 *
 * ------------------------------------------------------------*/

MOD_encode
{
    if(param->flag == TC_AUDIO)
    {
        if (p_write (param->buffer, param->size) != param->size) 
        {    
            perror("write audio frame");
            return(TC_EXPORT_ERROR);
        }      
        return (0); 
    }
  
    if (param->flag == TC_VIDEO) 
        return(0);

    // invalid flag
    return(TC_EXPORT_ERROR); 
}


/* ------------------------------------------------------------ 
 *
 * stop codec
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
    if (param->flag == TC_VIDEO) 
	return (0);
  
    if (param->flag == TC_AUDIO) 
    {
        if (pFile) 
	  pclose (pFile);
    
	pFile = NULL;

        return(0);
    }
  
    return (TC_EXPORT_ERROR); 
}

/* vim: sw=4
 */
