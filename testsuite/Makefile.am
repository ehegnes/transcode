# # Process this file with automake to produce Makefile.in.

EXTRA_DIST = \
	test.pl

AM_CPPFLAGS = \
	$(PTHREAD_CFLAGS) \
	-DMOD_PATH=\"$(MOD_PATH)\" \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	$(LIBTC_CFLAGS) \
	$(DLDARWIN_CFLAGS) \
	$(XIO_CFLAGS)

noinst_PROGRAMS = \
	test-acmemcpy \
	test-acmemcpy-speed \
	test-bufalloc \
	test-framecode \
	test-imgconvert \
	test-iodir \
	test-ratiocodes \
	test-tclog \
	test-tcmodule \
	test-tcmoduleinfo \
	test-tcstrdup

test_acmemcpy_SOURCES = test-acmemcpy.c
test_acmemcpy_LDADD = $(ACLIB_LIBS)

test_acmemcpy_speed_SOURCES = test-acmemcpy-speed.c
test_acmemcpy_speed_LDADD = $(ACLIB_LIBS)

test_bufalloc_SOURCES = test-bufalloc.c
test_bufalloc_LDADD = $(LIBTC_LIBS)

test_framecode_SOURCES = test-framecode.c
test_framecode_LDADD = $(LIBTC_LIBS)

test_imgconvert_SOURCES = test-imgconvert.c
test_imgconvert_LDADD = $(ACLIB_LIBS)

test_iodir_SOURCES = test-iodir.c
test_iodir_LDADD = $(LIBTC_LIBS)

test_ratiocodes_SOURCES = test-ratiocodes.c
test_ratiocodes_LDADD = $(LIBTC_LIBS)

test_tclog_SOURCES = test-tclog.c
test_tclog_LDADD = $(LIBTC_LIBS)

test_tcmodule_SOURCES = test-tcmodule.c
test_tcmodule_LDADD = $(LIBTC_LIBS)
test_tcmodule_LDFLAGS = -export-dynamic

test_tcmoduleinfo_SOURCES = test-tcmoduleinfo.c
test_tcmoduleinfo_LDADD = $(LIBTC_LIBS)

test_tcstrdup_SOURCES = test-tcstrdup.c
test_tcstrdup_LDADD = $(LIBTC_LIBS)
