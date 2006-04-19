/*
 *  tcprobe.c
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

#include "transcode.h"
#include "tcinfo.h"

#include <math.h>
#include "libtc/xio.h"
#include "ioaux.h"
#include "tc.h"
#include "demuxer.h"
#include "dvd_reader.h"

#define EXE "tcprobe"

#define MAX_BUF 1024

int verbose=TC_INFO;

int bitrate=ABITRATE;
int binary_dump=0;
int mplayer_dump=0;

void import_exit(int code)
{
  if(verbose & TC_DEBUG) import_info(code, EXE);
  exit(code);
}


/* ------------------------------------------------------------
 *
 * print a usage/version message
 *
 * ------------------------------------------------------------*/



void version(void)
{
    /* print id string to stderr */
    fprintf(stderr, "%s (%s v%s) (C) 2001-2003 Thomas Oestreich\n",
                    EXE, PACKAGE, VERSION);
}


static void usage(int status)
{
  version();

  fprintf(stderr,"\nUsage: %s [options] [-]\n", EXE);
  fprintf(stderr,"\t -i name        input file/directory/device/host name [stdin]\n");
  fprintf(stderr,"\t -B             binary output to stdout (used by transcode) [off]\n");
  fprintf(stderr,"\t -M             use EXPERIMENTAL mplayer probe [off]\n");
  fprintf(stderr,"\t -H n           probe n MB of stream [1]\n");
  fprintf(stderr,"\t -s n           skip first n bytes of stream [0]\n");
  fprintf(stderr,"\t -T title       probe for DVD title [off]\n");
  fprintf(stderr,"\t -b bitrate     audio encoder bitrate kBits/s [%d]\n", ABITRATE);
  fprintf(stderr,"\t -f seekfile    seek/index file [off]\n");
  fprintf(stderr,"\t -d verbosity   verbosity mode [1]\n");
  fprintf(stderr,"\t -v             print version\n");

  exit(status);
}


/* ------------------------------------------------------------
 *
 * universal extract thread frontend
 *
 * ------------------------------------------------------------*/

