/*
 *	import_v4l2.c
 *
 *	By Erik Slagter Sept 2003
 * 
 *	This file is part of transcode, a linux video stream processing tool
 *
 *	transcode is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2, or (at your option)
 *	any later version.
 *
 *	transcode is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with GNU Make; see the file COPYING. If not, write to
 *	the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *
 */

#define _ISOC9X_SOURCE 1

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <sys/soundcard.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>

#include <linux/types.h>

// The v4l2_buffer struct check is because some distributions protect that
// struct in videodev2 with a #ifdef __KERNEL__ (SuSE 9.0)

#if defined(HAVE_LINUX_VIDEODEV2_H) && defined(HAVE_STRUCT_V4L2_BUFFER)
#define _LINUX_TIME_H
#include <linux/videodev2.h>
#else
#include "videodev2.h"
#endif

#include "transcode.h"
#include "aclib/ac.h"

#define MOD_NAME		"import_v4l2.so"
#define MOD_VERSION		"v1.2.1 (2004-01-03)"
#define MOD_CODEC		"(video) v4l2 | (audio) pcm"
#define MOD_PRE			v4l2
#include "import_def.h"

/*
	use se ts=4 for correct layout

	Changelog

	1.0.0	EMS first published version
	1.0.1	EMS	added YUV422 and RGB support
				disable timestamp stuff for now, doesn't work anyways
					as long as tc core doesn't support it.
				missing mute control is not an error.
	1.0.2	EMS	changed parameter passing from -T to -x v4l2=a=x,b=y
				try various (native) capture formats before giving up
	1.0.3	EMS	changed "videodev2.h" back to <linux/videodev2.h>,
				it doesn't work with linux 2.6.0, #defines are wrong.
	        tibit   figure out if the system does have videodev2.h
					gcc-2.95 bugfix
			tibit   check for struct v4l2_buffer
	1.1.0	EMS	added dma overrun protection, use overrun_guard=0 to disable
					this prevents from crashing the computer when all 
					capture buffers are full while capturing, by stopping capturing
					when > 75% of the buffers are filled.
			EMS added YUV422 capture -> YUV420 transcode core conversion
					for those whose cards' hardware downsampling to YUV420 conversion is broken
	1.2.0	EMS added a trick to get a better a/v sync in the beginning:
					don't start audio (which seems always to be started first)
					until video is up and running using a mutex.
					This means that must not use -D anymore.
	1.2.1	EMS added bttv driver to blacklist 'does not support cropping
					info ioctl'
	TODO

	- add more conversion schemes
	- make conversion user-selectable
	- use more mmx/sse/3dnow
*/

#define module "[" MOD_NAME "]: "

typedef enum { resync_none, resync_clone, resync_drop } v4l2_resync_op;
typedef enum { v4l2_fmt_rgb, v4l2_fmt_yuv422packed, v4l2_fmt_yuv420planar } v4l2_fmt_t;
typedef enum { v4l2_param_int, v4l2_param_string, v4l2_param_fp } v4l2_param_type_t;

typedef struct
{
	int				from;
	v4l2_fmt_t		to;
	void			(*convert)(const char *, char *, size_t size, int xsize, int ysize);
	const char *	description;
} v4l2_format_convert_table_t;

typedef struct
{
	v4l2_param_type_t	type;
	const char *		name;
	size_t				length;
	union {
		char *		string;
		int	*		integer;
		double *	fp;
	} value;
} v4l2_parameter_t;

static int verbose_flag		= TC_QUIET;
static int capability_flag	= TC_CAP_RGB | TC_CAP_YUV | TC_CAP_YUV422 | TC_CAP_PCM;

static struct 
{
	void * start;
	size_t length;
} * v4l2_buffers;

static pthread_mutex_t	v4l2_av_start_mutex = PTHREAD_MUTEX_INITIALIZER;
static v4l2_fmt_t		v4l2_fmt;
static v4l2_resync_op	v4l2_video_resync_op = resync_none;

static int	v4l2_saa7134_video = 0;
static int	v4l2_bttv_video = 0;
static int	v4l2_saa7134_audio = 0;
static int	v4l2_overrun_guard = 1;
static int 	v4l2_resync_margin_frames = 0;
static int 	v4l2_resync_interval_frames = 0;
static int 	v4l2_buffers_count;
static int 	v4l2_video_fd = -1;
static int 	v4l2_audio_fd = -1;
static int 	v4l2_video_sequence = 0;
static int 	v4l2_audio_sequence = 0;
static int	v4l2_video_cloned = 0;
static int	v4l2_video_dropped = 0;
static int	v4l2_frame_rate;
static int	v4l2_width = 0;
static int	v4l2_height = 0;

