# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign

AM_CPPFLAGS = \
	-DMOD_PATH=\"@MOD_PATH@\" \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	@AC3_CFLAGS@ \
	@AVILIB_CFLAGS@ \
	@DLDARWININCL@ \
	@TCIOAUX_CFLAGS@ \
	@XIO_CFLAGS@

bin_PROGRAMS = \
	avifix \
	aviindex \
	avimerge \
	avisplit \
	avisync \
	tcmodinfo \
	tcmp3cut

avisync_SOURCES = \
	avisync.c \
	buffer.c \
	buffer.h \
	aud_scan.c \
	aud_scan.h
avisync_LDADD = \
	@AVILIB_LIBS@ \
	@XIO_LIBS@ \
	@ACLIB_LIBS@

avimerge_SOURCES = \
	avimerge.c \
	aud_scan.c \
	aud_scan.h
avimerge_LDADD = \
	@AVILIB_LIBS@ \
	@XIO_LIBS@ \
	@ACLIB_LIBS@

avifix_SOURCES = \
	avifix.c \
	aud_scan.c \
	aud_scan.h
avifix_LDADD = \
	@AVILIB_LIBS@ \
	@XIO_LIBS@ \
	@ACLIB_LIBS@

aviindex_SOURCES = \
	aviindex.c \
	aud_scan.c \
	aud_scan.h
aviindex_LDADD = \
	@AVILIB_LIBS@ \
	@XIO_LIBS@ \
	@ACLIB_LIBS@

avisplit_SOURCES = \
	avisplit.c \
	aud_scan.c \
	aud_scan.h
avisplit_LDADD = \
	@AVILIB_LIBS@ \
	@TCIOAUX_LIBS@ \
	@XIO_LIBS@ \
	@ACLIB_LIBS@

tcmodinfo_SOURCES = tcmodinfo.c
tcmodinfo_LDADD = \
	@DLDARWINLIB@ \
	@ACLIB_LIBS@ \
	-lm \
	$(top_builddir)/src/optstr.o
tcmodinfo_LDFLAGS = -export-dynamic 

tcmp3cut_SOURCES = tcmp3cut.c
