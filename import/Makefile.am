AUTOMAKE_OPTIONS = 1.3 foreign

INCLUDES = \
	-DMOD_PATH=\"@MOD_PATH@\" \
	-D_REENTRANT \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/libvo \
	@AC3_CFLAGS@ \
	@MPEG2CVS_CFLAGS@ \
	@AVILIB_CFLAGS@ \
	@XIO_CFLAGS@ \
	@LAME_CFLAGS@ \
	@DV_CFLAGS@ \
	@LIBMPEG3_CFLAGS@ \
	@A52_CFLAGS@ \
	@AVIFILE_CFLAGS@ \
	@MAGICK_CFLAGS@ \
	@QT_CFLAGS@ \
	@LIBXML2_CFLAGS@ \
	@DVDREAD_CFLAGS@ \
	@OGG_CFLAGS@ \
	@VORBIS_CFLAGS@ \
	@THEORA_CFLAGS@ \
	@LZO_CFLAGS@ \
	@FFMPEG_LIBS_CFLAGS@

if HAVE_QT
MODQT = import_mov.la
endif

if HAVE_LIBDVDREAD
DVD_MOD=import_dvd.la
endif

if HAVE_LIBMPEG3
MPEG3=import_mpeg3.la
endif

if LAME_3_89
MP123_MOD=import_mp3.la
MP123_LIB=@LAME_LIBS@
endif

if HAVE_A52
A52_MOD=a52_decore.la
endif

if HAVE_LZO
LZO_MOD=import_lzo.la
endif

if TRANSCODE_0_6
if HAVE_MAGICK
IM_MOD=import_im.la import_imlist.la
endif
endif

if HAVE_LIBXML2
LIBXML2_MOD=import_xml.la
endif

if HAVE_OGG
if HAVE_VORBIS
VORBISFILE=-lvorbisfile
endif
endif

if HAVE_FFMPEG
FFBIN_MOD=import_ffbin.la
endif

pkgdir=@MOD_PATH@

pkg_LTLIBRARIES = import_avi.la import_mpeg2.la import_vdrac3.la import_ac3.la \
		  import_dv.la import_vob.la import_raw.la import_nvrec.la \
		  import_yuv4mpeg.la import_lav.la import_null.la import_sh.la \
		  import_divx.la $(A52_MOD) $(MP123_MOD) $(DVD_MOD) $(MPEG3) \
		  $(IM_MOD) $(MODQT) af6_decore.la $(LIBXML2_MOD) \
		  import_mplayer.la import_rawlist.la import_ogg.la \
		  import_xvid.la import_ffmpeg.la $(LZO_MOD) import_vnc.la \
		  import_fraps.la import_pvn.la $(FFBIN_MOD)

import_null_la_SOURCES = import_null.c
import_null_la_LDFLAGS =  -module -avoid-version 

import_sh_la_SOURCES = import_sh.c
import_sh_la_LDFLAGS =  -module -avoid-version 

import_ogg_la_SOURCES = import_ogg.c
import_ogg_la_LDFLAGS =  -module -avoid-version 

import_xml_la_SOURCES = import_xml.c ioxml.c probe_xml.c ../src/zoom.c
import_xml_la_LDFLAGS =  -module -avoid-version 
import_xml_la_LIBADD = -lm @LIBXML2_LIBS@
import_xml_la_CFLAGS = $(AM_CFLAGS)   

import_yuv4mpeg_la_SOURCES = import_yuv4mpeg.c
import_yuv4mpeg_la_LDFLAGS =  -module -avoid-version 

import_mplayer_la_SOURCES = import_mplayer.c
import_mplayer_la_LDFLAGS = -module -avoid-version 

import_rawlist_la_SOURCES = import_rawlist.c ioaux.c
import_rawlist_la_LDFLAGS = -module -avoid-version -lm
import_rawlist_la_CFLAGS = $(AM_CFLAGS)

import_divx_la_SOURCES = import_divx.c
import_divx_la_LDFLAGS =  -module -avoid-version 

import_xvid_la_SOURCES = import_xvid.c
import_xvid_la_LDFLAGS =  -module -avoid-version 

