# # Process this file with automake to produce Makefile.in.
AUTOMAKE_OPTIONS = 1.3 foreign
DEFAULT_INCLUDES = -I$(top_srcdir) -I$(srcdir) -I$(top_builddir) -I.

CFLAGS = @CFLAGS@ -finline-functions -Wall 

noinst_LIBRARIES = libmp3.a


libmp3_a_SOURCES = VbrTag.c  amiga_mpega.c  bitstream.c  brhist.c  encoder.c  fft.c  get_audio.c  id3tag.c  lame.c  lametime.c  newmdct.c  parse.c  portableio.c  psymodel.c  quantize.c  quantize_pvt.c  reservoir.c  rtp.c  tables.c  takehiro.c  timestatus.c  util.c  vbrquantize.c  version.c  vorbis_interface.c 

noinst_HEADERS = version.h VbrTag.h util.h timestatus.h tables.h rtp.h reservoir.h quantize_pvt.h quantize.h psymodel.h portableio.h newmdct.h machine.h lametime.h lame.h l3side.h ieeefloat.h id3tag.h gtkanal.h gpkplotting.h get_audio.h fft.h encoder.h confdefs.h brhist.h bitstream.h 

EXTRA_DIST = LICENSE COPYING README API USAGE STYLEGUIDE

