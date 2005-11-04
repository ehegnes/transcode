/*
 *  af6_decore.cpp
 *
 *  Copyright (C) Thomas Östreich - January 2002
 *  Updated by Christian Vogelgsang <Vogelgsang@informatik.uni-erlangen.de>
 *
 *  This file is part of transcode, a video stream processing tool
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
#include <string.h>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_AVIFILE

#if HAVE_AVIFILE_INCLUDES == 7
#include <avm_fourcc.h>
#include <avifile.h>
#include <image.h>
#include <aviplay.h>
#include <avm_cpuinfo.h>
#include <utils.h>
#include <version.h>
#include <renderer.h>
#include <avm_creators.h>
#elif HAVE_AVIFILE_INCLUDES == 0
#include <avifile/avm_fourcc.h>
#include <avifile/avifile.h>
#include <avifile/image.h>
#include <avifile/aviplay.h>
#include <avifile/cpuinfo.h>
#include <avifile/utils.h>
#include <avifile/version.h>
#include <avifile/renderer.h>
#include <avifile/creators.h>
#endif

#include <iostream>

using namespace std;
using namespace Creators;

#ifdef __cplusplus
extern "C" {
#endif

#include "transcode.h"
#include "magic.h"
#include "aclib/imgconvert.h"
  
  void af6_decore(decode_t *decode)
  {
    int verbose_flag=TC_QUIET;
    static char *sync_str="Taf6"; 

    verbose_flag=decode->verbose;

    /* ----- AF6 VIDEO ----- */
    if(decode->select == TC_VIDEO) {

      IAviReadStream *vrs   = NULL;
      IAviReadFile   *vfile = NULL;
      unsigned int plane_size  = 0;
      ssize_t buffer_size = 0;
      int codec_error = 0;
      fourcc_t fcc = 0;
      ImageFormat srcfmt = IMG_NONE;
      int do_yuv = 0;
      long s_tot_frame,s_init_frame;

      /* for unpacking: */
      unsigned int unpack_size = 0;
      uint8_t *unpack_buffer = 0, *unpack[3];

      /* create a new file reader */
      vfile = CreateIAviReadFile(decode->name);
      
      /* setup decoder */
      vrs = vfile->GetStream(0, AviStream::Video);
      if((vrs==0) || (vrs->StartStreaming()!=0)) {
	fprintf(stderr, "(%s) ERROR: unable to decode movie\n", __FILE__);
	return;
      }

      /* decoder bitmap setup */
      BITMAPINFOHEADER bh;
      vrs->GetDecoder()->SetDestFmt(24);
      vrs->GetOutputFormat(&bh, sizeof(bh));      
      vrs->SetDirection(1);
      bh.biHeight=labs(bh.biHeight);
      bh.biWidth=labs(bh.biWidth);
      fprintf(stderr, "(%s) input video size: %dx%d\n",
	      __FILE__, bh.biWidth, bh.biHeight);
      plane_size = bh.biWidth * bh.biHeight;

      /* now check decoder capabilities */
      IVideoDecoder::CAPS caps = vrs->GetDecoder()->GetCapabilities();
      codec_error=0;
      switch (decode->format) {
      case TC_CODEC_RGB:
	do_yuv = 0;
	fprintf(stderr, "(%s) input: RGB\n", __FILE__);
	buffer_size = plane_size * 3;
	break;
	
      case TC_CODEC_YUV420P:
	do_yuv = 1;
	// Decide on AVI target format
	// see http://www.webartz.com/fourcc/fccyuv.htm

	// YUV 4:2:0 planar formats are supported directly:
	// I420 = IYUV = Y plane + U subplane + V subplane
	if(caps & (IVideoDecoder::CAP_I420|IVideoDecoder::CAP_IYUV)) {
	  fcc = fccI420;
	  fprintf(stderr, "(%s) input: YUV 4:2:0 planar data\n", __FILE__);
	  buffer_size = (plane_size * 3)/2;
	} 
	// YV12 = Y plane + V subplane + U subplane
	else if(caps & IVideoDecoder::CAP_YV12) {
	  fcc = fccYV12;
	  fprintf(stderr, "(%s) input: YVU 4:2:0 planar data\n", __FILE__);
	  buffer_size = (plane_size * 3)/2;
	} 
    
	// YUV 4:2:2 packed formats are supported via conversion:
	// YUY2 = Y0 U0 Y1 V0  Y2 U2 Y3 V2  ...
	else if(caps & IVideoDecoder::CAP_YUY2) {
	  fcc = fccI420;
	  srcfmt = IMG_YUY2;
	  fprintf(stderr, "(%s) input: YUYV 4:2:2 packed data\n", __FILE__);
	}
	// UYVY = U0 Y0 V0 Y1  U2 Y2 V2 Y3  ...
	else if(caps & IVideoDecoder::CAP_UYVY) {
	  fcc = fccI420;
	  srcfmt = IMG_UYVY;
	  fprintf(stderr, "(%s) input: UYVY 4:2:2 packed data\n", __FILE__);
	}
	// YVYU = Y0 V0 Y1 U0  Y2 V2 Y3 U2  ...
	else if(caps & IVideoDecoder::CAP_YVYU) {
	  fcc = fccI420;
	  srcfmt = IMG_YVYU;
	  fprintf(stderr, "(%s) input: YVYU 4:2:2 packed data\n", __FILE__);
	}
	// Hmm... unknown codec capability!
	else {
	  fprintf(stderr,"(%s) ERROR: codec supports only caps=%d\n",
		  __FILE__,caps);
	  codec_error=1;
	}
	break;
      default:
	fprintf(stderr, "(%s) ERROR: unknown tc format!!!\n", __FILE__);
	codec_error=1;
	break;
      }
      if(codec_error) {
	fprintf(stderr, "(%s) ERROR: codec not supported!!!\n", __FILE__);
	return;
      }

      /* YUV special setup */
      if(do_yuv) {
	vrs->GetDecoder()->SetDestFmt(BitmapInfo::BitCount(fcc), fcc);
      } else /* RGB */ {
	vrs->GetDecoder()->SetDestFmt(24);
      }

      /* prepare unpacker */
      if(srcfmt) {
	unpack_size = (plane_size * 3) / 2; /* target is 4:2:0 */
	unpack_buffer = (uint8_t *)malloc(unpack_size);
	if(unpack_buffer==0) {
	  fprintf(stderr,"(%s) ERROR: No memory for buffer!!!\n",__FILE__);
	  return;
	}
	unpack[0] = unpack_buffer;
	unpack[1] = unpack_buffer + plane_size;
	unpack[2] = unpack_buffer + plane_size + (plane_size >> 2);
      }
      
      s_tot_frame=vrs->GetLength();	//get the total number of the frames
      if (decode->frame_limit[1] < s_tot_frame) //added to enable the -C option of tcdecode
      {
        s_tot_frame=decode->frame_limit[1];
      }
      
      /* start at the beginning */
      vrs->SeekToKeyFrame(0);

      /* send sync token */
      fflush(stdout);
      tc_pwrite(decode->fd_out, (uint8_t *)sync_str, sizeof(sync_str));

      /* frame serve loop */
      /* by default decode->frame_limit[0]=0 and ipipe->frame_limit[1]=LONG_MAX so all frames are decoded */
      for(s_init_frame=0;(s_init_frame<=s_tot_frame)||(!vrs->Eof());s_init_frame++) { 
	/* fetch a frame */
	vrs->ReadFrame();
	CImage *imsrc = vrs->GetFrame();
	uint8_t *buf = imsrc->Data();

	if (s_init_frame >= decode->frame_limit[0]) //added to enable the -C option of tcdecode
	{
	  if(srcfmt) {
	    /* unpack and write unpacked data */
	    ac_imgconvert(&buf, srcfmt, unpack, IMG_YUV420P,
	                  bh.biWidth, bh.biHeight);
	    if(tc_pwrite(decode->fd_out, unpack_buffer, unpack_size)!= unpack_size) {
	      fprintf(stderr,"(%s) ERROR: Pipe write error!\n",__FILE__);
	      break;
	    }
	  } else {
	    /* directly write raw frame */
	    if(tc_pwrite(decode->fd_out, buf, buffer_size)!= buffer_size) {
	      fprintf(stderr,"(%s) ERROR: Pipe write error!\n",__FILE__);
	      break;
	    }
	  }
	}
      }
      
      /* cleanup */
      delete vfile;
      if(unpack_buffer!=0)
	free(unpack_buffer);
    }

    /* ----- AF6 AUDIO ----- */
    if (decode->select == TC_AUDIO) {
	
      IAviReadStream *ars = NULL;
      IAviReadFile   *afile= NULL;

      const unsigned int TOTAL_SECS = 2;
      unsigned int buffer_size;
      unsigned int sample_size = 0;
      unsigned int samples;
      uint8_t *buffer;
      long s_byte_read=0;

      /* create AVI audio file reader */
      afile = CreateIAviReadFile(decode->name);
      
      /* setup decoder */
      ars = afile->GetStream(0, AviStream::Audio);
      if((ars==0)||(ars->StartStreaming()!=0)) {
	fprintf(stderr, "(%s) ERROR: invalid audio stream!!!\n", __FILE__);
	return;
      }

      /* query wave format */
      WAVEFORMATEX wvFmt;
      if (ars->GetAudioFormatInfo(&wvFmt, 0) != 0) {
	fprintf(stderr, "(%s) ERROR: can't fetch audio format!!!\n", __FILE__);
	return;
      }
      fprintf(stderr, 
	      "(%s) file audio: %s, %d bits, %dCH, sample rate = %dHz\n", 
	      __FILE__, avm_wave_format_name(wvFmt.wFormatTag), 
	      wvFmt.wBitsPerSample,
	      wvFmt.nChannels, 
	      wvFmt.nSamplesPerSec);
      WAVEFORMATEX fmt;
      ars->GetOutputFormat(&fmt, sizeof(fmt));
      fprintf(stderr, 
	      "(%s) output audio: %s, %d bits, %dCH, sample rate = %dHz\n", 
	      __FILE__, avm_wave_format_name(fmt.wFormatTag), 
	      fmt.wBitsPerSample,
	      fmt.nChannels, 
	      fmt.nSamplesPerSec);      
      
      /* currently only supports PCM */
      if(fmt.wFormatTag!=WAVE_FORMAT_PCM) {
	fprintf(stderr,"(%s) ERROR: currently only PCM audio supported!!!\n",
		__FILE__);
	return;
      }

      /* calc sample size */
      switch(fmt.wBitsPerSample) {
      case 8:
	sample_size = 1;
	break;
      case 16:
	sample_size = 2;
	break;
      default:
	fprintf(stderr,"(%s) ERROR: Unknown sample size %d!!!\n",
		__FILE__,fmt.wBitsPerSample);
	break;
      }

      /* alloc audio buffer */
      samples = fmt.nSamplesPerSec * TOTAL_SECS;
      buffer_size = sample_size * fmt.nChannels * samples;
      /* here we increase the buffer size if necessary */
      if (ars->GetFrameSize() > buffer_size) {
	buffer_size = ars->GetFrameSize();
      }
      buffer = (uint8_t *)malloc(buffer_size);
      if(buffer==0) {
	fprintf(stderr,"(%s) ERROR: No memory for buffer!!!\n",__FILE__);
	return;
      }

      /* send sync token */
      fflush(stdout);
      tc_pwrite(decode->fd_out, (uint8_t *)sync_str, sizeof(sync_str));
      
      /* sample server loop */
      while(!ars->Eof()) { 
	unsigned int ret_samples;
	unsigned int ret_size;

	/* read sample data */
	if (ars->ReadFrames((void*) buffer, buffer_size, samples,
			ret_samples, ret_size)) {
	  fprintf(stderr, "(%s) error calling ars->ReadFrames(..,)\n",
		  __FILE__);
	  return;
	}
	
	if(verbose_flag & TC_STATS) 
	  fprintf(stderr, "(%s) audio: requested: %u, got: %u samples",
		  __FILE__, samples, ret_samples);

	s_byte_read+=ret_size;
	/* by default decode->frame_limit[0]=0 and decode->frame_limit[1]=LONG_MAX so all bytes are decoded */
	if ((s_byte_read >= decode->frame_limit[0]) && (s_byte_read <= decode->frame_limit[1])) //added to enable the -C option of tcdecode
	{
	  if ( s_byte_read - ret_size <(unsigned int)decode->frame_limit[0])
	  {
	    if((unsigned int)tc_pwrite(decode->fd_out,buffer+(ret_size-(s_byte_read-decode->frame_limit[0])),(s_byte_read-decode->frame_limit[0]))!=(unsigned int)(s_byte_read-decode->frame_limit[0])) 
	      break;
	  }
	  else
	  {
	    if((unsigned int)tc_pwrite(decode->fd_out,buffer,ret_size)!=ret_size) 
	      break;
	  }
	}
	else if ((s_byte_read> decode->frame_limit[0]) && (s_byte_read - ret_size <=(unsigned int)decode->frame_limit[1]))
	{
	  if((unsigned int)tc_pwrite(decode->fd_out,buffer,(s_byte_read-decode->frame_limit[1]))!=(unsigned int)(s_byte_read-decode->frame_limit[1])) 
	    break;
	}
	else if (s_byte_read - ret_size >(unsigned int)decode->frame_limit[1])
	{
	  break;
	}
	
      }
      
      if(verbose_flag & TC_DEBUG) 
	fprintf(stderr, "(%s) audio: eof\n", __FILE__);

      /* cleanup */
      delete afile;
      if(buffer!=0)
	free(buffer);
    }
    return;
  }

#ifdef __cplusplus
}
#endif

#else  // HAVE_AVIFILE

#ifdef __cplusplus
extern "C" {
#endif

#include "transcode.h"
#include "ioaux.h"

void af6_decore(info_t *decode)
{
  fprintf(stderr, "(%s) no support for avifile library configured - exit.\n", __FILE__);
  return;
}
#ifdef __cplusplus
}
#endif
#endif
