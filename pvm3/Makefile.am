## Makefile.am -- Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.3 foreign
DEFAULT_INCLUDES = -I$(top_srcdir) -I$(srcdir) -I$(top_builddir) -I.

INCLUDES = -Wall @PVM3_CFLAGS@ -I../src -I../aclib @TCIOAUX_CFLAGS@
CFLAGS = -D_REENTRANT -funroll-loops -ffast-math -Wall -DMOD_PATH=\"@MOD_PATH@\" @CFLAGS@   -g -export-dynamic

pkgdir=@MOD_PATH@

bin_PROGRAMS = tcpvmexportd
pkg_LTLIBRARIES			= pvm_functions.la

pvm_functions_la_SOURCES = pvm_functions.c 
pvm_functions_la_LDFLAGS =  -module -avoid-version 
pvm_functions_la_LIBADD  =  @PVM3_LIB@ 

tcpvmexportd_SOURCES = tcpvmexportd.c export_pvm_slave.c pvm_interface.c pvm_parser.c external_codec.c vob_pack_unpack.c ../src/tc_functions.c
tcpvmexportd_LDADD = -L../aclib -lac -lm @TCIOAUX_LIBS@

EXTRA_DIST 			= pvm_functions.h export_pvm_slave.h pvm_interface.h pvm_parser.h external_codec.h pvm_version.h vob_pack_unpack.h

install-data-local:
	@$(NORMAL_INSTALL)
	if test -h @bindir@/pvmgs; then \
		rm @bindir@/pvmgs; \
	fi;
	if test ! -e @bindir@/pvmgs; then \
		ln -s @PVM3_PVMGS@ @bindir@/pvmgs; \
	fi;
