# # Process this file with automake to produce Makefile.in.
AUTOMAKE_OPTIONS = 1.3 foreign
DEFAULT_INCLUDES = -I$(top_srcdir) -I$(srcdir) -I$(top_builddir) -I.

CFLAGS = -O2 -g -D_REENTRANT -DLINUX @CFLAGS@ @DVDREAD_CFLAGS@

noinst_LIBRARIES = libdvdread_tc.a

libdvdread_tc_a_SOURCES = dvd_reader.c dvd_udf.c ifo_read.c ifo_print.c \
	nav_read.c nav_print.c

noinst_HEADERS = nav_types.h nav_read.h nav_print.h ifo_types.h ifo_read.h ifo_print.h dvd_udf.h dvd_reader.h dvdcss.h bswap.h  

EXTRA_DIST = AUTHORS COPYING ChangeLog README NEWS TODO

