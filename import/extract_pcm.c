/*
 *  extract_pcm.c
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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "ioaux.h"
#include "avilib.h"

#define MAX_BUF 4096
char audio[MAX_BUF];


#define BUFFER_SIZE 262144
static uint8_t buffer[BUFFER_SIZE];
static FILE *in_file, *out_file;

static int verbose;

static unsigned int track_code;

static int cmp_16_bits(char *buf, long x)
{
  
  int16_t sync_word=0;

  if(0) {
    fprintf(stderr, "MAGIC: 0x%02lx 0x%02lx 0x%02lx 0x%02lx %s\n", (x >> 24) & 0xff, ((x >> 16) & 0xff), ((x >>  8) & 0xff), ((x      ) & 0xff), filetype(x));
    fprintf(stderr, " FILE: 0x%02x 0x%02x 0x%02x 0x%02x\n", buf[0] & 0xff, buf[1] & 0xff, buf[2] & 0xff, buf[3] & 0xff);
  }

  sync_word = (sync_word << 8) + (uint8_t) buf[0]; 
  sync_word = (sync_word << 8) + (uint8_t) buf[1]; 
  
  if(sync_word == (int16_t) x) return 1;

  // not found;
  return 0;
}

static void pes_lpcm_loop (void)
{
    static int mpeg1_skip_table[16] = {
	     1, 0xffff,      5,     10, 0xffff, 0xffff, 0xffff, 0xffff,
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff
    };

    int n=0;

    uint8_t * buf;
    uint8_t * end;
    uint8_t * tmp1=NULL;
    uint8_t * tmp2=NULL;
    int complain_loudly;

    complain_loudly = 1;
    buf = buffer;

    do {
      end = buf + fread (buf, 1, buffer + BUFFER_SIZE - buf, in_file);
      buf = buffer;
      
      //scan buffer
      while (buf + 4 <= end) {
	
	// check for valid start code
	if (buf[0] || buf[1] || (buf[2] != 0x01)) {
	  if (complain_loudly && (verbose & TC_DEBUG)) {
	    fprintf (stderr, "(%s) missing start code at %#lx\n",
		     __FILE__, ftell (in_file) - (end - buf));
	    if ((buf[0] == 0) && (buf[1] == 0) && (buf[2] == 0))
	      fprintf (stderr, "(%s) incorrect zero-byte padding detected - ignored\n", __FILE__);
	    complain_loudly = 0;
	  }
	  buf++;
	  continue;
	}// check for valid start code 
	
	if(verbose & TC_STATS) fprintf(stderr,"packet code 0x%x\n", buf[3]); 

	switch (buf[3]) {
	  
	case 0xb9:	/* program end code */
	  return;
	  
	case 0xba:	/* pack header */

	  /* skip */
	  if ((buf[4] & 0xc0) == 0x40)	        /* mpeg2 */
	    tmp1 = buf + 14 + (buf[13] & 7);
	  else if ((buf[4] & 0xf0) == 0x20)	/* mpeg1 */
	    tmp1 = buf + 12;
	  else if (buf + 5 > end)
	    goto copy;
	  else {
	    fprintf (stderr, "(%s) weird pack header\n", __FILE__);
	    import_exit(1);
	  }
	  
	  if (tmp1 > end)
	    goto copy;
	  buf = tmp1;
	  break;
	  

	case 0xbd:	/* private stream 1 */
	  tmp2 = buf + 6 + (buf[4] << 8) + buf[5];
	  if (tmp2 > end)
	    goto copy;
	  if ((buf[6] & 0xc0) == 0x80)	/* mpeg2 */
	    tmp1 = buf + 9 + buf[8];
	  else {	/* mpeg1 */
	    for (tmp1 = buf + 6; *tmp1 == 0xff; tmp1++)
	      if (tmp1 == buf + 6 + 16) {
		fprintf (stderr, "(%s) too much stuffing\n", __FILE__);
		buf = tmp2;
		break;
	      }
	    if ((*tmp1 & 0xc0) == 0x40)
	      tmp1 += 2;
	    tmp1 += mpeg1_skip_table [*tmp1 >> 4];
	  }

	  if(verbose & TC_STATS) fprintf(stderr,"track code 0x%x\n", *tmp1); 

	  if (*tmp1 == track_code) {   
	    
	    if (tmp1 < tmp2-2) 
	      
	      for(n=0; n<tmp2-tmp1-2; ++n) {
		if(cmp_16_bits(tmp1+n, TC_MAGIC_LPCM)) {
		  break;
		}
	      }
	    
	    tmp1 += n+2;
	    if (tmp1 < tmp2) fwrite (tmp1, tmp2-tmp1, 1, stdout);
	  }
	  buf = tmp2;
	  
	  break;
	  
	default:
	  if (buf[3] < 0xb9) {
	    fprintf (stderr, "(%s) looks like a video stream, not program stream\n", __FILE__);
	    import_exit(1);
	  }
	  
	  /* skip */
	  tmp1 = buf + 6 + (buf[4] << 8) + buf[5];
	  if (tmp1 > end)
	    goto copy;
	  buf = tmp1;
	  break;

	} //start code selection
      } //scan buffer
      
      if (buf < end) {
      copy:
	/* we only pass here for mpeg1 ps streams */
	memmove (buffer, buf, end - buf);
      }
      buf = buffer + (end - buf);
      
    } while (end == buffer + BUFFER_SIZE);
}


