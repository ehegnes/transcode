/*
 *  tcmodinfo.c
 *
 *  Copyright (C) Tilmann Bitterberg - August 2002
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

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "../src/framebuffer.h"
#include "../src/transcode.h"
#include "../src/filter.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#else
# ifdef SYSTEM_DARWIN
#  include "../libdldarwin/dlfcn.h"
# endif
#endif

#define EXE "tcmodinfo"
#define SIZE 8192 //Buffersize

void version()
{
    printf("%s (%s v%s) (C) 2001-2003 Thomas Östreich\n", EXE, PACKAGE, VERSION);
}

void usage()
{
    version();
    fprintf(stderr, "\nUsage: %s [options]\n", EXE);
    fprintf(stderr, "\t -i name           Module name information (like \'smooth\')\n");
    fprintf(stderr, "\t -p                Print the compiled-in MOD_PATH\n");
    fprintf(stderr, "\t -m path           Use PATH as MOD_PATH\n");
    fprintf(stderr, "\t -s socket         Connect to transcode socket\n");
    fprintf(stderr, "\n");
    exit(0);
}


int (*tc_filter)(void *ptr, void *opt);
static char module[TC_BUF_MAX];
filter_t filter[MAX_FILTER];
vob_t vob;

// dependencies
// Yeah, this sucks
vob_t *tc_get_vob() {return(&vob);}
int verbose  = 1;
int rgbswap  = 0;
int tc_accel = -1;    //acceleration code
int flip = 0;

void tc_error(char *string)
{
      fprintf(stderr, "critical error: %s - exit\n", string);
}




int load_plugin(char *path, int id) {
#if defined(__FreeBSD__) || defined(__APPLE__)
  const
#endif    
  char *error;

  int n;

  //replace "=" by "/0" in filter name
  
  if(strlen(filter[id].name)==0) return(-1);

  filter[id].options=NULL;

  for(n=0; n<strlen(filter[id].name); ++n) {
    if(filter[id].name[n]=='=') {
      filter[id].name[n]='\0';
      filter[id].options=filter[id].name+n+1;
      break;
    }
  }
  
  sprintf(module, "%s/filter_%s.so", path, filter[id].name);
  
  // try transcode's module directory
  
  filter[id].handle = dlopen(module, RTLD_LAZY); 

  if (!filter[id].handle) {
    fprintf(stderr, "[%s] loading filter module %s failed\n", EXE, module); 
    if ((error = dlerror()) != NULL) fputs(error, stderr);
    fputs("\n", stderr);
    return(-1);

  } else 
    //fprintf(stderr, "[%s] loading filter module (%d) %s\n", EXE, id, module); 
  
  filter[id].entry = dlsym(filter[id].handle, "tc_filter");   
  
  if ((error = dlerror()) != NULL)  {
    fputs(error, stderr);
    return(-1);
  }

  return(0);
}

void do_connect_socket (char *socketfile)
{
#ifdef NET_STREAM
    int sock, retval;  
    struct sockaddr_un server;  
    char buf[SIZE];
    fd_set rfds;
    struct timeval tv;
    ssize_t n;

    sock = socket(AF_UNIX, SOCK_STREAM, 0); 
    if (sock < 0) { 
	perror("opening stream socket"); 
	exit(1); 
    }  
    server.sun_family = AF_UNIX;  
    strcpy(server.sun_path, socketfile);

    if (connect(sock, (struct sockaddr *) &server, sizeof(struct sockaddr_un)) < 0)
    {  
	close(sock);  
	perror("connecting stream socket");
	exit(1);
    } 

    while (1) {
	/* Watch stdin (fd 0) to see when it has input. */
	FD_ZERO(&rfds);
	FD_SET(0, &rfds); // stdin
	FD_SET(sock, &rfds);
	/* Wait up to five seconds. */
	tv.tv_sec = 5;
	tv.tv_usec = 0;

	retval = select(sock+1, &rfds, NULL, NULL, NULL);
	/* Don't rely on the value of tv now! */

	memset(buf, 0, sizeof (buf));

	if (retval>0) {
	    if (FD_ISSET(0, &rfds)) {
		fgets(buf, SIZE, stdin);
	    }
	    if (FD_ISSET(sock, &rfds)) {
		if ( (n = read(sock, buf, SIZE)) < 0) {
		    perror("reading on stream socket");
		    break;
		} else if (n == 0) { // EOF
		    fprintf (stderr, "Server closed connection\n");
		    break;
		}
		printf("%s", buf);
		continue;
	    }
	}

	if (write(sock, buf, sizeof(buf)) < 0) 
	    perror("writing on stream socket");

	memset(buf, 0, sizeof (buf));
	
	if (read(sock, buf, SIZE) < 0) 
	    perror("reading on stream socket");

	printf("%s", buf);

	if (!isatty(0))
	    break;
    }

    close(sock); 