import_ffmpeg_la_SOURCES = import_ffmpeg.c 
import_ffmpeg_la_LDFLAGS =  -module -avoid-version 
import_ffmpeg_la_LIBADD = @FFMPEG_LIBS_LIBS@ -lm

import_im_la_SOURCES = import_im.c
import_im_la_LDFLAGS =  -module -avoid-version 
import_im_la_LIBADD = @MAGICK_LIBS@ -lm 

import_imlist_la_SOURCES = import_imlist.c
import_imlist_la_LDFLAGS =  -module -avoid-version 
import_imlist_la_LIBADD = @MAGICK_LIBS@ -lm 

import_mov_la_SOURCES = import_mov.c
import_mov_la_LDFLAGS =  -module -avoid-version 
import_mov_la_LIBADD = @QT_LIBS@ -lm 

import_lav_la_SOURCES = import_lav.c
import_lav_la_LDFLAGS =  -module -avoid-version 

import_raw_la_SOURCES = import_raw.c
import_raw_la_LDFLAGS =  -module -avoid-version 

import_nvrec_la_SOURCES = import_nvrec.c
import_nvrec_la_LDFLAGS =  -module -avoid-version 

import_mpeg2_la_SOURCES = import_mpeg2.c
import_mpeg2_la_LDFLAGS =  -module -avoid-version 

import_mpeg3_la_SOURCES = import_mpeg3.c
import_mpeg3_la_LDFLAGS =  -module -avoid-version 
import_mpeg3_la_LIBADD  = @LIBMPEG3_LIBS@

import_fraps_la_SOURCES = import_fraps.c 
import_fraps_la_LDFLAGS =  -module -avoid-version 

import_avi_la_SOURCES = import_avi.c 
import_avi_la_LDFLAGS =  -module -avoid-version 

import_ac3_la_SOURCES = import_ac3.c ioaux.c ac3scan.c
import_ac3_la_LDFLAGS =  -module -avoid-version 
import_ac3_la_LIBADD  = @AC3_LIBS@ -lm
import_ac3_la_CFLAGS = $(AM_CFLAGS)   

import_vdrac3_la_SOURCES = import_vdrac3.c ioaux.c ac3scan.c
import_vdrac3_la_LDFLAGS =  -module -avoid-version 
import_vdrac3_la_LIBADD  = @AC3_LIBS@ -lm
import_vdrac3_la_CFLAGS = $(AM_CFLAGS)   

a52_decore_la_SOURCES = a52_decore.c ioaux.c
a52_decore_la_LDFLAGS =  -module -avoid-version 
a52_decore_la_LIBADD  = ${A52_LIBS} @XIO_LIBS@
a52_decore_la_CFLAGS = $(AM_CFLAGS)   

af6_decore_la_SOURCES = af6_decore.cpp ioaux.c
af6_decore_la_LDFLAGS =  -module -avoid-version 
af6_decore_la_LIBADD  =  @AVIFILE_LIBS@ -lstdc++ -lm
af6_decore_la_CFLAGS = $(AM_CFLAGS)   

import_mp3_la_SOURCES = import_mp3.c ioaux.c
import_mp3_la_LDFLAGS =  -module -avoid-version 
import_mp3_la_CFLAGS = $(AM_CFLAGS)   

import_lzo_la_SOURCES = import_lzo.c 
import_lzo_la_LDFLAGS =  -module -avoid-version 
import_lzo_la_LIBADD  = @LZO_LIBS@ -lm

import_vob_la_SOURCES = import_vob.c ac3scan.c clone.c ioaux.c frame_info.c ivtc.c
import_vob_la_LDFLAGS =	 -module -avoid-version 
import_vob_la_LIBADD = @AC3_LIBS@ 
import_vob_la_CFLAGS = $(AM_CFLAGS)   

import_dvd_la_SOURCES = import_dvd.c ac3scan.c dvd_reader.c clone.c ioaux.c frame_info.c ivtc.c
import_dvd_la_LDFLAGS =  -module -avoid-version 
import_dvd_la_LIBADD =  @DVDREAD_LIBS@ @AC3_LIBS@ 
import_dvd_la_CFLAGS = $(AM_CFLAGS)

