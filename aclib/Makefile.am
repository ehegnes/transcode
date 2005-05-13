# # Process this file with automake to produce Makefile.in.

AM_CPPFLAGS = \
	$(PTHREAD_CFLAGS) \
	-DLINUX \
	-I$(top_srcdir)

noinst_LTLIBRARIES = libac.la

libac_la_SOURCES = \
	tcmemcpy.c \
	cputest.c \
	rescale.c \
	average.c \
	swap.c

EXTRA_DIST = \
	ac.h \
	cputest.h \
	movtest.c \
	actest.c \
	make.sh
