# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign
DEFAULT_INCLUDES = -I$(top_srcdir) -I$(srcdir) -I$(top_builddir) -I.

INCLUDES = -Wall -I./ -I../.. -I../../src/ -I$(srcdir)/../.. -I$(srcdir)/../../src -I../../libvo

CFLAGS = -fomit-frame-pointer -D_REENTRANT @CFLAGS@

pkgdir=@MOD_PATH@

pkg_LTLIBRARIES = \
	 filter_pp.la

filter_pp_la_SOURCES = filter_pp.c postprocess.c
filter_pp_la_LDFLAGS =  -module -avoid-version 


EXTRA_DIST = postprocess.h postprocess_internal.h postprocess_template.c mangle.h