import_dv_la_SOURCES = import_dv.c 
import_dv_la_LDFLAGS =  -module -avoid-version 

import_vnc_la_SOURCES = import_vnc.c 
import_vnc_la_LDFLAGS =  -module -avoid-version 

import_pvn_la_SOURCES = import_pvn.c pvn.c pvnglobals.c
import_pvn_la_LDFLAGS =  -module -avoid-version 
import_pvn_la_CFLAGS = $(AM_CFLAGS)   

import_ffbin_la_SOURCES = import_ffbin.c 
import_ffbin_la_LDFLAGS =  -module -avoid-version 

EXTRA_DIST =  \
   ac3scan.h \
   aux_pes.h \
   clone.h \
   demuxer.h \
   divx_decore.h \
   dvd_reader.h \
   frame_info.h \
   frame_video.h \
   import_def.h \
   ioaux.h \
   ioxml.h \
   ivtc.h \
   magic.h \
   mpg123.h \
   ogmstreams.h \
   packets.h \
   probe_xml.h \
   seqinfo.h \
   putvlc.h \
   getvlc.h \
   tc.h \
   pvn.h \
   pvnglobals.h

bin_PROGRAMS = tcextract tcdecode tcscan tcdemux tccat tcprobe tcxmlcheck tcxpm2rgb tcrequant

tcdecode_SOURCES = tcdecode.c      \
		   decode_ac3.c    \
		   decode_mpeg2.c  \
		   decode_yuv.c    \
		   fileinfo.c      \
		   ioaux.c         \
		   decode_dv.c     \
		   decode_mp3.c    \
		   mpg123.c        \
		   decode_a52.c    \
		   decode_af6.c    \
		   scan_dv.c       \
		   decode_lavc.c   \
		   decode_xvid.c   \
		   decode_ogg.c    \
		   decode_mov.c    \
		   decode_lzo.c

tcdecode_LDADD = \
	@FFMPEG_LIBS_LIBS@ \
	@LAME_LIBS@ \
	@AC3_LIBS@ \
	@MPEG2CVS_LIBS@ \
	-L../libvo -lvout \
	@DV_LIBS@ \
	$(VORBISFILE) \
	@THEORA_LIBS@ \
	@VORBIS_LIBS@ \
	@OGG_LIBS@ \
	@DLDARWINLIB@ \
	@QT_LIBS@ \
	@LZO_LIBS@ \
	@XIO_LIBS@ \
	-L../aclib -lac \
	-lm

tcdecode_CFLAGS = $(AM_CFLAGS)

# ----------------- #
# T C R E Q U A N T #
# ----------------- #

tcrequant_SOURCES = tcrequant.c
tcrequant_LDADD = @XIO_LIBS@ -L../aclib -lac -lm


# --------- #
# T C C A T #
# --------- #

tccat_SOURCES = tccat.c \
		dvd_reader.c \
		fileinfo.c \
		ioaux.c \
		iodump.c \
		scan_dv.c \
		extract_avi.c \
		ioxml.c \
		ts_reader.c

tccat_LDADD = @AVILIB_LIBS@ @DVDREAD_LIBS@ @DV_LIBS@ @DLDARWINLIB@ @XIO_LIBS@ @LIBXML2_LIBS@ -L../aclib -lac -lm
tccat_CFLAGS = $(AM_CFLAGS)

# ----------- #
# T C S C A N #
# ----------- #

tcscan_SOURCES = tcscan.c \
		 fileinfo.c \
		 bitrate.c \
		 ioaux.c \
		 scan_pes.c \
		 aux_pes.c \
		 mpg123.c \
		 ac3scan.c \
		 scan_dv.c

tcscan_LDADD = @LAME_LIBS@ @AC3_LIBS@ @AVILIB_LIBS@ @DV_LIBS@ @XIO_LIBS@ -L../aclib -lac -lm
tcscan_CFLAGS = $(AM_CFLAGS)

# ------------- #
# T C P R O B E #
# ------------- #

