/*
 *  import_xvid.c
 *
 *  Copyright (C) Thomas Östreich - January 2002
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
#include <unistd.h>
#include <string.h>

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#else
# ifdef SYSTEM_DARWIN
#  include "../libdldarwin/dlfcn.h"
# endif
#endif

#include "transcode.h"
#include "ioaux.h"

#include "../export/xvid2.h"

#define MOD_NAME    "decode_xvid"

static int verbose_flag=TC_QUIET;
static int frame_size=0;

static int (*XviD_decore)(void *para0, int opt, void *para1, void *para2);
static int (*XviD_init)(void *para0, int opt, void *para1, void *para2);
static void *XviD_decore_handle=NULL;
static void *handle=NULL;

static int global_colorspace;

static int x_dim, y_dim;

#define XVID_SHARED_LIB_NAME "libxvidcore.so"

static int xvid2_init(char *path) {

#ifdef __FreeBSD__
	const
#endif    
		char *error;
	char modules[6][TC_BUF_MAX];
	char *module;
	int i;
	

	/* First we build all lib names we will try to load
	 *  - xvid3 decoders to have bframe support
	 *  - then xvid2 decoders
	 *  - bare soname as a fallback */
	sprintf(modules[0], "%s/%s.%d", path, XVID_SHARED_LIB_NAME, 3);
	sprintf(modules[1], "%s.%d", XVID_SHARED_LIB_NAME, 3);
	sprintf(modules[2], "%s/%s.%d", path, XVID_SHARED_LIB_NAME, 2);
	sprintf(modules[3], "%s.%d", XVID_SHARED_LIB_NAME, 2);
	sprintf(modules[4], "%s/%s", path, XVID_SHARED_LIB_NAME);
	sprintf(modules[5], "%s", XVID_SHARED_LIB_NAME);

	for(i=0; i<6; i++) {
		module = modules[i];

		if(verbose_flag & TC_DEBUG)
			fprintf(stderr,	"[%s] Trying to load shared lib %s\n",
				MOD_NAME, module);

		/* Try loading the shared lib */
		handle = dlopen(modules[i], RTLD_GLOBAL| RTLD_LAZY);

		/* Test wether loading succeeded */
		if(handle != NULL)
			goto so_loaded;
	}

	/* None of the modules were available */
	fprintf(stderr, dlerror());
	return(-1);

 so_loaded:
	if(verbose_flag & TC_DEBUG)
		fprintf(stderr,	"[%s] Using shared lib %s\n",
			MOD_NAME, module);

	/* Import the XviD init entry point */
	XviD_init   = dlsym(handle, "xvid_init");
    
	/* Something went wrong */
	if((error = dlerror()) != NULL)  {
		fprintf(stderr, error);
		return(-1);
	}

	/* Import the XviD encoder entry point */
	XviD_decore = dlsym(handle, "xvid_decore");

	/* Something went wrong */
	if((error = dlerror()) != NULL)  {
		fprintf(stderr, error);
		return(-1);
	}

	return(0);

}

//temporary video buffer
static char *in_buffer;
static char *out_buffer;
#define BUFFER_SIZE SIZE_RGB_FRAME

static unsigned char *bufalloc(size_t size)
{

#ifdef HAVE_GETPAGESIZE
    long buffer_align=getpagesize();
#else
    long buffer_align=0;
#endif

    char *buf = malloc(size + buffer_align);

    long adjust;

    if (buf == NULL) {
	fprintf(stderr, "(%s) out of memory", __FILE__);
    }

    adjust = buffer_align - ((long) buf) % buffer_align;

    if (adjust == buffer_align)
	adjust = 0;

    return (unsigned char *) (buf + adjust);
}

/* ------------------------------------------------------------ 
 *
 * open stream
 *
 * ------------------------------------------------------------*/

