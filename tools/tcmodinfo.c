/*
 *  tcmodinfo.c
 *
 *  Copyright (C) Tilmann Bitterberg - August 2002
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
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
#  include "libdldarwin/dlfcn.h"
# endif
#endif

#include "dl_loader.h"
#include "framebuffer.h"
#include "transcode.h"
#include "filter.h"
#include "video_trans.h"

#include "tcmodule-core.h"

#define EXE "tcmodinfo"
#define SIZE 8192 //Buffersize

#define TYPE_UN 0x0
#define TYPE_IM 0x1
#define TYPE_FI 0x2
#define TYPE_EX 0x4

void version(void)
{
    printf("%s (%s v%s) (C) 2001-2003 Tilmann Bitterberg\n", EXE, PACKAGE, VERSION);
}

static void usage(int status)
{
    version();
    fprintf(stderr, "\nUsage: %s [options]\n", EXE);
    fprintf(stderr, "\t -i name           Module name information (like \'smooth\')\n");
    fprintf(stderr, "\t -p                Print the compiled-in MOD_PATH\n");
    fprintf(stderr, "\t -d verbosity      verbosity mode [0]\n");
    fprintf(stderr, "\t -m path           Use PATH as MOD_PATH\n");
    fprintf(stderr, "\t -s socket         Connect to transcode socket\n");
    //fprintf(stderr, "\t -t type           Type of module (filter, import, export)\n");
    //fprintf(stderr, "\t -f flavour        Flavour of module (video, audio)\n");
    fprintf(stderr, "\n");
    exit(status);
}


static char module[TC_BUF_MAX];
filter_t filter[MAX_FILTER];
vob_t vob;

// dependencies
// Yeah, this sucks
vob_t *tc_get_vob() {return(&vob);}
int verbose  = 0;
int rgbswap  = 0;
int tc_accel = -1;    //acceleration code
int flip = 0;
int max_frame_buffer=0;
int tc_x_preview = 32;
int tc_y_preview = 32;
int gamma_table_flag = 0;
int tc_socket_msgchar;
int tc_socket_msg_lock;
void tc_socket_config(void);
void tc_socket_disable(void);
void tc_socket_enable(void);
void tc_socket_list(void);
void tc_socket_load(void);
void tc_socket_parameter(void);
void tc_socket_preview(void);
void tc_socket_config(void) {}
void tc_socket_disable(void) {}
void tc_socket_enable(void) {}
void tc_socket_list(void) {}
void tc_socket_load(void) {}
void tc_socket_parameter(void) {}
void tc_socket_preview(void) {}

#ifndef HAVE_STRLCPY
size_t strlcpy(char *dst, const char *src, size_t size) { return(0); }
#endif

int (*TCV_export)(int opt, void *para1, void *para2);
int (*TCA_export)(int opt, void *para1, void *para2);
int (*TCV_import)(int opt, void *para1, void *para2);
int (*TCA_import)(int opt, void *para1, void *para2);

int tcv_import(int a, void *b, void *c) {
    return 0;
}

static void *tcmodinfo_load_module(char *mod_name, char *mod_path, int mode)
{
#ifdef SYS_BSD
  const
#endif
  char *error;
  void *handle;

  if(mode & TC_EXPORT) {

    tc_snprintf(module, sizeof(module), "%s/export_%s.so", ((mod_path==NULL)? TC_DEFAULT_MOD_PATH:mod_path), mod_name);

    if(verbose & TC_DEBUG)
      printf("loading %s export module %s\n", ((mode & TC_VIDEO)? "video": "audio"), module);

    handle = dlopen(module, RTLD_GLOBAL| RTLD_LAZY);

    if (!handle) {
      fputs (dlerror(), stderr);
      fprintf(stderr, "\n(%s) loading \"%s\" failed\n", __FILE__, module);
      return(NULL);
    }

    if(mode & TC_VIDEO) {
      TCV_export = dlsym(handle, "tc_export");
      if ((error = dlerror()) != NULL)  {
	fputs(error, stderr);
	return(NULL);
      }
    }

    if(mode & TC_AUDIO) {
      TCA_export = dlsym(handle, "tc_export");
      if ((error = dlerror()) != NULL)  {
	fputs(error, stderr);
	return(NULL);
      }
    }

    return(handle);
  }


  if(mode & TC_IMPORT) {

    tc_snprintf(module, sizeof(module), "%s/import_%s.so", ((mod_path==NULL)? TC_DEFAULT_MOD_PATH:mod_path), mod_name);

    //if(verbose & TC_DEBUG)
      printf("loading %s import module %s\n", ((mode & TC_VIDEO)? "video": "audio"), module);

    handle = dlopen(module, RTLD_GLOBAL| RTLD_LAZY);

    if (!handle) {
      fputs (dlerror(), stderr);
      fputs ("\n", stderr);
      return(NULL);
    }

    if(mode & TC_VIDEO) {
      TCV_import = dlsym(handle, "tc_import");
      if ((error = dlerror()) != NULL)  {
	fputs(error, stderr);
	fputs ("\n", stderr);
	return(NULL);
      }
    }


    if(mode & TC_AUDIO) {
      TCA_import = dlsym(handle, "tc_import");
      if ((error = dlerror()) != NULL)  {
	fputs(error, stderr);
	fputs ("\n", stderr);
	return(NULL);
      }
    }

    return(handle);
  }

  // wrong mode?
  return(NULL);
}


static int load_plugin(char *path, int id) {
#ifdef SYS_BSD
  const
#endif
  char *error;

  int n;

  //replace "=" by "/0" in filter name

  if(!filter[id].name) return(-1);

  filter[id].options=NULL;

  for(n=0; n<strlen(filter[id].name); ++n) {
    if(filter[id].name[n]=='=') {
      filter[id].name[n]='\0';
      filter[id].options=filter[id].name+n+1;
      break;
    }
  }

  tc_snprintf(module, sizeof(module), "%s/filter_%s.so", path, filter[id].name);

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
    fputs("\n", stderr);
    return(-1);
  }

  return(0);
}

static void do_connect_socket (char *socketfile)
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
    strlcpy(server.sun_path, socketfile, sizeof(server.sun_path));

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

    int ch;
    char *filename=NULL;
    char modpath[]=MOD_PATH;
    char *options = malloc (8192);
    int print_mod = 0;
    int connect_socket = 0;
    char *socketfile = NULL;
    char *newmodpath = NULL;
    int mod_type = TYPE_FI;
    int flags = TC_VIDEO;

    vframe_list_t ptr;

    memset (options, 0, 8192);
    memset (&vob, 0, sizeof(vob));
    memset (&ptr, 0, sizeof(ptr));

    ac_init(AC_ALL);

    if(argc==1) usage(1);

    while ((ch = getopt(argc, argv, "d:i:?vhpm:s:t:f:")) != -1)
    {

	switch (ch) {

    case 'd':
	    if(optarg[0]=='-') usage(1);
        verbose = atoi(optarg);
        break;

	case 'i':

	    if(optarg[0]=='-') usage(1);
	    filename = optarg;
	    break;

	case 'm':
	    newmodpath=optarg;
	    break;

	case 'f':
	    if (!optarg) { usage(1); }

	    if      (!strcmp(optarg, "audio"))
		flags = TC_AUDIO;
	    else if (!strcmp(optarg, "video"))
		flags = TC_VIDEO;
	    else
		flags = 0;
	    break;

	case 't':
	    if (!optarg) { usage(1); }

	    if      (!strcmp(optarg, "filter"))
		mod_type = TYPE_FI;
	    else if (!strcmp(optarg, "import"))
		mod_type = TYPE_IM;
	    else if (!strcmp(optarg, "export"))
		mod_type = TYPE_EX;
	    else
		mod_type = TYPE_UN;
	    break;

	case 's':
	    if(optarg[0]=='-') usage(1);
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
	    usage(0);
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

  if (mod_type == TYPE_UN) {
      fprintf(stderr, "[%s] Unknown Type (not in filter, import, export)\n", EXE);
  }

  if (filename==NULL) usage(1);

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
  vob.video_in_file   = "/dev/zero";

  //fprintf(stderr, "Module is (%s/filter_%s) (%d)\n", modpath, filename, getpid());

  if (mod_type & TYPE_FI) {
    int ret = 0, out = 0;
    TCFactory factory = NULL;

    /* needed by filter modules */
    TCVHandle tcv_handle = tcv_init();

    /* first of all, try using new module system */
    TCModule module = NULL;

    factory = tc_new_module_factory(((newmodpath) ?newmodpath :modpath), verbose);
    module = tc_new_module(factory, "filter", filename);
    if (module != NULL) {
        if (verbose >= TC_DEBUG) {
            tc_log_info(__FILE__, "using new module system");
        }
        /* overview and options */
        puts(tc_module_configure(module, "help"));
        /* module capabilities */
        tc_module_show_info(module, verbose);
        /* current configuration */
        puts("\ndefault module configuration:");
        puts(tc_module_configure(module, ""));
        tc_del_module(factory, module);
        out = 0;
    } else {
        if (verbose >= TC_DEBUG) {
            tc_log_info(__FILE__, "using old module system");
        }
        /* ok, fallback to old module system */
        filter[0].name = malloc(256);
        tc_snprintf (filter[0].name, 256, "%s", filename);

        if (load_plugin ( (newmodpath?newmodpath:modpath), 0) == 0) {
            options[0] = 'h';
            options[1] = 'e';
            options[2] = 'l';
            options[3] = 'p';
            ptr.tag = TC_FILTER_INIT;
            if ( (ret = filter[0].entry(&ptr, options))) {
                out=1;
            }
            //fprintf(stderr, "[%s]: (INIT) Filter %s returned (%d)\n", EXE, filename, ret);

            memset (options, 0, 8192);
            ptr.tag = TC_FILTER_GET_CONFIG;
            ret = filter[0].entry(&ptr, options);
            //fprintf(stderr, "[%s]: (CONF) Filter %s returned (%d)\n", EXE, filename, ret);
        }

        fputs("START\n", stdout);
        if (ret == 0) {
            fputs(options, stdout);
            out = 0;
        } else {
            out = 2;
        }
        fputs("END\n", stdout);
    }

    ret = tc_del_module_factory(factory);
    tcv_free(tcv_handle);
    return (out);
  }

  if (mod_type & TYPE_IM) {
      void *handle = NULL;

      transfer_t import_para;

      memset(&import_para, 0, sizeof(transfer_t));

      // start audio stream
      import_para.flag=flags;

      if ( (handle = tcmodinfo_load_module( filename, (newmodpath?newmodpath:modpath), TC_IMPORT | flags)) == 0){
	  return 1;
      } else
	  printf("Na hallo -- ok\n");

      if (flags & TC_VIDEO) {
	  if(TCV_import(TC_IMPORT_OPEN, &import_para, &vob)<0) {
	      fprintf(stderr, "video import module error: OPEN failed\n");
	      return(-1);
	  }
	  fputs("END\n", stdout);
      } else if (flags & TC_AUDIO) {
      }


  }


  return(1);
}

#include "libtc/static_optstr.h"

/* vim: sw=4
 */
