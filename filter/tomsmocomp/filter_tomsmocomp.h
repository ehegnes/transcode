/*
 *  filter_tomsmocomp.c
 *
 *  Filter access code (c) by Matthias Hopf - July 2004
 *  Base code taken from DScaler's tomsmocomp filter (c) 2002 Tom Barry,
 *  ported by Dirk Ziegelmeier for kdetv.
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
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "transcode.h"
#include "framebuffer.h"
#include "optstr.h"
#include "aclib/ac.h"
#include "dscaler_interface.h"


typedef struct {

    int SearchEffort;
    int UseStrangeBob;
    int TopFirst;

    int codec;
    int cpuflags;

    int width;
    int height;
    int size;
    int rowsize;

    uint8_t *frameIn, *framePrev, *frameOut;

    TDeinterlaceInfo DSinfo;

} tomsmocomp_t;


extern int tc_filter(vframe_list_t *ptr, char *options);

