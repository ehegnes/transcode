# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign
DEFAULT_INCLUDES = -I$(top_srcdir) -I$(srcdir) -I$(top_builddir) -I.

INCLUDES = -I./ -Wall -I../.. -I../../src/ -I$(srcdir)/../.. -I$(srcdir)/../../src

if BIG_ENDIAN
BG = -DBIG_ENDIAN
endif

CFLAGS = -D_REENTRANT @CFLAGS@ $(BG)

pkgdir=@MOD_PATH@

pkg_LTLIBRARIES = filter_divxkey.la 

filter_divxkey_la_SOURCES = filter_divxkey.c bitstream.c
filter_divxkey_la_LDFLAGS = -no-undefined -module -avoid-version 

EXTRA_DIST = *.h 