static char * v4l2_resync_previous_frame = 0;

static int	 		(*v4l2_memcpy)(char *, const char *, int) = (int (*)(char *, const char *, int))memcpy;
static void			(*v4l2_format_convert)(const char *, char *, size_t, int, int) = 0;

static void v4l2_convert_rgb24_rgb(const char *, char *, size_t, int, int);
static void v4l2_convert_bgr24_rgb(const char *, char *, size_t, int, int);
static void v4l2_convert_uyvy_yuv422(const char *, char *, size_t, int, int);
static void v4l2_convert_yuyv_yuv422(const char *, char *, size_t, int, int);
static void v4l2_convert_yuyv_yuv420(const char *, char *, size_t, int, int);
static void v4l2_convert_yvu420_yuv420(const char *, char *, size_t, int, int);
static void v4l2_convert_yuv420_yuv420(const char *, char *, size_t, int, int);

static v4l2_parameter_t v4l2_parameters[] =
{
	{ v4l2_param_int, "resync_margin",   0, { .integer = &v4l2_resync_margin_frames }},
	{ v4l2_param_int, "resync_interval", 0, { .integer = &v4l2_resync_interval_frames }},
	{ v4l2_param_int, "overrun_guard",   0, { .integer = &v4l2_overrun_guard }}
};

static v4l2_format_convert_table_t v4l2_format_convert_table[] = 
{
	{ V4L2_PIX_FMT_RGB24, v4l2_fmt_rgb, v4l2_convert_rgb24_rgb, "RGB24 [packed] -> RGB [packed] (no conversion)" },
	{ V4L2_PIX_FMT_BGR24, v4l2_fmt_rgb, v4l2_convert_bgr24_rgb, "BGR24 [packed] -> RGB [packed] (slow conversion)" },

	{ V4L2_PIX_FMT_UYVY, v4l2_fmt_yuv422packed, v4l2_convert_uyvy_yuv422, "UYUV [packed] -> YUV422 [packed] (no conversion)" },
	{ V4L2_PIX_FMT_YUYV, v4l2_fmt_yuv422packed, v4l2_convert_yuyv_yuv422, "YUYV [packed] -> YUV422 [packed] (slow conversion) " },

	{ V4L2_PIX_FMT_YVU420, v4l2_fmt_yuv420planar, v4l2_convert_yvu420_yuv420, "YVU420 [planar] -> YUV420 [planar] (no conversion)" },
	{ V4L2_PIX_FMT_YUV420, v4l2_fmt_yuv420planar, v4l2_convert_yuv420_yuv420, "YUV420 [planar] -> YUV420 [planar] (fast conversion)" },
	{ V4L2_PIX_FMT_UYVY,   v4l2_fmt_yuv420planar, v4l2_convert_yuyv_yuv420,   "UYVY [packed] -> YUV420 [planar] (slow conversion) " },
};

/* ============================================================ 
 * CONVERSION ROUTINES
 * ============================================================*/


static void v4l2_convert_rgb24_rgb(const char * source, char * dest, size_t size, int xsize, int ysize)
{
	size_t mysize = xsize * ysize * 3;

	if(mysize != size)
		fprintf(stderr, module "buffer sizes do not match (%d != %d)\n", (int)size, (int)mysize);

	v4l2_memcpy(dest, source,  mysize);
}

static void v4l2_convert_bgr24_rgb(const char * source, char * dest, size_t size, int xsize, int ysize)
{
	size_t mysize = xsize * ysize * 3;
	int offset;

	if(mysize != size)
		fprintf(stderr, module "buffer sizes do not match (%d != %d)\n", (int)size, (int)mysize);

	for(offset = 0; offset < mysize; offset += 3)
	{
		dest[offset + 0] = source[offset + 2];
		dest[offset + 1] = source[offset + 1];
		dest[offset + 2] = source[offset + 0];
	}
}

static void v4l2_convert_uyvy_yuv422(const char * source, char * dest, size_t size, int xsize, int ysize)
{
	size_t mysize = xsize * ysize * 2;

	if(mysize != size)
		fprintf(stderr, module "buffer sizes do not match (%d != %d)\n", (int)size, (int)mysize);

	v4l2_memcpy(dest, source, mysize);
}

