# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign
DEFAULT_INCLUDES = -I$(top_srcdir) -I$(srcdir) -I$(top_builddir) -I.

pkgdir=@MOD_PATH@

pkg_LTLIBRARIES = libioaux.la

INCLUDES = -I./

CFLAGS=-O3 -funroll-loops -ffast-math -DLINUX -Wall @CFLAGS@
libioaux_la_SOURCES = framecode.c configs.c

EXTRA_DIST = *.h *.l