#else
    fprintf(stderr, "No support for Netstreams compiled in\n");
    fflush (stderr);
#endif
}

int main(int argc, char *argv[])
{

    char ch;
    int ret;
    char *filename=NULL;
    char modpath[]=MOD_PATH;
    char *options = malloc (8192);
    int print_mod = 0;
    int connect_socket = 0;
    char *socketfile = NULL;
    char *newmodpath = NULL;

    vframe_list_t ptr;

    memset (options, 0, 8192);
    memset (&vob, 0, sizeof(vob));
    memset (&ptr, 0, sizeof(ptr));


    if(argc==1) usage();

    while ((ch = getopt(argc, argv, "i:?vhpm:s:")) != -1)
    {

	switch (ch) {

	case 'i':

	    if(optarg[0]=='-') usage();
	    filename = optarg;
	    break;

	case 'm':
	    newmodpath=optarg;
	    break;

	case 's':
	    if(optarg[0]=='-') usage();
	    connect_socket = 1;
	    socketfile = optarg;
	    break;

	case 'p':
	    print_mod = 1;
	    break;

	case 'v':

	    version();
	    exit(0);

	case '?':
	case 'h':
	default:
	    usage();
	    exit(0);
	}
    }


  if (print_mod) {
      printf("%s\n", modpath);
      exit (0);
  }

  if (connect_socket) {
      do_connect_socket(socketfile);
      exit (0);
  }


  if(filename==NULL) usage();

  // some arbitrary values for the filters
  vob.fps        = 25.0;
  vob.im_v_width = 32;
  vob.ex_v_width = 32;
  vob.im_v_height= 32;
  vob.ex_v_height= 32;
  vob.im_v_codec = CODEC_YUV;

  vob.a_rate          = 44100; 
  vob.mp3frequency    = 44100;
  vob.a_chan          = 2;
  vob.a_bits          = 16;
  
  //fprintf(stderr, "Module is (%s/filter_%s) (%d)\n", modpath, filename, getpid());

  snprintf (filter[0].name, 256, "%s", filename);

  if (load_plugin ( (newmodpath?newmodpath:modpath), 0) == 0) {
      int out=0;
  
      options[0] = 'h';
      options[1] = 'e';
      options[2] = 'l';
      options[3] = 'p';
      ptr.tag = TC_FILTER_INIT;
      if ( (ret = filter[0].entry(&ptr, options))){
	  out=1;
      }
      //fprintf(stderr, "[%s]: (INIT) Filter %s returned (%d)\n", EXE, filename, ret);

      memset (options, 0, 8192);
      ptr.tag = TC_FILTER_GET_CONFIG;
      ret = filter[0].entry(&ptr, options);
      //fprintf(stderr, "[%s]: (CONF) Filter %s returned (%d)\n", EXE, filename, ret);


      fputs("START\n", stdout);
      if (ret == 0) {
	  fputs(options, stdout);
	  out = 0;
      } else {
	  out = 2;
      }
      fputs("END\n", stdout);
      return (out);
  }

  
  return(1);
}

/* vim: sw=4
 */
