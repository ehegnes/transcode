/*
 *  filter_divxkey.c
 *
 *  Copyright (C) Thomas Östreich - December 2001
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

#define MOD_NAME    "filter_divxkey.so"
#define MOD_VERSION "v0.1 (2002-01-15)"
#define MOD_CAP     "check for DivX 4.xx / OpenDivX / DivX;-) keyframe"
#define MOD_AUTHOR  "Thomas Oestreich"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

/* -------------------------------------------------
 *
 * mandatory include files
 *
 *-------------------------------------------------*/

#include "transcode.h"
#include "framebuffer.h"
#include "import/magic.h"
#include "bitstream.h"
#include "optstr.h"

static char buffer[128];

vob_t *vob=NULL;

static DECODER dec;
static BITSTREAM bs;

uint32_t rounding;
uint32_t quant;
uint32_t fcode;

inline static int stream_read_char(char *d)
{
    return (*d & 0xff);
}

inline static unsigned int stream_read_dword(char *s)
{
    unsigned int y;
    y=stream_read_char(s);
    y=(y<<8)|stream_read_char(s+1);
    y=(y<<8)|stream_read_char(s+2);
    y=(y<<8)|stream_read_char(s+3);
    return y;
}

// Determine of the compressed frame is a keyframe for direct copy
int quicktime_divx4_is_key(unsigned char *data, long size)
{
        int result = 0;
        int i;

        for(i = 0; i < size - 5; i++)
        {
                if( data[i]     == 0x00 && 
                        data[i + 1] == 0x00 &&
                        data[i + 2] == 0x01 &&
                        data[i + 3] == 0xb6)
                {
                        if((data[i + 4] & 0xc0) == 0x0) 
                                return 1;
                        else
                                return 0;
                }
        }
        
        return result;
}

int quicktime_divx3_is_key(char *d)
{
    int32_t c=0;
    
    c=stream_read_dword(d);
    if(c&0x40000000) return(0);
    
    return(1);
}

/*-------------------------------------------------
 *
 * single function interface
 *
 *-------------------------------------------------*/

int tc_filter(vframe_list_t *ptr, char *options)
{

  int pre=0, vid=0;
  
  int cc0=0, cc1=0, cc2=0, cc3=0;

  // API explanation:
  // ================
  //
  // (1) need more infos, than get pointer to transcode global 
  //     information structure vob_t as defined in transcode.h.
  //
  // (2) 'tc_get_vob' and 'verbose' are exported by transcode.
  //
  // (3) filter is called first time with TC_FILTER_INIT flag set.
  //
  // (4) make sure to exit immediately if context (video/audio) or 
  //     placement of call (pre/post) is not compatible with the filters 
  //     intended purpose, since the filter is called 4 times per frame.
  //
  // (5) see framebuffer.h for a complete list of frame_list_t variables.
  //
  // (6) filter is last time with TC_FILTER_CLOSE flag set


  if(ptr->tag & TC_FILTER_GET_CONFIG) {
      optstr_filter_desc (options, MOD_NAME, MOD_CAP, MOD_VERSION, "Thomas Oestreich", "VE", "1");
      return 0;
  }


  //----------------------------------
  //
  // filter init
  //
  //----------------------------------
  

  if(ptr->tag & TC_FILTER_INIT) {
    
    if((vob = tc_get_vob())==NULL) return(-1);
    
    // filter init ok.
    
    if(verbose) printf("[%s] %s %s\n", MOD_NAME, MOD_VERSION, MOD_CAP);
    
    if(verbose) printf("[%s] options=%s\n", MOD_NAME, options);

    snprintf(buffer, sizeof(buffer), "%s-%s", PACKAGE, VERSION);
    
    //init filter

    if(verbose) printf("[%s] divxkey\n", MOD_NAME);

    return(0);
  }
  
  //----------------------------------
  //
  // filter close
  //
  //----------------------------------

  
  if(ptr->tag & TC_FILTER_CLOSE) {

    return(0);
  }
  
  //----------------------------------
  //
  // filter frame routine
  //
  //----------------------------------

  // tag variable indicates, if we are called before
  // transcodes internal video/audo frame processing routines
  // or after and determines video/audio context
  
  if(verbose & TC_STATS) printf("[%s] %s/%s %s %s\n", MOD_NAME, vob->mod_path, MOD_NAME, MOD_VERSION, MOD_CAP);
  
  // tag variable indicates, if we are called before
  // transcodes internal video/audo frame processing routines
  // or after and determines video/audio context
  
  pre = (ptr->tag & TC_PRE_PROCESS)? 1:0;
  vid = (ptr->tag & TC_VIDEO)? 1:0;

  if(pre && vid) {

      bs_init_tc(&bs, (char*) ptr->video_buf);
    
      cc0 = bs_vol(&bs, &dec);
      cc1 = bs_vop(&bs, &dec, &rounding, &quant, &fcode);
      
      if(verbose & TC_STATS) fprintf(stderr, "frame=%d vop=%d vol=%d (%d %d %d)\n", ptr->id, cc1, cc0, rounding, quant, fcode);
      

      // DivX ;-)
      
      if(vob->codec_flag == TC_CODEC_DIVX3) {
	  
	  if(ptr->video_size>4) cc3=quicktime_divx3_is_key((unsigned char *)ptr->video_buf);
	  
	  if(cc3) ptr->attributes |= TC_FRAME_IS_KEYFRAME;
	  if((verbose & TC_DEBUG) && cc3) fprintf(stderr, "key (intra) @ %d  \n", ptr->id);
	  
      }
      
      // DivX

      if(vob->codec_flag == TC_CODEC_DIVX4 || vob->codec_flag == TC_CODEC_DIVX5) {
	  
	  cc2=quicktime_divx4_is_key((unsigned char *)ptr->video_buf, (long) ptr->video_size);
	  if(cc2  && cc1 == I_VOP) ptr->attributes |= TC_FRAME_IS_KEYFRAME;
	  if((verbose & TC_DEBUG) && cc2 && cc1 == I_VOP) fprintf(stderr, "key (intra) @ %d  \n", ptr->id);
      }
  }
  
  return(0);
}
