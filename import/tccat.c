/*
 *  tccat.c
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
#include "libtc/libtc.h"
#include "libtc/iodir.h"
#include "libtc/xio.h"
#include "ioaux.h"
#include "tc.h"
#include "dvd_reader.h"

#include <sys/types.h>

#ifdef HAVE_LIBDVDREAD
#ifdef HAVE_LIBDVDREAD_INC
#include <dvdread/dvd_reader.h>
#else
#include <dvd_reader.h>
#endif
#else
#include "dvd_reader.h"
#endif

#define EXE "tccat"

#define MAX_BUF 1024
char buf[MAX_BUF];

int verbose=TC_INFO;

void import_exit(int code)
{
  if (verbose & TC_DEBUG)
    tc_log_msg(EXE, "(pid=%d) exit (code %d)", (int) getpid(), code);
  exit(code);
}


/* ------------------------------------------------------------
 *
 * source extract thread
 *
 * ------------------------------------------------------------*/

#define IO_BUF_SIZE 1024
#define DVD_VIDEO_LB_LEN 2048

static void tccat_thread(info_t *ipipe)
{

  const char *name=NULL;
  int found=0, itype=TC_MAGIC_UNKNOWN, type=TC_MAGIC_UNKNOWN;

  int verbose_flag;
  int vob_offset;

  info_t ipipe_avi;
  TCDirList tcdir;

  verbose_flag = ipipe->verbose;
  vob_offset = ipipe->vob_offset;

  switch(ipipe->magic) {

  case TC_MAGIC_DVD_PAL:
  case TC_MAGIC_DVD_NTSC:

    if(verbose_flag & TC_DEBUG)
      tc_log_msg(__FILE__, "%s", filetype(ipipe->magic));

    dvd_read(ipipe->dvd_title, ipipe->dvd_chapter, ipipe->dvd_angle);
    break;

  case TC_MAGIC_TS:

    ts_read(ipipe->fd_in, ipipe->fd_out, ipipe->ts_pid);
    break;

  case TC_MAGIC_RAW:

    if(verbose_flag & TC_DEBUG)
      tc_log_msg(__FILE__, "%s", filetype(ipipe->magic));

    if(vob_offset>0) {

      off_t off;

      //get filesize in units of packs (2kB)
      off = lseek(ipipe->fd_in, vob_offset * (off_t) DVD_VIDEO_LB_LEN,
		  SEEK_SET);

      if( off != ( vob_offset * (off_t) DVD_VIDEO_LB_LEN ) ) {
	tc_log_warn(__FILE__, "unable to seek to block %d", vob_offset); //drop this chunk/file
	goto vob_skip2;
      }
    }

    tc_preadwrite(ipipe->fd_in, ipipe->fd_out);

  vob_skip2:
    break;

  case TC_MAGIC_DIR:

    //PASS 1: check file type - file order not important

    if(tc_dirlist_open(&tcdir, ipipe->name, 0)<0) {
      tc_log_error(__FILE__, "unable to open dirlist \"%s\"", ipipe->name);
      exit(1);
    } else if(verbose_flag & TC_DEBUG)
      tc_log_msg(__FILE__, "scanning dirlist \"%s\"", ipipe->name);

    while((name=tc_dirlist_scan(&tcdir))!=NULL) {

      if((ipipe->fd_in = open(name, O_RDONLY))<0) {
	tc_log_perror(__FILE__, "file open");
	exit(1);
      }

      //first valid magic must be the same for all
      //files to follow


      itype = fileinfo(ipipe->fd_in, 0);

      close(ipipe->fd_in);

      if(itype == TC_MAGIC_UNKNOWN || itype == TC_MAGIC_PIPE ||
	 itype == TC_MAGIC_ERROR) {

	tc_log_error(__FILE__,"this version of transcode supports only");
	tc_log_error(__FILE__,"directories containing files of identical file type.");
	tc_log_error(__FILE__,"Please clean up dirlist %s and restart.", ipipe->name);

	tc_log_error(__FILE__,"file %s with filetype %s is invalid for dirlist mode.", name, filetype(itype));

	exit(1);
      } // error


      switch(itype) {

	// supported file types
      case TC_MAGIC_VOB:
      case TC_MAGIC_DV_PAL:
      case TC_MAGIC_DV_NTSC:
      case TC_MAGIC_AC3:
      case TC_MAGIC_YUV4MPEG:
      case TC_MAGIC_AVI:
      case TC_MAGIC_MPEG:

	if(!found) type=itype;

	if(itype!=type) {
	  tc_log_error(__FILE__,"multiple filetypes not valid for dirlist mode.");
	  exit(1);
	}
	found=1;
	break;

      default:
	tc_log_error(__FILE__, "invalid filetype %s for dirlist mode.", filetype(type));
	exit(1);
      } // check itype
    } // process files

    tc_dirlist_close(&tcdir);

    if(!found) {
      tc_log_error(__FILE__, "no valid files found in %s", name);
      exit(1);
    } else if(verbose_flag & TC_DEBUG)
      tc_log_msg(__FILE__, "%s", filetype(type));



    //PASS 2: dump files in correct order

    if(tc_dirlist_open(&tcdir, ipipe->name, 1)<0) {
      tc_log_error(__FILE__, "unable to sort dirlist entries\"%s\"", name);
      exit(1);
    }

    while((name=tc_dirlist_scan(&tcdir))!=NULL) {

      if((ipipe->fd_in = open(name, O_RDONLY))<0) {
	tc_log_perror(__FILE__, "file open");
	exit(1);
      } else if(verbose_flag & TC_STATS)
	tc_log_msg(__FILE__, "processing %s", name);


      //type determined in pass 1

      switch(type) {

      case TC_MAGIC_VOB:

	if(vob_offset>0) {

	  off_t off, size;

	  //get filesize in units of packs (2kB)
	  size  = lseek(ipipe->fd_in, 0, SEEK_END);

	  lseek(ipipe->fd_in, 0, SEEK_SET);

	  if(size > vob_offset * (off_t) DVD_VIDEO_LB_LEN) {
	    // offset within current file
	    off = lseek(ipipe->fd_in, vob_offset * (off_t) DVD_VIDEO_LB_LEN, SEEK_SET);
	    vob_offset = 0;
	  } else {
	    vob_offset -= size/DVD_VIDEO_LB_LEN;
	    goto vob_skip;
	  }
	}

	tc_preadwrite(ipipe->fd_in, ipipe->fd_out);

      vob_skip:
	break;

      case TC_MAGIC_DV_PAL:
      case TC_MAGIC_DV_NTSC:
      case TC_MAGIC_AC3:
      case TC_MAGIC_YUV4MPEG:
      case TC_MAGIC_MPEG:

	tc_preadwrite(ipipe->fd_in, ipipe->fd_out);


	break;

      case TC_MAGIC_AVI:

	//extract and concatenate streams

	ac_memcpy(&ipipe_avi, ipipe, sizeof(info_t));

	//real AVI file name
	ipipe_avi.name = (char *)name;
	ipipe_avi.magic = TC_MAGIC_AVI;

	extract_avi(&ipipe_avi);

	break;

      default:
	tc_log_error(__FILE__, "invalid filetype %s for dirlist mode.", filetype(type));
	exit(1);
      }

      close(ipipe->fd_in);

    }//process files

    tc_dirlist_close(&tcdir);

    break;
  }
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
  fprintf(stderr,"    -i name          input file/directory%s name\n",
#ifdef HAVE_LIBDVDREAD
	  "/device/mountpoint"
#else
	  ""
#endif
  );
  fprintf(stderr,"    -t magic         file type [autodetect]\n");
#ifdef HAVE_LIBDVDREAD
  fprintf(stderr,"    -T t[,c[-d][,a]] DVD title[,chapter(s)[,angle]] [1,1,1]\n");
  fprintf(stderr,"    -L               process all following chapters [off]\n");
#endif
  fprintf(stderr,"    -S n             seek to VOB stream offset nx2kB [0]\n");
  fprintf(stderr,"    -P               stream DVD ( needs -T )\n");
  fprintf(stderr,"    -a               dump AVI-file/socket audio stream\n");
  fprintf(stderr,"    -n id            transport stream id [0x10]\n");
  fprintf(stderr,"    -d mode          verbosity mode\n");
  fprintf(stderr,"    -v               print version\n");

  exit(status);

}

