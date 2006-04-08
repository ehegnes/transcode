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

pkg_LTLIBRARIES = \
	encode_copy.la \
	encode_null.la 

encode_copy_la_SOURCES = encode_copy.c
encode_copy_la_LDFLAGS = -module -avoid-version

encode_null_la_SOURCES = encode_null.c
encode_null_la_LDFLAGS = -module -avoid-version

