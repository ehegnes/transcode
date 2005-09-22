/*
 * Copyright (C) Thomas �streich - June 2001
 *
 * This file is part of transcode, a video stream processing tool
 *
 * --> deinterlace code taken from the xine project:
 * Copyright (C) 2001 the xine project
 *
 * transcode is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * Deinterlace routines by Miguel Freitas
 * based of DScaler project sources (deinterlace.sourceforge.net)
 *
 * Currently only available for Xv driver and MMX extensions
 */

#ifndef __DEINTERLACE_H__
#define __DEINTERLACE_H__

void deinterlace_bob_yuv_mmx( uint8_t *pdst, uint8_t *psrc,
			      int width, int height );

#include "aclib/mmx.h"

#endif
