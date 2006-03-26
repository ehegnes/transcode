# # Process this file with automake to produce Makefile.in.

AM_CPPFLAGS = \
	$(PTHREAD_CFLAGS) \
	-DMOD_PATH=\"$(MOD_PATH)\" \
	-DTC_ENCODER_NG=1 \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/libtc \
	-I$(top_srcdir)/aclib \
	$(AC3_CFLAGS) \
	$(AVILIB_CFLAGS) \
	$(DLDARWININCL) \
	$(TCIOAUX_CFLAGS) \
	$(XIO_CFLAGS)

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
	aud_scan_avi.c \
	aud_scan_avi.h \
	aud_scan.h
avisync_LDADD = \
	$(AVILIB_LIBS) \
	$(XIO_LIBS) \
	$(ACLIB_LIBS) \
	$(LIBTC_LIBS)

avimerge_SOURCES = \
	avimerge.c \
	aud_scan.c \
	aud_scan_avi.c \
	aud_scan_avi.h \
	aud_scan.h
avimerge_LDADD = \
	$(AVILIB_LIBS) \
	$(XIO_LIBS) \
	$(ACLIB_LIBS) \
	$(LIBTC_LIBS)

avifix_SOURCES = \
	avifix.c \
	aud_scan.c \
	aud_scan_avi.c \
	aud_scan_avi.h \
	aud_scan.h
avifix_LDADD = \
	$(AVILIB_LIBS) \
	$(XIO_LIBS) \
	$(ACLIB_LIBS) \
	$(LIBTC_LIBS)

aviindex_SOURCES = \
	aviindex.c \
	aud_scan.c \
	aud_scan_avi.c \
	aud_scan_avi.h \
	aud_scan.h
aviindex_LDADD = \
	$(AVILIB_LIBS) \
	$(XIO_LIBS) \
	$(ACLIB_LIBS) \
	$(LIBTC_LIBS)

avisplit_SOURCES = \
	avisplit.c \
	aud_scan.c \
	aud_scan_avi.c \
	aud_scan_avi.h \
	aud_scan.h
avisplit_LDADD = \
	$(AVILIB_LIBS) \
	$(TCIOAUX_LIBS) \
	$(XIO_LIBS) \
	$(ACLIB_LIBS) \
	$(LIBTC_LIBS)

tcmodinfo_SOURCES = tcmodinfo.c
tcmodinfo_LDADD = \
	$(DLDARWINLIB) \
	$(ACLIB_LIBS) \
	$(AVILIB_LIBS) \
	$(LIBTC_LIBS) \
	$(LIBTCVIDEO_LIBS) \
	$(PTHREAD_LIBS) \
	-lm
tcmodinfo_LDFLAGS = -export-dynamic

tcmp3cut_SOURCES = \
	tcmp3cut.c \
	aud_scan.c \
	aud_scan.h
tcmp3cut_LDADD = \
	$(LIBTC_LIBS)
