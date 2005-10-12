/*
 *  iodump.c
 *
 *  Copyright (C) Thomas �streich - June 2001
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
#include "ioaux.h"
#include "iodir.h"
#include "tc.h"

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


int dvd_read(int arg_title, int arg_chapter, int arg_angle);

#define IO_BUF_SIZE 1024
#define DVD_VIDEO_LB_LEN 2048

static int verbose_flag=TC_QUIET;

/* ------------------------------------------------------------ 
 *
 * source extract thread
 *
 * ------------------------------------------------------------*/

void tccat_thread(info_t *ipipe)
{
  
  char *name=NULL;
  int found=0, itype=TC_MAGIC_UNKNOWN, type=TC_MAGIC_UNKNOWN;

  int vob_offset=0;

  info_t ipipe_avi;

#ifdef NET_STREAM
  struct sockaddr_in sin;
  struct hostent *hp;

  int port, vs;
  char *iobuf;

  int bytes;
  int error=0;

#endif

  verbose_flag = ipipe->verbose;
  vob_offset = ipipe->vob_offset;

  switch(ipipe->magic) {

  case TC_MAGIC_DVD_PAL:
  case TC_MAGIC_DVD_NTSC:
    
    if(verbose_flag & TC_DEBUG) fprintf(stderr, "(%s) %s\n", __FILE__, filetype(ipipe->magic));
    
    dvd_read(ipipe->dvd_title, ipipe->dvd_chapter, ipipe->dvd_angle);
    break;

  case TC_MAGIC_TS:
    
    ts_read(ipipe->fd_in, ipipe->fd_out, ipipe->ts_pid);
    break;

  case TC_MAGIC_RAW:

    if(verbose_flag & TC_DEBUG) fprintf(stderr, "(%s) %s\n", __FILE__, filetype(ipipe->magic));
   
    if(vob_offset>0) {
      
      off64_t off;
      
      //get filesize in units of packs (2kB)
      off = lseek64(ipipe->fd_in, vob_offset * (int64_t) DVD_VIDEO_LB_LEN, 
		    SEEK_SET);
      
      if( off != ( vob_offset * (int64_t) DVD_VIDEO_LB_LEN ) ) {
	fprintf(stderr, "unable to seek to block %d\n", vob_offset); //drop this chunk/file
	goto vob_skip2;
      } 
    }
    
    p_readwrite(ipipe->fd_in, ipipe->fd_out);
    
  vob_skip2:
    break;
    
#ifdef NET_STREAM
  case TC_MAGIC_SOCKET:
      
      port = (ipipe->select==1) ? TC_DEFAULT_APORT:TC_DEFAULT_VPORT;
    
      if(( hp = gethostbyname(ipipe->name)) == NULL) {
	
	  fprintf(stderr, "[%s] host %s unknown\n", __FILE__, ipipe->name);
	  ipipe->error=1;
	  return;
      }
      
      // get socket file descriptor 
      
      if(( vs = socket(AF_INET, SOCK_STREAM, 0)) <0) {
	
	perror("socket");
	ipipe->error=1;
	return;
      }
  
      sin.sin_family = AF_INET;
      sin.sin_port = htons(port);
  
      bcopy(hp->h_addr, &sin.sin_addr, hp->h_length);
  
  
      if(connect(vs, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
	
	perror("connect");
	ipipe->error=1;
	return;
      }

      //start streaming
      
      if(!(iobuf = malloc(IO_BUF_SIZE))) {
	fprintf(stderr, "(%s) out of memory\n", __FILE__);
	ipipe->error=1;
	return;
      }  
      
      do {
	
	bytes=p_read(vs, iobuf, IO_BUF_SIZE);
	
	// error on read?
	if(bytes<0) {
	  ipipe->error=1;
	  return;
	}   
	
	// read stream end?
	if(bytes!=IO_BUF_SIZE) error=1;
	
	// write stream problems?
	if(p_write(ipipe->fd_out, iobuf, bytes)!= bytes) error=1;
      } while(!error);
      
      close(vs);
      
      free(iobuf);

      break;
#endif

  case TC_MAGIC_DIR:
    
    //PASS 1: check file type - file order not important

    if((tc_open_directory(ipipe->name))<0) { 
      fprintf(stderr, "(%s) unable to open directory \"%s\"\n", __FILE__, ipipe->name);
      exit(1);
    } else if(verbose_flag & TC_DEBUG) 
      fprintf(stderr, "(%s) scanning directory \"%s\"\n", __FILE__, ipipe->name);
    
    while((name=tc_scan_directory(ipipe->name))!=NULL) {	
      
      if((ipipe->fd_in = open(name, O_RDONLY))<0) {
	perror("file open");
	exit(1);
      }
      
      //first valid magic must be the same for all
      //files to follow
      
      
      itype = fileinfo(ipipe->fd_in, 0);
      
      close(ipipe->fd_in);
      
      if(itype == TC_MAGIC_UNKNOWN || itype == TC_MAGIC_PIPE ||
	 itype == TC_MAGIC_ERROR) {      
	
	fprintf(stderr,"\n\nerror: this version of transcode supports only\n");
	fprintf(stderr,"directories containing files of identical file type.\n");
	fprintf(stderr,"Please clean up directory %s and restart.\n", ipipe->name);
	
	fprintf(stderr,"file %s with filetype %s is invalid for directory mode.\n", name, filetype(itype));
	
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
	  fprintf(stderr,"\nerror: multiple filetypes not valid for directory mode.\n");
	  exit(1);
	}
	found=1;
	break;
	
      default:
	fprintf(stderr,"\nerror: invalid filetype %s for directory mode.\n", filetype(type));
	exit(1);
      } // check itype
    } // process files
    
    tc_close_directory();
    
    if(!found) {
      fprintf(stderr,"\nerror: no valid files found in %s\n", name);
      exit(1);
    } else if(verbose_flag & TC_DEBUG) 
      fprintf(stderr, "(%s) %s\n", __FILE__, filetype(type));
      

    
    //PASS 2: dump files in correct order
    
    if((tc_open_directory(ipipe->name))<0) { 
      fprintf(stderr, "(%s) unable to sort directory entries\"%s\"\n", __FILE__, name);
      exit(1);
    }

    if((tc_sortbuf_directory(ipipe->name))<0) { 
      fprintf(stderr, "(%s) unable to sort directory entries\"%s\"\n", __FILE__, name);
      exit(1);
    }
    
    while((name=tc_scan_directory(ipipe->name))!=NULL) {
      
      if((ipipe->fd_in = open(name, O_RDONLY))<0) {
	perror("file open");
	exit(1);
      } else if(verbose_flag & TC_STATS) 
	fprintf(stderr, "(%s) processing %s\n", __FILE__, name);
      
      
      //type determined in pass 1
      
      switch(type) {
	
      case TC_MAGIC_VOB:

	if(vob_offset>0) {
	  
	  off64_t off, size;
	  
	  //get filesize in units of packs (2kB)
	  size  = lseek64(ipipe->fd_in, 0, SEEK_END);

	  lseek64(ipipe->fd_in, 0, SEEK_SET);

	  if(size > vob_offset * (int64_t) DVD_VIDEO_LB_LEN) {
	    // offset within current file
	    off = lseek64(ipipe->fd_in, vob_offset * (int64_t) DVD_VIDEO_LB_LEN, SEEK_SET);
	    vob_offset = 0;
	  } else {
	    vob_offset -= size/DVD_VIDEO_LB_LEN;
	    goto vob_skip;
	  } 
	}

	p_readwrite(ipipe->fd_in, ipipe->fd_out);
	
      vob_skip:
	break;

      case TC_MAGIC_DV_PAL:
      case TC_MAGIC_DV_NTSC:
      case TC_MAGIC_AC3: 
      case TC_MAGIC_YUV4MPEG:
      case TC_MAGIC_MPEG:
	
	p_readwrite(ipipe->fd_in, ipipe->fd_out);
	
	
	break;
	
      case TC_MAGIC_AVI:
	
	//extract and concatenate streams

	ac_memcpy(&ipipe_avi, ipipe, sizeof(info_t));
	
	//real AVI file name
	ipipe_avi.name = name;
	ipipe_avi.magic = TC_MAGIC_AVI;

	extract_avi(&ipipe_avi);	

	break;

      default:
	fprintf(stderr,"\nerror: invalid filetype %s for directory mode.\n", filetype(type));
	exit(1);
      }
      
      close(ipipe->fd_in);

    }//process files
    
    tc_close_directory();
    tc_freebuf_directory();

    break;
  }
}


int fileinfo_dir(char *dname, int *fd, long *magic)
{    
    char *name=NULL;
    
    //check file type - file order not important
    
    if((tc_open_directory(dname))<0) { 
	fprintf(stderr, "(%s) unable to open directory \"%s\"\n", __FILE__, dname);
	exit(1);
    } else if(verbose_flag & TC_DEBUG) 
	
	fprintf(stderr, "(%s) scanning directory \"%s\"\n", __FILE__, dname);
    
    if((name=tc_scan_directory(dname))==NULL) return(-1); 	

    if((*fd= open(name, O_RDONLY))<0) {
	perror("open file");
	return(-1);
    }

    tc_close_directory();
    
    //first valid magic must be the same for all
    //files to follow, but is not checked here
    
    *magic = fileinfo(*fd, 0);
    return(0);
}