tcprobe_SOURCES = tcprobe.c \
		  probe_stream.c \
		  fileinfo.c \
		  bitrate.c \
		  ioaux.c \
		  extract_avi.c \
		  scan_pes.c \
		  aux_pes.c \
		  mpg123.c \
		  ac3scan.c \
		  dvd_reader.c \
		  probe_dvd.c \
		  decode_dv.c \
		  extract_yuv.c \
		  probe_nuv.c \
		  probe_wav.c \
		  iodump.c \
		  probe_socket.c \
		  scan_dv.c \
		  probe_im.c \
		  probe_mov.c \
		  probe_xml.c \
		  ioxml.c \
		  probe_lav.c \
		  probe_ogg.c \
		  ts_reader.c \
		  probe_vnc.c \
		  extract_mxf.c \
		  probe_v4l.c \
		  probe_pvn.c \
		  pvn.c \
		  pvnglobals.c

tcprobe_LDADD = \
	@LAME_LIBS@ \
	@AVILIB_LIBS@ \
	@AC3_LIBS@ \
	@DVDREAD_LIBS@ \
	@DV_LIBS@ \
	@MAGICK_LIBS@ \
	@QT_LIBS@ \
	@VORBIS_LIBS@ \
	@THEORA_LIBS@ \
	@OGG_LIBS@ \
	@DLDARWINLIB@ \
	@XIO_LIBS@ \
	@LIBXML2_LIBS@ \
	-L../aclib -lac \
	-lm

tcprobe_CFLAGS = $(AM_CFLAGS)

# ----------------- #
# T C E X T R A C T #
# ----------------- #

tcextract_SOURCES = tcextract.c \
		    extract_ac3.c \
		    extract_mpeg2.c \
		    extract_pcm.c \
		    fileinfo.c \
		    extract_yuv.c \
		    extract_rgb.c \
		    extract_dv.c \
		    ioaux.c \
		    extract_mp3.c \
		    scan_dv.c \
		    aux_pes.c \
		    extract_avi.c \
		    extract_ogm.c \
		    extract_lzo.c \
		    extract_mxf.c

tcextract_LDADD = \
	@AC3_LIBS@ \
	@MPEG2CVS_LIBS@ \
	-L../libvo -lvout \
	@AVILIB_LIBS@ \
	@DV_LIBS@ \
	@THEORA_LIBS@ \
	@OGG_LIBS@ \
	@VORBIS_LIBS@ \
	@XIO_LIBS@ \
	-L../aclib -lac \
	-lm

tcextract_CFLAGS = $(AM_CFLAGS)

# ------------- #
# T C D E M U X #
# ------------- #

tcdemux_SOURCES = tcdemux.c \
		  fileinfo.c \
		  ioaux.c \
		  dvd_reader.c \
		  seqinfo.c \
		  demuxer.c \
		  aux_pes.c \
		  packets.c \
		  scan_pack.c \
		  scan_dv.c \
		  demux_pass.c

tcdemux_LDADD = \
	@AC3_LIBS@ \
	@MPEG2CVS_LIBS@ \
	-L../libvo -lvout \
	@AVILIB_LIBS@ \
	@DVDREAD_LIBS@ \
	@DV_LIBS@ \
	@DLDARWINLIB@ \
	@XIO_LIBS@ \
	-L../aclib -lac \
	-lm

tcdemux_CFLAGS = $(AM_CFLAGS)

# ------------------- #
# T C X M L C H E C K #
# ------------------- # 

tcxmlcheck_SOURCES = tcxmlcheck.c \
   		     probe_xml.c \
		     ioxml.c \
		     ioaux.c \
		     fileinfo.c \
		     scan_dv.c

tcxmlcheck_LDADD =  @DV_LIBS@ @XIO_LIBS@ @LIBXML2_LIBS@ -L../aclib -lac -lm
tcxmlcheck_CFLAGS = $(AM_CFLAGS)

# ----------------- #
# T C X P M 2 R G B #
# ----------------- # 

tcxpm2rgb_SOURCES = tcxpm2rgb.c

if HAVE_LIBLVE
LVEDIR=lve
endif


if TRANSCODE_0_6
SUBDIRS = nuv af6 net v4l yuv $(LVEDIR)
else
SUBDIRS = nuv af6 $(LVEDIR)
endif