static void v4l2_convert_yuyv_yuv422(const char * source, char * dest, size_t size, int xsize, int ysize)
{
	size_t mysize = xsize * ysize * 2;
	int offset;

	if(mysize != size)
		fprintf(stderr, module "buffer sizes do not match (%d != %d)\n", (int)size, (int)mysize);

	for(offset = 0; offset < mysize; offset += 4)
	{
		dest[offset + 0] = source[offset + 1];
		dest[offset + 1] = source[offset + 0];
		dest[offset + 2] = source[offset + 3];
		dest[offset + 3] = source[offset + 2];
	}
}

static void v4l2_convert_yuyv_yuv420(const char * source, char * dest, size_t dest_size, int xsize, int ysize)
{
	int i, j, w2;
	char * y, * u, * v;

    w2 = xsize / 2;

    // I420

	y = dest;
	v = dest + xsize * ysize;
	u = dest + xsize * ysize * 5 / 4;

	for(i = 0; i < ysize; i += 2)
	{
		for (j = 0; j < w2; j++)
		{
			/* UYVY.  The byte order is CbY'CrY' */

			*u++ = *source++;
			*y++ = *source++;
			*v++ = *source++;
			*y++ = *source++;
		}

		//downsampling

		u -= w2;
		v -= w2;

		/* average every second line for U and V */

		for(j = 0; j < w2; j++)
		{
			int un = *u & 0xff;
			int vn = *v & 0xff;

			un += *source++ & 0xff;
			*u++ = un>>1;

			*y++ = *source++;

			vn += *source++ & 0xff;
			*v++ = vn>>1;

			*y++ = *source++;
		}
	}
}

static void v4l2_convert_yvu420_yuv420(const char * source, char * dest, size_t size, int xsize, int ysize)
{
	size_t mysize = xsize * ysize * 3 / 2;

	if(mysize != size)
		fprintf(stderr, module "buffer sizes do not match (%d != %d)\n", (int)size, (int)mysize);

	v4l2_memcpy(dest, source, mysize);
}

static void v4l2_convert_yuv420_yuv420(const char * source, char * dest, size_t size, int xsize, int ysize)
{
	size_t mysize = xsize * ysize * 3 / 2;
	int yplane_size		= mysize * 4 / 6;
	int uplane_size		= mysize * 1 / 6;

	int yplane_offset	= 0;
	int u1plane_offset	= yplane_size + 0;
	int u2plane_offset	= yplane_size + uplane_size;

	if(mysize != size)
		fprintf(stderr, module "buffer sizes do not match (%d != %d)\n", (int)size, (int)mysize);

	v4l2_memcpy(dest + yplane_offset,  source + yplane_offset,  yplane_size);
	v4l2_memcpy(dest + u1plane_offset, source + u2plane_offset, uplane_size);
	v4l2_memcpy(dest + u2plane_offset, source + u1plane_offset, uplane_size);
}

/* ============================================================ 
 * UTILS
 * ============================================================*/

static inline int min(int a, int b)
{
	return(a < b ? a : b);
}

static int v4l2_mute(int flag)
{
	struct v4l2_control control;

	control.id = V4L2_CID_AUDIO_MUTE;
	control.value = flag;

	if(ioctl(v4l2_video_fd, VIDIOC_S_CTRL, &control) < 0)
		if(verbose_flag & TC_INFO)
			perror(module "VIDIOC_S_CTRL");

	return(1);
}

static int v4l2_video_clone_frame(char *dest, size_t size)
{
	if(!v4l2_resync_previous_frame)
		memset(dest, 0, size);
	else
		v4l2_memcpy(dest, v4l2_resync_previous_frame, size);

	return(1);
}

static void v4l2_save_frame(const char * source, size_t length)
{
	if(!v4l2_resync_previous_frame)
		v4l2_resync_previous_frame = malloc(length);

	v4l2_memcpy(v4l2_resync_previous_frame, source, length);
}

static int v4l2_video_grab_frame(char * dest, size_t length)
{
	static struct v4l2_buffer buffer;
	static int next_buffer_ix = 0;
	int ix;

	// get buffer

	buffer.type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buffer.memory	= V4L2_MEMORY_MMAP;

	if(ioctl(v4l2_video_fd, VIDIOC_DQBUF, &buffer) < 0)
	{
		perror(module "VIDIOC_DQBUF");
		return(0);
	}

	// check sync

	ix	= buffer.index;

	if(next_buffer_ix != ix)
		fprintf(stderr, "\n" module "video frame missed (buffer pool) (%d/%d)\n", ix, next_buffer_ix);

	next_buffer_ix = (ix + 1) % v4l2_buffers_count;

	// copy frame

	if(dest)
		v4l2_format_convert(v4l2_buffers[ix].start, dest, length, v4l2_width, v4l2_height);

	// enqueue buffer again

	if(ioctl(v4l2_video_fd, VIDIOC_QBUF, &buffer) < 0)
	{
		perror(module "VIDIOC_QBUF");
		return(0);
	}

	return(1);
}

