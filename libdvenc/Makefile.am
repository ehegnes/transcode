# Process this file with automake to produce Makefile.in.
AUTOMAKE_OPTIONS = 1.3 foreign
DEFAULT_INCLUDES = -I$(top_srcdir) -I$(srcdir) -I$(top_builddir) -I.

INCLUDES = -Wall -I./ -I../src -I$(srcdir)/../src
CFLAGS = -O3 -g -funroll-loops -ffast-math -DLINUX @CFLAGS@ @DV_CFLAGS@

noinst_LIBRARIES = libdvenc.a

DVENC1 = dvenc.c 

if HAVE_DV
DVENC2 = enc_audio_input.c enc_input.c enc_output.c enc_output2.c 
if HAVE_X86CPU
DVENC3 = rgbtoyuv.S encode_x86.S transpose_x86.S
endif
endif

libdvenc_a_SOURCES = $(DVENC1) $(DVENC2) $(DVENC3) 
noinst_HEADERS = enc_output.h enc_input.h enc_audio_input.h dvenc.h 

EXTRA_DIST = README.encoder COPYRIGHT AUTHORS \
   dvenc.c \
   dvenc.h \
   enc_audio_input.c \
   enc_audio_input.h \
   enc_audio_input2.h \
   enc_input.c \
   enc_input.h \
   enc_output.c \
   enc_output.h \
   enc_output2.c \
   enc_output2.h \
   encode_x86.S \
   headers2.h \
   rgbtoyuv.S \
   transpose_x86.S
