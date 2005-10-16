/*
 *  export_net.c
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

#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>

#include "transcode.h"
#include "ioaux.h"

#define MOD_NAME    "export_net.so"
#define MOD_VERSION "v0.0.2 (2003-01-09)"
#define MOD_CODEC   "(video) RGB/YUV | (audio) PCM/AC3"

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_PCM|TC_CAP_RGB|TC_CAP_YUV|TC_CAP_AC3;

#define MOD_PRE net
#include "export_def.h"

static int vns=0, ans=0;
static int aport, vport;
static pthread_t thread1, thread2;

static int size;

#if 0  /* get this from ioaux.c */
static size_t p_write (int fd, char *buf, size_t len)
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
#endif

static void vlisten(void)
{
    
  struct sockaddr_in fsin;  
  int fromlen;

  tc_tag_info(MOD_NAME, "[%s] waiting for clients to connect ...");
  
  if(listen(vport, 2) < 0) {
      perror("listen");
      return;
  }
  
  fromlen=sizeof(fsin);
  
  if((vns = accept(vport, (struct sockaddr *)&fsin, &fromlen)) < 0) {
      perror("accept");
      return;
  }
  
  tc_tag_info(MOD_NAME, "client connected (video request)");

  return;
}

static void alisten(void)
{
    
  struct sockaddr fsin;  
  int fromlen;

  tc_tag_info(MOD_NAME, "waiting for clients to connect ...");
  
  if(listen(aport, 2) < 0) {
      perror("listen");
      return;
  }
  
  fromlen=sizeof(fsin);
  
  if((ans = accept(aport, &fsin, &fromlen)) < 0) {
      perror("accept");
      return;
  }
  
  tc_tag_info(MOD_NAME, "client connected (audio request)");

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
    if(pthread_create(&thread2, NULL, (void *) alisten, NULL)!=0) {
	tc_tag_warn(MOD_NAME, "failed to start listen (audio) thread");
        return(TC_EXPORT_ERROR);
    }

    return(TC_EXPORT_OK);
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
    if(pthread_create(&thread1, NULL, (void *) vlisten, NULL)!=0) {
	tc_tag_warn(MOD_NAME, "failed to start listen (video) thread");
	return(TC_EXPORT_ERROR);
    }

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
	    if(verbose & TC_DEBUG) tc_tag_info(MOD_NAME, "(V) waiting");
	    sleep(1);
	}

	if(verbose & TC_DEBUG) tc_tag_info(MOD_NAME, "(V) write (%d,%d)", param->size, size);

	if(p_write(vns, (char *) param->buffer, size)!=size) {
	  perror("video write");
	  return(TC_EXPORT_ERROR);
	}
	
	return(0);
    }
    
    if(param->flag == TC_AUDIO) {

	while(ans==0) {
	    if(verbose & TC_DEBUG) tc_tag_info(MOD_NAME, "(A) waiting");
	    sleep(1);
	}

	if(verbose & TC_DEBUG) tc_tag_info(MOD_NAME, "(A) write (%d)", param->size);
	
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

