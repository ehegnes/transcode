# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign
DEFAULT_INCLUDES = -I$(top_srcdir) -I$(srcdir) -I$(top_builddir) -I.

lib_LTLIBRARIES = libtcioaux.la

INCLUDES = -I./

CFLAGS=-O3 -funroll-loops -ffast-math -DLINUX -Wall @CFLAGS@
libtcioaux_la_SOURCES = framecode.c configs.c

EXTRA_DIST = *.h *.l




