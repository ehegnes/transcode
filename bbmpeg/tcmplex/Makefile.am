# # Process this file with automake to produce Makefile.in.
AUTOMAKE_OPTIONS = 1.3 foreign
DEFAULT_INCLUDES = -I$(top_srcdir) -I$(srcdir) -I$(top_builddir) -I.

CFLAGS = -O3 -funroll-loops -ffast-math -DLINUX @CFLAGS@
INCLUDES = -Wall -I./ -I ../ -I../../ -I../../src -I$(srcdir)/.. -I$(srcdir)/../.. -I$(srcdir)/../../src

#EXTRA_DIST = *.h 

bin_PROGRAMS = tcmplex

tcmplex_SOURCES = tcmplex.c domplex.c multplex.c inits.c interact.c timecode.c inptstrm.c buffer.c systems.c
tcmplex_LDADD = -L../ -lbbmpeg -lm
