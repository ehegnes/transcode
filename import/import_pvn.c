/*
 *  import_pvn.c
 *
 *  Copyright (C) Jacob (Jack) Gryn - July 2004
 *
 *  Based on import_im module by Thomas Östreich - June 2001
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
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "transcode.h"

#define MOD_NAME    "import_pvn.so"
#define MOD_VERSION "v0.11 (2004-07-23)"
#define MOD_CODEC   "(video) PVN"

static int verbose_flag=TC_QUIET;
static int capability_flag=TC_CAP_RGB|TC_CAP_VID|TC_CAP_AUD;

#define MOD_PRE pvn
#include "import_def.h"

#include "pvn.h"

#define MAX_BUF 1024
char import_cmd_buf[MAX_BUF];

static FILE *fd=NULL;

static PVNParam inParams, tmpParams;
static unsigned char *buf=NULL;
static unsigned char *tmpBuf=NULL;
static long tmpBufSize=0;
static long bufSize=0;

static unsigned int inbufFormat;

/* ------------------------------------------------------------ 
 *
 * open stream
 *
 * ------------------------------------------------------------*/

MOD_open
{
  param->fd = NULL;

  if(param->flag == TC_AUDIO) {
      return(0);
  }
  
  if(param->flag == TC_VIDEO) {

    fd=fopen(vob->video_in_file, "rb");
    if(fd == NULL)
      return(TC_IMPORT_ERROR);

    if(readPVNHeader(fd, &inParams) == INVALID)
      return(TC_IMPORT_ERROR);

    bufSize=calcPVNPageSize(inParams);
    buf=(unsigned char *)malloc(bufSize);

    PVNParamCopy(&tmpParams, &inParams);

    if(inParams.magic[3] == 'f')
      inbufFormat=FORMAT_FLOAT;
    else if(inParams.magic[3] == 'd')
      inbufFormat=FORMAT_DOUBLE;
    else if(inParams.magic[2] == '4')
    {
      inbufFormat=FORMAT_BIT;
      tmpParams.magic[2]='5';
    }
    else if(inParams.magic[3] == 'b')
      inbufFormat=FORMAT_INT;
    else if(inParams.magic[3] == 'a')
      inbufFormat=FORMAT_UINT;
    else
    {
      fprintf(stderr, "Unknown PVN format");
      return(TC_IMPORT_ERROR);
    }

    tmpParams.magic[3]='a';
    tmpParams.maxcolour=8.0;
    tmpBufSize=calcPVNPageSize(tmpParams);
    tmpBuf=(unsigned char *)malloc(tmpBufSize);

    if(inParams.framerate==0)
    {
      fprintf(stderr,"Setting Frame Rate to default of 15\n");
      inParams.framerate=15;
    }

    vob->has_audio=0;
    vob->decolor=(inParams.magic[2]=='6') ? 1:0;
    vob->fps         = inParams.framerate; 
    vob->im_v_height = inParams.height;
    vob->im_v_width  = inParams.width;
    vob->im_v_size   = tmpBufSize;
    vob->v_bpp    = 8; // may need to leave this out
    return(0);
  }

  return(TC_IMPORT_ERROR);
}


/* ------------------------------------------------------------ 
 *
 * decode  stream
 *
 * ------------------------------------------------------------*/

MOD_decode 
{
    int i;
    param->size=tmpBufSize;
    if(fread(buf, bufSize, 1, fd)==0) return(TC_IMPORT_ERROR);

    if(inbufFormat == FORMAT_BIT)
      inParams.maxcolour=inParams.width; // if BIT format, bufconvert requires width in the maxcolour field
    if(bufConvert(buf, bufSize, inbufFormat, inParams.maxcolour,tmpBuf,tmpBufSize,FORMAT_UINT,tmpParams.maxcolour) != OK)
    {
      fprintf(stderr, "Buffer conversion error!\n");
      return(TC_IMPORT_ERROR);
    }

    if (inParams.magic[2]=='6')
      tc_memcpy(param->buffer, tmpBuf, tmpBufSize);
    else
    {
      param->size*=3;
      for(i=0; i < inParams.height*inParams.width; i++)
      {
        param->buffer[3*i + 0]=tmpBuf[i];
        param->buffer[3*i + 1]=tmpBuf[i];
        param->buffer[3*i + 2]=tmpBuf[i];
      }
    }
    param->attributes |= TC_FRAME_IS_KEYFRAME;

    return(0);
}

/* ------------------------------------------------------------ 
 *
 * close stream
 *
 * ------------------------------------------------------------*/

MOD_close
{  
  if(buf!=NULL)
    free(buf);
  if(tmpBuf!=NULL)
    free(tmpBuf);
  if(fd != NULL)
    fclose(fd);

  return(0);
}


