/*
 *  decode_a52.c
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
#include <string.h>
#include <sys/errno.h>
#include <errno.h>
#include <unistd.h>
#include <dlfcn.h>

#include "transcode.h"
#include "ioaux.h"

static int verbose;
static char *mod_path=MOD_PATH;

#define MODULE "a52_decore.so"

// dl stuff
static int (*a52_decore)(info_t *ipipe);
static void *handle;
static char module[TC_BUF_MAX];

int a52_init(char *path) {
#ifdef __FreeBSD
    const
#endif    
    char *error;

    sprintf(module, "%s/%s", path, MODULE);
  
    if(verbose & TC_DEBUG) 
	fprintf(stderr, "loading external module %s\n", module); 

    // try transcode's module directory

    handle = dlopen(module, RTLD_NOW); 
    
    
    if (!handle) {
      
      //try the default:

      //      handle = dlopen(MODULE, RTLD_GLOBAL| RTLD_LAZY);

      if (!handle) {
	fputs (dlerror(), stderr);
	return(-1);
      }
    }
    
    a52_decore = dlsym(handle, "a52_decore");   
    
    if ((error = dlerror()) != NULL)  {
      fputs(error, stderr);
      return(-1);
    }

    return(0);
}

/* ------------------------------------------------------------ 
 *
 * decoder thread
 *
 * ------------------------------------------------------------*/


void decode_a52(info_t *ipipe)
{

  verbose = ipipe->verbose;
  
  //load the codec
  if(a52_init(mod_path)<0) {
    printf("failed to init ATSC A-52 stream decoder");
    import_exit(1);
  }
  
  a52_decore(ipipe);
  
  //remove codec
  dlclose(handle);
  
  import_exit(0);
}