int main(int argc, char *argv[])
{

    info_t ipipe;

    long
	stream_stype = TC_STYPE_UNKNOWN,
	stream_magic = TC_MAGIC_UNKNOWN,
	stream_codec = TC_CODEC_UNKNOWN;

    int ch, i, n, cc=0, probe_factor=1, skip=0;


    int dvd_title=1, dvd_title_set=0;
    char *name=NULL;
    char *nav_seek_file=NULL;

    char *c_ptr=NULL, *c_new="(*)", *c_old="";

    long frame_time=0;

    pid_t pid=getpid();

    //proper initialization
    memset(&ipipe, 0, sizeof(info_t));

    while ((ch = getopt(argc, argv, "i:vBMd:T:f:b:s:H:?h")) != -1) {

	switch (ch) {

	case 'b':

	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  bitrate = atoi(optarg);

	  if(bitrate < 0) {
	    tc_log_error(EXE, "invalid bitrate for option -b");
	    exit(1);
	  }
	  break;


	case 'i':

	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  name = optarg;

	  break;

	case 'f':

	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  nav_seek_file = optarg;

	  break;

	case 'd':

	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  verbose = atoi(optarg);

	  break;

	case 's':

	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  skip = atoi(optarg);

	  break;

	case 'H':

	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  probe_factor = atoi(optarg);

	  if(probe_factor < 0) {
	    tc_log_error(EXE, "invalid parameter for option -H");
	    exit(1);
	  }
	  break;

	case 'B':

	  binary_dump = 1;

	  tc_pwrite(STDOUT_FILENO, (uint8_t *) &pid, sizeof(pid_t));

	  break;

    case 'M':

	  mplayer_dump = 1;

	  break;

	case 'T':

	  if(optarg[0]=='-') usage(EXIT_FAILURE);
	  dvd_title = atoi(optarg);
	  dvd_title_set = 1;

	  break;

	case 'v':

	  version();
	  exit(0);
	  break;

	case 'h':
	  usage(EXIT_SUCCESS);
	default:
	    usage(EXIT_FAILURE);
	}
    }

    /* ------------------------------------------------------------
     *
     * fill out defaults for info structure
     *
     * ------------------------------------------------------------*/

    // assume defaults
    if(name==NULL) stream_stype=TC_STYPE_STDIN;

    if(optind < argc) {
	if(strcmp(argv[optind],"-")!=0) usage(EXIT_FAILURE);
	stream_stype=TC_STYPE_STDIN;
    }

    // need at least a file name
    if(argc==1) usage(EXIT_FAILURE);

    ipipe.verbose = verbose;

    //--------------------------------------
    //
    //  scan for valid source:
    //
    //  (I)   DVD device/image
    //  (II)  directory
    //  (III) regular file
    //  (IV)  network host
    //
    //--------------------------------------

    /* do not try to mess with the stream */
    if (stream_stype != TC_STYPE_STDIN) {
      cc = tc_probe_path(name);
      switch(cc) {
        case TC_PROBE_PATH_INVALID:	/* non-existent source */
          exit(1);

        case TC_PROBE_PATH_FILE:	/* regular file */
          if (mplayer_dump) {
            stream_magic = TC_MAGIC_MPLAYER;
            ipipe.seek_allowed = 0;
          } else if (!dvd_title_set || (dvd_verify(name) < 0)) {
            if ((ipipe.fd_in = xio_open(name, O_RDONLY)) < 0) {
              perror("file open");
              return(-1);
            }
            stream_magic = fileinfo(ipipe.fd_in, skip);
            ipipe.seek_allowed = 1;
          } else {                        /* DVD image */
            stream_magic = TC_MAGIC_DVD;
            ipipe.seek_allowed = 0;
          }
          break;

        case TC_PROBE_PATH_RELDIR:        /* relative path to directory */
          if (fileinfo_dir(name, &ipipe.fd_in, &stream_magic) < 0)
            exit(1);
          ipipe.seek_allowed = 0;
          break;

        case TC_PROBE_PATH_ABSPATH:       /* absolute path */
          ipipe.seek_allowed = 0;
          if (dvd_verify(name) < 0) {
            /* normal directory - no DVD copy */
            if (fileinfo_dir(name, &ipipe.fd_in, &stream_magic) < 0)
              exit(1);
          } else {
            stream_magic = TC_MAGIC_DVD;
          }
          break;

        case TC_PROBE_PATH_NET:		/* network host */
          ipipe.seek_allowed = 0;
          stream_magic = TC_MAGIC_SOCKET;
          break;

        case TC_PROBE_PATH_BKTR:	/* bktr device */
          ipipe.seek_allowed = 0;
          stream_magic = TC_MAGIC_BKTR_VIDEO;
          break;

        case TC_PROBE_PATH_SUNAU:	/* sunau device */
          ipipe.seek_allowed = 0;
          stream_magic = TC_MAGIC_SUNAU_AUDIO;
          break;

        case TC_PROBE_PATH_OSS:	/* OSS device */
          ipipe.seek_allowed = 0;
          stream_magic = TC_MAGIC_OSS_AUDIO;
          break;

        case TC_PROBE_PATH_V4L_VIDEO:	/* v4l video device */
          ipipe.seek_allowed = 0;
          stream_magic = TC_MAGIC_V4L_VIDEO;
          break;

        case TC_PROBE_PATH_V4L_AUDIO:	/* v4l audio device */
          ipipe.seek_allowed = 0;
          stream_magic = TC_MAGIC_V4L_AUDIO;
          break;

        default:
          exit(1);

      } /* probe_path */

    } else {
      ipipe.fd_in = STDIN_FILENO;
      ipipe.seek_allowed = 0;
      stream_magic = streaminfo(ipipe.fd_in);
    }

    /* fill out defaults for info structure */
    ipipe.fd_out = STDOUT_FILENO;

    ipipe.magic = stream_magic;
    ipipe.stype = stream_stype;
    ipipe.codec = stream_codec;
    ipipe.name = name;
    ipipe.dvd_title = dvd_title;
    ipipe.factor = probe_factor;
    ipipe.nav_seek_file = nav_seek_file;

    /* ------------------------------------------------------------
     *
     * codec specific section
     *
     * note: user provided values overwrite autodetection!
     *
     * ------------------------------------------------------------*/


    if(verbose) tc_log_msg(EXE, "%s\n", filetype(stream_magic));

    tcprobe_thread(&ipipe);

    switch(ipipe.error) {

    case 1:
      if(verbose) tc_log_warn(EXE, "failed to probe source");
      return(1);
      break;

    case 2:
      if(verbose) tc_log_warn(EXE, "filetype/codec not yet supported by '%s'", PACKAGE);
      return(1);
      break;
    }

    //-------------------------------------
    //
    // transcode only support mode:
    //
    // write source info structure to stdout
    //
    //-------------------------------------

    if(binary_dump) {
      tc_pwrite(ipipe.fd_out, (uint8_t *) ipipe.probe_info, sizeof(ProbeInfo));
      exit(0);
    }

    //-------------------------------------------
    //
    // user mode:
    //
    // recommended transcode command line options:
    //
    //-------------------------------------------

    printf("[%s] summary for %s, (*) = not default, 0 = not detected\n", EXE, ((stream_magic==TC_STYPE_STDIN)?"-" : ipipe.name));

    // frame size

    c_ptr=c_old;
    if(ipipe.probe_info->width != PAL_W || ipipe.probe_info->height != PAL_H) c_ptr=c_new;

    if(ipipe.probe_info->width==0 || ipipe.probe_info->height==0) goto audio;

    printf("%18s %s %dx%d [%dx%d] %s\n", "import frame size:", "-g", ipipe.probe_info->width, ipipe.probe_info->height, PAL_W, PAL_H, c_ptr);

    // aspect ratio

    c_ptr=c_old;
    if(ipipe.probe_info->asr != 1) c_ptr=c_new;

    switch(ipipe.probe_info->asr) {

    case 1:
      printf("%18s %s %s\n", "aspect ratio:", "1:1", c_ptr);
      break;

    case  2:
    case  8:
    case 12:
      printf("%18s %s %s\n", "aspect ratio:", "4:3", c_ptr);
      break;

    case 3:
      printf("%18s %s %s\n", "aspect ratio:", "16:9", c_ptr);
      break;

    case 4:
      printf("%18s %s %s\n", "aspect ratio:", "2.21:1", c_ptr);
      break;
    }

    // frame rate

    c_ptr=c_old;
    if(ipipe.probe_info->fps != PAL_FPS) c_ptr=c_new;

    frame_time = (ipipe.probe_info->fps != 0) ?
                 (long)(1. / ipipe.probe_info->fps * 1000) : 0;

    printf("%18s %s %.3f [%.3f] frc=%d %s\n", "frame rate:", "-f",
           ipipe.probe_info->fps, PAL_FPS, ipipe.probe_info->frc, c_ptr);

    if (ipipe.probe_info->pts_start && ipipe.probe_info->bitrate)
        printf("%18s PTS=%.4f, frame_time=%ld ms, bitrate=%ld kbps\n", "",
               ipipe.probe_info->pts_start, frame_time,
               ipipe.probe_info->bitrate);
    else
        if (ipipe.probe_info->pts_start)
            printf("%18s PTS=%.4f, frame_time=%ld ms\n", "",
                   ipipe.probe_info->pts_start, frame_time);

 audio:

    // audio parameter

    for(n=0; n<TC_MAX_AUD_TRACKS; ++n) {

      int D_arg=0, D_arg_ms=0;
      double pts_diff=0.;

      if(ipipe.probe_info->track[n].format != 0 &&
         ipipe.probe_info->track[n].chan > 0) {

	c_ptr=c_old;
	if (ipipe.probe_info->track[n].samplerate != RATE ||
            ipipe.probe_info->track[n].chan != CHANNELS  ||
            ipipe.probe_info->track[n].bits != BITS ||
            ipipe.probe_info->track[n].format != CODEC_AC3)
            c_ptr=c_new;

	printf("%18s -a %d [0] -e %d,%d,%d [%d,%d,%d] -n 0x%x [0x%x] %s\n",
               "audio track:",
               ipipe.probe_info->track[n].tid,
               ipipe.probe_info->track[n].samplerate,
               ipipe.probe_info->track[n].bits,
               ipipe.probe_info->track[n].chan,
               RATE, BITS, CHANNELS,
               ipipe.probe_info->track[n].format,
               CODEC_AC3, c_ptr);

	if(ipipe.probe_info->track[n].pts_start
	   && ipipe.probe_info->track[n].bitrate)

	  printf("%18s PTS=%.4f, bitrate=%d kbps\n", " ",
                 ipipe.probe_info->track[n].pts_start,
                 ipipe.probe_info->track[n].bitrate);

	if((ipipe.probe_info->track[n].pts_start) &&
	   (ipipe.probe_info->track[n].bitrate == 0))
	  printf("%18s PTS=%.4f\n", " ",
                 ipipe.probe_info->track[n].pts_start);

	if ((ipipe.probe_info->track[n].pts_start == 0) &&
	    (ipipe.probe_info->track[n].bitrate))
	  printf("%18s bitrate=%d kbps\n", " ",
                 ipipe.probe_info->track[n].bitrate);

	if (ipipe.probe_info->pts_start>0 &&
	    ipipe.probe_info->track[n].pts_start>0 &&
	    ipipe.probe_info->fps!=0) {
	  pts_diff = ipipe.probe_info->pts_start - ipipe.probe_info->track[n].pts_start;
	  D_arg = (int) (pts_diff * ipipe.probe_info->fps);
	  D_arg_ms = (int) ((pts_diff - D_arg/ipipe.probe_info->fps)*1000);

	  printf("%18s -D %d --av_fine_ms %d (frames & ms) [0] [0]\n", " ", D_arg, D_arg_ms);
	}
      }
    }

    // subtitles

    i=0;
    for(n=0; n<TC_MAX_AUD_TRACKS; ++n) {
      if(ipipe.probe_info->track[n].attribute & PACKAGE_SUBTITLE) ++i;
    }

    if(i) printf("detected (%d) subtitle(s)\n", i);

    // P-units

    if(ipipe.probe_info->unit_cnt)
      printf("detected (%d) presentation unit(s) (SCR reset)\n", ipipe.probe_info->unit_cnt+1);

    // no audio

    if(ipipe.probe_info->num_tracks==0)
      printf("%18s %s\n", "no audio track:", "use \"null\" import module for audio");


    //encoder bitrate infos (DVD only)

    if(stream_magic == TC_MAGIC_DVD_PAL || stream_magic == TC_MAGIC_DVD_NTSC ||stream_magic == TC_MAGIC_DVD) {
      enc_bitrate((long)ceil(ipipe.probe_info->fps*ipipe.probe_info->time), ipipe.probe_info->fps, bitrate, EXE, 0);
    } else {

      if(ipipe.probe_info->frames > 0) {
	unsigned long dur_ms;
	unsigned int dur_h, dur_min, dur_s;
	if(ipipe.probe_info->fps < 0.100)
	  dur_ms=(long)ipipe.probe_info->frames*frame_time;
	else
	  dur_ms=(long)((float)ipipe.probe_info->frames*1000/ipipe.probe_info->fps);
	dur_h=dur_ms/3600000;
	dur_min=(dur_ms%=3600000)/60000;
	dur_s=(dur_ms%=60000)/1000;
	dur_ms%=1000;
	printf("%18s %ld frames, frame_time=%ld msec, duration=%u:%02u:%02u.%03lu\n", "length:",
	  ipipe.probe_info->frames, frame_time, dur_h, dur_min, dur_s, dur_ms);
      }
    }

    if(ipipe.fd_in != STDIN_FILENO) xio_close(ipipe.fd_in);

    return(0);
}

#include "libtc/static_xio.h"
