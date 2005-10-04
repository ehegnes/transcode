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
	colorspace.c \
	swap.c

EXTRA_DIST = \
	ac.h \
	colorspace.h \
	cputest.h \
	mmx.h \
	movtest.c \
	actest.c \
	make.sh \
	COPYING.libvo \
	AUTHORS.libvo \
	README.libvo