static int v4l2_video_count_buffers(void)
{
	struct v4l2_buffer buffer;
	int ix;
	int buffers_filled = 0;

	for(ix = 0; ix < v4l2_buffers_count; ix++)
	{
		buffer.type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buffer.memory	= V4L2_MEMORY_MMAP;
		buffer.index	= ix;

		if(ioctl(v4l2_video_fd, VIDIOC_QUERYBUF, &buffer) < 0)
		{
			perror(module "VIDIOC_QUERYBUF");
			return(-1);
		}

		if(buffer.flags & V4L2_BUF_FLAG_DONE)
			buffers_filled++;
	}

	return(buffers_filled);
}

static void v4l2_parse_options(const char * options_in)
{
	int ix;
	int first;
	char * options;
	char * options_ptr;
	char * option;
	char * result;
	char * name;
	char * value;
	v4l2_parameter_t * pt;

	if(!options_in)
		return;

	options = options_ptr = strdup(options_in);

	if(!options || (!(option = malloc(strlen(options) * sizeof(char)))))
	{
		fprintf(stderr, module "cannot malloc - options not parsed\n");
		return;
	}

	for(first = 1;; first = 0)
	{
		result = strtok_r(first ? options : 0, ":", &options_ptr);

		if(!result)
			break;
		
		name = result;
		value = strchr(result, '=');

		if(!value)
			value = "1";
		else
			*value++ = '\0';

		for(ix = 0; ix < (sizeof(v4l2_parameters) / sizeof(*v4l2_parameters)); ix++)
		{
			pt = &v4l2_parameters[ix];

			if(!strcmp(pt->name, name))
			{
				switch(pt->type)
				{
					case(v4l2_param_int): *(pt->value.integer) = strtoul(value, 0, 10); break;
					case(v4l2_param_fp): *(pt->value.fp) = strtod(value, 0); break;
						case(v4l2_param_string):
					{
						strncpy(pt->value.string, value, pt->length);
						pt->value.string[pt->length] = '\0';
						break;
					}
				}

				break;
			}
		}
	}

	free(options);
}

/* ============================================================ 
 * V4L2 CORE
 * ============================================================*/

