/*
 *  aud_aux.c
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
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <inttypes.h>

#include "aud_aux.h"
#include "ac3.h"
#include "../aclib/ac.h"

/* ------------------------------------------------------------ 
 *
 *      out-- PCM   MP2/3   AC3
 *   in 
 *   |
 *   PCM       X      X          
 * 
 *   MP2/3            X
 *
 *   AC3                     X
 *
 *
 *-------------------------------------------------------------*/

/* todo:
 * zoomplayer: no audio
 * nandub: write 16 bit in header
 */

static char *buffer = NULL;
static char *input_buffer = NULL;
static int input_buffer_len = 0;
static int buffer_len = 0;
static int buffer_size = SIZE_PCM_FRAME;

static lame_global_flags *lgf;
static int lame_status_flag=0;

static int mute=0;
static int info_shown=0;
static int lame_flush=0;

static int verbose=TC_DEBUG, i_codec, o_codec;

static int bitrate=0, sample_size=0, aud_mono=0, bitrate_flag=0;

static avi_t *avifile1=NULL, *avifile2=NULL;

static FILE *fd=NULL;

// AVI file information for subsequent calls of open routine:
static int avi_aud_codec, avi_aud_bitrate;
static long avi_aud_rate;
static int avi_aud_chan, avi_aud_bits;

#ifdef LAME_3_89
void no_debug(const char *format, va_list ap) {return;}
#else
extern char *get_lame_version();
#endif

int tc_get_mp3_header(unsigned char* hbuf, int* chans, int* freq);
#define tc_decode_mp3_header(hbuf)  tc_get_mp3_header(hbuf, NULL, NULL)

#define aud_size_len  (2*1152)

/* ------------------------------------------------------------ 
 *
 * init audio encoder
 *
 * ------------------------------------------------------------*/

char * lame_error2str(int error);

