# # Process this file with automake to produce Makefile.in.

AM_CPPFLAGS = \
	$(PTHREAD_CFLAGS) \
	$(MPEG2CVS_CFLAGS)

noinst_LTLIBRARIES = libvout.la

libvout_la_SOURCES = \
	cpu_accel.c \
	mm_accel.h \
	mmx.h \
	rgb2yuv.c \
	rgb2yuv.h \
	video_out.c \
	video_out.h \
	video_out_internal.h \
	video_out_ppm.c \
	video_out_yuv.c \
	yuv2rgb.c yuv2rgb.h \
	yuv2rgb_mlib.c \
	yuv2rgb_mmx.c

EXTRA_DIST = \
	COPYING \
	AUTHORS \
	README
