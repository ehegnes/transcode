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

#include <linux/types.h>
#include "videodev2.h"
#include <sys/soundcard.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>

#include "transcode.h"
#include "aclib/ac.h"

#define ts_mul 1000000UL
#define ts_div 1UL

#define MOD_NAME		"import_v4l2.so"
#define MOD_VERSION		"v1.0.0 (2003-11-08)"
#define MOD_CODEC		"(video) v4l2 | (audio) pcm"
#define MOD_PRE			v4l2
#include "import_def.h"

#define module "[" MOD_NAME "]: "

typedef enum { resync_none, resync_clone, resync_drop } v4l2_resync_op;

static int verbose_flag = TC_QUIET;
// disable RGB because of possible bug in saa7134 driver that crashes computer
//static int capability_flag=TC_CAP_RGB|TC_CAP_YUV|TC_CAP_PCM;
static int capability_flag = TC_CAP_YUV | TC_CAP_PCM;

static struct 
{
	void * start;
	size_t length;
} * v4l2_buffers;

static v4l2_resync_op v4l2_video_resync_op = resync_none;

static int	v4l2_yuv = 1;

static int	v4l2_saa7134_video = 0;
static int	v4l2_saa7134_audio = 0;

static int 	v4l2_resync_margin_frames = 0;
static int 	v4l2_resync_interval_frames = 5;
static int 	v4l2_resync_framerate_base = ts_mul;

static unsigned long long	v4l2_timestamp_start = 0;
static unsigned long long	v4l2_timestamp_last = 0;

static int 			v4l2_buffers_count;
static int 			v4l2_video_fd = -1;
static int 			v4l2_audio_fd = -1;
static int 			v4l2_video_sequence = 0;
static int 			v4l2_audio_sequence = 0;
static int			v4l2_video_cloned = 0;
static int			v4l2_video_dropped = 0;
static int			v4l2_video_missed = 0;
static char *	 	v4l2_resync_previous_frame = 0;

static int			v4l2_frame_rate;
static int			v4l2_frame_interval;

static int	 		(*v4l2_memcpy)(char *, const char *, int) = (int (*)(char *, const char *, int))memcpy;

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
	{
		perror(module "VIDIOC_S_CTRL");
		return(0);
	}

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

static void v4l2_framecopy(char * dest, const char * source, size_t length)
{
	int yplane_size		= length * 4 / 6;
	int uplane_size		= length * 1 / 6;

	int yplane_offset	= 0;
	int u1plane_offset	= yplane_size + 0;
	int u2plane_offset	= yplane_size + uplane_size;

	if(v4l2_yuv)
	{
		v4l2_memcpy(dest + yplane_offset,  source + yplane_offset,  yplane_size);
		v4l2_memcpy(dest + u1plane_offset, source + u2plane_offset, uplane_size);
		v4l2_memcpy(dest + u2plane_offset, source + u1plane_offset, uplane_size);
	}
	else
		v4l2_memcpy(dest, source, length);
}

