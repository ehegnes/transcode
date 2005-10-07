# # Process this file with automake to produce Makefile.in.

AM_CPPFLAGS = \
	$(PTHREAD_CFLAGS) \
	-DLINUX \
	-I$(top_srcdir)

noinst_LTLIBRARIES = libac.la

libac_la_SOURCES = \
	average.c \
	cputest.c \
	imgconvert.c \
	img_rgb_packed.c \
	img_yuv_mixed.c \
	img_yuv_packed.c \
	img_yuv_planar.c \
	img_yuv_rgb.c \
	rescale.c \
	swap.c \
	tcmemcpy.c

EXTRA_DIST = \
	ac.h \
	cputest.h \
	imgconvert.h \
	img_internal.h
	mmx.h \
	movtest.c \
	actest.c \
	make.sh
