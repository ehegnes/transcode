/*
 *  tcdemux.c
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

#include "xio.h"
#include "ioaux.h"
#include "tc.h"
#include "demuxer.h"

#define EXE "tcdemux"

#define MAX_BUF     1024

void tcdemux_pass_through(info_t *ipipe, int *pass);
int verbose=TC_QUIET;

/* ------------------------------------------------------------ 
 *
 * auxiliary routines
 *
 * ------------------------------------------------------------*/

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

  fprintf(stderr,"\nUsage: %s [options]\n", EXE);

  fprintf(stderr,"\t-i name          input file name [stdin]\n");
  fprintf(stderr,"\t-t magic         input file type [autodetect]\n");
  fprintf(stderr,"\t-x codec         process only packs with codec payload [all]\n");
  fprintf(stderr,"\t-S unit[,s1-s2]  presentation unit[,s1-s2] sequences [0,all]\n");
  fprintf(stderr,"\t-a ach[,vch]     extract audio[,video] track [0,0]\n");
  fprintf(stderr,"\t-s 0xnn          sync with private substream id 0xnn [off]\n");
  fprintf(stderr,"\t-M mode          demuxer PES A-V sync mode (0=off|1=PTS only|2=full) [1]\n");
  fprintf(stderr,"\t-O               do not skip initial sequence\n");
  fprintf(stderr,"\t-P name          write synchronization data to file\n");
  fprintf(stderr,"\t-W               write navigation data to stdout\n");
  fprintf(stderr,"\t-f fps           frame rate [%.3f]\n", PAL_FPS);
  fprintf(stderr,"\t-d mode          verbosity mode\n");
  fprintf(stderr,"\t-A n[,m[...]]    pass-through packet payload id\n");
  fprintf(stderr,"\t-H               sync hard to supplied fps (no smooth drop)\n");
  fprintf(stderr,"\t-v               print version\n");

  exit(status);
  
}


/* ------------------------------------------------------------ 
 *
 * demuxer thread frontend 
 *
 * ------------------------------------------------------------*/

