# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign
DEFAULT_INCLUDES = -I$(top_srcdir) -I$(srcdir) -I$(top_builddir) -I.

noinst_LIBRARIES = libtcioaux.a

CFLAGS=-O3 -funroll-loops -ffast-math -DLINUX -Wall @CFLAGS@
libtcioaux_a_SOURCES = framecode.c configs.c

noinst_HEADERS = configs.h framecode.h
EXTRA_DIST = configs.h framecode.h framecode.l
