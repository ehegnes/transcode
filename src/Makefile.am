# # Process this file with automake to produce Makefile.in.

AM_CPPFLAGS = \
	$(PTHREAD_CFLAGS) \
	-DMOD_PATH=\"$(MOD_PATH)\" \
	-I$(top_srcdir) \
	-I$(top_srcdir)/export \
	-I$(top_srcdir)/import \
	$(XIO_CFLAGS) \
	$(ACLIB_CFLAGS) \
	$(LIBAVCODEC_CFLAGS) \
	$(DLDARWININCL) \
	$(SUPPORT_INCLUDES)

AM_LDFLAGS = -export-dynamic

bin_PROGRAMS = transcode

transcode_LDADD = \
	$(DLDARWINLIB) \
	$(LIBTC_LIBS) \
	$(SUPPORT_LIBS) \
	$(TCIOAUX_LIBS) \
	$(ACLIB_LIBS) \
	$(AVILIB_LIBS) \
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
	filter.h \
	frame_threads.h \
	framebuffer.h \
	iodir.h \
	optstr.h \
	probe.h \
	socket.h \
	split.h \
	tc_defaults.h \
	transcode.h \
	usage.h \
	video_trans.h \
	probe_export.h \
	zoom.h

transcode_SOURCES = \
	transcode.c \
	audio_buffer.c \
	audio_trans.c \
	callback.c \
	counter.c \
	decoder.c \
	dl_loader.c \
	encoder.c \
	filter.c \
	frame_postprocess.c \
	frame_preprocess.c \
	frame_threads.c \
	info_server.c \
	iodir.c \
	optstr.c \
	probe.c \
	socket.c \
	split.c \
	video_buffer.c \
	video_rgb.c \
	video_trans.c \
	video_yuv.c \
	video_yuv422.c \
	zoom.c
