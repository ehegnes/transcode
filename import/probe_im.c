/*
 *  probe_im.c
 *
 *  Copyright (C) Thomas Oestreich - June 2001
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

#include <stdlib.h>
#include <stdio.h>

#ifdef HAVE_IMAGEMAGICK
/* Note: because of ImageMagick bogosity, this must be included first, so
 * we can undefine the PACKAGE_* symbols it splats into our namespace.
 * However, since we need config.h to find out whether we can include it
 * in the first place, we also have to undefine the symbols beforehand. */
# undef PACKAGE_BUGREPORT
# undef PACKAGE_NAME
# undef PACKAGE_STRING
# undef PACKAGE_TARNAME
# undef PACKAGE_VERSION
# include <magick/api.h>
# undef PACKAGE_BUGREPORT
# undef PACKAGE_NAME
# undef PACKAGE_STRING
# undef PACKAGE_TARNAME
# undef PACKAGE_VERSION
#endif

#include "transcode.h"
#include "tcinfo.h"
#include "ioaux.h"
#include "tc.h"
#include "libtc/libtc.h"

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
	tc_log_error(__FILE__, "no support for ImageMagick compiled - exit.");
	ipipe->error=1;
	return;
}

#endif

