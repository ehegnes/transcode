# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign
DEFAULT_INCLUDES = -I$(top_srcdir) -I$(srcdir) -I$(top_builddir) -I.

noinst_LIBRARIES = libioaux.a

INCLUDES = -I./

CFLAGS=-O3 -funroll-loops -ffast-math -DLINUX -Wall @CFLAGS@
libioaux_a_SOURCES = framecode.c configs.c

EXTRA_DIST = *.h *.l




