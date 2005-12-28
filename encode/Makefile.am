# # Process this file with automake to produce Makefile.in.

AM_CPPFLAGS = \
	$(PTHREAD_CFLAGS) \
	-DMOD_PATH=\"$(MOD_PATH)\" \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/libtc \
	$(ACLIB_CFLAGS) \
	$(LIBDV_CFLAGS) \
	$(LZO_CFLAGS)

pkgdir = $(MOD_PATH)

if HAVE_LIBDV
MOD_LIBDV = encode_dv.la
endif

pkg_LTLIBRARIES = \
	encode_copy.la \
	encode_null.la \
	$(MOD_LIBDV)

encode_copy_la_SOURCES = encode_copy.c
encode_copy_la_LDFLAGS = -module -avoid-version

encode_null_la_SOURCES = encode_null.c
encode_null_la_LDFLAGS = -module -avoid-version

## MODS_LIBDV
encode_dv_la_SOURCES = encode_dv.c 
encode_dv_la_LDFLAGS = -module -avoid-version
encode_dv_la_LIBADD = $(LIBDV_LIBS)

#pkg_DATA = encode_af6.conf xvid2.cfg xvid3.cfg xvid4.cfg

#EXTRA_DIST = \
#	xvid4.cfg

