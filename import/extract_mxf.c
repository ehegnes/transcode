/*
 *  extract_mxf.c
 *
 *  Copyright (C) Tilmann Bitterberg - October 2003
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

#undef DDBUG
//#define DDBUG

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>

#include "avilib.h"
#include "ioaux.h"
#include "aux_pes.h"
#include "tc.h"

static int verbose;

/* ------------------------------------------------------------ 
 *
 * extract thread
 *
 * magic: TC_MAGIC_MXF
 *
 * ------------------------------------------------------------*/


void extract_mxf(info_t *ipipe)
{
    import_exit(0);
}

/* Probing */
void probe_mxf(info_t *ipipe)
{
}