int audio_init(vob_t *vob, int debug)
{

    verbose=debug;

    i_codec = vob->im_a_codec;
    o_codec = vob->ex_a_codec;
    
    // defaults:
    avi_aud_bitrate = vob->mp3bitrate;
    avi_aud_codec = vob->ex_a_codec;

    avi_aud_bits=(vob->dm_bits != vob->a_bits) ? vob->dm_bits : vob->a_bits;
    avi_aud_chan=(vob->dm_chan != vob->a_chan) ? vob->dm_chan : vob->a_chan;
    avi_aud_rate=(vob->mp3frequency != 0) ? vob->mp3frequency : vob->a_rate;

    lame_flush=vob->lame_flush;

    sample_size = (avi_aud_bits>>3) * avi_aud_chan; //for encoding

    if(avi_aud_chan==1) aud_mono = 1;

    if (vob->amod_probed && 
	strlen(vob->amod_probed)>=4 && 
	strncmp(vob->amod_probed,"null", 4) == 0) 
    {
	fprintf(stderr, "(%s) No amod probed, muting\n", __FILE__);
	mute = 1;
	return 0;
    }

    if(!sample_size && i_codec != CODEC_NULL) {
	fprintf(stderr, "(%s) invalid sample size %d detected - invalid audio format in=0x%x\n", __FILE__, sample_size, i_codec);
	return(TC_EXPORT_ERROR); 
    }	

    if ( (buffer=malloc(SIZE_PCM_FRAME)) == NULL)
	return TC_EXPORT_ERROR;
    if ( (input_buffer=malloc(SIZE_PCM_FRAME)) == NULL)
	return TC_EXPORT_ERROR;
    memset (buffer, 0, buffer_size);
    memset (input_buffer, 0, buffer_size);

    if(verbose & TC_DEBUG) fprintf(stderr, "(%s) audio submodule in=0x%x out=0x%x\n", __FILE__, i_codec, o_codec);

    switch(i_codec) {
	
    case CODEC_PCM:
      
      /* ------------------------------------------------------ 
       *
       * PCM processing 
       *
       *------------------------------------------------------*/
      
	switch(o_codec) {
	  
      case CODEC_NULL:
	
	mute=1;
	break;
	
      case CODEC_MP2:
      case CODEC_MP3:
	
	if(!lame_status_flag) {

#ifdef LAME_3_89

	int preset = 0;

	  if((lgf=lame_init())<0) {
	    fprintf(stderr, "(%s) lame encoder init failed\n", __FILE__);
	    return(TC_EXPORT_ERROR);
	  }

	  if(!(verbose & TC_DEBUG)) lame_set_msgf(lgf, no_debug);
	  if(!(verbose & TC_DEBUG)) lame_set_debugf(lgf, no_debug);
	  if(!(verbose & TC_DEBUG)) lame_set_errorf(lgf, no_debug);

	  lame_set_bWriteVbrTag(lgf,0);

	  lame_set_quality(lgf, vob->mp3quality);
	  if(vob->a_vbr){  // VBR:
	      lame_set_VBR(lgf, vob->a_vbr); 
	      lame_set_VBR_q(lgf, vob->mp3quality); // 1 = best vbr q  6=~128k
	      //if(vob->mp3bitrate>0) lame_set_VBR_mean_bitrate_kbps(lgf,vob->mp3bitrate);
	  } else {
	      lame_set_VBR(lgf, 0); 
	      lame_set_brate(lgf, vob->mp3bitrate);
	  }


	  if(vob->bitreservoir==TC_FALSE) lame_set_disable_reservoir(lgf, 1);

	  lame_set_in_samplerate(lgf, vob->a_rate);
	  lame_set_num_channels(lgf, (avi_aud_chan>2 ? 2:avi_aud_chan));

	  //jstereo/mono
	  lame_set_mode(lgf, (avi_aud_chan>1 ? JOINT_STEREO:MONO)); 

          //sample rate
	  lame_set_out_samplerate(lgf, avi_aud_rate);

	  //asm 
#if HAVE_LAME >= 392
	  if(tc_accel & MM_MMX) lame_set_asm_optimizations(lgf, MMX, 1);
	  if(tc_accel & MM_3DNOW) lame_set_asm_optimizations(lgf, AMD_3DNOW, 1);
	  if(tc_accel & MM_SSE) lame_set_asm_optimizations(lgf, SSE, 1);

	  // do the preset stuff
	  if (vob->lame_preset && strlen (vob->lame_preset)) {
	      char *c = strchr (vob->lame_preset, ',');
	      int fast = 0;
	      if (c && *c && *(c+1)) {
		  if (strcmp(c+1, "fast"))
		      fast = 1;
	      }

	      if      (!strcmp (vob->lame_preset, "standard")) {
		  preset = fast?STANDARD_FAST:STANDARD;
		  vob->a_vbr = 1;
	      }
#if HAVE_LAME >= 393
	      else if (!strcmp (vob->lame_preset, "medium")) {
		  preset = fast?MEDIUM_FAST:MEDIUM;
		  vob->a_vbr = 1;
	      }
#endif
	      else if (!strcmp (vob->lame_preset, "extreme")) {
		  preset = fast?EXTREME_FAST:EXTREME;
		  vob->a_vbr = 1;
	      }
	      else if (!strcmp (vob->lame_preset, "insane")) {
		  preset = INSANE;
		  vob->a_vbr = 1;
	      }
	      else if ( atoi(vob->lame_preset) != 0) {
		  vob->a_vbr = 1;
		  preset = atoi(vob->lame_preset);
		  avi_aud_bitrate = preset;
	      }
	      else 
		  fprintf(stderr, "(%s) Error: preset \'%s\' not supported\n", __FILE__, vob->lame_preset);

	      if (preset) {
		  //if (verbose & TC_DBUG) 
		      printf("(%s) using preset |%d| from lame_preset |%s|\n", __FILE__, preset, vob->lame_preset);

		  lame_set_preset(lgf, preset);
	      }
	  }
#endif
	  
	  lame_init_params(lgf);

	  if(verbose) fprintf(stderr,"(%s) using lame-%s\n", __FILE__, get_lame_version());
	  
	  if(verbose & TC_DEBUG) {
            fprintf(stderr, "(%s) PCM->%s\n", __FILE__, ((o_codec==CODEC_MP3)?"MP3":"MP2"));
            fprintf(stderr, "(%s)           bitrate: %d kbit/s\n", __FILE__, vob->mp3bitrate);
	    fprintf(stderr, "(%s) output samplerate: %d Hz\n", __FILE__, (vob->mp3frequency > 0 ? vob->mp3frequency:vob->a_rate));
	  }
#else
	  
	  if((lgf = malloc(sizeof(lame_global_flags)))==NULL) {
	    fprintf(stderr, "(%s) out of memory", __FILE__);
	    return(TC_EXPORT_ERROR); 
	  }
	  
	  if(lame_init(lgf)<0) {
	    fprintf(stderr, "(%s) lame encoder init failed\n", __FILE__);
	    return(TC_EXPORT_ERROR);
	  }
	  
	  lgf->silent=1;
	  lgf->VBR=vbr_off;
	  lgf->in_samplerate=vob->a_rate;
	  lgf->num_channels=(avi_aud_chan>2 ? 2:avi_aud_chan);
	  lgf->mode=(avi_aud_chan>1 ? 1:3);
	  lgf->brate=(vob->mp3bitrate*1000)/8/125;

          if (vob->mp3frequency==0) vob->mp3frequency=vob->a_rate;
	  lgf->out_samplerate=vob->mp3frequency;
	  
	  lame_init_params(lgf);
	  
	  if(verbose & TC_DEBUG) {
            fprintf(stderr, "(%s) PCM->%s\n", __FILE__, ((o_codec==CODEC_MP3)?"MP3":"MP2"));
            fprintf(stderr, "(%s)           bitrate: %d kbit/s\n", __FILE__, vob->mp3bitrate);
            fprintf(stderr, "(%s) output samplerate: %d Hz\n", __FILE__, lgf->out_samplerate);
          }

	  if(verbose) fprintf(stderr,"(%s) using lame-%s (static)\n", __FILE__, (char *) get_lame_version());
#endif	  
	  
	  // init lame encoder only on first call
	  lame_status_flag=1;
	  }
	
	break;
	
      case CODEC_PCM:
	
	// adjust bitrate
	avi_aud_bitrate=(vob->a_rate*4)/1000*8; //magic
	if(verbose & TC_DEBUG) fprintf(stderr, "(%s) PCM->PCM\n", __FILE__);
	break;
	
      default:
	fprintf(stderr, "(%s) audio codec in=0x%x out=0x%x conversion not supported\n", __FILE__, i_codec, o_codec);
	return(TC_EXPORT_ERROR);
      }
      
      break;
      
    case CODEC_MP2:
    case CODEC_MP3:
      
      /* ------------------------------------------------------ 
       *
       * MPEG audio processing 
       *
       *------------------------------------------------------*/
	
      // only pass through supported
      
      switch(o_codec) {

      case CODEC_NULL:
	
	mute=1;
	break;	

      case CODEC_MP2:
      case CODEC_MP3:
	break;
	
      default:
	fprintf(stderr, "(%s) audio codec in=0x%x out=0x%x conversion not supported\n", __FILE__, i_codec, o_codec);
	return(TC_EXPORT_ERROR);
      }
      
      break;
      
    case CODEC_AC3:
      
      /* ------------------------------------------------------ 
       *
       * AC3 processing 
       *
       *------------------------------------------------------*/
      
      // only pass through supported
      switch(o_codec) {
	
      case CODEC_NULL:
	
	mute=1;
	break;

      case CODEC_AC3:
	// the bitrate can only be determined in the encoder section
	if(verbose & TC_DEBUG) fprintf(stderr, "(%s) AC3->AC3\n", __FILE__);

	//set to 1 after bitrate is determined
	bitrate_flag=0;
	
	break;
	
      default:
	fprintf(stderr, "(%s) 0x%x->0x%x not supported - exit\n", __FILE__, i_codec, o_codec);
	return(TC_EXPORT_ERROR);
      } //out codec for pcm
      
      break;
      
    case CODEC_NULL:
      
      /* ------------------------------------------------------ 
       *
       * no audio requested
       *
       *------------------------------------------------------*/
      
      mute=1;
      break;


    case CODEC_RAW:
      
      /* ------------------------------------------------------ 
       *
       * audio pass-through mode
       *
       *------------------------------------------------------*/

      if(vob->pass_flag & TC_AUDIO) {

	if(avifile1==NULL) 
	  if(NULL == (avifile1 = AVI_open_input_file(vob->audio_in_file,1))) {
	    AVI_print_error("avi open error");
	    return(TC_EXPORT_ERROR); 
	  }

	// set correct pass-through track:
	AVI_set_audio_track(avifile1, vob->a_track);

	//small hack to fix incorrect samplerates caused by 
	//transcode < 0.5.0-20011109 
	if (vob->mp3frequency==0) vob->mp3frequency=AVI_audio_rate(avifile1);
	avi_aud_rate   =  vob->mp3frequency;
	
	avi_aud_chan   =  AVI_audio_channels(avifile1);
	avi_aud_bits   =  AVI_audio_bits(avifile1);
	
	avi_aud_codec   =  AVI_audio_format(avifile1);
	avi_aud_bitrate =  AVI_audio_mp3rate(avifile1);
	
	if(avifile1!=NULL) {
	  AVI_close(avifile1);
	  avifile1=NULL;
	}
      } else mute=1;
      
      break;
      
    default:
      fprintf(stderr, "(%s) audio codec 0x%x not supported - exit\n", __FILE__, i_codec);
      return(TC_EXPORT_ERROR);
    } //in codec
    
    return(TC_EXPORT_OK);
}