extern void import_exit(int ret);

/* ------------------------------------------------------------ 
 *
 * pcm extract thread
 *
 * magic: TC_MAGIC_AVI 
 *        TC_MAGIC_RAW  <-- default
 *        TC_MAGIC_WAW
 *        TC_MAGIC_VOB
 *
 * ------------------------------------------------------------*/


void extract_pcm(info_t *ipipe)
{

  avi_t *avifile;
     
  long frames, bytes, padding, n;

  int error=0;

  struct wave_header wave;


  /* ------------------------------------------------------------ 
   *
   * AVI
   *
   * ------------------------------------------------------------*/
  
  // AVI

  switch (ipipe->magic) {
    
  case TC_MAGIC_AVI:
    
    if(ipipe->stype == TC_STYPE_STDIN){
	fprintf(stderr, "(%s) invalid magic/stype - exit\n", __FILE__);
      error=1;
      break;
    }
    
    // scan file
    if(NULL == (avifile = AVI_open_fd(ipipe->fd_in,1))) {
      AVI_print_error("AVI open");
      break;
    }
  
    //set selected for multi-audio AVI-files
    AVI_set_audio_track(avifile, ipipe->track);
  
    // get total audio size
   bytes=ipipe->frame_limit[1] - ipipe->frame_limit[0];
   if (ipipe->frame_limit[1] ==LONG_MAX)
   {
     bytes = AVI_audio_bytes(avifile);
   }
   AVI_set_audio_position(avifile,ipipe->frame_limit[0]);
    
    padding = bytes % MAX_BUF;
    frames = bytes / MAX_BUF;
    for (n=0; n<frames; ++n) {
      
      if(AVI_read_audio(avifile, audio, MAX_BUF)<0) {
	error=1;
	break;
      }
      
      if(p_write(ipipe->fd_out, audio, MAX_BUF)!= MAX_BUF) {
	error=1;
	break;
      }
    }
    
    if((bytes = AVI_read_audio(avifile, audio, padding)) < padding) 
      error=1;
      
    if(p_write(ipipe->fd_out, audio, bytes)!= bytes) error=1;

    break;

  /* ------------------------------------------------------------ 
   *
   * WAV
   *
   * ------------------------------------------------------------*/
  
  // WAV
  
  case TC_MAGIC_WAV:
    
    if(p_read(ipipe->fd_in, (char *)&wave, sizeof(wave))!= sizeof(wave) ) {
      error=1;
      break;
    }
    
    // get total audio size
    bytes = wave.riff.len - sizeof(wave);
    
    if(bytes<=0) { 
      error=1;
      break;
    }

    do {
      if((bytes=p_read(ipipe->fd_in, audio, MAX_BUF))!=MAX_BUF) error=1;
      if(p_write(ipipe->fd_out, audio, bytes)!= bytes) error=1;
    } while(!error);
    
    break;

    /* ------------------------------------------------------------ 
     *
     * VOB
     *
     * ------------------------------------------------------------*/
    
    // VOB
    
  case TC_MAGIC_VOB:

      in_file = fdopen(ipipe->fd_in, "r");
      out_file = fdopen(ipipe->fd_out, "w");
      
      track_code = 0xA0 + ipipe->track;      
      pes_lpcm_loop();
      
      fclose(in_file);
      fclose(out_file);
      
    break;
    

    /* ------------------------------------------------------------ 
     *
     * RAW
     *
     * ------------------------------------------------------------*/
    
    // RAW
    
  case TC_MAGIC_RAW:

  default:

      if(ipipe->magic == TC_MAGIC_UNKNOWN)
	  fprintf(stderr, "(%s) no file type specified, assuming %s\n", 
		  __FILE__, filetype(TC_MAGIC_RAW));

   	bytes=ipipe->frame_limit[1] - ipipe->frame_limit[0];
   	//skip the first ipipe->frame_limit[0] bytes
	if (ipipe->frame_limit[0]!=0)
		if (fseek(ipipe->fd_in,ipipe->frame_limit[0],SEEK_SET) !=0)
		{
			error=1;
			break;
		}
   	if (ipipe->frame_limit[1] ==LONG_MAX)
   	{
    		error=p_readwrite(ipipe->fd_in, ipipe->fd_out);
	}
	else
   	{
   		padding = bytes % MAX_BUF;
   		frames = bytes / MAX_BUF;
   		for (n=0; n<frames; ++n) 
  		{
      			if(p_read(ipipe->fd_in, audio, MAX_BUF)!= MAX_BUF) 
      			{
				error=1;
				break;
      			}
			if(p_write(ipipe->fd_out, audio, MAX_BUF)!= MAX_BUF) 
			{
				error=1;
				break;
      			}
    		}
   		if (padding !=0)
		{
      			if(p_read(ipipe->fd_in, audio, padding)!= padding) 
      			{
				error=1;
				break;
      			}
			if(p_write(ipipe->fd_out, audio, padding)!= padding) 
			{
				error=1;
				break;
      			}
		}
	}
      
      break;
  }

  if(error) //need 
  	import_exit(error);
}

