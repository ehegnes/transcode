/*
 *  af6_decore.cpp
 *
 *  Copyright (C) Thomas Östreich - January 2002
 *  Updated by Christian Vogelgsang <Vogelgsang@informatik.uni-erlangen.de>
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
#include <string.h>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_AVIFILE

#if HAVE_AVIFILE_INCLUDES == 7
#include <avifile-0.7/avm_fourcc.h>
#include <avifile-0.7/avifile.h>
#include <avifile-0.7/image.h>
#include <avifile-0.7/aviplay.h>
#include <avifile-0.7/cpuinfo.h>
#include <avifile-0.7/utils.h>
#include <avifile-0.7/version.h>
#include <avifile-0.7/renderer.h>
#include <avifile-0.7/creators.h>
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
#include "ioaux.h"
  
  void af6_decore(info_t *ipipe)
  {
    int verbose_flag=TC_QUIET;
    static char *sync_str="Taf6"; 

    verbose_flag=ipipe->verbose;

    /* ----- AF6 VIDEO ----- */
    if(ipipe->select == TC_VIDEO) {

      IAviReadStream *vrs   = NULL;
      IAviReadFile   *vfile = NULL;
      unsigned int plane_size  = 0;
      ssize_t buffer_size = 0;
      int codec_error = 0;
      fourcc_t fcc = 0;
      int do_yuv = 0;

      /* unpacker stuff */
      int unpack = 0;
      int lumi_first = 0;
      char *pack_buffer = 0;
      char *packY=0,*packU=0,*packV=0;
      ssize_t pack_size = 0;
      long s_tot_frame,s_init_frame;

      /* create a new file reader */
      vfile = CreateIAviReadFile(ipipe->name);
      
      /* setup decoder */
      vrs = vfile->GetStream(0, AviStream::Video);
      if((vrs==0) || (vrs->StartStreaming()!=0)) {
	fprintf(stderr, "(%s) ERROR: unable to decode movie\n", __FILE__);
	ipipe->error=1;
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
      switch (ipipe->format) {
      case TC_CODEC_RGB:
	do_yuv = 0;
	fprintf(stderr, "(%s) input: RGB\n", __FILE__);
	buffer_size = plane_size * 3;
	break;
	
      case TC_CODEC_YV12:
	do_yuv = 1;
	// Decide on AVI target format
	// see http://www.webartz.com/fourcc/fccyuv.htm

	// YUV 4:2:0 planar formats are supported directly:
	// YV12 = Y plane + V subplane + U subplane
	if(caps & IVideoDecoder::CAP_YV12) {
	  fcc=fccYV12;
	  fprintf(stderr, "(%s) input: YVU 4:2:0 planar data\n", __FILE__);
	  buffer_size = (plane_size * 3)/2;
	} 
	// I420 = IYUV = Y plane + U subplane + V subplane
	else if(caps & (IVideoDecoder::CAP_I420|IVideoDecoder::CAP_IYUV)) {
	  fcc=fccI420;
	  fprintf(stderr, "(%s) input: YUV 4:2:0 planar data\n", __FILE__);
	  buffer_size = (plane_size * 3)/2;
	} 
    
	// YUV 4:2:2 packed formats are supported via conversion:
	// YUY2 = Y0 U0 Y1 V0  Y2 U2 Y3 V2  ...
	else if(caps & IVideoDecoder::CAP_YUY2) {
	  fcc=fccYUY2;
	  fprintf(stderr, "(%s) input: YUYV 4:2:2 packed data\n", __FILE__);
	  unpack=1;
	  lumi_first=1;
	}
	// UYVY = U0 Y0 V0 Y1  U2 Y2 V2 Y3  ...
	else if(caps & IVideoDecoder::CAP_UYVY) {
	  fcc=fccUYVY;
	  fprintf(stderr, "(%s) input: UYVY 4:2:2 packed data\n", __FILE__);
	  unpack=1;
	  lumi_first=0;
	}
	// YVYU = Y0 V0 Y1 U0  Y2 V2 Y3 U2  ...
	else if(caps & IVideoDecoder::CAP_YVYU) {
	  fcc=fccYVYU;
	  fprintf(stderr, "(%s) input: YVYU 4:2:2 packed data\n", __FILE__);
	  unpack=1;
	  lumi_first=1;
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
	ipipe->error=1;
	return;
      }

      /* YUV special setup */
      if(do_yuv) {
	vrs->GetDecoder()->SetDestFmt(BitmapInfo::BitCount(fcc), fcc);
      } else /* RGB */ {
	vrs->GetDecoder()->SetDestFmt(24);
      }

      /* prepare unpacker */
      if(unpack) {
	pack_size = (plane_size * 3)/2; /* target is 4:2:0 */
	pack_buffer = (char *)malloc(pack_size);
	if(pack_buffer==0) {
	  fprintf(stderr,"(%s) ERROR: No memory for buffer!!!\n",__FILE__);
	  ipipe->error=1;
	  return;
	}
	packY = pack_buffer;
	packU = pack_buffer + plane_size;
	packV = pack_buffer + plane_size + (plane_size >> 2);
      }
      
      s_tot_frame=vrs->GetLength();	//get the total number of the frames
      if (ipipe->frame_limit[1] < s_tot_frame) //added to enable the -C option of tcdecode
      {
             s_tot_frame=ipipe->frame_limit[1];
      }
      
      /* start at the beginning */
      vrs->SeekToKeyFrame(0);

      /* send sync token */
      fflush(stdout);
      p_write(ipipe->fd_out, sync_str, sizeof(sync_str));

      /* frame serve loop */
      /* by default ipipe->frame_limit[0]=0 and ipipe->frame_limit[1]=LONG_MAX so all frames are decoded */
      for(s_init_frame=0;(s_init_frame<=s_tot_frame)||(!vrs->Eof());s_init_frame++) { 
	/* fetch a frame */
	vrs->ReadFrame();
	CImage *imsrc = vrs->GetFrame();
	char *buf = (char *)imsrc->Data();

	if (s_init_frame >= ipipe->frame_limit[0]) //added to enable the -C option of tcdecode
	{
		if(unpack) {
		  /* unpack and write unpacked data */
		  char *Y = packY;
		  char *U = packU;
		  char *V = packV;
		  int x,y;
		  int subw = bh.biWidth >> 1;
		  int subh = bh.biHeight >> 1;

		  /* 4:2:2 packed -> 4:2:0 planar -- SLOW! MMX anyone???? :) */
		  if(lumi_first) {

		    for(y=0;y<subh;y++) {
		      for(x=0;x<subw;x++) {
			*(Y++) = *(buf++);
			*(U++) = *(buf++);
			*(Y++) = *(buf++);
			*(V++) = *(buf++);
		      }
		      for(x=0;x<subw;x++) {
			*(Y++) = *(buf++);
			buf++;
			*(Y++) = *(buf++);
			buf++;
		      }
		    }
		  } else {
		    for(y=0;y<subh;y++) {
		      for(x=0;x<subw;x++) {
			*(U++) = *(buf++);
			*(Y++) = *(buf++);
			*(V++) = *(buf++);
			*(Y++) = *(buf++);
		      }
		      for(x=0;x<subw;x++) {
			buf++;
			*(Y++) = *(buf++);
			buf++;
			*(Y++) = *(buf++);
		      }
		    }
		  }
		  /* write unpacked frame */
		  if(p_write(ipipe->fd_out, pack_buffer, pack_size)!= pack_size) {
		    fprintf(stderr,"(%s) ERROR: Pipe write error!\n",__FILE__);
		    ipipe->error=1;
		    break;
		  }
		} else {
		  /* directly write raw frame */
		  if(p_write(ipipe->fd_out, buf, buffer_size)!= buffer_size) {
		    fprintf(stderr,"(%s) ERROR: Pipe write error!\n",__FILE__);
		    ipipe->error=1;
		    break;
		  }
		}
	}
      }
      
      /* cleanup */
      delete vfile;
      if(pack_buffer!=0)
	free(pack_buffer);
    }

    /* ----- AF6 AUDIO ----- */
    if(ipipe->select == TC_AUDIO) {
	
      IAviReadStream *ars = NULL;
      IAviReadFile   *afile= NULL;

      const unsigned int TOTAL_SECS = 2;
      unsigned int buffer_size;
      unsigned int sample_size = 0;
      unsigned int samples;
      char *buffer;
      long s_byte_read=0;

      /* create AVI audio file reader */
      afile = CreateIAviReadFile(ipipe->name);
      
      /* setup decoder */
      ars = afile->GetStream(0, AviStream::Audio);
      if((ars==0)||(ars->StartStreaming()!=0)) {
	fprintf(stderr, "(%s) ERROR: invalid audio stream!!!\n", __FILE__);
	ipipe->error=1;
	return;
      }

      /* query wave format */
      WAVEFORMATEX wvFmt;
      if (ars->GetAudioFormatInfo(&wvFmt, 0) != 0) {
	fprintf(stderr, "(%s) ERROR: can't fetch audio format!!!\n", __FILE__);
	ipipe->error=1;
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
	ipipe->error=1;
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
      buffer = (char *)malloc(buffer_size);
      if(buffer==0) {
	fprintf(stderr,"(%s) ERROR: No memory for buffer!!!\n",__FILE__);
	ipipe->error=1;
	return;
      }

      /* send sync token */
      fflush(stdout);
      p_write(ipipe->fd_out, sync_str, sizeof(sync_str));
      
      /* sample server loop */
      while(!ars->Eof()) { 
	unsigned int ret_samples;
	unsigned int ret_size;

	/* read sample data */
	if (ars->ReadFrames((void*) buffer, buffer_size, samples,
			ret_samples, ret_size)) {
	  fprintf(stderr, "(%s) error calling ars->ReadFrames(..,)\n",
		  __FILE__);
	  ipipe->error=1;
	  return;
	}
	
	if(verbose_flag & TC_STATS) 
	  fprintf(stderr, "(%s) audio: requested: %u, got: %u samples",
		  __FILE__, samples, ret_samples);

	s_byte_read+=ret_size;
	/* by default ipipe->frame_limit[0]=0 and ipipe->frame_limit[1]=LONG_MAX so all bytes are decoded */
	if ((s_byte_read >= ipipe->frame_limit[0]) && (s_byte_read <= ipipe->frame_limit[1])) //added to enable the -C option of tcdecode
	{
		if (s_byte_read - ret_size <ipipe->frame_limit[0])
		{
			if((unsigned int)p_write(ipipe->fd_out,buffer+(ret_size-(s_byte_read-ipipe->frame_limit[0])),(s_byte_read-ipipe->frame_limit[0]))!=(s_byte_read-ipipe->frame_limit[0])) 
			{
				ipipe->error=1;
			  	break;
			}
		}
		else
		{
			if((unsigned int)p_write(ipipe->fd_out,buffer,ret_size)!=ret_size) 
			{
			  ipipe->error=1;
			  break;
			}
		}
	}
	else if ((s_byte_read> ipipe->frame_limit[0]) && (s_byte_read - ret_size <=ipipe->frame_limit[1]))
	{
		if((unsigned int)p_write(ipipe->fd_out,buffer,(s_byte_read-ipipe->frame_limit[1]))!=(s_byte_read-ipipe->frame_limit[1])) 
		{
		  	ipipe->error=1;
		 	break;
		}
	}
	else if (s_byte_read - ret_size >ipipe->frame_limit[1])
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

#else

#ifdef __cplusplus
extern "C" {
#endif

#include "transcode.h"
#include "ioaux.h"

void af6_decore(info_t *ipipe)
{
  fprintf(stderr, "(%s) no support for avifile library configured - exit.\n", __FILE__);
  ipipe->error=1;
  return;
}
#ifdef __cplusplus
}
#endif
#endif
