/*
 *  info_server.c
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

static size_t pp_write (int fd, char *buf, size_t len)
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

void server_thread(vob_t *vob)
{

#ifdef NET_STREAM
  
  struct sockaddr_in sin, fsin;  
  
  int port;
  int fromlen;
  int on = 1;
  int ans;
  
  // open and bind to net socket 
  
  if((port = socket(AF_INET, SOCK_STREAM, 0)) <0) {
    perror("socket");
    return;
  }
  
  bzero((char*)&sin, sizeof(sin));
  
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr= htonl(INADDR_ANY); 
  sin.sin_port = htons(TC_DEFAULT_PPORT);
  
  // from squid: avoid blocking of port on exit
  if (setsockopt(port, SOL_SOCKET, SO_REUSEADDR, (char *) &on, 
		 sizeof(on)) < 0) {
    perror("setsockopt (SO_REUSEADDR)");
    return;
  }
  
  if(bind(port, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
    if(verbose & TC_DEBUG) printf("[%s] server thread: failed - address already in use\n", PACKAGE);
    return;
  }
  
  if(verbose & TC_DEBUG) printf("[%s] server thread: waiting for clients to connect ...\n", PACKAGE);
  
  for (;;) {
    
    if(listen(port, 2) < 0) {
      perror("listen");
      return;
    }
    
    fromlen=sizeof(fsin);
    
    if((ans = accept(port, (struct sockaddr *)&fsin, &fromlen)) < 0) {
      perror("accept");
      return;
    }
    
    if(pp_write(ans, (char *) vob, sizeof(vob_t))!= sizeof(vob_t)) {
      perror("write");
      return;
    }
    
    if(verbose & TC_DEBUG) printf("(%s) server thread: client connected - sendinf info\n", __FILE__);
    
    close(ans);
  } // get next connection

#endif

}
