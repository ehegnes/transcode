# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign
DEFAULT_INCLUDES = -I$(top_srcdir) -I$(srcdir) -I$(top_builddir) -I. @DLDARWININCL@ -I../libioaux -I$(srcdir)/../../libioaux 

CFLAGS = -Wall -I../src -I../avilib -I$(srcdir)/../src -I$(srcdir)/../avilib -I$(srcdir)/../libac3 @CFLAGS@ -DMOD_PATH=\"@MOD_PATH@\"

EXTRA_DIST = buffer.h aud_scan.h

bin_PROGRAMS = avimerge avisplit avifix avisync aviindex tcmodinfo
 
avisync_SOURCES = avisync.c buffer.c aud_scan.c
avisync_LDADD = -L../avilib -lavi

avimerge_SOURCES = avimerge.c aud_scan.c
avimerge_LDADD = -L../avilib -lavi

avifix_SOURCES = avifix.c aud_scan.c
avifix_LDADD = -L../avilib -lavi

aviindex_SOURCES = aviindex.c aud_scan.c
aviindex_LDADD = -L../avilib -lavi

avisplit_SOURCES = avisplit.c aud_scan.c 
avisplit_LDADD = -L../avilib -lavi -L../libioaux -ltcioaux

tcmodinfo_SOURCES = tcmodinfo.c
tcmodinfo_LDADD = @DLDARWINLIB@ -lm ../src/optstr.o
tcmodinfo_LDFLAGS = -rdynamic