/* ------------------------------------------------------------ 
 *
 * open audio output file
 *
 * ------------------------------------------------------------*/


int audio_open(vob_t *vob, avi_t *avifile)
{
  

  if(mute) return(0);
  
  if(vob->out_flag) {
    
    if(fd==NULL) {
      
      if((fd = fopen(vob->audio_out_file, "w"))<0) {
	fprintf(stderr, "(%s) fopen audio file\n", __FILE__);
	return(TC_EXPORT_ERROR);
      }     
    } // open audio output file
    
    if(verbose & TC_DEBUG) 
      fprintf(stderr, "(%s) sending audio output to %s\n", __FILE__, vob->audio_out_file);
    
  } else {
    
    if(avifile==NULL) {
      mute=1;
      
      if(verbose) fprintf(stderr,"(%s) no option -m found, muting sound\n", __FILE__);
      return(0);
    }
    
    AVI_set_audio(avifile, avi_aud_chan, avi_aud_rate, avi_aud_bits, avi_aud_codec, avi_aud_bitrate);
    AVI_set_audio_vbr(avifile, vob->a_vbr);

    if(avifile2==NULL) avifile2 = avifile; //save for close
    
    if((verbose & TC_DEBUG) && (!info_shown))
      fprintf(stderr, "(%s) format=0x%x, rate=%ld Hz, bits=%d, channels=%d, bitrate=%d\n", __FILE__, avi_aud_codec, avi_aud_rate, avi_aud_bits, avi_aud_chan, avi_aud_bitrate);
  }
  
  info_shown=1;
  return(TC_EXPORT_OK);
}


