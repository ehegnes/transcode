# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign
DEFAULT_INCLUDES = -I$(top_srcdir) -I$(srcdir) -I$(top_builddir) -I. @DLDARWININCL@ @TCIOAUX_CFLAGS@

CFLAGS = -Wall -I../src -I$(srcdir)/../src @AVILIB_CFLAGS@ -I$(srcdir)/../libac3 @CFLAGS@ -DMOD_PATH=\"@MOD_PATH@\"

bin_PROGRAMS = avimerge avisplit avifix avisync aviindex tcmodinfo tcmp3cut
 
avisync_SOURCES = avisync.c buffer.c buffer.h aud_scan.c aud_scan.h
avisync_LDADD = @AVILIB_LIBS@ -L../libxio -lxio @IBP_LIBS@ @LIBXML2_LIBS@ 

avimerge_SOURCES = avimerge.c aud_scan.c aud_scan.h
avimerge_LDADD = @AVILIB_LIBS@ -L../libxio -lxio @IBP_LIBS@ @LIBXML2_LIBS@

avifix_SOURCES = avifix.c aud_scan.c aud_scan.h
avifix_LDADD = @AVILIB_LIBS@ -L../libxio -lxio @IBP_LIBS@ @LIBXML2_LIBS@

aviindex_SOURCES = aviindex.c aud_scan.c aud_scan.h
aviindex_LDADD = @AVILIB_LIBS@ -L../libxio -lxio @IBP_LIBS@ @LIBXML2_LIBS@

avisplit_SOURCES = avisplit.c aud_scan.c aud_scan.h
avisplit_LDADD = @AVILIB_LIBS@ @TCIOAUX_LIBS@ -L../libxio -lxio @IBP_LIBS@ @LIBXML2_LIBS@

tcmodinfo_SOURCES = tcmodinfo.c
tcmodinfo_LDADD = @DLDARWINLIB@ -ldl -lm ../src/optstr.o
tcmodinfo_LDFLAGS = -rdynamic 

tcmp3cut_SOURCES = tcmp3cut.c