int v4l2_video_init(int layout, const char * device, int width, int height, int fps,
		const char * options)
{
	int ix, found;
	v4l2_format_convert_table_t * fcp;

	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_format format;
	struct v4l2_requestbuffers reqbuf;
	struct v4l2_buffer buffer;
	struct v4l2_capability caps;
	struct v4l2_streamparm streamparm;
	v4l2_std_id std;

	if(pthread_mutex_trylock(&v4l2_av_start_mutex))
	{
		perror(module "lock av mutex");
		return(1);
	}

#if defined(ARCH_X86) && defined(HAVE_ASM_NASM)
	const char * memcpy_name = 0;

	if(tc_accel & MM_SSE2)
	{
		v4l2_memcpy = (void *)ac_memcpy_sse2;
		memcpy_name = "sse2";
	}
	else
		if(tc_accel & MM_SSE)
		{
			v4l2_memcpy = (void *)ac_memcpy_sse;
			memcpy_name = "sse";
		}
		else
			if(tc_accel & MM_MMX)
			{
				v4l2_memcpy = (void *)ac_memcpy_mmx;
				memcpy_name = "mmx";
			}

	if(verbose_flag & TC_INFO)
	{
		if(!memcpy_name)
			fprintf(stderr, module "accelerated memcpy disabled by autodetection / flags\n");
		else
			fprintf(stderr, module "accelerated memcpy using: %s\n", memcpy_name);
	}
#else
	if(verbose_flag & TC_INFO)
	{
		fprintf(stderr, module "accelerated memcpy disabled by config\n");
	}
#endif

	switch(layout)
	{
		case(CODEC_RGB): 	v4l2_fmt = v4l2_fmt_rgb;			break;
		case(CODEC_YUV): 	v4l2_fmt = v4l2_fmt_yuv420planar;	break;
		case(CODEC_YUV422): v4l2_fmt = v4l2_fmt_yuv422packed;	break;

		default:
		{
			fprintf(stderr, module "layout (%d) must be one of CODEC_RGB, CODEC_YUV or CODEC_YUV422\n", layout);
			return(1);
		}
	}

	v4l2_parse_options(options);

	if(verbose_flag & TC_INFO)
	{
		if(v4l2_resync_margin_frames == 0 || v4l2_resync_interval_frames == 0)
			fprintf(stderr, module "%s", "resync disabled\n");
		else
			fprintf(stderr, module "resync enabled, margin = %d frames, interval = %d frames, \n",
					v4l2_resync_margin_frames,
					v4l2_resync_interval_frames);
	}

	if((v4l2_video_fd = open(device, O_RDWR, 0)) < 0)
	{
		fprintf(stderr, module "cannot open video device %s\n", device);
		return(1);
	}

	if(ioctl(v4l2_video_fd, VIDIOC_QUERYCAP, &caps) < 0)
	{
		fprintf(stderr, module "driver does not support querying capabilities\n");
		return(1);
	}

	if((!caps.capabilities & V4L2_CAP_VIDEO_CAPTURE))
	{
		fprintf(stderr, module "driver does not support video capture\n");
		return(1);
	}

	if((!caps.capabilities & V4L2_CAP_STREAMING))
	{
		fprintf(stderr, module "driver does not support streaming (mmap) video capture\n");
		return(1);
	}

	if(verbose_flag & TC_INFO)
	{
		fprintf(stderr, module "video grabbing, driver = %s, card = %s\n",
				caps.driver, caps.card);
	}

	if(!strncmp(caps.driver, "saa713", 6))
	{
		if(verbose_flag & TC_INFO)
			fprintf(stderr, module "video input from saa7134 driver detected\n");
		v4l2_saa7134_video = 1;
	}

	if(!strncmp(caps.driver, "bttv", 4))
	{
		if(verbose_flag & TC_INFO)
			fprintf(stderr, module "video input from bttv driver detected\n");
		v4l2_bttv_video = 1;
	}

	v4l2_width = width;
	v4l2_height = height;

	if(!v4l2_saa7134_video && !v4l2_bttv_video)
	{
		cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		if(ioctl(v4l2_video_fd, VIDIOC_CROPCAP, &cropcap) < 0)
		{
			perror(module "VIDIOC_CROPCAP");
			return(1);
		}

		if((width > cropcap.bounds.width) || (height > cropcap.bounds.height) || (width < 0) || (height < 0))
		{
			fprintf(stderr, module "dimensions exceed maximum crop area: %dx%d\n", cropcap.bounds.width, cropcap.bounds.height);
			return(1);
		}

		if(verbose_flag & TC_INFO)
		{
			fprintf(stderr, module "cropcap: %dx%d +%d+%d\n", 
					cropcap.bounds.width,
					cropcap.bounds.height,
					cropcap.bounds.top,
					cropcap.bounds.left);
		}
	
		crop.type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c.width	= cropcap.defrect.width;
		crop.c.height	= cropcap.defrect.height;
		crop.c.left		= cropcap.defrect.left;
		crop.c.top		= cropcap.defrect.top;

		if(ioctl(v4l2_video_fd, VIDIOC_S_CROP, &crop) < 0)
		{
			perror(module "VIDIOC_S_CROP: ");
			return(1);
		}

		memset(&streamparm, 0, sizeof(streamparm));

		streamparm.type										= V4L2_BUF_TYPE_VIDEO_CAPTURE;
		streamparm.parm.capture.capturemode					= 0;
		streamparm.parm.capture.timeperframe.numerator 		= 1;
		streamparm.parm.capture.timeperframe.denominator	= fps;

		if(ioctl(v4l2_video_fd, VIDIOC_S_PARM, &streamparm) < 0)
		{
			perror(module "VIDIOC_S_PARM: ");
			return(1);
		}
	}

	for(ix = 0, fcp = v4l2_format_convert_table, found = 0; ix < (sizeof(v4l2_format_convert_table) / sizeof(*v4l2_format_convert_table)); ix++)
	{
		if(fcp[ix].to != v4l2_fmt)
			continue;

		memset(&format, 0, sizeof(format));
		format.type					= V4L2_BUF_TYPE_VIDEO_CAPTURE;
		format.fmt.pix.width		= width;
		format.fmt.pix.height		= height;
		format.fmt.pix.pixelformat	= fcp[ix].from;

		if(ioctl(v4l2_video_fd, VIDIOC_S_FMT, &format) < 0)
			perror(module "VIDIOC_S_FMT: ");
		else
		{
			v4l2_format_convert = fcp[ix].convert;
			fprintf(stderr, module "Pixel format conversion: %s\n", fcp[ix].description);
			found = 1;
			break;
		}
	}

	if(!found)
	{
		fprintf(stderr, module "no usable pixel format supported by card\n");
		return(1);
	}

	if(ioctl(v4l2_video_fd, VIDIOC_G_STD, &std) < 0)
	{
		perror(module "VIDIOC_QUERYSTD: ");
		return(1);
	}

	if(std & V4L2_STD_525_60)
		v4l2_frame_rate = 30;
	else
		if(std & V4L2_STD_625_50)
			v4l2_frame_rate = 25;
		else
		{
			fprintf(stderr, module "unknown TV std, defaulting to 50 Hz field rate\n");
			v4l2_frame_rate = 25;
		}

	if(verbose_flag & TC_INFO)
		fprintf(stderr, module "receiving %d frames / sec\n", v4l2_frame_rate);

	// get buffer data

	reqbuf.type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	reqbuf.memory	= V4L2_MEMORY_MMAP;
	reqbuf.count	= 32;

	if(ioctl(v4l2_video_fd, VIDIOC_REQBUFS, &reqbuf) < 0)
	{
		perror(module "VIDIOC_REQBUFS");
		return(1);
	}

	v4l2_buffers_count = reqbuf.count;

	if(v4l2_buffers_count < 2)
	{
		fprintf(stderr, module "not enough buffers for capture\n");
		return(1);
	}

	if(verbose_flag & TC_INFO)
		fprintf(stderr, module "%d buffers available\n", v4l2_buffers_count);

	if(!(v4l2_buffers = calloc(v4l2_buffers_count, sizeof(*v4l2_buffers))))
	{
		fprintf(stderr, module "out of memory\n");
		return(1);
	}

	// mmap

	for(ix = 0; ix < v4l2_buffers_count; ix++)
	{
		buffer.type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buffer.memory	= V4L2_MEMORY_MMAP;
		buffer.index	= ix;

		if(ioctl(v4l2_video_fd, VIDIOC_QUERYBUF, &buffer) < 0)
		{
			perror(module "VIDIOC_QUERYBUF");
			return(1);
		}

		v4l2_buffers[ix].length	= buffer.length;
		v4l2_buffers[ix].start	= mmap(0, buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED, v4l2_video_fd, buffer.m.offset);

		if(v4l2_buffers[ix].start == MAP_FAILED)
		{
			perror(module "mmap");
			return(1);
		}
	}

	// enqueue all buffers

	for(ix = 0; ix < v4l2_buffers_count; ix++)
	{
		buffer.type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buffer.memory	= V4L2_MEMORY_MMAP;
		buffer.index	= ix;

		if(ioctl(v4l2_video_fd, VIDIOC_QBUF, &buffer) < 0)
		{
			perror(module "VIDIOC_QBUF");
			return(1);
		}
	}

	// unmute

	if(!v4l2_mute(0))
		return(1);

	// start capture

	if(ioctl(v4l2_video_fd, VIDIOC_STREAMON, &v4l2_video_fd /* ugh, needs valid memory location */) < 0)
	{
		perror(module "VIDIOC_STREAMON");
		return(1);
	}

	return(0);
}

