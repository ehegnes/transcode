/*
 *	import_v4l2.c
 *
 *	By Erik Slagter <erik@slagter.name> Sept 2003
 * 
 *	This file is part of transcode, a video stream processing tool
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

#define MOD_NAME		"import_v4l2.so"
#define MOD_VERSION		"v1.4.0 (2005-10-08)"
#define MOD_CODEC		"(video) v4l2 | (audio) pcm"

#include "transcode.h"

static int verbose_flag		= TC_QUIET;
static int capability_flag	= TC_CAP_RGB | TC_CAP_YUV | TC_CAP_YUV422 | TC_CAP_PCM;

#define MOD_PRE			v4l2
#include "import_def.h"

#define _ISOC9X_SOURCE 1

#include <sys/soundcard.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <linux/types.h>

// The v4l2_buffer struct check is because some distributions protect that
// struct in videodev2 with a #ifdef __KERNEL__ (SuSE 9.0)

#if defined(HAVE_LINUX_VIDEODEV2_H) && defined(HAVE_STRUCT_V4L2_BUFFER)
#define _LINUX_TIME_H
#include <linux/videodev2.h>
#else
#include "videodev2.h"
#endif

#include "aclib/imgconvert.h"


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
			tibit added mmx version of yuy2_to_uyvy
					hacked in alternate fields (#if 0'ed) 
					fixed a typo (UYUV -> UYVY)
	1.2.2	EMS	fixed av sync mutex not yet grabbed problem with "busy" wait
	1.3.0	EMS	added cropping cap, removed saa7134 and bttv specific code, not
					necessary
	1.3.1	EMS make conversion user-selectable
	1.3.2	EMS removed a/v sync mutex, doesn't work as expected
			EMS added explicit colour format / frame rate selection
			EMS deleted disfunctional experimental alternating fields code
			EMS added experimental code to make sa7134 survive sync glitches
	1.3.3	EMS adapted fast memcpy to new default transcode method
	1.3.4	EMS fixed RGB24 capturing bug when using saa7134.
	1.3.5	EMS test with unrestricted cloning/dropping of frames using resync_interval=0
	            adjusted saa7134 audio message to make clear the user must take action
	1.4.0	AC  switch to aclib for image conversion

*/

#define module "[" MOD_NAME "] "

typedef enum { resync_none, resync_clone, resync_drop } v4l2_resync_op;
typedef enum { v4l2_param_int, v4l2_param_string, v4l2_param_fp } v4l2_param_type_t;

