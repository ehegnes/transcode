/*
 *  probe_socket.c
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
#include "ioaux.h"

#ifdef NET_STREAM

#include <sys/types.h>
#include <sys/socket.h>

static vob_t *ivob;

vob_t *probe_host(char *server)
{

  struct sockaddr_in sin;
  struct hostent *hp;
  
  int s;
  
  hp = gethostbyname(server);    
    
  // get socket file descriptor 
      
  if(( s = socket(AF_INET, SOCK_STREAM, 0)) <0) {
	
    perror("socket");
    return(NULL);
  }
  
  sin.sin_family = AF_INET;
  sin.sin_port = htons(TC_DEFAULT_PPORT);
  
  bcopy(hp->h_addr, &sin.sin_addr, hp->h_length);
  
  
  if(connect(s, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
    perror("connect");
    return(NULL);
  }

  read(s, (char *) ivob, sizeof(vob_t));

  close(s);
  
  return(ivob);
}

void probe_net(info_t *ipipe)
{
  
  ivob = (vob_t *) malloc(sizeof(vob_t));  

  if(probe_host(ipipe->name)==NULL) {
    ipipe->error=1;
    return;
  }
   
  // copy relevant information

  ipipe->probe_info->width  = ivob->ex_v_width;
  ipipe->probe_info->height = ivob->ex_v_height;
  ipipe->probe_info->fps = ivob->fps;
  
  ipipe->probe_info->track[0].samplerate = ivob->a_rate;
  ipipe->probe_info->track[0].chan = ivob->a_chan;
  ipipe->probe_info->track[0].bits = ivob->a_bits;
  ipipe->probe_info->track[0].format = ivob->ex_a_codec;
  
  ipipe->probe_info->magic = TC_MAGIC_SOCKET;

  if(ipipe->probe_info->track[0].chan>0) ipipe->probe_info->num_tracks=1;

  ipipe->probe_info->codec = (ivob->im_v_codec==CODEC_RGB) ? TC_CODEC_RGB:TC_CODEC_YV12;
  
  free(ivob);

  return;  
  
}

#endif
