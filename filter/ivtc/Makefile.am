# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign
DEFAULT_INCLUDES = -I$(top_srcdir) -I$(srcdir) -I$(top_builddir) -I.

INCLUDES = -Wall -I./ -I../.. -I../../src/ -I$(srcdir)/../.. -I$(srcdir)/../../src

CFLAGS = -D_REENTRANT @CFLAGS@

pkgdir=@MOD_PATH@

pkg_LTLIBRARIES = filter_decimate.la filter_ivtc.la 

filter_decimate_la_SOURCES = filter_decimate.c
filter_decimate_la_LDFLAGS = -no-undefined -module -avoid-version 

filter_ivtc_la_SOURCES = filter_ivtc.c
filter_ivtc_la_LDFLAGS = -no-undefined -module -avoid-version 

EXTRA_DIST = README.Inverse.Telecine



