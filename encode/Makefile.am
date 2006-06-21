# # Process this file with automake to produce Makefile.in.

AM_CPPFLAGS = \
	$(PTHREAD_CFLAGS) \
	-DMOD_PATH=\"$(MOD_PATH)\" \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src

pkgdir = $(MOD_PATH)

if HAVE_LZO
ENCODE_LZO = encode_lzo.la
endif

if HAVE_X264
ENCODE_X264 = encode_x264.la
endif

if HAVE_XVID
ENCODE_XVID = encode_xvid.la
endif

pkg_LTLIBRARIES = \
	encode_copy.la \
	$(ENCODE_LZO) \
	encode_null.la \
	$(ENCODE_X264) \
	$(ENCODE_XVID)

encode_copy_la_SOURCES = encode_copy.c
encode_copy_la_LDFLAGS = -module -avoid-version

encode_lzo_la_SOURCES = encode_lzo.c
encode_lzo_la_CPPFLAGS = $(AM_CPPFLAGS) $(LZO_CFLAGS)
encode_lzo_la_LDFLAGS = -module -avoid-version
encode_lzo_la_LIBADD = $(LZO_LIBS)

encode_null_la_SOURCES = encode_null.c
encode_null_la_LDFLAGS = -module -avoid-version

encode_x264_la_SOURCES = encode_x264.c
encode_x264_la_CPPFLAGS = $(AM_CPPFLAGS) $(X264_CFLAGS)
encode_x264_la_LDFLAGS = -module -avoid-version
encode_x264_la_LIBADD = $(X264_LIBS)

encode_xvid_la_SOURCES = encode_xvid.c
encode_xvid_la_LDFLAGS = -module -avoid-version
encode_xvid_la_LIBADD = $(XVID_LIBS)

EXTRA_DIST = \
	x264.cfg \
	xvid.cfg
