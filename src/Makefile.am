# # Process this file with automake to produce Makefile.in.
AUTOMAKE_OPTIONS = 1.3 foreign
DEFAULT_INCLUDES = -I$(top_srcdir) -I$(srcdir) -I$(top_builddir) -I.

CFLAGS = -D_REENTRANT -Wall -funroll-loops -ffast-math -finline-functions @DLDARWININCL@ @SUPPORTINCL@ -I../libxio -I../export -I../import -I$(srcdir)/../export -I$(srcdir)/../import -export-dynamic -I../aclib -DMOD_PATH=\"@MOD_PATH@\" @CFLAGS@

bin_PROGRAMS = transcode

if SYSTEM_DARWIN
transcode_LDFLAGS= -Xlinker -m
endif


#AM_LDFLAGS=`ar t ../ffmpeg/libavcodec/libtcavcodec.a | sed -e '/^__/d' -e 's@^@../ffmpeg/libavcodec/@' | tr '\n' ' '` `ar t ../avilib/libavi.a | sed -e '/^__/d' -e 's@^@../avilib/@' | tr '\n' ' '`

transcode_LDADD= @DLDARWINLIB@ @SUPPORTLIB@ @TCIOAUX_LIBS@ -L../aclib -lac -L../ffmpeg/libavcodec -ltcavcodec @AVILIB_LIBS@ -L../libvo -lvout -L../libxio -lxio @IBP_LIBS@ @LIBXML2_LIBS@ -lm 

EXTRA_DIST =  \
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
   tc_functions.h \
   tc_func_excl.h \
   probe_export.h \
   zoom.h

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
			video_yuv422.c\
			filter.c\
			counter.c\
			zoom.c\
			probe.c\
			split.c\
			callback.c\
			socket.c\
			optstr.c\
			tc_functions.c\
			info_server.c








