# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign
DEFAULT_INCLUDES = -I$(top_srcdir) -I$(srcdir) -I$(top_builddir) -I.

INCLUDES = -Wall -I./ -I../.. -I../../src/ -I$(srcdir)/../.. -I$(srcdir)/../../src

CFLAGS = -D_REENTRANT @CFLAGS@

pkgdir=@MOD_PATH@

pkg_LTLIBRARIES = filter_resample.la 

filter_resample_la_SOURCES = filter_resample.c resample.c soxfuncs.c
filter_resample_la_LDFLAGS =  -module -avoid-version 

EXTRA_DIST = *.h 