int	v4l2_video_get_frame(size_t size, char * data)
{
	int buffers_filled = 0;
	int dummy;

	if(v4l2_video_sequence == 0)
	{
		for(dummy = 0; dummy < 5; dummy++)
			if(!v4l2_video_grab_frame(0, 0))
				return(1);

		if(pthread_mutex_unlock(&v4l2_av_start_mutex))
		{
			perror(module "unlock av start mutex by video read frame\n");
			return(1);
		}

		sched_yield();
	}

	if(v4l2_overrun_guard)
	{
		buffers_filled = v4l2_video_count_buffers();

		if(buffers_filled > (v4l2_buffers_count * 3 / 4))
		{
			fprintf(stderr, module "ERROR: running out of capture buffers (%d left from %d total), stopping capture\n", v4l2_buffers_count - buffers_filled, v4l2_buffers_count);

			if(ioctl(v4l2_video_fd, VIDIOC_STREAMOFF, &dummy) < 0)
				perror(module "VIDIOC_STREAMOFF");

			return(1);
		}
	}

	switch(v4l2_video_resync_op)
	{
		case(resync_clone):
		{
			if(!v4l2_video_clone_frame(data, size))
				return(1);
			break;
		}

		case(resync_drop):
		{
			if(!v4l2_video_grab_frame(0, 0))
				return(1);
			if(!v4l2_video_grab_frame(data, size))
				return(1);
			break;
		}

		case(resync_none):
		{
			if(!v4l2_video_grab_frame(data, size))
				return(1);
			break;
		}

		default:
		{
			fprintf(stderr, module "impossible case\n");
			return(1);
		}
	}

	v4l2_video_resync_op = resync_none;

	if(((v4l2_resync_margin_frames != 0) && (v4l2_resync_interval_frames != 0)) &&
			((v4l2_video_sequence % v4l2_resync_interval_frames) == 0) &&
			(v4l2_video_sequence != 0) && (v4l2_audio_sequence != 0))
	{
		if(abs(v4l2_audio_sequence - v4l2_video_sequence) > v4l2_resync_margin_frames)
		{
			if(v4l2_audio_sequence > v4l2_video_sequence)
			{
				v4l2_save_frame(data, size);
				v4l2_video_cloned++;
				v4l2_video_resync_op = resync_clone;
			}
			else
			{
				v4l2_video_resync_op = resync_drop;
				v4l2_video_dropped++;
			}
		}

		if(v4l2_video_resync_op != resync_none && (verbose_flag & TC_INFO))
		{
			fprintf(stderr, "\n" module "OP: %s VS/AS: %d/%d C/D: %d/%d\n",
					v4l2_video_resync_op == resync_drop ? "drop" : "clone",
					v4l2_video_sequence,
					v4l2_audio_sequence,
					v4l2_video_cloned,
					v4l2_video_dropped);
		}
	}

	v4l2_video_sequence++;

	return(0);
}

