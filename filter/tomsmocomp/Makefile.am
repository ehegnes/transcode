# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign
DEFAULT_INCLUDES = -I$(top_srcdir) -I$(srcdir) -I$(top_builddir) -I.

INCLUDES = -Wall -I./ -I../.. -I../../src/ -I$(srcdir)/../.. -I$(srcdir)/../../src -I../../aclib/

CFLAGS = -D_REENTRANT @CFLAGS@ -fno-omit-frame-pointer

pkgdir=@MOD_PATH@

pkg_LTLIBRARIES = filter_tomsmocomp.la 

filter_tomsmocomp_la_SOURCES = filter_tomsmocomp.c transcoders.c tomsmocompfilter_mmx.c tomsmocompfilter_3dnow.c tomsmocompfilter_sse.c

filter_tomsmocomp_la_LDFLAGS =  -module -avoid-version
filter_tomsmocomp_la_LIBADD = -L../../aclib/ -lac