/* ------------------------------------------------------------ 
 *
 * encode audio frame
 *
 * ------------------------------------------------------------*/


int audio_encode(char *aud_buffer, int aud_size, avi_t *avifile)
{
  
    int i, outsize;
    
    char *outbuf=0;
    int header_len = 0;
    int count=0;
    int audio_already_written = 0;
    int write_audio=1;
    
    uint16_t sync_word = 0;

    if(mute) return(0);

    // defaults
    outbuf  = aud_buffer;
    outsize = aud_size;
    
    if(verbose & TC_STATS) fprintf(stderr, "(%s) audio submodule: in=0x%x out=0x%x\n %d bytes\n", __FILE__, i_codec, o_codec, aud_size);

    switch(i_codec) {
      
    case CODEC_PCM:
      
      /* ------------------------------------------------------ 
       *
       * PCM processing 
       *
       *------------------------------------------------------*/
      
      switch(o_codec) {
	
      case CODEC_MP2:
      case CODEC_MP3:

        memcpy (input_buffer + input_buffer_len, aud_buffer, aud_size);
	input_buffer_len += aud_size;
	
	// this codec has to write the audio in a loop so we don't use the 
	// "general" write after the switch statement.

	audio_already_written = 1;

#define DEBUG
#undef DEBUG

	while(buffer_len < 4 && input_buffer_len >= aud_size_len) {


	    if(aud_mono) {

		outsize = lame_encode_buffer(lgf, (short int *) input_buffer,
			(short int *) input_buffer, aud_size_len/2, 
			buffer + buffer_len, buffer_size - buffer_len);

	    } else {

		outsize = lame_encode_buffer_interleaved(lgf, (short int *) input_buffer, 
			aud_size_len/4, buffer + buffer_len, buffer_size - buffer_len);

	    }

	    if(outsize<0) {
		fprintf(stderr, "(%s) lame encoding error|1| (%s)\n", __FILE__, lame_error2str(outsize));
		return(TC_EXPORT_ERROR); 
	    }
	    // please, please rewrite me.
	    memmove (input_buffer, input_buffer+aud_size_len, buffer_size - aud_size_len);

	    buffer_len += outsize;
	    input_buffer_len -= aud_size_len;
	    ++count;
#ifdef DEBUG
	    fprintf(stderr, "(%s) |1| count(%d) outsize(%d) buffer_len(%d) consumed(%d)\n", 
		    __FILE__, count, outsize, buffer_len, count*aud_size_len);
#endif
	}

	header_len = tc_decode_mp3_header(buffer);

#ifdef DEBUG
	fprintf(stderr, "(%s) header_len(%d) buffer_len(%d) input_buffer_len(%d)\n", 
		__FILE__, header_len, buffer_len, input_buffer_len);
#endif

	while (buffer_len < header_len || input_buffer_len >= aud_size_len) {
	    write_audio = 1;

	    // don't have any more data
	    if (aud_size_len > input_buffer_len) {
		write_audio = 0; //buffer for more audio;
#ifdef DEBUG
		fprintf(stderr, "(%s) MOVING from(%d) len(%d)\n", 
			__FILE__, (count)*aud_size_len, aud_size - (count)*aud_size_len);
#endif
		memmove(input_buffer, input_buffer + input_buffer_len, buffer_size-input_buffer_len);
		break;
	    }

	    while (input_buffer_len >= aud_size_len) {

		if(aud_mono) {

		    outsize = lame_encode_buffer(lgf, (short int *) input_buffer,
			    (short int *) input_buffer, aud_size_len/2, 
			    buffer + buffer_len, buffer_size - buffer_len);
		} else {

		    outsize = lame_encode_buffer_interleaved(lgf, (short int *)input_buffer, 
			    aud_size_len/4, buffer + buffer_len, buffer_size - buffer_len);

		}

		if(outsize<0) {
		    fprintf(stderr, "(%s) lame encoding error |2|(%s)\n", __FILE__, lame_error2str(outsize));
		    return(TC_EXPORT_ERROR); 
		}
		// please, please rewrite me.
		memmove (input_buffer, input_buffer+aud_size_len, buffer_size - aud_size_len);
		buffer_len += outsize;
		input_buffer_len -= aud_size_len;
		++count;

#ifdef DEBUG
		fprintf(stderr, "(%s) |2| count(%d) outsize(%d) buffer_len(%d) consumed(%d) input_buffer_len(%d)\n", 
			__FILE__, count, outsize, buffer_len, count*aud_size_len, input_buffer_len);
#endif
	    }
	}


	if(outsize<0) {
	    fprintf(stderr, "(%s) lame encoding error |3|(%d)\n", __FILE__, outsize);
	    return(TC_EXPORT_ERROR); 
	}
#ifdef DEBUG
	fprintf(stderr, "(%s) |3| buffer_len(%d) headerlen(%d) aud_size(%d) write=%s\n", 
		__FILE__, buffer_len, header_len, aud_size, (write_audio?"yes":"no"));
#endif

	if (header_len > 0 && write_audio) {
	    int doit=1;
	    int inner_len = header_len;

	    while (buffer_len>=inner_len && doit) {
#ifdef DEBUG
		fprintf(stderr, "XXX do the write out! (%d)\n", inner_len);
#endif
		if(fd != NULL) {

		    if(fwrite(buffer, inner_len, 1, fd) != 1) {    
			fprintf(stderr, "(%s) audio file write error\n", __FILE__);
			return(TC_EXPORT_ERROR);
		    }

		}  else {

		    if(AVI_write_audio(avifile, buffer, inner_len)<0) {
			AVI_print_error("AVI file audio write error");
			return(TC_EXPORT_ERROR);
		    } 
		}

		memmove (buffer, buffer+inner_len, buffer_size-inner_len);
		buffer_len -= inner_len;
		inner_len = tc_decode_mp3_header(buffer);
		if (inner_len<0)
		    doit=0;
	    }
	} // write_audio

      break;
	
      case CODEC_PCM:  
	  break;
      }
      
      break;
      
    case CODEC_MP2:
    case CODEC_MP3:
    case CODEC_RAW:
      
      /* ------------------------------------------------------ 
       *
       * MPEG audio processing 
       *
       *------------------------------------------------------*/
      
      // nothing to do
      break;
      
    case CODEC_AC3:
      
      /* ------------------------------------------------------ 
       *
       * AC3 processing 
       *
       *------------------------------------------------------*/
      
      
      if(!bitrate_flag) {
	// try to determine bitrate from audio frame:
	
	for(i=0;i<aud_size-3;++i) {
	  sync_word = (sync_word << 8) + (uint8_t) aud_buffer[i]; 
	  if(sync_word == 0x0b77) {
	    if((bitrate = get_ac3_bitrate(&aud_buffer[i+1]))<0) bitrate=0;
	    break;
	  }
	}
	
	//assume bitrate > 0 is OK.
	if(bitrate > 0) {
	  AVI_set_audio_bitrate(avifile, bitrate);
	  if(verbose & TC_DEBUG) 
	    fprintf(stderr, "(%s) bitrate %d kBits/s\n", __FILE__, bitrate);
	  bitrate_flag=1;
	}
      }
      // nothing else to do
      
      break;
      
    case CODEC_NULL:
      
      /* ------------------------------------------------------ 
       *
       * no audio requested
       *
       *------------------------------------------------------*/
      
      break;	
      
    default:
      fprintf(stderr, "invalid export codec request 0x%x\n", i_codec);
      return(TC_EXPORT_ERROR);
    } // switch
    

    // write audio to AVI file
    
    if(mute)  return(0);

    if(audio_already_written) return (0);

    if(fd != NULL) {
	
	if(outsize) {
	    if(fwrite(outbuf, outsize, 1, fd) != 1) {    
		fprintf(stderr, "(%s) audio file write error\n", __FILE__);
		return(TC_EXPORT_ERROR);
	    }
	}
    } else {
      
	if(AVI_write_audio(avifile, outbuf, outsize)<0) {
	    AVI_print_error("AVI file audio write error");
	    return(TC_EXPORT_ERROR);
	} 
    }
    
    return(0);
}