static int v4l2_video_grab_frame(char * dest, size_t length)
{
	static struct v4l2_buffer buffer;
	static int next_buffer_ix = 0;
	unsigned long long ts_frame, ts_diff;
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

	ts_frame = ((unsigned int)buffer.timestamp.tv_sec * ts_mul) + ((unsigned int)buffer.timestamp.tv_usec / ts_div);
	ts_diff = ts_frame - v4l2_timestamp_last ;

	if((v4l2_timestamp_last != 0) && (ts_frame != 0) && (ts_diff > (unsigned long long)v4l2_frame_interval))
	{
		ts_diff -= (unsigned long long)v4l2_frame_interval;

		//fprintf(stderr, "diff = %llu, ts_diff = %llu, max = %llu\n", ts_frame - v4l2_timestamp_last, ts_diff, (unsigned long long)v4l2_frame_interval >> 2);

		if(ts_diff > ((unsigned long long)v4l2_frame_interval >> 2))
				v4l2_video_missed += (ts_diff / (unsigned long long)v4l2_frame_interval) + 1;

			//fprintf(stderr, "\n" module "possibly missed video frame (frame delay > frame rate) (%llu ms / %llu frames)\n",
					//ts_diff / (1000 * ts_div), ts_diff / (unsigned long long)v4l2_frame_interval);
	}

	v4l2_timestamp_last = ts_frame;

	// copy frame

	if(dest)
		v4l2_framecopy(dest, v4l2_buffers[ix].start, length);

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

/* ============================================================ 
 * V4L2 CORE
 * ============================================================*/

int v4l2_video_init(const char * device, int width, int height, int fps,
		int resync_margin, int resync_interval)
{
	int ix;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_format format;
	struct v4l2_requestbuffers reqbuf;
	struct v4l2_buffer buffer;
	struct v4l2_capability caps;
	struct v4l2_streamparm streamparm;
	v4l2_std_id std;
	struct timeval tv;


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

	v4l2_resync_margin_frames = resync_margin - 1;
	v4l2_resync_interval_frames = resync_interval;

	if(verbose_flag & TC_INFO)
		fprintf(stderr, module "resync %s, margin = %d frames, interval = %d frames, \n",
					v4l2_resync_margin_frames != 0 ? "enabled" : "disabled",
					v4l2_resync_margin_frames,
					v4l2_resync_interval_frames);

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

	if(!v4l2_saa7134_video)
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

	memset(&format, 0, sizeof(format));

	format.type					= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	format.fmt.pix.width		= width;
	format.fmt.pix.height		= height;
	format.fmt.pix.pixelformat	= v4l2_yuv ? V4L2_PIX_FMT_YUV420 : V4L2_PIX_FMT_BGR24;

	if(ioctl(v4l2_video_fd, VIDIOC_S_FMT, &format) < 0)
	{
		perror(module "VIDIOC_S_FMT: ");
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
			fprintf(stderr, module "unknown TV std, defaulting to 50 Hz\n");
			v4l2_frame_rate = 25;
		}

	v4l2_frame_interval = v4l2_resync_framerate_base / v4l2_frame_rate;

	if(verbose_flag & TC_INFO)
		fprintf(stderr, module "receiving %d frames / sec, frame interval = %G ms\n", v4l2_frame_rate, (double)v4l2_frame_interval / (ts_mul / 1000));

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

	gettimeofday(&tv, 0);
	v4l2_timestamp_start = ((unsigned long)tv.tv_sec * ts_mul) + ((unsigned long)tv.tv_usec / ts_div);

	return(0);
}

int	v4l2_video_get_frame(size_t size, char * data, int * attributes)
{
	int buffers_filled = 0;

	*attributes |= TC_FRAME_IS_INTERLACED;

	if(verbose_flag & TC_DEBUG)
	{
		buffers_filled = v4l2_video_count_buffers();
		if(buffers_filled > (v4l2_buffers_count * 4 / 3))
			fprintf(stderr, module "info: running out of capture buffers (%d left)\n", v4l2_buffers_count - buffers_filled);
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
			fprintf(stderr, "\n" module "OP: %s VS/AS: %d/%d C/D/M: %d/%d/%d\n",
					v4l2_video_resync_op == resync_drop ? "drop" : "clone",
					v4l2_video_sequence,
					v4l2_audio_sequence,
					v4l2_video_cloned,
					v4l2_video_dropped,
					v4l2_video_missed);
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
		struct timeval tv;
		unsigned long long now;
		gettimeofday(&tv, 0);
		now = (unsigned long)(tv.tv_sec * ts_mul) + (unsigned long)(tv.tv_usec / ts_div);

		fprintf(stderr, "\n" module "Totals: sequence V/A: %d/%d, frames C/D/M: %d/%d/%d\n",
				v4l2_video_sequence,
				v4l2_audio_sequence,
				v4l2_video_cloned,
				v4l2_video_dropped,
				v4l2_video_missed);

		fprintf(stderr, module "        time spent recording: according to card: %G sec, according to host: %G sec\n",
				(double)(v4l2_video_sequence * v4l2_frame_interval) / ts_mul,
				(double)(now - v4l2_timestamp_start) / ts_mul);
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

		if(vob->im_v_codec == CODEC_RGB)
		{
			v4l2_yuv = 0;
		}
		else
		{
			if(vob->im_v_codec == CODEC_YUV)
			{
				if(!vob->im_v_string || !strcmp(vob->im_v_string, "yuv422"))
				{
					v4l2_yuv = 1;
				}
				else
				{
					fprintf(stderr, module "codec string != yuv422: %s\n", vob->im_v_string);
					return(TC_IMPORT_ERROR);
				}
			}
			else
			{
				fprintf(stderr, module "codec != CODEC_RGB && codec != CODEC_YUV\n");
				fprintf(stderr, module "codec: %d\n", vob->im_v_codec);
	
				return(TC_IMPORT_ERROR);
			}
		}

		if(v4l2_video_init(vob->video_in_file, vob->im_v_width, vob->im_v_height, vob->fps,
					vob->dvd_title, vob->dvd_chapter1))
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
		if(v4l2_video_get_frame(param->size, param->buffer, &param->attributes))
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
