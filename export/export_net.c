/*
 *  export_net.c
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

#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>

#include "transcode.h"

#define MOD_NAME    "export_net.so"
#define MOD_VERSION "v0.0.1 (2001-11-21)"
#define MOD_CODEC   "(video) RGB/YUV | (audio) PCM/AC3"

#define MOD_PRE net
#include "export_def.h"

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_PCM|TC_CAP_RGB|TC_CAP_YUV|TC_CAP_AC3;

static int vns=0, ans=0;
static int aport, vport;
static pthread_t thread1, thread2;

static int size;

size_t p_write (int fd, char *buf, size_t len)
{
   size_t n = 0;
   size_t r = 0;

   while (r < len) {
      n = write (fd, buf + r, len - r);
      if (n < 0)
         return n;
      
      r += n;
   }
   return r;
}

void vlisten() 
{
    
  struct sockaddr_in fsin;  
  int fromlen;

  printf("[%s] waiting for clients to connect ...\n", MOD_NAME);
  
  if(listen(vport, 2) < 0) {
      perror("listen");
      return;
  }
  
  fromlen=sizeof(fsin);
  
  if((vns = accept(vport, (struct sockaddr *)&fsin, &fromlen)) < 0) {
      perror("accept");
      return;
  }
  
  printf("[%s] client connected (video request)\n", MOD_NAME);

  return;
}

void alisten() 
{
    
  struct sockaddr fsin;  
  int fromlen;

  printf("[%s] waiting for clients to connect ...\n", MOD_NAME);
  
  if(listen(aport, 2) < 0) {
      perror("listen");
      return;
  }
  
  fromlen=sizeof(fsin);
  
  if((ans = accept(aport, &fsin, &fromlen)) < 0) {
      perror("accept");
      return;
  }
  
  printf("[%s] client connected (audio request)\n", MOD_NAME);

  return;
}


/* ------------------------------------------------------------ 
 *
 * init codec
 *
 * ------------------------------------------------------------*/

MOD_init
{
  struct sockaddr_in sin;  
  
  int on = 1;

  if(param->flag == TC_AUDIO) {

    // open and bind to net socket 
    
    if((aport = socket(AF_INET, SOCK_STREAM, 0)) <0) {
      perror("socket");
      return(TC_EXPORT_ERROR);
    }
    
    bzero((char*)&sin, sizeof(sin));
    
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr= htonl(INADDR_ANY); 
    sin.sin_port = htons(TC_DEFAULT_APORT);
         
    // from squid: avoid blocking of port on exit
    if (setsockopt(aport, SOL_SOCKET, SO_REUSEADDR, (char *) &on, 
		   sizeof(on)) < 0) {
      perror("setsockopt (SO_REUSEADDR)");
      return(TC_EXPORT_ERROR);
    }
 
    if(bind(aport,(struct sockaddr *) &sin, sizeof(sin)) < 0) {
      perror("bind");
      return(TC_EXPORT_ERROR);
    }

    // start the listen thread     
    if(pthread_create(&thread2, NULL, (void *) alisten, NULL)!=0)
	tc_error("failed to start listen (audio) thread");

    return(0);
  }
  
  if(param->flag == TC_VIDEO) {
    
    // open and bind to net socket 
    
    if((vport = socket(AF_INET, SOCK_STREAM, 0)) <0) {
      perror("socket");
      return(TC_EXPORT_ERROR);
    }
    
    bzero((char*)&sin, sizeof(sin));
    
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr= htonl(INADDR_ANY); 
    sin.sin_port = htons(TC_DEFAULT_VPORT);
         
    // from squid: avoid blocking of port on exit
    if (setsockopt(vport, SOL_SOCKET, SO_REUSEADDR, (char *) &on, 
		   sizeof(on)) < 0) {
      perror("setsockopt (SO_REUSEADDR)");
      return(TC_EXPORT_ERROR);
    }
 
    if(bind(vport, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
      perror("bind");
      return(TC_EXPORT_ERROR);
    }

    // start the listen thread     
    if(pthread_create(&thread1, NULL, (void *) vlisten, NULL)!=0)
	tc_error("failed to start listen (video) thread");

    //size?

    size = vob->ex_v_height*vob->ex_v_width*3/2;

    return(0);
  }
  
  // invalid flag
  return(TC_EXPORT_ERROR); 
}

/* ------------------------------------------------------------ 
 *
 * open outputfile
 *
 * ------------------------------------------------------------*/

MOD_open
{

    if(param->flag == TC_VIDEO) return(0);
    if(param->flag == TC_AUDIO) return(0);
  
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
    
    if(param->flag == TC_VIDEO) {

	while(vns==0) {
	    if(verbose & TC_DEBUG) printf("[%s] (V) waiting\n", MOD_NAME);
	    sleep(1);
	}

	if(verbose & TC_DEBUG) printf("[%s] (V) write (%d,%d)\n", MOD_NAME, param->size, size);

	if(p_write(vns, (char *) param->buffer, size)!=size) {
	  perror("video write");
	  return(TC_EXPORT_ERROR);
	}
	
	return(0);
    }
    
    if(param->flag == TC_AUDIO) {

	while(ans==0) {
	    if(verbose & TC_DEBUG) printf("[%s] (A) waiting\n", MOD_NAME);
	    sleep(1);
	}

	if(verbose & TC_DEBUG) printf("[%s] (A) write (%d)\n", MOD_NAME, param->size);
	
	if(p_write(ans, (char *) param->buffer, param->size)!=param->size) {
	  perror("audio write");
	  return(TC_EXPORT_ERROR);
	}
	
	return(0);
    }
    
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
  
  if(param->flag == TC_VIDEO) return(0);
  if(param->flag == TC_AUDIO) return(0);

  // invalid flag
  return(TC_EXPORT_ERROR);
}

/* ------------------------------------------------------------ 
 *
 * close outputfiles
 *
 * ------------------------------------------------------------*/

MOD_close
{  

  if(param->flag == TC_VIDEO) {
    close(vns);
    return(0);
  }
  
  if(param->flag == TC_AUDIO) {
    close(ans);
    return(0);
  }

  // invalid flag
  return(TC_EXPORT_ERROR);  

}

