/*
 *  probe_stream.c
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <errno.h>
#include <unistd.h>

#include "transcode.h"
#include "ioaux.h"
#include "tc.h"
#include "aux_pes.h"

static int verbose;

static probe_info_t probe_info;

/* ------------------------------------------------------------ 
 *
 * probe thread
 *
 * ------------------------------------------------------------*/

void tcprobe_thread(info_t *ipipe)
{
    
    verbose = ipipe->verbose; 

    ipipe->probe_info = &probe_info;
    ipipe->probe = 1; 

    //date will be completed by subroutines
    memset((char*) &probe_info, 0, sizeof(probe_info_t));
    
    /* ------------------------------------------------------------ 
     *
     * check file type/magic and take action to probe for contents
     *
     * ------------------------------------------------------------*/
    

    switch(ipipe->magic) {
      
    case TC_MAGIC_AVI:     // AVI file
      probe_avi(ipipe);
      
      break;

    case TC_MAGIC_TIFF1:   // ImageMagick images
    case TC_MAGIC_TIFF2:
    case TC_MAGIC_JPEG:
    case TC_MAGIC_BMP:
    case TC_MAGIC_PNG:
    case TC_MAGIC_GIF:
    case TC_MAGIC_PPM:
    case TC_MAGIC_PGM:
      probe_im(ipipe);

      break;

    case TC_MAGIC_V4L_VIDEO:
	ipipe->probe_info->magic = TC_MAGIC_V4L_VIDEO;
	ipipe->probe_info->width  = 352;
	ipipe->probe_info->height = 288;

	break;
    case TC_MAGIC_V4L_AUDIO:
	ipipe->probe_info->magic = TC_MAGIC_V4L_AUDIO;

	ipipe->probe_info->track[0].samplerate = 48000;
	ipipe->probe_info->track[0].chan = 2;
	ipipe->probe_info->track[0].bits = 16;
	ipipe->probe_info->track[0].format = CODEC_PCM;
	ipipe->probe_info->num_tracks = 1;

	break;

    case TC_MAGIC_OGG:
	probe_ogg(ipipe);
	break;

    case TC_MAGIC_CDXA:    // RIFF 
      probe_pes(ipipe);
      ipipe->probe_info->attributes |= TC_INFO_NO_DEMUX;

      ipipe->probe_info->magic = TC_MAGIC_CDXA;
      
      break;
      
    case TC_MAGIC_VOB:     // VOB program stream
      probe_pes(ipipe);
      
      //NTSC video/film

      
      if(verbose & TC_DEBUG) fprintf(stderr, "att0=%d, att1=%d\n", ipipe->probe_info->ext_attributes[0], ipipe->probe_info->ext_attributes[1]);
      
      if(ipipe->probe_info->codec==TC_CODEC_MPEG2
	 && ipipe->probe_info->height==480 && ipipe->probe_info->width==720) {
	
	if(ipipe->probe_info->ext_attributes[0] > 2 * ipipe->probe_info->ext_attributes[1] || ipipe->probe_info->ext_attributes[1] == 0) ipipe->probe_info->is_video=1;
		
	if(ipipe->probe_info->is_video) {
	  ipipe->probe_info->fps=NTSC_VIDEO;
	  ipipe->probe_info->frc=4;
	} else { 
	  ipipe->probe_info->fps=NTSC_FILM;
	  ipipe->probe_info->frc=1;
	} 
      }
      
      //MPEG video, no program stream
      
      if(ipipe->probe_info->codec==TC_CODEC_MPEG1) 
	ipipe->probe_info->magic=TC_MAGIC_MPG;
      
      //check for need of special import module, that does not rely on 2k packs
      
      if(ipipe->probe_info->attributes & TC_INFO_NO_DEMUX) {
	ipipe->probe_info->codec=TC_CODEC_MPEG;
	ipipe->probe_info->magic=TC_MAGIC_MPG;
      }
      
      break;
      
    case TC_MAGIC_M2V:     // MPEG ES
      probe_pes(ipipe);

      // make sure not to use the demuxer
      ipipe->probe_info->codec=TC_CODEC_MPEG;
      ipipe->probe_info->magic=TC_MAGIC_MPG;

      break;

    case TC_MAGIC_MPEG:     // MPEG PES
      probe_pes(ipipe);

      ipipe->probe_info->attributes |= TC_INFO_NO_DEMUX;

      break;

    case TC_MAGIC_DVD:
    case TC_MAGIC_DVD_PAL:
    case TC_MAGIC_DVD_NTSC:

      probe_dvd(ipipe);
      break;

    case TC_MAGIC_YUV4MPEG:
      probe_yuv(ipipe);
      break;

    case TC_MAGIC_NUV:
      probe_nuv(ipipe);
      break;

    case TC_MAGIC_MOV:
      probe_mov(ipipe);
      break;

    case TC_MAGIC_XML:
      probe_xml(ipipe);
      break;

    case TC_MAGIC_LAV:
      probe_lav(ipipe);
      break;

      //case TC_MAGIC_OGG:
      //not yet implemented
      //break;
      
    case TC_MAGIC_TS:
      //not yet implemented
      break;

    case TC_MAGIC_WAV:
      probe_wav(ipipe);
      break;

    case TC_MAGIC_AC3:
	probe_ac3(ipipe);
	break;

    case TC_MAGIC_MP3:
    case TC_MAGIC_MP3_2:
    case TC_MAGIC_MP3_2_5:
    case TC_MAGIC_MP2:
	probe_mp3(ipipe);
	break;

    case TC_MAGIC_DV_PAL:
    case TC_MAGIC_DV_NTSC:
      probe_dv(ipipe);

      ipipe->probe_info->magic=TC_MAGIC_DV_PAL;

      break;

#ifdef NET_STREAM
    case TC_MAGIC_SOCKET:
      probe_net(ipipe);
      break;
#endif

    default:
      ipipe->error=2;
    }
    if (ipipe->magic == TC_MAGIC_XML)
	ipipe->probe_info->magic_xml=TC_MAGIC_XML;	//used in transcode to load import_xml and to have the correct type of the video/audio
    else 
	ipipe->probe_info->magic_xml=ipipe->probe_info->magic;
    
    return;
}

  
