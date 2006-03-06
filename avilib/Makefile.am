# # Process this file with automake to produce Makefile.in.

AM_CPPFLAGS = \
	$(PTHREAD_CFLAGS) \
	$(XIO_CFLAGS)

noinst_LTLIBRARIES = libavi.la libwav.la

libavi_la_SOURCES = \
	avidump.c \
	avilib.c \
	avilib.h \
	avimisc.c \
	os.h

libwav_la_SOURCES = \
	wavlib.c \
	wavlib.h

EXTRA_DIST = README.avilib
