/*
 *  import_yuv.c  (C) Marek B³aszkowski Apr 2002
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

/* Module info:
 * module can read YUV frames from a given dir (-i option)
 * it start to search then encode frame: 0000.yuv, 0001.yuv and so on. 
 * If some frames are missing module try to find a next aviable frame 
 * until MAXFRM frame is count.
 * Remeber to add -V option to transcode (only YUV mode is supported) 
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

#define MOD_NAME    "import_yuv.so"
#define MOD_VERSION "v0.1.0 (2002-04-12)"
#define MOD_CODEC   "(video) YUV files "

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_YUV;

#define MOD_PRE yuv
#include "import_def.h"

#define MAX_BUF 1024
#define MAXFRM 10000    // max count of search/encode missing frames
char import_cmd_buf[MAX_BUF];

static int frm;
static char fname[1024];
static FILE *fd;

/* ------------------------------------------------------------ 
 *
 * open stream
 *
 * ------------------------------------------------------------*/

MOD_open
{

    if(param->flag == TC_VIDEO) {
      
	if(verbose_flag & TC_DEBUG)	printf("yuv start MOD_open video\n");
	frm=0;
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
    
   // if(verbose_flag & TC_DEBUG) printf("(V) read yuv ");
	sprintf(fname,"%s/%04d.yuv",vob->video_in_file,frm);	
	 if (!(fd = fopen(fname,"rb"))){
	 	if(verbose_flag & TC_DEBUG)	fprintf(stderr,
							"warning: missing frame %d, searching next...",frm);
			while (frm < MAXFRM){ 
					frm++;
					sprintf(fname,"%s/%04d.yuv",vob->video_in_file,frm);
					fd=fopen(fname,"rb");
		 			if(fd){
				if(verbose_flag & TC_DEBUG)	fprintf(stderr,"found %d \n",frm);
						 goto cont;
					}
			}
	return(TC_IMPORT_ERROR);
	}
cont:
	fread(param->buffer,param->size,1,fd);	
	frm++;
	if(fd)fclose(fd);
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

    printf("[%s] disconnect video \n", MOD_NAME);
    return(0);
  }
  
  return(TC_IMPORT_ERROR);
}


