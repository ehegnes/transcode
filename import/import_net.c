/*
 *  import_net.c
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
#include <unistd.h>
#include <string.h>

#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "transcode.h"
#include "ioaux.h"

#define MOD_NAME    "import_net.so"
#define MOD_VERSION "v0.0.1 (2001-11-21)"
#define MOD_CODEC   "(video) RGB/YUV | (audio) PCM"

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_RGB|TC_CAP_YUV|TC_CAP_PCM;

#define MOD_PRE net
#include "import_def.h"

#define MAX_BUF 1024
char import_cmd_buf[MAX_BUF];

static int vs, as;

#if 0  /* get this from ioaux.c */
static size_t p_read(int fd, char *buf, size_t len)
{
   size_t n = 0;
   size_t r = 0;

   while (r < len) {
      n = read (fd, buf + r, len - r);

      if (n <= 0)
	  return r;
      r += n;
   }

   return r;
}
#endif

/* ------------------------------------------------------------ 
 *
 * open stream
 *
 * ------------------------------------------------------------*/

MOD_open
{

    char *server;

    struct sockaddr_in sin;
    struct hostent *hp;

    if(param->flag == TC_VIDEO) {
      
      // init default values 
      server=vob->video_in_file;

      if(( hp = gethostbyname(server)) == NULL) {
	
	fprintf(stderr, "[%s] host %s unknown\n", MOD_NAME, server);
	return(TC_EXPORT_ERROR);
      }
      
      // get socket file descriptor 
      
      if(( vs = socket(AF_INET, SOCK_STREAM, 0)) <0) {
	
	perror("socket");
	return(TC_EXPORT_ERROR);
      }
  
      sin.sin_family = AF_INET;
      sin.sin_port = htons(TC_DEFAULT_VPORT);
  
      bcopy(hp->h_addr, &sin.sin_addr, hp->h_length);
  
  
      if(connect(vs, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
	
	perror("connect");
	return(TC_EXPORT_ERROR);
      }

      param->fd = NULL;

      return(0);
    }
    
    if(param->flag == TC_AUDIO) {

      // init default values 
      server=vob->audio_in_file;

      if(( hp = gethostbyname(server)) == NULL) {
	
	fprintf(stderr, "[%s] host %s unknown\n", MOD_NAME, server);
	return(TC_EXPORT_ERROR);
      }
      
      // get socket file descriptor 
      
      if(( as = socket(AF_INET, SOCK_STREAM, 0)) <0) {
	
	perror("socket");
	return(TC_EXPORT_ERROR);
      }
  
      sin.sin_family = AF_INET;
      sin.sin_port = htons(TC_DEFAULT_APORT);
  
      bcopy(hp->h_addr, &sin.sin_addr, hp->h_length);
  
  
      if(connect(as, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
	
	perror("connect");
	return(TC_EXPORT_ERROR);
      }

      param->fd = NULL;

      return(0);
    }
  
  return(TC_IMPORT_ERROR);
}


/* ------------------------------------------------------------ 
 *
 * decode  stream
 *
 * ------------------------------------------------------------*/

MOD_decode {

  if(param->flag == TC_VIDEO) {
    
    if(verbose_flag & TC_DEBUG) printf("(V) read\n");
    
    if(p_read(vs, (char *) param->buffer, param->size)!=param->size) {
      return(TC_IMPORT_ERROR);
    }
    return(0);
  }
  
  if(param->flag == TC_AUDIO) {

    if(verbose_flag & TC_DEBUG) printf("(A) read\n");    
    
    if(p_read(as, (char *) param->buffer, param->size)!=param->size) {
      return(TC_IMPORT_ERROR);
    }
    return(0);
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
  if(param->flag == TC_VIDEO) {

    printf("[%s] disconnect\n", MOD_NAME);
    if(close(vs) < 0)  perror("close socket");
    return(0);
  }
  
  if(param->flag == TC_AUDIO) {

    printf("[%s] disconnect\n", MOD_NAME);
    if(close(as) < 0)  perror("close socket");
    return(0);
  }
  
  return(TC_IMPORT_ERROR);
}