int audio_close()
{    
  
  if(mute) return(0);

  // reset bitrate flag for AC3 pass-through
  bitrate_flag = 0;
  
#ifdef LAME_3_89
  
  switch(o_codec) {
    
  case CODEC_MP2:
  case CODEC_MP3:
    
    if(lame_flush) {

      int outsize=0;

      outsize = lame_encode_flush(lgf, buffer, 0);

      if(verbose & TC_DEBUG) fprintf(stderr, "(%s) flushing %d audio bytes\n", __FILE__, outsize);    
      
      if(outsize > 0) {

	if(fd != NULL) {
	  if(fwrite(buffer, outsize, 1, fd) != 1) {    
	    fprintf(stderr, "(%s) audio file write error\n", __FILE__);
	    return(TC_EXPORT_ERROR);
	  } 
	} else {

	  if(avifile2!=NULL) {

	    if(AVI_append_audio(avifile2, buffer, outsize)<0) {
	      AVI_print_error("AVI file audio write error");
	      return(TC_EXPORT_ERROR);
	    }
	  }
	}
      }
    }
    
    break;
  }
  
#endif  
  
  if(fd!=NULL) {
    fclose(fd);
    fd=NULL;
  }
  
  return(0);
}

int audio_stop()
{    

  if(mute) return(0);

#ifdef LAME_3_89

  switch(o_codec) {
    
  case CODEC_MP2:
  case CODEC_MP3:
    
    lame_close(lgf);
    break;
  }

#endif

  return(0);
}

