# # Process this file with automake to produce Makefile.in.

EXTRA_DIST = \
	test.pl

AM_CPPFLAGS = \
	$(PTHREAD_CFLAGS) \
	-DMOD_PATH=\"$(MOD_PATH)\" \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/libtc \
	$(DLDARWININCL) \
	$(TCIOAUX_CFLAGS) \
	$(XIO_CFLAGS)

noinst_PROGRAMS = \
	test_bufalloc \
	test_iodir \
	test_tcmoduleinfo \
	test_tcstrdup \
	test_tclog \
	test_tcmodule

test_bufalloc_SOURCES = test-bufalloc.c
test_bufalloc_LDADD = $(LIBTC_LIBS)

test_iodir_SOURCES = test-iodir.c
test_iodir_LDADD = $(LIBTC_LIBS)

test_tcmoduleinfo_SOURCES = test-tcmoduleinfo.c
test_tcmoduleinfo_LDADD = $(LIBTC_LIBS)

test_tcstrdup_SOURCES = test-tcstrdup.c
test_tcstrdup_LDADD = $(LIBTC_LIBS)

test_tclog_SOURCES = test-tclog.c
test_tclog_LDADD = $(LIBTC_LIBS)

test_tcmodule_SOURCES = test-tcmodule.c
test_tcmodule_LDADD = $(LIBTC_LIBS)
test_tcmodule_LDFLAGS = -export-dynamic