int v4l2_video_grab_stop(void)
{
	int dummy, ix;

	// mute

	if(!v4l2_mute(1))
		return(1);

	if(ioctl(v4l2_video_fd, VIDIOC_STREAMOFF, &dummy /* ugh */) < 0)
	{
		perror(module "VIDIOC_STREAMOFF");
		return(1);
	}

	for(ix = 0; ix < v4l2_buffers_count; ix++)
		munmap(v4l2_buffers[ix].start, v4l2_buffers[ix].length);

	close(v4l2_video_fd);
	v4l2_video_fd = -1;

	free(v4l2_resync_previous_frame);
	v4l2_resync_previous_frame = 0;

	return(0);
}

int v4l2_audio_init(const char * device, int rate, int bits, int channels)
{
	int tmp;

	if((v4l2_audio_fd = open(device, O_RDONLY, 0)) < 0)
	{
		perror(module "open audio device");
		return(1);
	}

	if(bits != 8 && bits != 16)
	{
		fprintf(stderr, module "bits/sample must be 8 or 16\n");
		return(1);
	}

	tmp = bits == 8 ? AFMT_U8 : AFMT_S16_LE;

	if(ioctl(v4l2_audio_fd, SNDCTL_DSP_SETFMT, &tmp) < 0)
	{
		perror(module "SNDCTL_DSP_SETFMT");
		return(1);
	}

	if(ioctl(v4l2_audio_fd, SNDCTL_DSP_CHANNELS, &channels) < 0)
	{
		perror(module "SNDCTL_DSP_CHANNELS");
		return(1);
	}

	// check for saa7134
	// hope this test will do: set sampling to "0 khz", check if this returns "OK" and "32 khz"

	tmp = 0;

	if(ioctl(v4l2_audio_fd, SOUND_PCM_WRITE_RATE, &tmp) < 0)
		(void)0;
	else
	{
		if(ioctl(v4l2_audio_fd, SOUND_PCM_READ_RATE, &tmp) < 0)
		{
			perror(module "SOUND_PCM_READ_RATE");
			return(1);
		}

		if(tmp == 32000)
			v4l2_saa7134_audio = 1;
	}

	if(v4l2_saa7134_audio)
	{
		if(verbose_flag & TC_INFO)
			fprintf(stderr, module "audio input from saa7134 detected\n");

		rate = 32000;
	}

	if(ioctl(v4l2_audio_fd, SOUND_PCM_WRITE_RATE, &rate) < 0)
	{
		perror(module "SOUND_PCM_WRITE_RATE");
		return(1);
	}

	return(0);
}

