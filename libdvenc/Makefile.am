# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src

AM_CPPFLAGS = \
	-DLINUX \
	@LIBDV_CFLAGS@

AM_CFLAGS = \
	-g \
	-funroll-loops \
	-ffast-math

noinst_LTLIBRARIES = libdvenc.la

DVENC1 = dvenc.c

if HAVE_LIBDV
DVENC2 = \
	enc_audio_input.c \
	enc_input.c \
	enc_output.c \
	enc_output2.c
if ARCH_X86
DVENC3 = \
	rgbtoyuv.S \
	encode_x86.S \
	transpose_x86.S
endif
endif

libdvenc_la_SOURCES = \
	$(DVENC1) \
	$(DVENC2) \
	$(DVENC3)

noinst_HEADERS = \
	enc_output.h \
	enc_input.h \
	enc_audio_input.h \
	dvenc.h

EXTRA_DIST = \
	README.encoder \
	COPYRIGHT \
	AUTHORS \
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
