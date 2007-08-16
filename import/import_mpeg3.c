/*
 *  import_mpeg3.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
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

#define MOD_NAME    "import_mpeg3.so"
#define MOD_VERSION "v0.3 (2002-09-20)"
#define MOD_CODEC   "(video) MPEG2 | (audio) MPEG/AC3/PCM"

#include "transcode.h"

static int verbose_flag = TC_QUIET;
static int capability_flag = TC_CAP_RGB | TC_CAP_YUV | TC_CAP_AUD | TC_CAP_PCM;

#define MOD_PRE mpeg3
#include "import_def.h"

#include <libmpeg3.h>


#define BUFSIZE 65536

#define FRAMES_TO_PREFETCH 8

// We need different structures for audio/video import
static mpeg3_t* file = NULL;
static mpeg3_t* file_a = NULL;

static int codec, stream_id;
static int height, width; 

static int astreamid = 0;

static unsigned char framebuffer[PAL_W*PAL_H*3];
static unsigned char extrabuffer[PAL_W*3+4];
static unsigned char *rowptr[PAL_H];

static unsigned short *read_buffer;
static unsigned short *prefetch_buffer;
static unsigned int prefetch_len;

static unsigned char *y_output, *u_output, *v_output;

/* ------------------------------------------------------------
 *
 * open stream
 *
 * ------------------------------------------------------------*/

