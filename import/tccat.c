/*
 *  tccat.c
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

#include "transcode.h"

#include <sys/errno.h>
#include "xio.h"
#include "ioaux.h"
#include "tc.h"
#include "dvd_reader.h"

#define EXE "tccat"

#define MAX_BUF 1024
char buf[MAX_BUF];

static int verbose=TC_INFO;

extern int errno;

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


void usage(int status)
{
  version(EXE);
  
  fprintf(stderr,"\nUsage: %s [options]\n", EXE);
#ifdef HAVE_LIBDVDREAD
#ifdef NET_STREAM
  fprintf(stderr,"\t -i name          input file/directory/device/mountpoint/host name\n");
#else
  fprintf(stderr,"\t -i name          input file/directory/device/mountpoint name\n");
#endif
#else
#ifdef NET_STREAM
  fprintf(stderr,"\t -i name          input file/directory/host name\n");
#else
  fprintf(stderr,"\t -i name          input file/directory name\n");
#endif
#endif

  fprintf(stderr,"\t -t magic         file type [autodetect]\n");

#ifdef HAVE_LIBDVDREAD
  fprintf(stderr,"\t -T t[,c[-d][,a]] DVD title[,chapter(s)[,angle]] [1,1,1]\n");
  fprintf(stderr,"\t -L               process all following chapters [off]\n");
#endif
  fprintf(stderr,"\t -S n             seek to VOB stream offset nx2kB [0]\n");
  fprintf(stderr,"\t -P               stream DVD ( needs -T )\n");
  fprintf(stderr,"\t -a               dump AVI-file/socket audio stream\n");
  fprintf(stderr,"\t -n id            transport stream id [0x10]\n");
  fprintf(stderr,"\t -d mode          verbosity mode\n");
  fprintf(stderr,"\t -v               print version\n");

  exit(status);
  
}

# define IS_STDIN    1
# define IS_FILE     2
# define IS_DVD      3
# define IS_DIR      4
# define IS_SOCKET   5
# define IS_TS       6

/* ------------------------------------------------------------ 
 *
 * universal extract frontend 
 *
 * ------------------------------------------------------------*/

int main(int argc, char *argv[])
{

  struct stat fbuf;
#ifdef NET_STREAM
  struct hostent *hp;
#endif
  info_t ipipe;
  size_t namelen;
  long sret;

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
	    fprintf(stderr, "invalid parameter for option -T\n");
	    exit(1);
	  }
	}
      }
      
      source = IS_DVD;
      
      if(chapter2!=-1) {
	if(chapter2<chapter1) {
	  fprintf(stderr, "invalid parameter for option -T\n");
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
      version(EXE);
      exit(0);
      break;
      
    case 'h':
      usage(EXIT_SUCCESS);
    default:
      usage(EXIT_FAILURE);
    }
  }

  //DVD debugging information
  if(verbose & TC_DEBUG && source == IS_DVD) fprintf(stderr, "T=%d %d %d %d %d\n", n, title, chapter1, chapter2, angle);
  
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
    fprintf(stderr, "error: invalid directory/path_to_device\n");
    usage(EXIT_FAILURE);
  }
  
  // do not try to mess with the stdin stream
  if((source!=IS_DVD) && (source!=IS_TS) && (source!=IS_STDIN)) {
    
    // file or directory?
    
    if(stat(name, &fbuf)) {

#ifdef NET_STREAM
	// no file, maybe host?

	if((hp = gethostbyname(name)) != NULL) {
	    source=IS_SOCKET;
	    goto cont;
	}
#endif
	fprintf(stderr, "(%s) invalid file \"%s\"\n", __FILE__, name);
	exit(1);
    }
    
    //default
    source=IS_FILE;
    if(S_ISDIR(fbuf.st_mode)) source=IS_DIR;
  }

#ifdef NET_STREAM
 cont:
#endif

  // fill out defaults for info structure
  ipipe.fd_out = STDOUT_FILENO;
  
  ipipe.verbose = verbose;
  
  ipipe.dvd_title = title;
  ipipe.dvd_chapter = chapter1;
  ipipe.dvd_angle = angle;

  ipipe.ts_pid = ts_pid;

  ipipe.vob_offset = vob_offset;

  if (name) {
      namelen = strlen(name) + 1;
      if ((ipipe.name = malloc(namelen)) == NULL) {
          fprintf(stderr, "(%s) could not allocate memory\n", __FILE__);
          exit(1);
      }
      sret = strlcpy(ipipe.name, name, namelen);
      if (tc_test_string(__FILE__, __LINE__, namelen, sret, errno))
          exit(1);
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
      perror("file open");
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
      fprintf(stderr, "[%s] (pid=%d) failed to open DVD %s\n", EXE, getpid(), name);
      exit(1);
    }
    
    ipipe.magic = TC_MAGIC_DVD_PAL;
    
    dvd_query(title, &max_chapters, &max_angles);

    // set chapternumbers now we know how much there are
    start_chapter = (chapter1!=-1 && chapter1 <=max_chapters) ? chapter1:1;
      end_chapter = (chapter2!=-1 && chapter2 <=max_chapters) ? chapter2:max_chapters;
      
    for(j=start_chapter; j<end_chapter+1; ++j) {
      ipipe.dvd_chapter=j;
      if(verbose & TC_DEBUG) fprintf(stderr, "[%s] (pid=%d) processing chapter (%d/%d)\n", EXE, getpid(), j, max_chapters);

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
	  if(verbose & TC_DEBUG) fprintf(stderr, "[%s] (pid=%d) processing chapter (%d)\n", EXE, getpid(), chapter1);
          tccat_thread(&ipipe);
    }*/
    
    dvd_close();
    
    break;
    
    // ------------------
    // stdin/regular file
    // ------------------
    
  case IS_FILE:
    
    if((ipipe.fd_in = open(name, O_RDONLY))<0) {
      perror("file open");
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

  case IS_SOCKET:
      
    //stream out:
    ipipe.magic = TC_MAGIC_SOCKET;
    
    tccat_thread(&ipipe);
    
    break;

  }
  
  return(0);
}

#include "libxio/static_xio.h"