int v4l2_audio_grab_frame(size_t size, char * buffer)
{
	int left;
	int offset;
	int received;
	int result, err;

	if(v4l2_audio_sequence == 0) // wait for video to start
	{
		errno	= 0;
		result	= pthread_mutex_trylock(&v4l2_av_start_mutex);
		err		= errno;

		if(!result)
		{
			fprintf(stderr, module "av start mutex not locked!\n");
			return(1);
		}

		if((result != EBUSY) && (err != EBUSY))
		{
			perror(module "av start mutex trylock");
			fprintf(stderr, module "result = %d, error = %d\n", result, err);
			return(1);
		}

		if(pthread_mutex_lock(&v4l2_av_start_mutex))
		{
			perror(module "av mutex lock");
			return(1);
		}

		if(pthread_mutex_unlock(&v4l2_av_start_mutex))
		{
			perror(module "av mutex unlock");
			return(1);
		}

		if(pthread_mutex_destroy(&v4l2_av_start_mutex))
		{
			perror(module "av mutex destroy");
			return(1);
		}
	}

	for(left = size, offset = 0; left > 0;)
	{
		received = read(v4l2_audio_fd, buffer + offset, left);

		if(received == 0)
			fprintf(stderr, module "audio grab: received == 0\n");

		if(received < 0)
		{
			if(errno == EINTR)
				received = 0;
			else
			{
				perror(module "read audio");
				return(TC_IMPORT_ERROR);
			}
		}

		if(received > left)
		{
			fprintf(stderr, module "read returns more bytes than requested! (requested: %d, returned: %d\n", left, received);
			return(TC_IMPORT_ERROR);
		}

		offset += received;
		left -= received;
	}

	v4l2_audio_sequence++;

	return(0);
}

int v4l2_audio_grab_stop(void)
{
	close(v4l2_audio_fd);

	if(verbose_flag & TC_INFO)
	{
		fprintf(stderr, "\n" module "Totals: sequence V/A: %d/%d, frames C/D: %d/%d\n",
				v4l2_video_sequence,
				v4l2_audio_sequence,
				v4l2_video_cloned,
				v4l2_video_dropped);
	}

	return(0);
}

/* ============================================================ 
 * TRANSCODE INTERFACE 
 * ============================================================*/

/* ------------------------------------------------------------ 
 * open stream
 * ------------------------------------------------------------*/

MOD_open
{
	if(param->flag == TC_VIDEO)
	{
		if(verbose_flag & TC_INFO)
			fprintf(stderr, module "v4l2 video grabbing\n");

		if(v4l2_video_init(vob->im_v_codec, vob->video_in_file, vob->im_v_width, vob->im_v_height, vob->fps,
					vob->im_v_string))
			return(TC_IMPORT_ERROR);

		return(0);
	}
	else
		if(param->flag == TC_AUDIO)
		{
			if(verbose_flag & TC_INFO)
				fprintf(stderr, module "v4l2 audio grabbing\n");

			if(v4l2_audio_init(vob->audio_in_file, vob->a_rate, vob->a_bits, vob->a_chan))
				return(TC_IMPORT_ERROR);

			return(0);
		}
		else
		{
			fprintf(stderr, module "unsupported request (init)\n");
			return(TC_IMPORT_ERROR);
		}

	return(0);
}

/* ------------------------------------------------------------ 
 * decode  stream
 * ------------------------------------------------------------*/

MOD_decode
{
	if(param->flag == TC_VIDEO)
	{
		if(v4l2_video_get_frame(param->size, param->buffer))
		{
			fprintf(stderr, module "error in grabbing video\n");
			return(TC_IMPORT_ERROR);
		}
	}
	else
	{
		if(param->flag == TC_AUDIO)
		{
			if(v4l2_audio_grab_frame(param->size, param->buffer))
			{
				fprintf(stderr, module "error in grabbing audio\n");
				return(TC_IMPORT_ERROR);
			}
		}
		else
		{
			fprintf(stderr, module "unsupported request (decode)\n");
			return(TC_IMPORT_ERROR);
		}
	}

	return(0);
}

/* ------------------------------------------------------------ 
 * close stream
 * ------------------------------------------------------------*/

MOD_close
{
	if(param->flag == TC_VIDEO)
	{
		v4l2_video_grab_stop();
	}
	else
		if(param->flag == TC_AUDIO)
		{
			v4l2_audio_grab_stop();
		}
		else
		{
			fprintf(stderr, module "unsupported request (close)\n");
			return(TC_IMPORT_ERROR);
		}

	return(0);
}
