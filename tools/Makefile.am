# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign
DEFAULT_INCLUDES = -I$(top_srcdir) -I$(srcdir) -I$(top_builddir) -I. @DLDARWININCL@ -I../libioaux -I$(srcdir)/../../libioaux 

CFLAGS = -Wall -I../src -I../avilib -I../libac3 -I$(srcdir)/../src -I$(srcdir)/../avilib -I$(srcdir)/../libac3 @CFLAGS@ -DMOD_PATH=\"@MOD_PATH@\"

EXTRA_DIST = *.h 

bin_PROGRAMS = avimerge avisplit avifix avisync tcmodinfo
 
avisync_SOURCES = avisync.c buffer.c
avisync_LDADD = -L../avilib -lavi

avimerge_SOURCES = avimerge.c 
avimerge_LDADD = -L../avilib -lavi

avifix_SOURCES = avifix.c 
avifix_LDADD = -L../avilib -lavi

avisplit_SOURCES = avisplit.c
avisplit_LDADD = -L../avilib -lavi -L../libioaux -ltcioaux

tcmodinfo_SOURCES = tcmodinfo.c
tcmodinfo_LDADD = @DLDARWINLIB@ -lm ../src/optstr.o
tcmodinfo_LDFLAGS = -rdynamic

