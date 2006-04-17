# # Process this file with automake to produce Makefile.in.

AM_CPPFLAGS = \
	$(PTHREAD_CFLAGS) \
	-DMOD_PATH=\"$(MOD_PATH)\" \
	-I$(top_srcdir) \
	-I$(top_srcdir)/export \
	-I$(top_srcdir)/import \
	$(LIBTC_CFLAGS) \
	$(ACLIB_CFLAGS) \
	$(AVILIB_CFLAGS) \
	$(WAVLIB_CFLAGS) \
	$(LIBAVCODEC_CFLAGS) \
	$(DLDARWININCL)

AM_LDFLAGS = -export-dynamic

bin_PROGRAMS = transcode

transcode_LDADD = \
	$(DLDARWINLIB) \
	$(LIBTC_LIBS) \
	$(LIBTCAUDIO_LIBS) \
	$(LIBTCVIDEO_LIBS) \
	$(ACLIB_LIBS) \
	$(AVILIB_LIBS) \
	$(WAVLIB_LIBS) \
	$(XIO_LIBS) \
	$(LIBXML2_LIBS) \
	$(PTHREAD_LIBS) \
	-lm

EXTRA_DIST = \
	audio_trans.h \
	counter.h \
	decoder.h \
	dl_loader.h \
	encoder.h \
	encoder-common.h \
	encoder-buffer.h \
	filter.h \
	frame_threads.h \
	framebuffer.h \
	probe.h \
	socket.h \
	split.h \
	tc_defaults.h \
	transcode.h \
	usage.h \
	video_trans.h \
	probe_export.h

transcode_SOURCES = \
	transcode.c \
	audio_buffer.c \
	audio_trans.c \
	callback.c \
	counter.c \
	decoder.c \
	dl_loader.c \
	encoder.c \
	encoder-common.c \
	encoder-buffer.c \
	filter.c \
	frame_threads.c \
	info_server.c \
	probe.c \
	socket.c \
	split.c \
	video_buffer.c \
	video_trans.c