# define IS_STDIN    1
# define IS_FILE     2
# define IS_DVD      3
# define IS_DIR      4
# define IS_TS       5

/* ------------------------------------------------------------
 *
 * universal extract frontend
 *
 * ------------------------------------------------------------*/

int main(int argc, char *argv[])
{

  struct stat fbuf;
  info_t ipipe;

  int user=0, source=0;

  int end_chapter, start_chapter;

  int title=1, chapter1=1, chapter2=-1, angle=1, n=0, j, loop=0, stream=0, audio=0;

  int max_chapters;
  int max_angles;
  int max_titles;

  int vob_offset=0;
  int ch, ts_pid=0x10;
  char *magic="", *name=NULL;

  //proper initialization
  memset(&ipipe, 0, sizeof(info_t));

  while ((ch = getopt(argc, argv, "S:T:d:i:vt:LaP?hn:")) != -1) {

    switch (ch) {

    case 'i':

      if(optarg[0]=='-') usage(EXIT_FAILURE);
      name = optarg;

      break;

    case 'T':

      n = sscanf(optarg,"%d,%d-%d,%d", &title, &chapter1, &chapter2, &angle);

      if (n != 4) {

	n = sscanf(optarg,"%d,%d-%d", &title, &chapter1, &chapter2);

	if (n != 3) {

	  n = sscanf(optarg,"%d,%d,%d", &title, &chapter1, &angle);
          // only do one chapter !
	  chapter2=chapter1;

	  if(n<0 || n>3) {
	    tc_log_error(EXE, "invalid parameter for option -T");
	    exit(1);
	  }
	}
      }

      source = IS_DVD;

      if(chapter2!=-1) {
	if(chapter2<chapter1) {
	  tc_log_error(EXE, "invalid parameter for option -T");
	  exit(1);
	}
	if(chapter2-chapter1>=1) loop=1;
      }

      if(chapter1==-1) loop=1;

      break;

    case 'L':
      loop=1;
      chapter2=INT_MAX;

      break;

    case 'P':
      stream=1;

      break;

    case 'a':
      audio=1;
      break;

    case 'd':

      if(optarg[0]=='-') usage(EXIT_FAILURE);
      verbose = atoi(optarg);

      break;


    case 'n':

      if(optarg[0]=='-') usage(EXIT_FAILURE);
      ts_pid = strtol(optarg, NULL, 16);

      source = IS_TS;

      break;

    case 'S':

      if(optarg[0]=='-') usage(EXIT_FAILURE);
      vob_offset = atoi(optarg);

      break;


    case 't':

      if(optarg[0]=='-') usage(EXIT_FAILURE);
      magic = optarg;
      user=1;

      if(strcmp(magic,"dvd")==0) {
	source=IS_DVD;
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

  //DVD debugging information
  if((verbose & TC_DEBUG) && source == IS_DVD)
    tc_log_msg(EXE, "T=%d %d %d %d %d", n, title, chapter1, chapter2, angle);

  /* ------------------------------------------------------------
   *
   * fill out defaults for info structure
   *
   * ------------------------------------------------------------*/

  // no autodetection yet
  if(argc==1) {
    usage(EXIT_FAILURE);
  }

  // assume defaults
  if(name==NULL) {
    source=IS_STDIN;
    ipipe.fd_in = STDIN_FILENO;
  }

  // no stdin for DVD
  if(name==NULL && source==IS_DVD) {
    tc_log_error(EXE, "invalid directory/path_to_device\n");
    usage(EXIT_FAILURE);
  }

  // do not try to mess with the stdin stream
  if((source!=IS_DVD) && (source!=IS_TS) && (source!=IS_STDIN)) {

    // file or directory?

    if(stat(name, &fbuf)) {

	tc_log_error(EXE, "invalid file \"%s\"", name);
	exit(1);
    }

    //default
    source=IS_FILE;
    if(S_ISDIR(fbuf.st_mode)) source=IS_DIR;
  }

  // fill out defaults for info structure
  ipipe.fd_out = STDOUT_FILENO;

  ipipe.verbose = verbose;

  ipipe.dvd_title = title;
  ipipe.dvd_chapter = chapter1;
  ipipe.dvd_angle = angle;

  ipipe.ts_pid = ts_pid;

  ipipe.vob_offset = vob_offset;

  if (name) {
      if ((ipipe.name = tc_strdup(name)) == NULL) {
          tc_log_error(EXE, "could not allocate memory");
          exit(1);
      }
      if (strlen(ipipe.name) != strlen(name)) {
          /* should never happen */
          tc_log_error(__FILE__, "can't fully copy the file name");
          exit(1);
      }
  } else {
      ipipe.name = NULL;
  }

  ipipe.select = audio;

  /* ------------------------------------------------------------
   *
   * source specific section
   *
   * ------------------------------------------------------------*/

  switch(source) {

    // ---
    // TS
    // ---

  case IS_TS:

    if((ipipe.fd_in = xio_open(name, O_RDONLY))<0) {
      tc_log_perror(EXE, "file open");
      exit(1);
    }

    ipipe.magic = TC_MAGIC_TS;

    tccat_thread(&ipipe);

    xio_close(ipipe.fd_in);

    break;

    // ---
    // DVD
    // ---

  case IS_DVD:

    if(dvd_init(name, &max_titles, verbose)<0) {
      tc_log_error(EXE, "(pid=%d) failed to open DVD %s", getpid(), name);
      exit(1);
    }

    ipipe.magic = TC_MAGIC_DVD_PAL;

    dvd_query(title, &max_chapters, &max_angles);

    // set chapternumbers now we know how much there are
    start_chapter = (chapter1!=-1 && chapter1 <=max_chapters) ? chapter1:1;
      end_chapter = (chapter2!=-1 && chapter2 <=max_chapters) ? chapter2:max_chapters;

    for(j=start_chapter; j<end_chapter+1; ++j) {
      ipipe.dvd_chapter=j;
      if(verbose & TC_DEBUG)
	tc_log_msg(EXE, "(pid=%d) processing chapter (%d/%d)", getpid(), j, max_chapters);

      if(stream) {
        dvd_stream(title,j);
      }else{
        tccat_thread(&ipipe);
      }
    }
/*
    } else if(stream) {

      dvd_stream(title,chapter1,chapter2);

    } else {
	  if(verbose & TC_DEBUG)
	    tc_log_msg(EXE, "(pid=%d) processing chapter (%d)", getpid(), chapter1);
          tccat_thread(&ipipe);
    }*/

    dvd_close();

    break;

    // ------------------
    // stdin/regular file
    // ------------------

  case IS_FILE:

    if((ipipe.fd_in = open(name, O_RDONLY))<0) {
      tc_log_perror(EXE, "file open");
      exit(1);
    }

  case IS_STDIN:

    //stream out:
    ipipe.magic = TC_MAGIC_RAW;

    tccat_thread(&ipipe);

    if(ipipe.fd_in != STDIN_FILENO) close(ipipe.fd_in);

    break;

    // ---------
    // directory
    // ---------

  case IS_DIR:

    //stream out:
    ipipe.magic = TC_MAGIC_DIR;

    tccat_thread(&ipipe);

    break;

    // ------
    // socket
    // ------

  }

  return(0);
}

#include "libtc/static_xio.h"
