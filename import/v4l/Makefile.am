# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign
DEFAULT_INCLUDES = -I$(top_srcdir) -I$(srcdir) -I$(top_builddir) -I.

INCLUDES = -I./ -I../../src -I../ -I../../ -Wall -I$(srcdir)/../../src -I$(srcdir)/.. -I$(srcdir)/../.. -I../../libioaux -I$(srcdir)/../../libioaux 

CFLAGS = -D_REENTRANT @CFLAGS@

pkgdir=@MOD_PATH@

if HAVE_V4L
MOD=import_v4l.la 
endif

pkg_LTLIBRARIES = $(MOD) 

import_v4l_la_SOURCES = import_v4l.c common.c video.c audio.c frequencies.c
import_v4l_la_LDFLAGS = -no-undefined -module -avoid-version 
 
EXTRA_DIST = audio.h channels.h common.h counter.h frequencies.h vcr.h video.h videodev.h

