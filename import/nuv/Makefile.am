# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign
DEFAULT_INCLUDES = -I$(top_srcdir) -I$(srcdir) -I$(top_builddir) -I.

INCLUDES = -Wall -I./ -I../ -I../../src -I$(srcdir)/.. -I$(srcdir)/../.. -I$(srcdir)/../../src
 
CFLAGS = -D_REENTRANT -funroll-loops -ffast-math -Wall -DMOD_PATH=\"@MOD_PATH@\" @CFLAGS@ 

pkgdir=@MOD_PATH@
pkg_LTLIBRARIES=import_nuv.la 

import_nuv_la_SOURCES = RTjpegN.c  import_nuv.c  minilzo.c  resample.c  rtjpeg_aud_plugin.c rtjpeg_vid_plugin.c soxfuncs.c
import_nuv_la_LDFLAGS = -no-undefined -module -avoid-version 
import_nuv_la_LIBADD = -lm

EXTRA_DIST = *.h LICENSE.Nuppelvideo LICENSE.rtjpeg README.rtjpeg


