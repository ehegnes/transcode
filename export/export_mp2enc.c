/*
 *  export_mp2enc.c
 *
 *  Georg Ludwig - January 2002
 *
 *  Parts of export_wav and export_mpeg2enc used for this file
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

#include "probe_export.h"

#define MOD_NAME    "export_mp2enc.so"
#define MOD_VERSION "v1.0.9 (2003-10-30)"
#define MOD_CODEC   "(audio) MPEG 1/2"

static int 			verbose_flag	= TC_QUIET;
static int 			capability_flag	= TC_CAP_PCM;

#define MOD_PRE mp2enc
#include "export_def.h"

static FILE* 			pFile 		= NULL;
static struct wave_header 	rtf;

static char *mpa=".mpa";

/* ------------------------------------------------------------ 
 *
 * open outputfile
 *
 * ------------------------------------------------------------*/

MOD_open
{
    int verb;

    /* check for mp2enc */
    if (tc_test_program("mp2enc") != 0) return (TC_EXPORT_ERROR);
          
    if (param->flag == TC_AUDIO) 
    {
        char buf [PATH_MAX];
	int srate, brate;
	char *chan;

	mpa = audio_ext;

	//tibit: do not write to /dev/null.m1v
	if (vob->audio_out_file && strlen(vob->audio_out_file)>=9 && !strncmp(vob->audio_out_file, "/dev/null", 9)) {
	    mpa="";
	}

        verb = (verbose & TC_DEBUG) ? 2:0;

	srate = (vob->mp3frequency != 0) ? vob->mp3frequency : vob->a_rate;
	brate = vob->mp3bitrate;
	chan = (vob->dm_chan>=2) ? "-s": "-m";
	
	if(((unsigned)snprintf(buf, PATH_MAX, "mp2enc -v %d -r %d -b %d %s -o \"%s%s\" %s", verb, srate, brate, chan, vob->audio_out_file, mpa, (vob->ex_a_string?vob->ex_a_string:""))>=PATH_MAX)) {
	  perror("cmd buffer overflow");
	  return(TC_EXPORT_ERROR);
	} 
	
        if(verbose & TC_INFO) printf("[%s] (%d/%d) cmd=%s\n", MOD_NAME, (int)strlen(buf), PATH_MAX, buf);
	
        if((pFile = popen (buf, "w")) == NULL)
	  return(TC_EXPORT_ERROR);
	
        if (AVI_write_wave_header (fileno (pFile), &rtf) != 0)
	{    
      	    perror("write wave header");
      	    return(TC_EXPORT_ERROR);
        }     
   
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
        memset((char *) &rtf, 0, sizeof(rtf));
    
        strncpy(rtf.riff.id, "RIFF", 4);
        rtf.riff.len = sizeof(struct riff_struct)
	             + sizeof(struct chunk_struct)
		     + sizeof(struct common_struct);
        strncpy(rtf.riff.wave_id, "WAVE",4);
        strncpy(rtf.format.id, "fmt ",4);
    
        rtf.format.len = sizeof (struct common_struct);
	
        rtf.common.wFormatTag        = CODEC_PCM;
        rtf.common.dwSamplesPerSec   = vob->a_rate;
        rtf.common.dwAvgBytesPerSec  = vob->dm_chan*vob->a_rate*vob->dm_bits/8;
        rtf.common.wChannels         = vob->dm_chan;
        rtf.common.wBitsPerSample    = vob->dm_bits;
        rtf.common.wBlockAlign       = vob->dm_chan*vob->dm_bits/8;

        strncpy(rtf.data.id, "data",4);

	if ( !(probe_export_attributes & TC_PROBE_NO_EXPORT_AEXT))
		audio_ext = mpa;
	  
        fprintf(stderr, "[%s] *** init-v *** !\n", MOD_NAME); 
    
        return(0);
    }
  
    if (param->flag == TC_VIDEO) 
	return(0);  
  
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
    if(param->flag == TC_AUDIO)
    {
	if (AVI_write_wave_pcm_data(
		fileno (pFile),
		param->buffer, param->size
		) != param->size)
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

