# # Makefile.am -- Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.3 foreign

AM_CPPFLAGS = \
	-D_REENTRANT \
	-DMOD_PATH=\"@MOD_PATH@\" \
	@PVM3_CFLAGS@ \
	@ACLIB_CFLAGS@ \
	@TCIOAUX_CFLAGS@

AM_CFLAGS = \
	-g \
	-funroll-loops \
	-ffast-math

AM_LDFLAGS = -export-dynamic

pkgdir = @MOD_PATH@

bin_PROGRAMS = tcpvmexportd
pkg_LTLIBRARIES	= pvm_functions.la

pvm_functions_la_SOURCES = pvm_functions.c 
pvm_functions_la_LDFLAGS = -module -avoid-version 
pvm_functions_la_LIBADD  = @PVM3_LIB@ 

tcpvmexportd_SOURCES = \
	tcpvmexportd.c \
	export_pvm_slave.c \
	pvm_interface.c \
	pvm_parser.c \
	external_codec.c \
	vob_pack_unpack.c \
	../src/tc_functions.c

tcpvmexportd_LDADD = @TCIOAUX_LIBS@ @ACLIB_LIBS@ -lm

EXTRA_DIST = \
	pvm_functions.h \
	export_pvm_slave.h \
	pvm_interface.h \
	pvm_parser.h \
	external_codec.h \
	pvm_version.h \
	vob_pack_unpack.h

install-data-local:
	@$(NORMAL_INSTALL)
	if test -h @bindir@/pvmgs; then \
		rm @bindir@/pvmgs; \
	fi;
	if test ! -e @bindir@/pvmgs; then \
		ln -s @PVM3_PVMGS@ @bindir@/pvmgs; \
	fi;
