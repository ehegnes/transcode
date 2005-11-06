/*
 *  probe_im.c
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_IMAGEMAGICK
/* ImageMagick leaves these defined, grr */
# undef PACKAGE_BUGREPORT
# undef PACKAGE_NAME
# undef PACKAGE_TARNAME
# undef PACKAGE_VERSION
# undef PACKAGE_STRING
# include <magick/api.h>
# undef PACKAGE_BUGREPORT
# undef PACKAGE_NAME
# undef PACKAGE_TARNAME
# undef PACKAGE_VERSION
# undef PACKAGE_STRING
#endif

#include "ioaux.h"
#include "tc.h"

#ifdef HAVE_IMAGEMAGICK

void probe_im(info_t *ipipe)
{
	ExceptionInfo exception_info;
	Image* image;
	ImageInfo* image_info;

	InitializeMagick("");

	GetExceptionInfo(&exception_info);
	image_info = CloneImageInfo((ImageInfo*)NULL);
	strlcpy(image_info->filename, ipipe->name, MaxTextExtent);
	image = ReadImage(image_info, &exception_info);
	if (image == NULL)
	{
		MagickError(exception_info.severity, exception_info.reason, exception_info.description);
		DestroyImageInfo(image_info);
		ipipe->error=1;
		return;
	}
  
	/* read all video parameter from input file */
	ipipe->probe_info->width = image->columns;
	ipipe->probe_info->height = image->rows;

	/* slide show? */
	ipipe->probe_info->fps = 1;
  
	ipipe->probe_info->magic = ipipe->magic;
	ipipe->probe_info->codec = TC_CODEC_RGB;

	DestroyImage(image);
	DestroyImageInfo(image_info);

	return;
}

#else   // HAVE_IMAGEMAGICK

void probe_im(info_t *ipipe)
{
	fprintf(stderr, "(%s) no support for ImageMagick compiled - exit.\n", __FILE__);
	ipipe->error=1;
	return;
}

#endif

