/*
 *  export_yuv4mpeg.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
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

#include "transcode.h"
#include "libtc/libtc.h"
#include "libtcvideo/tcvideo.h"
#include "aud_aux.h"

#define MOD_NAME    "export_yuv4mpeg.so"
#define MOD_VERSION "v0.1.8 (2003-08-23)"
#define MOD_CODEC   "(video) YUV4MPEG2 | (audio) MPEG/AC3/PCM"

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_YUV|TC_CAP_PCM|TC_CAP_AC3|TC_CAP_AUD|TC_CAP_RGB;

#define MOD_PRE yuv4mpeg
#include "export_def.h"

#if defined(HAVE_MJPEGTOOLS_INC)
#include "yuv4mpeg.h"
#include "mpegconsts.h"
#else
#include "mjpegtools/yuv4mpeg.h"
#include "mjpegtools/mpegconsts.h"
#endif

#ifndef DAR_4_3
# define DAR_4_3      {   4, 3   }
# define DAR_16_9     {  16, 9   }
# define DAR_221_100  { 221, 100 }
# define SAR_UNKNOWN  {   0, 0   }
#endif

static const y4m_ratio_t dar_4_3 = DAR_4_3;
static const y4m_ratio_t dar_16_9 = DAR_16_9;
static const y4m_ratio_t dar_221_100 = DAR_221_100;
static const y4m_ratio_t sar_UNKNOWN = SAR_UNKNOWN;


static int fd, size;
static TCVHandle tcvhandle = 0;
static ImageFormat srcfmt;

static y4m_stream_info_t y4mstream;

/* ------------------------------------------------------------
 *
 * init codec
 *
 * ------------------------------------------------------------*/

MOD_init
{

    if(param->flag == TC_VIDEO) {

	if (vob->im_v_codec == CODEC_YUV) {
	    srcfmt = IMG_YUV_DEFAULT;
	} else if (vob->im_v_codec == CODEC_YUV422) {
	    srcfmt = IMG_YUV422P;
	} else if (vob->im_v_codec == CODEC_RGB) {
	    srcfmt = IMG_RGB_DEFAULT;
	} else {
	    tc_log_warn(MOD_NAME, "unsupported video format %d",
		    vob->im_v_codec);
	    return(TC_EXPORT_ERROR);
	}
	if (!(tcvhandle = tcv_init())) {
	    tc_log_warn(MOD_NAME, "image conversion init failed");
	    return(TC_EXPORT_ERROR);
	}

	return(0);
    }

    if(param->flag == TC_AUDIO) return(audio_init(vob, verbose_flag));

    // invalid flag
    return(TC_EXPORT_ERROR);
}

static void asrcode2asrratio(int asr, y4m_ratio_t *r)
{
    switch (asr) {
    case 2: *r = dar_4_3; break;
    case 3: *r = dar_16_9; break;
    case 4: *r = dar_221_100; break;
    case 1: r->n = 1; r->d = 1; break;
    case 0: default: *r = sar_UNKNOWN; break;
    }
}

/* ------------------------------------------------------------
 *
 * open outputfile
 *
 * ------------------------------------------------------------*/

MOD_open
{

  int asr;
  //char dar_tag[20];
  y4m_ratio_t framerate;
  y4m_ratio_t asr_rate;


  if(param->flag == TC_VIDEO) {

    // video

    //note: this is the real framerate of the raw stream
    framerate = (vob->ex_frc==0) ? mpeg_conform_framerate(vob->ex_fps):mpeg_framerate(vob->ex_frc);
    if (framerate.n==0 && framerate.d==0) {
	framerate.n=vob->ex_fps*1000;
	framerate.d=1000;
    }

    asr = (vob->ex_asr<0) ? vob->im_asr:vob->ex_asr;
    asrcode2asrratio(asr, &asr_rate);

    y4m_init_stream_info(&y4mstream);
    y4m_si_set_framerate(&y4mstream,framerate);
    y4m_si_set_interlace(&y4mstream,vob->encode_fields );
    y4m_si_set_sampleaspect(&y4mstream, y4m_guess_sar(vob->ex_v_width, vob->ex_v_height, asr_rate));
    /*
    tc_snprintf( dar_tag, 19, "XM2AR%03d", asr );
    y4m_xtag_add( y4m_si_xtags(&y4mstream), dar_tag );
    */
    y4m_si_set_height(&y4mstream,vob->ex_v_height);
    y4m_si_set_width(&y4mstream,vob->ex_v_width);

    size = vob->ex_v_width * vob->ex_v_height * 3/2;

    if((fd = open(vob->video_out_file, O_RDWR|O_CREAT|O_TRUNC,
		  S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH))<0) {
	  perror("open file");
	  return(TC_EXPORT_ERROR);
    }

    if( y4m_write_stream_header( fd, &y4mstream ) != Y4M_OK ){
      perror("write stream header");
      return(TC_EXPORT_ERROR);
    }

    return(0);
  }


  if(param->flag == TC_AUDIO) return(audio_open(vob, NULL));

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
    y4m_frame_info_t info;

    if(param->flag == TC_VIDEO) {
	vob_t *vob = tc_get_vob();

	if (!tcv_convert(tcvhandle, param->buffer, vob->ex_v_width,
			 vob->ex_v_height, srcfmt, IMG_YUV420P)) {
	    tc_log_warn(MOD_NAME, "image format conversion failed");
	    return(TC_EXPORT_ERROR);
	}

#ifdef USE_NEW_MJPEGTOOLS_CODE
	y4m_init_frame_info(&info);

	if(y4m_write_frame_header( fd, &y4mstream, &info ) != Y4M_OK )
	{
	    perror("write frame header");
	    return(TC_EXPORT_ERROR);
	}
#else
	y4m_init_frame_info(&info);

	if(y4m_write_frame_header( fd, &info) != Y4M_OK )
	{
	    perror("write frame header");
	    return(TC_EXPORT_ERROR);
	}
#endif

	//do not trust param->size
	if(tc_pwrite(fd, param->buffer, size) != size) {
	    perror("write frame");
	    return(TC_EXPORT_ERROR);
	}

	return(0);
    }

    if(param->flag == TC_AUDIO) return(audio_encode(param->buffer, param->size, NULL));

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

  if(param->flag == TC_VIDEO) {

      return(0);
  }
  if(param->flag == TC_AUDIO) return(audio_stop());

  return(TC_EXPORT_ERROR);
}

/* ------------------------------------------------------------
 *
 * close outputfiles
 *
 * ------------------------------------------------------------*/

MOD_close
{

  if(param->flag == TC_AUDIO) return(audio_close());
  if(param->flag == TC_VIDEO) {
    tcv_free(tcvhandle);
    close(fd);
    return(0);
  }
  return(TC_EXPORT_ERROR);

}