typedef struct
{
	int				v4l_format;
	ImageFormat		from;
	ImageFormat		to;
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

static struct 
{
	void * start;
	size_t length;
} * v4l2_buffers;

static char *			v4l2_device;
static ImageFormat		v4l2_fmt;
static v4l2_resync_op	v4l2_video_resync_op = resync_none;

static int	v4l2_saa7134_audio = 0;
static int	v4l2_overrun_guard = 0;
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
static int	v4l2_crop_width = 0;
static int	v4l2_crop_height = 0;
static int	v4l2_crop_left = 0;
static int	v4l2_crop_top = 0;
static int	v4l2_crop_enabled = 0;
static int	v4l2_convert_index = -2;

static char *	v4l2_resync_previous_frame = 0;
static char		v4l2_crop_parm[128] = "";
static char		v4l2_format_string[128] = "";

static v4l2_parameter_t v4l2_parameters[] =
{
	{ v4l2_param_int, 		"resync_margin",	0,							{ .integer	= &v4l2_resync_margin_frames }},
	{ v4l2_param_int, 		"resync_interval",	0,							{ .integer	= &v4l2_resync_interval_frames }},
	{ v4l2_param_int, 		"overrun_guard",	0,							{ .integer	= &v4l2_overrun_guard }},
	{ v4l2_param_string,	"crop",				sizeof(v4l2_crop_parm),		{ .string	= v4l2_crop_parm }},
	{ v4l2_param_int,		"convert",			0,							{ .integer	= &v4l2_convert_index }},
	{ v4l2_param_string,	"format",			sizeof(v4l2_format_string),	{ .string	= v4l2_format_string }}
};

static v4l2_format_convert_table_t v4l2_format_convert_table[] = 
{
	{ V4L2_PIX_FMT_RGB24,   IMG_RGB24,   IMG_RGB_DEFAULT, "RGB24 [packed] -> RGB [packed] (no conversion" },
	{ V4L2_PIX_FMT_BGR24,   IMG_BGR24,   IMG_RGB_DEFAULT, "BGR24 [packed] -> RGB [packed]" },
	{ V4L2_PIX_FMT_RGB32,   IMG_RGBA32,  IMG_RGB_DEFAULT, "RGB32 [packed] -> RGB [packed]" },
	{ V4L2_PIX_FMT_BGR32,   IMG_BGRA32,  IMG_RGB_DEFAULT, "BGR32 [packed] -> RGB [packed]" },
	{ V4L2_PIX_FMT_GREY,    IMG_GRAY8,   IMG_RGB_DEFAULT, "8-bit grayscale -> RGB [packed]" },

	{ V4L2_PIX_FMT_YYUV,    IMG_YUV422P, IMG_YUV422P,     "YUV422 [planar] -> YUV422 [planar] (no conversion)" },
	{ V4L2_PIX_FMT_UYVY,    IMG_UYVY,    IMG_YUV422P,     "UYVY [packed] -> YUV422 [planar] (no conversion)" },
	{ V4L2_PIX_FMT_YUYV,    IMG_YUY2,    IMG_YUV422P,     "YUY2 [packed] -> YUV422 [planar]" },
	{ V4L2_PIX_FMT_YUV420,  IMG_YUV420P, IMG_YUV422P,     "YUV420 [planar] -> YUV422 [planar]" },
	{ V4L2_PIX_FMT_YVU420,  IMG_YV12,    IMG_YUV422P,     "YVU420 [planar] -> YUV422 [planar]" },
	{ V4L2_PIX_FMT_Y41P,    IMG_YUV411P, IMG_YUV422P,     "YUV411 [planar] -> YUV422 [planar]" },
	{ V4L2_PIX_FMT_GREY,    IMG_GRAY8,   IMG_YUV422P,     "8-bit grayscale -> YUV422 [planar]" },

	{ V4L2_PIX_FMT_YUV420,  IMG_YUV420P, IMG_YUV_DEFAULT, "YUV420 [planar] -> YUV420 [planar] (no conversion)" },
	{ V4L2_PIX_FMT_YVU420,  IMG_YV12,    IMG_YUV_DEFAULT, "YVU420 [planar] -> YUV420 [planar]" },
	{ V4L2_PIX_FMT_YYUV,    IMG_YUV422P, IMG_YUV_DEFAULT, "YUV422 [planar] -> YUV420 [planar]" },
	{ V4L2_PIX_FMT_Y41P,    IMG_YUV411P, IMG_YUV_DEFAULT, "YUV411 [planar] -> YUV420 [planar]" },
	{ V4L2_PIX_FMT_UYVY,    IMG_UYVY,    IMG_YUV_DEFAULT, "UYVY [packed] -> YUV420 [planar]" },
	{ V4L2_PIX_FMT_YUYV,    IMG_YUY2,    IMG_YUV_DEFAULT, "YUY2 [packed] -> YUV420 [planar]" },
	{ V4L2_PIX_FMT_GREY,    IMG_GRAY8,   IMG_YUV_DEFAULT, "8-bit grayscale -> YUV420 [planar]" },
};

/* ============================================================ 
 * IMAGE FORMAT CONVERSION ROUTINE
 * ============================================================*/

static void v4l2_format_convert(uint8_t *source, uint8_t *dest,
                                int width, int height)
{
    v4l2_format_convert_table_t *conv;
    uint8_t *srcplanes[3], *destplanes[3];

    if (v4l2_convert_index < 0)
        return;
    conv = &v4l2_format_convert_table[v4l2_convert_index];
    srcplanes[0] = source;
    destplanes[0] = dest;
    if (IS_YUV_FORMAT(conv->from))
        YUV_INIT_PLANES(srcplanes, source, conv->from, width, height);
    if (IS_YUV_FORMAT(conv->to))
        YUV_INIT_PLANES(destplanes, dest, conv->to, width, height);
    ac_imgconvert(srcplanes, conv->from, destplanes, conv->to, width, height);
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
		ac_memcpy(dest, v4l2_resync_previous_frame, size);

	return(1);
}

static void v4l2_save_frame(const char * source, size_t length)
{
	if(!v4l2_resync_previous_frame)
		v4l2_resync_previous_frame = malloc(length);

	ac_memcpy(v4l2_resync_previous_frame, source, length);
}

static int v4l2_video_grab_frame(char * dest, size_t length)
{
	static struct v4l2_buffer buffer;
	int ix;
	int eio = 0;

	// get buffer

	buffer.type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buffer.memory	= V4L2_MEMORY_MMAP;

	if(ioctl(v4l2_video_fd, VIDIOC_DQBUF, &buffer) < 0)
	{
		perror(module "VIDIOC_DQBUF");

		if(errno != EIO)
			return(0);
		else
		{
			eio = 1;

			for(ix = 0; ix < v4l2_buffers_count; ix++)
			{
				buffer.type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;
				buffer.memory	= V4L2_MEMORY_MMAP;
				buffer.index	= ix;
				buffer.flags	= 0;

				if(ioctl(v4l2_video_fd, VIDIOC_DQBUF, &buffer) < 0)
					perror("recover DQBUF");
			}

			for(ix = 0; ix < v4l2_buffers_count; ix++)
			{
				buffer.type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;
				buffer.memory	= V4L2_MEMORY_MMAP;
				buffer.index	= ix;
				buffer.flags	= 0;

				if(ioctl(v4l2_video_fd, VIDIOC_QBUF, &buffer) < 0)
					perror("recover QBUF");
			}
		}
	}
	
	ix	= buffer.index;

	// copy frame

	if(dest) {
		v4l2_format_convert(v4l2_buffers[ix].start, dest, v4l2_width, v4l2_height);
	}
	
	// enqueue buffer again

	if(!eio)
	{
		buffer.type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buffer.memory	= V4L2_MEMORY_MMAP;
		buffer.flags	= 0;
	
		if(ioctl(v4l2_video_fd, VIDIOC_QBUF, &buffer) < 0)
		{
			perror(module "VIDIOC_QBUF");
			return(0);
		}
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

	options = options_ptr = tc_strdup(options_in);

	if(!options || (!(option = malloc(strlen(options) * sizeof(char)))))
	{
		fprintf(stderr, module "Cannot malloc - options not parsed\n");
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
						pt->value.string[pt->length - 1] = '\0';
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

static int v4l2_video_init(int layout, const char * device, int width,
			   int height, int fps, const char * options)
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
	v4l2_std_id stdid;
	struct v4l2_standard standard;

	switch(layout)
	{
		case(CODEC_RGB):    v4l2_fmt = IMG_RGB_DEFAULT; break;
		case(CODEC_YUV):    v4l2_fmt = IMG_YUV_DEFAULT; break;
		case(CODEC_YUV422): v4l2_fmt = IMG_YUV422P;     break;

		default:
		{
			fprintf(stderr, module "layout (%d) must be one of CODEC_RGB, CODEC_YUV or CODEC_YUV422\n", layout);
			return(1);
		}
	}

	v4l2_parse_options(options);

	if(v4l2_convert_index == -1)	// list
	{
		for(ix = 0, fcp = v4l2_format_convert_table, found = 0; ix < (sizeof(v4l2_format_convert_table) / sizeof(*v4l2_format_convert_table)); ix++)
			fprintf(stderr, module "conversion index: %d = %s\n", ix, fcp[ix].description);

		return(1);
	}

	if(verbose_flag & TC_INFO)
	{
		if(v4l2_resync_margin_frames == 0)
			fprintf(stderr, module "%s", "resync disabled\n");
		else
			fprintf(stderr, module "resync enabled, margin = %d frames, interval = %d frames, \n",
					v4l2_resync_margin_frames,
					v4l2_resync_interval_frames);
	}

	if(device)
		v4l2_device = tc_strdup(device);

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
		fprintf(stderr, module "video grabbing, driver = %s, card = %s\n",
				caps.driver, caps.card);

	v4l2_width = width;
	v4l2_height = height;

	for(ix = 0, fcp = v4l2_format_convert_table, found = 0; ix < (sizeof(v4l2_format_convert_table) / sizeof(*v4l2_format_convert_table)); ix++)
	{
		if(fcp[ix].to != v4l2_fmt)
			continue;

		if((v4l2_convert_index >= 0) && (v4l2_convert_index != ix))
			continue;

		memset(&format, 0, sizeof(format));
		format.type					= V4L2_BUF_TYPE_VIDEO_CAPTURE;
		format.fmt.pix.width		= width;
		format.fmt.pix.height		= height;
		format.fmt.pix.pixelformat	= fcp[ix].v4l_format;

		if(ioctl(v4l2_video_fd, VIDIOC_S_FMT, &format) < 0)
			perror(module "VIDIOC_S_FMT: ");
		else
		{
			fprintf(stderr, module "Pixel format conversion: %s\n", fcp[ix].description);
			v4l2_convert_index = ix;
			found = 1;
			break;
		}
	}

	if(!found)
	{
		fprintf(stderr, module "no usable pixel format supported by card\n");
		return(1);
	}

	memset(&streamparm, 0, sizeof(streamparm));
	streamparm.type										= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	streamparm.parm.capture.capturemode					= 0;
	streamparm.parm.capture.timeperframe.numerator 		= 1e7;
	streamparm.parm.capture.timeperframe.denominator	= fps;

	if(ioctl(v4l2_video_fd, VIDIOC_S_PARM, &streamparm) < 0)
	{
		fprintf(stderr, module "driver does not support setting parameters (ioctl(VIDIOC_S_PARM) returns \"%s\")\n",
			errno <= sys_nerr ? sys_errlist[errno] : "unknown");
	}

	if(!strcmp(v4l2_format_string, "list")) // list
	{
		for(ix = 0; ix < 128; ix++)
		{
			standard.index = ix;

			if(ioctl(v4l2_video_fd, VIDIOC_ENUMSTD, &standard) < 0)
			{
				if(errno == EINVAL)
					break;

				perror("VIDIOC_ENUMSTD");
				return(1);
			}

			fprintf(stderr, module "%s\n", standard.name);
		}

		return(1);
	}

	if(strlen(v4l2_format_string) > 0)
	{
		for(ix = 0; ix < 128; ix++)
		{
			standard.index = ix;

			if(ioctl(v4l2_video_fd, VIDIOC_ENUMSTD, &standard) < 0)
			{
				if(errno == EINVAL)
					break;

				perror("VIDIOC_ENUMSTD");
				return(1);
			}

			if(!strcasecmp(standard.name, v4l2_format_string))
				break;
		}

		if(ix == 128)
		{
			fprintf(stderr, module "unknown format %s\n", v4l2_format_string);
			return(1);
		}
		
		if(ioctl(v4l2_video_fd, VIDIOC_S_STD, &standard.id) < 0)
		{
			perror(module "VIDIOC_S_STD");
			return(-1);
		}

		if(verbose_flag & TC_INFO)
			fprintf(stderr, module "colour & framerate standard set to: [%s]\n", standard.name);
	}

	if(ioctl(v4l2_video_fd, VIDIOC_G_STD, &stdid) < 0)
	{
		perror(module "VIDIOC_QUERYSTD: ");
		return(1);
	}

	if(stdid & V4L2_STD_525_60)
		v4l2_frame_rate = 30;
	else
		if(stdid & V4L2_STD_625_50)
			v4l2_frame_rate = 25;
		else
		{
			fprintf(stderr, module "unknown TV std, defaulting to 50 Hz field rate\n");
			v4l2_frame_rate = 25;
		}

	if(verbose_flag & TC_INFO)
	{
		fprintf(stderr, module "checking colour & framerate standards: ");

		for(ix = 0; ix < 128; ix++)
		{
			standard.index = ix;

			if(ioctl(v4l2_video_fd, VIDIOC_ENUMSTD, &standard) < 0)
			{
				if(errno == EINVAL)
					break;

				perror("\n" module "VIDIOC_ENUMSTD");
				return(1);
			}

			if(standard.id == stdid)
				fprintf(stderr, "[%s] ", standard.name);
		}

		fputs("\n", stderr);
		fprintf(stderr, module "receiving %d frames / sec\n", v4l2_frame_rate);
	}

	if(strcmp(v4l2_crop_parm, ""))
	{
		if(sscanf(v4l2_crop_parm, "%ux%u+%ux%u", &v4l2_crop_width, &v4l2_crop_height,
					&v4l2_crop_left, &v4l2_crop_top) == 4)
			v4l2_crop_enabled = 1;
		else
		{
			v4l2_crop_height = v4l2_crop_width = 
				v4l2_crop_top = v4l2_crop_left = 0;
			v4l2_crop_enabled = 0;
		}
	}

	if((verbose_flag & TC_INFO) && v4l2_crop_enabled)
	{
		fprintf(stderr, module "source frame set to: %dx%d+%dx%d\n",
			v4l2_crop_width, v4l2_crop_height,
			v4l2_crop_left, v4l2_crop_top);
	}

	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if(ioctl(v4l2_video_fd, VIDIOC_CROPCAP, &cropcap) < 0)
	{
		fprintf(stderr, module "driver does not support cropping (ioctl(VIDIOC_CROPCAP) returns \"%s\"), disabled\n",
			errno <= sys_nerr ? sys_errlist[errno] : "unknown");
	}
	else
	{
		fprintf(stderr, module "frame size: %dx%d\n", width, height);
		fprintf(stderr, module "cropcap bounds: %dx%d +%d+%d\n", 
				cropcap.bounds.width,
				cropcap.bounds.height,
				cropcap.bounds.left,
				cropcap.bounds.top);
		fprintf(stderr, module "cropcap defrect: %dx%d +%d+%d\n", 
				cropcap.defrect.width,
				cropcap.defrect.height,
				cropcap.defrect.left,
				cropcap.defrect.top);
		fprintf(stderr, module "cropcap pixelaspect: %d/%d\n",
				cropcap.pixelaspect.numerator,
				cropcap.pixelaspect.denominator);

		if((width > cropcap.bounds.width) || (height > cropcap.bounds.height) || (width < 0) || (height < 0))
		{
			fprintf(stderr, module "capturing dimensions exceed maximum crop area: %dx%d\n", cropcap.bounds.width, cropcap.bounds.height);
			return(1);
		}

		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		if(ioctl(v4l2_video_fd, VIDIOC_G_CROP, &crop) < 0)
		{
			fprintf(stderr, module "driver does not support inquering cropping parameters (ioctl(VIDIOC_G_CROP) returns \"%s\")\n",
				errno <= sys_nerr ? sys_errlist[errno] : "unknown");
		}
		else
		{
			fprintf(stderr, module "default cropping: %dx%d +%d+%d\n", 
				crop.c.width,
				crop.c.height,
				crop.c.left,
				crop.c.top);
		}

		if(v4l2_crop_enabled)
		{
			crop.type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;
			crop.c.width	= v4l2_crop_width;
			crop.c.height	= v4l2_crop_height;
			crop.c.left		= v4l2_crop_left;
			crop.c.top		= v4l2_crop_top;

			if(ioctl(v4l2_video_fd, VIDIOC_S_CROP, &crop) < 0)
			{
				perror("VIDIOC_S_CROP");
				return(1);
			}

			crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

			if(ioctl(v4l2_video_fd, VIDIOC_G_CROP, &crop) < 0)
			{
				fprintf(stderr, module "driver does not support inquering cropping parameters (ioctl(VIDIOC_G_CROP) returns \"%s\")\n",
					errno <= sys_nerr ? sys_errlist[errno] : "unknown");
			}
			else
			{
				fprintf(stderr, module "cropping after set frame source: %dx%d +%d+%d\n", 
					crop.c.width,
					crop.c.height,
					crop.c.left,
					crop.c.top);
			}
		}
	}

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

static int v4l2_video_get_frame(size_t size, char * data)
{
	int buffers_filled = 0;
	int dummy;

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

	if(		(v4l2_resync_margin_frames != 0)	&&
			(v4l2_video_sequence != 0)			&&
			(v4l2_audio_sequence != 0)			&&
			((v4l2_resync_interval_frames == 0) || (v4l2_video_sequence % v4l2_resync_interval_frames) == 0)	)
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

static int v4l2_video_grab_stop(void)
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

static int v4l2_audio_init(const char * device, int rate, int bits,
			   int channels)
{
	int tmp;

	if((v4l2_audio_fd = open(device, O_RDONLY, 0)) < 0)
	{
		perror(module "open audio device");
		return(1);
	}

	if(!strcmp(device, "/dev/null") || !strcmp(device, "/dev/zero"))
		return(0);

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
			fprintf(stderr, module "Audio input from saa7134 detected, you should set audio sample rate to 32 Khz using -e\n");
	}
	else
	{
		if(ioctl(v4l2_audio_fd, SOUND_PCM_WRITE_RATE, &rate) < 0)
		{
			perror(module "SOUND_PCM_WRITE_RATE");
			return(1);
		}
	}

	return(0);
}

static int v4l2_audio_grab_frame(size_t size, char * buffer)
{
	int left;
	int offset;
	int received;

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

static int v4l2_audio_grab_stop(void)
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
