# # Process this file with automake to produce Makefile.in.
AUTOMAKE_OPTIONS = 1.3 foreign
DEFAULT_INCLUDES = -I$(top_srcdir) -I$(srcdir) -I$(top_builddir) -I.

CFLAGS = -D_REENTRANT -Wall -funroll-loops -ffast-math -finline-functions @DLDARWININCL@ @SUPPORTINCL@ -I../export -I../import -I$(srcdir)/../export -I$(srcdir)/../import -export-dynamic -I../aclib -DMOD_PATH=\"@MOD_PATH@\" @CFLAGS@ --include $(top_srcdir)/src/transcode.h 

bin_PROGRAMS = transcode

transcode_LDADD= @DLDARWINLIB@ @SUPPORTLIB@ -L../libioaux -ltcioaux -L../aclib -lac -lm 

EXTRA_DIST = *.h

transcode_SOURCES =	transcode.c\
			iodir.c\
			decoder.c\
			encoder.c\
			dl_loader.c\
			frame_threads.c\
			video_buffer.c\
			audio_buffer.c\
			frame_preprocess.c\
			frame_postprocess.c\
			video_trans.c\
			audio_trans.c\
			video_rgb.c\
			video_yuv.c\
			filter.c\
			counter.c\
			zoom.c\
			probe.c\
			split.c\
			callback.c\
			socket.c\
			optstr.c\
			info_server.c








