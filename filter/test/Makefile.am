# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign
DEFAULT_INCLUDES = -I$(top_srcdir) -I$(srcdir) -I$(top_builddir) -I.

INCLUDES = -Wall -I./ -I../.. -I../../src/ -I$(srcdir)/../.. -I$(srcdir)/../../src -I../../aclib/

CFLAGS = -D_REENTRANT @CFLAGS@

pkgdir=@MOD_PATH@

pkg_LTLIBRARIES = filter_test.la 

filter_test_la_SOURCES = filter_test.c
filter_test_la_LDFLAGS = -no-undefined -module -avoid-version 
filter_test_la_LIBADD = -L../../aclib/ -lac