// from mencoder
//----------------------- mp3 audio frame header parser -----------------------

static int tabsel_123[2][3][16] = {
   { {0,32,64,96,128,160,192,224,256,288,320,352,384,416,448,0},
     {0,32,48,56, 64, 80, 96,112,128,160,192,224,256,320,384,0},
     {0,32,40,48, 56, 64, 80, 96,112,128,160,192,224,256,320,0} },

   { {0,32,48,56,64,80,96,112,128,144,160,176,192,224,256,0},
     {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,0},
     {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,0} }
};
static long freqs[9] = { 44100, 48000, 32000, 22050, 24000, 16000 , 11025 , 12000 , 8000 };

/*
 * return frame size or -1 (bad frame)
 */
int tc_get_mp3_header(unsigned char* hbuf, int* chans, int* srate){
    int stereo, ssize, crc, lsf, mpeg25, framesize;
    int padding, bitrate_index, sampling_frequency;
    unsigned long newhead = 
      hbuf[0] << 24 |
      hbuf[1] << 16 |
      hbuf[2] <<  8 |
      hbuf[3];


#if 1
    // head_check:
    if( (newhead & 0xffe00000) != 0xffe00000 ||  
        (newhead & 0x0000fc00) == 0x0000fc00){
	//fprintf( stderr, "[%s] head_check failed\n", __FILE__);
	return -1;
    }
#endif

    if((4-((newhead>>17)&3))!=3){ 
      fprintf( stderr, "[%s] not layer-3\n", __FILE__); 
      return -1;
    }

    if( newhead & ((long)1<<20) ) {
      lsf = (newhead & ((long)1<<19)) ? 0x0 : 0x1;
      mpeg25 = 0;
    } else {
      lsf = 1;
      mpeg25 = 1;
    }

    if(mpeg25)
      sampling_frequency = 6 + ((newhead>>10)&0x3);
    else
      sampling_frequency = ((newhead>>10)&0x3) + (lsf*3);

    if(sampling_frequency>8){
	fprintf( stderr, "[%s] invalid sampling_frequency\n", __FILE__);
	return -1;  // valid: 0..8
    }

    crc = ((newhead>>16)&0x1)^0x1;
    bitrate_index = ((newhead>>12)&0xf);
    padding   = ((newhead>>9)&0x1);
//    fr->extension = ((newhead>>8)&0x1);
//    fr->mode      = ((newhead>>6)&0x3);
//    fr->mode_ext  = ((newhead>>4)&0x3);
//    fr->copyright = ((newhead>>3)&0x1);
//    fr->original  = ((newhead>>2)&0x1);
//    fr->emphasis  = newhead & 0x3;

    stereo    = ( (((newhead>>6)&0x3)) == 3) ? 1 : 2;

    if(!bitrate_index){
      fprintf( stderr, "[%s] Free format not supported.\n", __FILE__);
      return -1;
    }

    if(lsf)
      ssize = (stereo == 1) ? 9 : 17;
    else
      ssize = (stereo == 1) ? 17 : 32;
    if(crc) ssize += 2;

    framesize = tabsel_123[lsf][2][bitrate_index] * 144000;

    if(!framesize){
	fprintf( stderr, "[%s] invalid framesize/bitrate_index\n", __FILE__);
	return -1;  // valid: 1..14
    }

    framesize /= freqs[sampling_frequency]<<lsf;
    framesize += padding;

//    if(framesize<=0 || framesize>MAXFRAMESIZE) return FALSE;
    if(srate) *srate = freqs[sampling_frequency];
    if(chans) *chans = stereo;

    return framesize;
}

char * lame_error2str(int error)
{
    switch (error) {
	case -1: return "-1:  mp3buf was too small";
	case -2: return "-2:  malloc() problem";
	case -3: return "-3:  lame_init_params() not called";
	case -4: return "-4:  psycho acoustic problems";
	case -5: return "-5:  ogg cleanup encoding error";
	case -6: return "-6:  ogg frame encoding error";
	default: return "";
    }
}
