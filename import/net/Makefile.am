# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign
DEFAULT_INCLUDES = -I$(top_srcdir) -I$(srcdir) -I$(top_builddir) -I.

INCLUDES = -Wall -I./ -I../ -I../../src -I$(srcdir)/.. -I$(srcdir)/../.. -I$(srcdir)/../../src
 
CFLAGS = -D_REENTRANT -funroll-loops -ffast-math -Wall -DMOD_PATH=\"@MOD_PATH@\" @CFLAGS@ 

if NET_STREAM
MOD = import_net.la
endif

pkgdir=@MOD_PATH@
pkg_LTLIBRARIES=$(MOD) 

import_net_la_SOURCES = import_net.c 
import_net_la_LDFLAGS = -no-undefined -module -avoid-version 

