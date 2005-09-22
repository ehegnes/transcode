# # Process this file with automake to produce Makefile.in.

AM_CPPFLAGS = \
	$(PTHREAD_CFLAGS) \
	-DLINUX \
	-I$(top_srcdir) \
	$(MPEG2CVS_CFLAGS)

noinst_LTLIBRARIES = libac.la

libac_la_SOURCES = \
	tcmemcpy.c \
	cputest.c \
	rescale.c \
	average.c \
	swap.c \
	rgb2yuv.c \
	rgb2yuv.h \
	yuv2rgb.c \
	yuv2rgb.h \
	yuv2rgb_mmx.c \
	yuv2rgb_mlib.c

EXTRA_DIST = \
	ac.h \
	cputest.h \
	mmx.h \
	movtest.c \
	actest.c \
	make.sh \
	COPYING.libvo \
	AUTHORS.libvo \
	README.libvo
