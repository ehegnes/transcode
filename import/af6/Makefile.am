# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign
DEFAULT_INCLUDES = -I$(top_srcdir) -I$(srcdir) -I$(top_builddir) -I.

INCLUDES = -Wall -I./ -I../ -I../../src -I$(srcdir)/.. -I$(srcdir)/../.. -I$(srcdir)/../../src
 
CFLAGS = -D_REENTRANT -funroll-loops -ffast-math -Wall -DMOD_PATH=\"@MOD_PATH@\" @CFLAGS@ 

if AVIFILE_0_6
MOD = import_af6.la
else
MOD =
endif

pkgdir=@MOD_PATH@
pkg_LTLIBRARIES=$(MOD) 

import_af6_la_SOURCES = import_af6.c
import_af6_la_LDFLAGS = -no-undefined -module -avoid-version 

#EXTRA_DIST = *.h