void decode_xvid(decode_t *decode)
{
    XVID_INIT_PARAM xinit;
    XVID_DEC_PARAM xparam;
    int xerr;
    char *codec_str;

    XVID_DEC_FRAME xframe;
    long bytes_read=0;
    long frame_length=0;
    char *mp4_ptr=NULL;

    codec_str = "DIVX"; // XXX:

    if(strlen(codec_str)==0) {
	printf("invalid AVI file codec\n");
	goto error; 
    }
    if (!strcasecmp(codec_str, "DIV3") ||
	    !strcasecmp(codec_str, "MP43") ||
	    !strcasecmp(codec_str, "MPG3") ||
	    !strcasecmp(codec_str, "AP41")) {
	fprintf(stderr, "[%s] The XviD codec does not support MS-MPEG4v3 " \
		"(aka DivX ;-) aka DivX3).\n", MOD_NAME);
	goto error;
    }

    //load the codec
    if(xvid2_init(MOD_PATH)<0) {
	printf("failed to init Xvid codec");
	goto error; 
    }

    xinit.cpu_flags = 0;
    XviD_init(NULL, 0, &xinit, NULL);

    //important parameter
    xparam.width = decode->width;
    xparam.height = decode->height;
    x_dim = xparam.width;
    y_dim = xparam.height;

    xerr = XviD_decore(NULL, XVID_DEC_CREATE, &xparam, NULL);

    if(xerr == XVID_ERR_FAIL) {
	printf("codec open error");
	goto error;
    }
    XviD_decore_handle=xparam.handle;

    switch(decode->format) {
	case TC_CODEC_RGB:
	    global_colorspace = XVID_CSP_RGB24 | XVID_CSP_VFLIP;
	    frame_size = xparam.width * xparam.height * 3;
	    break;
	case TC_CODEC_YV12:
	    global_colorspace = XVID_CSP_YV12;
	    frame_size = (xparam.width * xparam.height * 3)/2;
	    break;
    }

    if ((in_buffer = bufalloc(BUFFER_SIZE))==NULL) {
	perror("out of memory");
	goto error;
    } else {
	memset(in_buffer, 0, BUFFER_SIZE);  
    }
    if ((out_buffer = bufalloc(BUFFER_SIZE))==NULL) {
	perror("out of memory");
	goto error;
    } else {
	memset(out_buffer, 0, BUFFER_SIZE);  
    }

    /* ------------------------------------------------------------ 
     *
     * decode  stream
     *
     * ------------------------------------------------------------*/


    bytes_read = p_read(decode->fd_in, (char*) in_buffer, BUFFER_SIZE);
    mp4_ptr = in_buffer;

    do {
	int mp4_size = (in_buffer + BUFFER_SIZE - mp4_ptr);

	if( bytes_read < 0)
	    goto error; 

	// HOW? if (key) param->attributes |= TC_FRAME_IS_KEYFRAME;

	/* buffer more than half empty -> Fill it */
	if (mp4_ptr > in_buffer + BUFFER_SIZE/2) {
	    int rest = (in_buffer + BUFFER_SIZE - mp4_ptr);
	    fprintf(stderr, "FILL rest %d\n", rest);

	    /* Move data if needed */
	    if (rest)
		memcpy(in_buffer, mp4_ptr, rest);

	    /* Update mp4_ptr */
	    mp4_ptr = in_buffer; 

	    /* read new data */
	    if ( (bytes_read = p_read(decode->fd_in, (char*) (in_buffer+rest), BUFFER_SIZE - rest) ) < 0) {
		fprintf(stderr, "read failed read (%ld) should (%d)\n", bytes_read, BUFFER_SIZE - rest);
		import_exit(1);
	    }
	}

	xframe.bitstream = mp4_ptr;
	xframe.length = mp4_size;
	xframe.stride = x_dim;
	xframe.image = out_buffer;
	xframe.colorspace = global_colorspace;
	xframe.general = 0;

	xerr = XviD_decore(XviD_decore_handle, XVID_DEC_DECODE, &xframe, NULL);
	if (xerr != XVID_ERR_OK) {
	    //fprintf(stderr, "[%s] frame decoding failed. Perhaps you're trying to "
            //                "decode MS-MPEG4v3 (aka DivX ;-) aka DivX3)?\n", MOD_NAME);
	    goto out;
	}
	frame_length = xframe.length;
	mp4_ptr += frame_length;

//	fprintf(stderr, "[%s] decoded frame (%ld) (%d)\n", MOD_NAME, frame_length, frame_size);

	if (p_write (decode->fd_out, (char *)out_buffer, frame_size) != frame_size) {
	    fprintf(stderr, "writeout failed\n");
	    goto error;
	}

    } while (1);


    goto out;


error:
out:
    xerr = XviD_decore(XviD_decore_handle, XVID_DEC_DESTROY, NULL, NULL);
    if (xerr == XVID_ERR_FAIL)
	printf("encoder close error");

    //remove codec
    dlclose(handle);

    import_exit(0); 
}

