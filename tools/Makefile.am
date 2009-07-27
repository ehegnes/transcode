# # Process this file with automake to produce Makefile.in.

AM_CPPFLAGS = \
	$(PTHREAD_CFLAGS) \
	-DMOD_PATH=\"$(MOD_PATH)\" \
	-DPROF_PATH=\"$(PROF_PATH)\" \
	-DREGISTRY_PATH=\"$(REGISTRY_PATH)\" \
	-DTCEXPORT_PROFILE=1 \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src

bin_PROGRAMS = \
	avifix \
	aviindex \
	avimerge \
	avisplit \
	avisync \
	tccfgshow \
	tcexport \
	tcmodchain \
	tcmodinfo \
	tcmp3cut \
	tcyait

avisync_SOURCES = \
	avisync.c \
	avimisc.c \
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
	$(LIBTC_LIBS) \
	$(LIBTCUTIL_LIBS)

avimerge_SOURCES = \
	avimerge.c \
	avimisc.c \
	aud_scan.c \
	aud_scan_avi.c \
	aud_scan_avi.h \
	aud_scan.h
avimerge_LDADD = \
	$(AVILIB_LIBS) \
	$(XIO_LIBS) \
	$(ACLIB_LIBS) \
	$(LIBTC_LIBS) \
	$(LIBTCUTIL_LIBS)

avifix_SOURCES = \
	avifix.c \
	avimisc.c
avifix_LDADD = \
	$(AVILIB_LIBS) \
	$(XIO_LIBS) \
	$(ACLIB_LIBS) \
	$(LIBTC_LIBS) \
	$(LIBTCUTIL_LIBS)

aviindex_SOURCES = \
	aviindex.c \
	avimisc.c \
	aud_scan.c \
	aud_scan.h
aviindex_LDADD = \
	$(AVILIB_LIBS) \
	$(XIO_LIBS) \
	$(ACLIB_LIBS) \
	$(LIBTC_LIBS) \
	$(LIBTCUTIL_LIBS)

avisplit_SOURCES = \
	avisplit.c \
	avimisc.c \
	aud_scan.c \
	aud_scan_avi.c \
	aud_scan.h
	aud_scan_avi.h
avisplit_LDADD = \
	$(AVILIB_LIBS) \
	$(XIO_LIBS) \
	$(ACLIB_LIBS) \
	$(LIBTC_LIBS) \
	$(LIBTCUTIL_LIBS)

tccfgshow_SOURCES = tccfgshow.c
tccfgshow_CPPFLAGS = $(AM_CPPFLAGS) \
	$(DLDARWIN_CFLAGS)
tccfgshow_LDADD = \
	$(LIBTC_LIBS) \
	$(LIBTCEXPORT_LIBS) \
	$(LIBTCUTIL_LIBS) \
	$(LIBTCMODULE_LIBS)

tcmodinfo_SOURCES = tcmodinfo.c tcstub.c
tcmodinfo_CPPFLAGS = $(AM_CPPFLAGS) \
	$(DLDARWIN_CFLAGS) \
	-DTC_FRAMEBUFFER_STUBS=1
tcmodinfo_LDADD = \
	$(DLDARWIN_LIBS) \
	$(ACLIB_LIBS) \
	$(AVILIB_LIBS) \
	$(GRAPHICSMAGICK_LIBS) \
	$(LIBTC_LIBS) \
	$(LIBTCEXT_LIBS) \
	$(LIBTCMODULE_LIBS) \
	$(LIBTCVIDEO_LIBS) \
	$(LIBTCUTIL_LIBS) \
	$(PTHREAD_LIBS) \
	$(WAVLIB_LIBS) \
	-lm
tcmodinfo_LDFLAGS = -export-dynamic

tcmodchain_SOURCES = tcmodchain.c tcstub.c
tcmodchain_CPPFLAGS = $(AM_CPPFLAGS) \
	$(DLDARWIN_CFLAGS) \
	-DTC_FRAMEBUFFER_STUBS=1
tcmodchain_LDADD = \
	$(DLDARWIN_LIBS) \
	$(ACLIB_LIBS) \
	$(AVILIB_LIBS) \
	$(GRAPHICSMAGICK_LIBS) \
	$(LIBTC_LIBS) \
	$(LIBTCEXT_LIBS) \
	$(LIBTCUTIL_LIBS) \
	$(LIBTCMODULE_LIBS) \
	$(LIBTCVIDEO_LIBS) \
	$(PTHREAD_LIBS) \
	$(WAVLIB_LIBS) \
	-lm
tcmodchain_LDFLAGS = -export-dynamic

tcexport_SOURCES = tcexport.c \
	tcstub.c \
	rawsource.c \
	rawsource.h \
	runcontrol.c \
	../src/framebuffer.c \
	../src/dl_loader.c \
	../src/probe.c
tcexport_CPPFLAGS = $(AM_CPPFLAGS) \
	$(DLDARWIN_CFLAGS)
tcexport_LDADD = \
	$(DLDARWIN_LIBS) \
	$(ACLIB_LIBS) \
	$(AVILIB_LIBS) \
	$(GRAPHICSMAGICK_LIBS) \
	$(LIBTC_LIBS) \
	$(LIBTCEXT_LIBS) \
	$(LIBTCVIDEO_LIBS) \
	$(LIBTCAUDIO_LIBS) \
	$(LIBTCEXPORT_LIBS) \
	$(LIBTCMODULE_LIBS) \
	$(LIBTCUTIL_LIBS) \
	$(XIO_LIBS) \
	$(PTHREAD_LIBS) \
	$(WAVLIB_LIBS) \
	-lm
tcexport_LDFLAGS = -export-dynamic

tcmp3cut_SOURCES = \
	tcmp3cut.c \
	aud_scan.c \
	aud_scan.h
tcmp3cut_LDADD = \
	$(LIBTC_LIBS) \
	$(LIBTCUTIL_LIBS)

tcyait_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/filter
tcyait_SOURCES = \
	tcyait.c
tcyait_LDADD = \
	-lm