MOD_open
{
  int i = 0;
#if TC_LIBMPEG3_VERSION >= 170
  int retcode = 0;
#endif

  param->fd = NULL;

  //open stream
  // I don't know which comes first and if were using both audio
  // and video import

  if (param->flag == TC_VIDEO) {
      if (!file) {
	  if (!file_a) {
#if TC_LIBMPEG3_VERSION >= 170
	      if((file = mpeg3_open(vob->video_in_file, &retcode))==NULL) {
		  fprintf(stderr, "open file failed with error %i\n", retcode);
#else
	      if((file = mpeg3_open(vob->video_in_file))==NULL) {
		  fprintf(stderr, "open file failed with error\n");
#endif
		  return(TC_IMPORT_ERROR);
	      }
	      if (verbose & TC_DEBUG)
                  printf("[%s] Opened video NO copy\n", MOD_NAME);
	  } else if (file_a) {
#if TC_LIBMPEG3_VERSION >= 170
	      if((file = mpeg3_open_copy(vob->video_in_file, file_a, &retcode))==NULL) {
		  fprintf(stderr, "open file failed with error %i\n", retcode);
#else
	      if((file = mpeg3_open_copy(vob->video_in_file, file_a))==NULL) {
		  fprintf(stderr, "open file failed with error\n");
#endif
		  return(TC_IMPORT_ERROR);
	      }
	      if (verbose & TC_DEBUG)
                  printf("[%s] Opened video WITH copy\n", MOD_NAME);
	  }
      }
  }
  if (param->flag == TC_AUDIO) {
      if (!file_a) {
	  if (!file) {
#if TC_LIBMPEG3_VERSION >= 170
	      if((file_a = mpeg3_open(vob->audio_in_file, &retcode))==NULL) {
		  fprintf(stderr, "open file failed with error %i\n", retcode);
#else
	      if((file_a = mpeg3_open(vob->audio_in_file))==NULL) {
		  fprintf(stderr, "open file failed with error\n");
#endif
		  return(TC_IMPORT_ERROR);
	      }
	      if (verbose & TC_DEBUG)
                  printf("[%s] Opened audio NO copy\n", MOD_NAME);
	  } else if (file) {
#if TC_LIBMPEG3_VERSION >= 170
	      if((file_a = mpeg3_open_copy(vob->audio_in_file, file, &retcode))==NULL) {
		  fprintf(stderr, "open file failed\n");
#else
	      if((file_a = mpeg3_open_copy(vob->audio_in_file, file))==NULL) {
		  fprintf(stderr, "open file failed\n");
#endif
		  return(TC_IMPORT_ERROR);
	      }
	      if (verbose & TC_DEBUG)
                  printf("[%s] Opened audio WITH copy\n", MOD_NAME);
	  }
      }
  }


  if(param->flag == TC_AUDIO) {
      int astream;
      int a_rate, a_chan;
      long a_samp;

      mpeg3_set_cpus(file_a,1);

  
      if (!mpeg3_has_audio(file_a)) {
	  printf("[%s] No audio found\n", MOD_NAME);
	  return TC_IMPORT_ERROR;
      }
      astream = mpeg3_total_astreams(file_a);
      if (verbose & TC_DEBUG)
          printf("[%s] <%d> audio streams found, we only handle one"
                 " stream right now\n", 
	         MOD_NAME, astream);

      astreamid = vob->a_track;
      a_rate = mpeg3_sample_rate(file_a, astreamid);
      a_chan = mpeg3_audio_channels(file_a, astreamid);
      a_samp = -1;

      if (verbose & TC_DEBUG)
	  printf("[%s] <%d> Channels, <%d> Samplerate, <%ld> Samples,"
                 " <%d> fch, <%s> Format\n",
                 MOD_NAME, a_chan, a_rate, a_samp, vob->im_a_size,
	         mpeg3_audio_format(file_a, astreamid));

      if (a_rate != vob->a_rate) {
	  fprintf(stderr, "[%s] Audio parameter mismatch (rate)\n", MOD_NAME);
	  return TC_IMPORT_ERROR;
      }

      if (a_chan != vob->a_chan) {
	  fprintf(stderr, "[%s] Audio parameter mismatch (%d!=%d channels)\n",
                          MOD_NAME, a_chan, vob->a_chan);
	  //return TC_IMPORT_ERROR;
      }


      if (vob->im_a_string) {
	  long sample = strtol(vob->im_a_string, (char **)NULL, 0);
	  mpeg3_set_sample(file_a, sample*vob->im_a_size/2, astreamid);
      }

      // prefetch 
      prefetch_len = vob->im_a_size * FRAMES_TO_PREFETCH;
      
      read_buffer = malloc (prefetch_len);
      prefetch_buffer = malloc (prefetch_len);
      if (!read_buffer || !prefetch_buffer) {
	  fprintf(stderr, "[%s] malloc failed at %d\n", MOD_NAME, __LINE__);
	      return TC_IMPORT_ERROR;
      }

      return(TC_IMPORT_OK);
  }

  if(param->flag == TC_VIDEO) {

    if(!mpeg3_check_sig(vob->video_in_file)) return(TC_IMPORT_ERROR);

    mpeg3_set_cpus(file,1);


    codec=vob->im_v_codec;

    stream_id = vob->v_track;

    width=vob->im_v_width;
    height=vob->im_v_height;

    switch(codec) {
    
    case CODEC_RGB:
      
    for (i=0; i<height; i++)
    {
	if(i==height-1)
	{
	    rowptr[i]=extrabuffer;
	}
	rowptr[i]=framebuffer+(width*(height-1)*3)-(width*3*i);
    }
    break;

    case CODEC_YUV:
      
      y_output=framebuffer;
      v_output=y_output+width*height;
      u_output=v_output+((width*height)>>2);
      break;
    }
    
    if (vob->im_v_string) {
	long sample = strtol(vob->im_v_string, (char **)NULL, 0);
	mpeg3_set_frame(file, sample, stream_id);
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
    
  int i, block;

  if(param->flag == TC_AUDIO) {
      
	int channel = 0;
	int result = 0;
	static int framenum = 0;
	int len = prefetch_len/(vob->a_chan * sizeof(int16_t));

	int16_t *output_ptr;
	int16_t *input_ptr;
	int16_t *input_end;

	// I have to do the prefetch stuff, otherwise audio read performance
	// drops down to an incredible low value.
	// probably because of libmpeg3s bazillion fseeks -- tibit

	if (framenum%FRAMES_TO_PREFETCH == 0) {
	    // Zero output channels
	    memset(read_buffer, 0, prefetch_len);
	    memset(prefetch_buffer, 0, prefetch_len);

	    for(channel = 0; (channel < vob->a_chan) && !result; channel++) {
		if(channel == 0) {
		    result = mpeg3_read_audio(file_a, 
			    NULL, 
			    read_buffer, 
			    channel, 
			    len, 
			    astreamid);
		} else {
		    result = mpeg3_reread_audio(file_a, 
			    NULL, 
			    read_buffer, 
			    channel, 
			    len, 
			    astreamid);
		}

		// Interleave for output
		output_ptr = (int16_t*)prefetch_buffer + channel;
		input_ptr = (int16_t*)read_buffer;
		input_end = input_ptr + len;

		while(input_ptr < input_end)
		{
			*output_ptr = *input_ptr++;
			output_ptr += vob->a_chan;
		}
		if (result) return TC_IMPORT_ERROR;
	    }
	} 
	tc_memcpy (param->buffer, 
		(char *)prefetch_buffer + 
                           (framenum % FRAMES_TO_PREFETCH) * vob->im_a_size, 
		vob->im_a_size);
	framenum++;
      return(TC_IMPORT_OK);
    }


  if(param->flag == TC_VIDEO) {

    switch(codec) {
    
    case CODEC_RGB:
      
      if(mpeg3_read_frame(file, rowptr, 0, 0, width, height, width,
                          height, MPEG3_BGR888, stream_id))
          return(TC_IMPORT_ERROR);
      
      block = width * 3; 
      param->size = block * height;
    
      for(i=0; i<height; ++i) 
	tc_memcpy(param->buffer+(i-1)*block,rowptr[height-i-1], block);

      break;

    case CODEC_YUV:

      if(mpeg3_read_yuvframe(file, y_output, u_output, v_output, 0, 0,
                             width, height, stream_id))
          return(TC_IMPORT_ERROR);

      param->size = (width * height * 3)>>1;
      tc_memcpy(param->buffer, framebuffer,param->size);
      break;
    }
    return(TC_IMPORT_OK);
  }
  return(TC_IMPORT_ERROR);
}

/* ------------------------------------------------------------ 
 *
 * close stream
 *
 * ------------------------------------------------------------*/

MOD_close
{  
    
    if(param->fd != NULL) pclose(param->fd);

    if(param->flag == TC_VIDEO) {
	if (file) {
	    mpeg3_close(file);
	    file = NULL;
	}
	return TC_IMPORT_OK;
    }
    if(param->flag == TC_AUDIO) {
	if (file_a) {
	    mpeg3_close(file_a);
	    file_a = NULL;
	}
	if (prefetch_buffer) {free (prefetch_buffer); prefetch_buffer=NULL;}
	if (read_buffer) {free (read_buffer); read_buffer=NULL;}
	return TC_IMPORT_OK;
    }

    return(TC_IMPORT_ERROR);
}
