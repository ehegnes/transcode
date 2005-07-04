/*
 *  probe_pvn.c
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

#include "ioaux.h"
#include "tc.h"

#include "pvn.h"

static PVNParam inParams;
FILE *fd;

void probe_pvn(info_t *ipipe)
{
	fd=fopen(ipipe->name, "rb");
	if(fd == NULL)
	  return;

	if(readPVNHeader(fd, &inParams) == INVALID)
        {
          ipipe->error=1;
          fclose(fd);
	  return;
        }
  
	ipipe->probe_info->width = inParams.width;
	ipipe->probe_info->height = inParams.height;
	ipipe->probe_info->fps = inParams.framerate;
	ipipe->fps = inParams.framerate;
	ipipe->probe_info->codec = TC_CODEC_RGB;
	ipipe->probe_info->magic = ipipe->magic;
//	ipipe->probe_info->asr = 
//	ipipe->probe_info->frc = 

	ipipe->probe_info->frames = inParams.depth;

        fclose(fd); 
	return;
}