int main(int argc, char *argv[])
{

    info_t ipipe;

    int user=0, n, demux_mode=TC_DEMUX_SEQ_ADJUST;

    int pass_mode=0, pass[5];

    double fps=PAL_FPS;
    
    long 
	stream_stype = TC_STYPE_UNKNOWN, 
	stream_codec = TC_CODEC_UNKNOWN, 
	stream_magic = TC_MAGIC_UNKNOWN;

    int ch;
    char *magic="", *codec=NULL, *name=NULL;

    int keep_initial_seq=0;
    int hard_fps_flag=0;

    char *logfile=SYNC_LOGFILE;

    int pack_sl=PACKAGE_ALL;

    int a_track=0, v_track=0, subid=0x80;
    
    //defaults:
    int unit_seek=0, resync_seq1=0, resync_seq2=INT_MAX;

    ac_init(AC_ALL);

    //proper initialization
    memset(&ipipe, 0, sizeof(info_t));

    for(n=0; n<5; ++n) pass[n]=0;
  
    while ((ch = getopt(argc, argv, "A:a:d:x:i:vt:S:M:f:P:WHs:O?h")) != -1) {
	
      switch (ch) {
	
      case 'i': 
	
	if(optarg[0]=='-') usage(EXIT_FAILURE);
	name = optarg;
      
	break;

      case 'O': 
	
	keep_initial_seq = 1;
      
	break;

      case 'P': 
	
	if(optarg[0]=='-') usage(EXIT_FAILURE);
	logfile = optarg;
	break;

      case 'S': 
      
	if(optarg[0]=='-') usage(EXIT_FAILURE);

	if((n = sscanf(optarg,"%d,%d-%d", &unit_seek, &resync_seq1, &resync_seq2))<0) {
	  fprintf(stderr, "invalid parameter for option -S\n");
	  exit(1);
      }

      if(unit_seek<0) {
	fprintf(stderr, "error: invalid unit parameter for option -S\n");
	usage(EXIT_FAILURE);
	exit(1);
      }

      if(resync_seq1<0 || resync_seq2<0 || resync_seq1>=resync_seq2) {
	  fprintf(stderr, "error: invalid sequence parameter for option -S\n");
	  usage(EXIT_FAILURE);
	  exit(1);
      }

      break;
   
	
      case 'd': 
	
	if(optarg[0]=='-') usage(EXIT_FAILURE);
	verbose = atoi(optarg);
	
	break;

	
      case 'f': 
	
	if(optarg[0]=='-') usage(EXIT_FAILURE);
	fps = atof(optarg);
	
	break;

      case 'W': 
	demux_mode = TC_DEMUX_SEQ_LIST;
	logfile=NULL;
	break;

      case 'H':
	hard_fps_flag = 1;
	break;
	
      case 'x': 
	
	if(optarg[0]=='-') usage(EXIT_FAILURE);
	codec = optarg;

	if(strcmp(codec,"ac3")==0) {
	  pack_sl=PACKAGE_AUDIO_AC3;
	  stream_codec = TC_CODEC_AC3;
	}
	
	if(strcmp(codec,"mpeg2")==0) {
	  pack_sl=PACKAGE_VIDEO;
	  stream_codec = TC_CODEC_MPEG2;
	} 
	
	if(strcmp(codec,"mp3")==0) {
	  pack_sl=PACKAGE_AUDIO_MP3;
	  stream_codec = TC_CODEC_MP3;
	} 
	
	if(strcmp(codec,"pcm")==0) {
	  pack_sl=PACKAGE_AUDIO_PCM;
	  stream_codec = TC_CODEC_PCM;
	} 
	
	if(strcmp(codec,"ps1")==0) {
	  pack_sl=PACKAGE_SUBTITLE;
	  stream_codec = TC_CODEC_SUB;
	} 
	
	break;
	
      case 't': 
	
	if(optarg[0]=='-') usage(EXIT_FAILURE);
	magic = optarg;
	user=1;
	
	break;

      case 's': 
	
	if(optarg[0]=='-') usage(EXIT_FAILURE);
	subid = strtol(optarg, NULL, 16);
	break;

      case 'A': 
	
	if(optarg[0]=='-') usage(EXIT_FAILURE);
	
	if ((n = sscanf(optarg,"%x,%x,%x,%x,%x", &pass[0], &pass[1], &pass[2], &pass[3], &pass[4]))<=0) {
	    fprintf(stderr, "invalid parameter for option -A\n");
	    exit(1);
	}

	pass_mode=1;
	
	break;

      case 'M': 
	
	if(optarg[0]=='-') usage(EXIT_FAILURE);
	demux_mode = atoi(optarg);
	
	if(demux_mode==TC_DEMUX_OFF) verbose=TC_QUIET;
	
	if(demux_mode<0 || demux_mode>TC_DEMUX_MAX_OPTS) {
	  fprintf(stderr, "invalid parameter for option -M\n");
	  exit(1);
	}
	
	break;
	
      case 'a': 
	
	if(optarg[0]=='-') usage(EXIT_FAILURE);
	
	if ((n = sscanf(optarg,"%d,%d", &a_track, &v_track))<=0) {
	  fprintf(stderr, "invalid parameter for option -a\n");
	  exit(1);
	}
	
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

    // no autodetection yet
    
    if(argc==1) {
      usage(EXIT_FAILURE);
    }
    
    // do not try to mess with the stream
    if(stream_stype!=TC_STYPE_STDIN) {
      
      if(tc_file_check(name)) exit(1);
      
      if((ipipe.fd_in = xio_open(name, O_RDONLY))<0) {
	perror("open file");
	exit(1);
      } 
      
      // try to find out the filetype
      
      stream_magic=fileinfo(ipipe.fd_in, 0);
      
      if(verbose) fprintf(stderr, "[%s] (pid=%d) %s\n", EXE, getpid(), filetype(stream_magic));
      
    } else ipipe.fd_in = STDIN_FILENO;
    
    // fill out defaults for info structure
    ipipe.fd_out = STDOUT_FILENO;
    
    ipipe.magic = stream_magic;
    ipipe.stype = stream_stype;
    ipipe.codec = stream_codec;

    ipipe.verbose = verbose;

    ipipe.ps_unit = unit_seek;
    ipipe.ps_seq1 = resync_seq1; 
    ipipe.ps_seq2 = resync_seq2; 

    ipipe.demux  = demux_mode;
    ipipe.select = pack_sl;
    ipipe.keep_seq = keep_initial_seq;
    ipipe.subid = subid;
    ipipe.fps = fps;

    ipipe.hard_fps_flag = hard_fps_flag;
    ipipe.track = a_track;
    ipipe.name  = logfile;

    //FIXME: video defaults to 0
   
    /* ------------------------------------------------------------ 
     *
     * main processing mode
     *
     * ------------------------------------------------------------*/

    if(pass_mode) 
	tcdemux_pass_through(&ipipe, pass);
    else 
	tcdemux_thread(&ipipe);
    
    return(0);
}

#include "libxio/static_xio.h"
