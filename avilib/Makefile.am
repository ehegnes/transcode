# Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign

noinst_LIBRARIES = libavi.a

INCLUDES = -I$(top_srcdir)/libxio

libavi_a_SOURCES = \
	avidump.c \
	avilib.c avilib.h \
	avimisc.c \
	os.h

EXTRA_DIST = README.avilib
