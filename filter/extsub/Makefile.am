# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign
DEFAULT_INCLUDES = -I$(top_srcdir) -I$(srcdir) -I$(top_builddir) -I.

INCLUDES = -I./ -Wall -I../.. -I../../src/ -I$(srcdir)/../.. -I$(srcdir)/../../src

CFLAGS = -D_REENTRANT @CFLAGS@

pkgdir=@MOD_PATH@

pkg_LTLIBRARIES = filter_extsub.la filter_extsub2.la 

filter_extsub_la_SOURCES = filter_extsub.c subproc.c subtitle_buffer.c
filter_extsub_la_LDFLAGS = -no-undefined -module -avoid-version 

filter_extsub2_la_SOURCES = filter_extsub.c subproc.c subtitle_buffer.c
filter_extsub2_la_LDFLAGS = -no-undefined -module -avoid-version 

EXTRA_DIST = *.h 


