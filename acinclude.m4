dnl AC_C_ATTRIBUTE_ALIGNED
dnl define ATTRIBUTE_ALIGNED_MAX to the maximum alignment if this is supported
AC_DEFUN([AC_C_ATTRIBUTE_ALIGNED],
    [AC_CACHE_CHECK([__attribute__ ((aligned ())) support],
	[ac_cv_c_attribute_aligned],
	[ac_cv_c_attribute_aligned=0
	for ac_cv_c_attr_align_try in 2 4 8 16 32 64; do
	    AC_TRY_COMPILE([],
		[static char c __attribute__ ((aligned($ac_cv_c_attr_align_try))) = 0; return c;],
		[ac_cv_c_attribute_aligned=$ac_cv_c_attr_align_try])
	done])
    if test x"$ac_cv_c_attribute_aligned" != x"0"; then
	AC_DEFINE_UNQUOTED([ATTRIBUTE_ALIGNED_MAX],
	    [$ac_cv_c_attribute_aligned],[maximum supported data alignment])
    fi])

dnl AC_TRY_CXXFLAGS (CXXFLAGS, [ACTION-IF-WORKS], [ACTION-IF-FAILS])
dnl check if $CXX supports a given set of cflags
AC_DEFUN([AC_TRY_CXXFLAGS],
    [AC_MSG_CHECKING([if $CXX supports $1 flags])
    SAVE_CXXFLAGS="$CXXFLAGS"
    CXXFLAGS="$1"
    AC_TRY_COMPILE([],[],[ac_cv_try_cflags_ok=yes],[ac_cv_try_cflags_ok=no])
    CXXFLAGS="$SAVE_CXXFLAGS"
    AC_MSG_RESULT([$ac_cv_try_cflags_ok])
    if test x"$ac_cv_try_cflags_ok" = x"yes"; then
	ifelse([$2],[],[:],[$2])
    else
	ifelse([$3],[],[:],[$3])
    fi])


dnl AC_TRY_CFLAGS (CFLAGS, [ACTION-IF-WORKS], [ACTION-IF-FAILS])
dnl check if $CC supports a given set of cflags
AC_DEFUN([AC_TRY_CFLAGS],
    [AC_MSG_CHECKING([if $CC supports $1 flags])
    SAVE_CFLAGS="$CFLAGS"
    CFLAGS="$1"
    AC_TRY_COMPILE([],[],[ac_cv_try_cflags_ok=yes],[ac_cv_try_cflags_ok=no])
    CFLAGS="$SAVE_CFLAGS"
    AC_MSG_RESULT([$ac_cv_try_cflags_ok])
    if test x"$ac_cv_try_cflags_ok" = x"yes"; then
	ifelse([$2],[],[:],[$2])
    else
	ifelse([$3],[],[:],[$3])
    fi])


dnl AC_CHECK_GENERATE_INTTYPES_H (INCLUDE-DIRECTORY)
dnl generate a default inttypes.h if the header file does not exist already
AC_DEFUN([AC_CHECK_GENERATE_INTTYPES],
    [AC_CHECK_HEADER([inttypes.h],[],
	[AC_COMPILE_CHECK_SIZEOF([char],[1])
	AC_COMPILE_CHECK_SIZEOF([short],[2])
	AC_COMPILE_CHECK_SIZEOF([int],[4])
	AC_COMPILE_CHECK_SIZEOF([long long],[8])
	cat >$1/inttypes.h << EOF
#ifndef _INTTYPES_H
#define _INTTYPES_H
/* default inttypes.h for people who do not have it on their system */
#if (!defined __int8_t_defined) && (!defined __BIT_TYPES_DEFINED__)
#define __int8_t_defined
typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
#ifdef ARCH_X86
typedef signed long long int64_t;
#endif
#endif
#if (!defined _LINUX_TYPES_H)
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
#ifdef ARCH_X86
typedef unsigned long long uint64_t;
#endif
#endif
#endif
EOF
	])])


dnl AC_COMPILE_CHECK_SIZEOF (TYPE SUPPOSED-SIZE)
dnl abort if the given type does not have the supposed size
AC_DEFUN([AC_COMPILE_CHECK_SIZEOF],
    [AC_MSG_CHECKING(that size of $1 is $2)
    AC_TRY_COMPILE([],[switch (0) case 0: case (sizeof ($1) == $2):;],[],
	[AC_MSG_ERROR([can not build a default inttypes.h])])
    AC_MSG_RESULT([yes])])


dnl 
dnl pvm3
dnl 

AC_DEFUN([AM_PATH_PVM3],
[
AC_ARG_WITH(pvm3, AC_HELP_STRING([--with-pvm3],[Enable pvm3 code (yes)]),[case "${withval}" in
  yes) with_pvm3=yes;;
  no)  with_pvm3=no;;
  *) AC_MSG_ERROR(bad value ${withval} for --with-pvm3) ;;
esac], with_pvm3=yes)

AC_ARG_WITH(pvm3-lib,AC_HELP_STRING([--with-pvm3-lib=PFX],[prefix where local pvm3 libraries are installed]), pvm3_lib="$withval",pvm3_lib="/usr${deflib}")
AC_ARG_WITH(pvm3-include,AC_HELP_STRING([--with-pvm3-include=PFX],[prefix where local pvm3 includes are installed]), pvm3_include="$withval",pvm3_include="/usr/include")

	have_pvm3=no
	PVM3_CFLAGS=""
	PVM3_LIB=""
	PVM3_PVMGS=""
	if test x$with_pvm3 = "x"yes ; then
		if test x$pvm3_lib != "x" ; then
			AC_CHECK_FILE($pvm3_include/pvm3.h, [pvm3_inc=yes])
			AC_CHECK_FILE($pvm3_lib/libpvm3.a, [pvm3_libs=yes])
			AC_CHECK_FILE($pvm3_lib/libgpvm3.a, [pvm3_libs=yes])
			AC_CHECK_FILE($pvm3_lib/pvmgs, [pvm3_libs=yes])
			if test x$pvm3_inc != "x" ; then
				if test x$pvm3_libs != "x" ; then
					AC_DEFINE(HAVE_PVM3)
					AC_MSG_CHECKING([for pvm3 version >= 3.4])
					PVM3_CFLAGS="-I$pvm3_include"
					PVM3_LIB="-L$pvm3_lib -lpvm3 -lgpvm3" 
					PVM3_PVMGS="$pvm3_lib/pvmgs"
					CFLAGS_OLD=$CFLAGS
					CFLAGS="$CFLAGS $PVM3_CFLAGS"
					AC_TRY_RUN([
						#include <stdio.h>
						#include <pvm3.h>
						int main () 
						{
						  if ((PVM_MAJOR_VERSION ==3)&&(PVM_MINOR_VERSION<4))
						  {
							printf("You need to upgrade pvm3 to version > 3.4\n");
							return(1);
						  }
						  if (PVM_MAJOR_VERSION <3)
						  {
							printf("You need to upgrade pvm3 to version > 3.4\n");
							return(1);
						  }
						  return 0;
						}
					],, no_pvm3=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
					CFLAGS=$CFLAGS_OLD
  					if test "x$no_pvm3" = x ; then
     						AC_MSG_RESULT(yes)
						have_pvm3=yes
					else
     						AC_MSG_RESULT(no)
						have_pvm3=no
					fi
				else
					have_pvm3=no
				fi
			else
				have_pvm3=no
			fi
		fi
	fi
AC_SUBST(PVM3_CFLAGS)
AC_SUBST(PVM3_LIB)
AC_SUBST(PVM3_PVMGS)
])

dnl AM_PATH_AVIFILE([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for AVIFILE, and define AVIFILE_CFLAGS and AVIFILE_LIBS
dnl

AC_DEFUN([AM_PATH_AVIFILE],
[
dnl 
dnl Get the cflags and libraries from the avifile-config script
dnl

AVIFILE_CFLAGS=""
AVIFILE_LIBS=""

AC_ARG_WITH(avifile-mods,AC_HELP_STRING([--with-avifile-mods], [build avifile dependent modules (yes)]), avifile_mods="$withval", avifile_mods=yes)

if test x"$avifile_mods" = xyes; then

AC_ARG_WITH(avifile-exec-prefix,AC_HELP_STRING([--with-avifile-exec-prefix=PFX],[prefix where avifile is installed]),
	  avifile_exec_prefix="$withval", avifile_exec_prefix="")

  if test x$avifile_exec_prefix != x ; then
     avifile_args="$avifile_args --exec-prefix=$avifile_exec_prefix"
     if test x${AVIFILE_CONFIG+set} != xset ; then
        AVIFILE_CONFIG=$avifile_exec_prefix/bin/avifile-config
     fi
  fi

  AC_PATH_PROG(AVIFILE_CONFIG, avifile-config, no)


  if test "$AVIFILE_CONFIG" = "no" ; then
    have_avifile=no
  else

    AC_MSG_CHECKING(for avifile - version >= "0.7.25")

    avifile_major_version=`$AVIFILE_CONFIG $avifile_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
    avifile_minor_version=`$AVIFILE_CONFIG $avifile_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
    avifile_micro_version=`$AVIFILE_CONFIG $avifile_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`
    
    if test $avifile_major_version -ge 0 && test $avifile_minor_version -ge 7 && \
       test $avifile_micro_version -ge 25 ; then
         have_avifile=yes
    else
      have_avifile=no
    fi

    AC_MSG_RESULT([$have_avifile])

    if test "$have_avifile" = "yes" ; then

    AC_DEFINE(HAVE_AVIFILE)
    have_avifile=yes
    AVIFILE_CFLAGS=`$AVIFILE_CONFIG $avifileconf_args --cflags`
    AVIFILE_LIBS=`$AVIFILE_CONFIG $avifileconf_args --libs`

    dnl check if avifile-config --cflags ends with .*/avifile
    dnl and strip it if so.

    have_avifile_includes=0
    avifile_cflags_safe="$AVIFILE_CFLAGS"

    case "$AVIFILE_CFLAGS" in
      */avifile*) 
      AVIFILE_CFLAGS=`echo $AVIFILE_CFLAGS | sed 's,/avifile\(-[[0-9]]\.[[0-9]]\)*$,,'`
      ;;
    esac

    case "$avifile_cflags_safe" in
      */avifile-0.7*)
      have_avifile_includes=7
      ;;
    esac

    AC_DEFINE_UNQUOTED(HAVE_AVIFILE_INCLUDES, $have_avifile_includes, [Define this if your avifile suffixes a version number])
    fi
  fi	

else
have_avifile=no

fi dnl build modules

  AC_SUBST(AVIFILE_CFLAGS)
  AC_SUBST(AVIFILE_LIBS)

])



dnl AM_PATH_LAME([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for LAME, and define LAME_CFLAGS and LAME_LIBS
dnl

AC_DEFUN([AM_PATH_LAME],
[

AC_ARG_WITH(lame, AC_HELP_STRING([--with-lame],[use installed lame library (yes)]),[case "${withval}" in
  yes) ;;
  no)  ;;
  *) AC_MSG_ERROR(bad value ${withval} for --with-lame) ;;
esac], with_lame=yes)

AC_ARG_WITH(lame-includes,AC_HELP_STRING([--with-lame-includes=PFX],[prefix where local lame includes are installed (optional)]),
	  lame_includes="$withval",lame_includes="")

AC_ARG_WITH(lame-libs,AC_HELP_STRING([--with-lame-libs=PFX],[prefix where local lame libs are installed (optional)]),
	  lame_libs="$withval", lame_libs="")

LAME_LIBS=""
LAME_CFLAGS=""

lame89=no
have_lame=no
lame_version=1

if test x$with_lame = "x"yes ; then

	if test x$lame_includes != "x" ; then
	    with_lame_i="$lame_includes/include"
        else
	    with_lame_i="/usr/include"
        fi

        if test x$lame_libs != x ; then
	    with_lame_l="$lame_libs/lib"	
        else
	    with_lame_l="/usr${deflib}"
        fi
	
AC_CHECK_FILE($with_lame_i/lame/lame.h, [AC_DEFINE(HAVE_LAME_INC, 1, [Have Lame includes in separate path]) lame_inc=yes])
if test x"$lame_inc" != xyes; then 
AC_CHECK_FILE(/usr/local/include/lame/lame.h, [AC_DEFINE([HAVE_LAME_INC], [Have Lame includes in separate path]) lame_inc=yes])
fi
if test x"$lame_inc" != xyes; then 
AC_CHECK_FILE(/sw/include/lame/lame.h, [AC_DEFINE([HAVE_LAME_INC], [Have Lame includes in separate path]) lame_inc=yes])
fi
	AC_MSG_CHECKING([lame version])
	ac_save_CFLAGS="$CFLAGS"
	ac_save_LIBS="$LIBS"
	CFLAGS="$CFLAGS -I$with_lame_i"
	if test x"$is_osx" = x"true"; then
	  LIBS="-L$with_lame_l -lmp3lame -lm $LIBS"
	else 
	  LIBS="-L$with_lame_l -lmp3lame -lm $LIBS -Wl,-rpath -Wl,$with_lame_l"
	fi
	AC_TRY_RUN([
#include <stdio.h>

#ifdef HAVE_LAME_INC
#include <lame/lame.h>
#else
#include <lame.h>
#endif


int main () {
  lame_version_t lv;
  get_lame_version_numerical(&lv);
  if(lv.alpha || lv.beta) lv.minor--;
  printf("%d%d\n", lv.major, lv.minor);
  return 0;
}
],lame_version="`./conftest$ac_exeext`",lame_version=1,)
	CFLAGS="$ac_save_CFLAGS"
	LIBS="$ac_save_LIBS"

	dnl define HAVE_LAME to version number
	AC_CHECK_LIB(mp3lame, lame_init,
       	[LAME_CFLAGS="-I$with_lame_i -I/usr/local/include" 
         LAME_LIBS="-L$with_lame_l -lmp3lame -lm"
       	AC_DEFINE_UNQUOTED([HAVE_LAME], $lame_version, [Have the lame lib]) 
	AC_DEFINE_UNQUOTED([LAME_3_89], 1, [Have Lame-3.89 or newer])	
	hav_lame=yes
	lame89=yes
	have_lame=yes], [have_lame=no lame89=no], 
       	-L$with_lame_l -lmp3lame -lm)

fi   


if test x"$have_lame" != "xyes"; then
	
	dnl use included lib

	with_lame_i="../libmp3lame"		
	with_lame_l="../libmp3lame"	
     
	AC_CHECK_FILE(./libmp3lame/lame.h, 
	[AC_DEFINE(HAVE_LAME)
        have_lame=yes
        LAME_CFLAGS="-I$with_lame_i" 
        LAME_LIBS="-L$with_lame_l -lmp3"], have_lame=no)
fi

AC_SUBST(LAME_3_89)
AC_SUBST(LAME_CFLAGS)
AC_SUBST(LAME_LIBS)
])

dnl AM_PATH_OGG([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for libogg, and define OGG_CFLAGS and OGG_LIBS
dnl
AC_DEFUN([AM_PATH_OGG],
[dnl 
dnl Get the cflags and libraries
dnl
AC_ARG_WITH(ogg, AC_HELP_STRING([--with-ogg],[Compile in libogg support]),[case "${withval}" in
  yes) ;;
  no)  ;;
  *) AC_MSG_ERROR(bad value ${withval} for --with-ogg) ;;
esac], with_ogg=yes)

AC_ARG_WITH(ogg-includes,AC_HELP_STRING([--with-ogg-includes=PFX],[prefix where local ogg includes are installed (optional)]),
	  ogg_includes="$withval",ogg_includes="")

AC_ARG_WITH(ogg-libs,AC_HELP_STRING([--with-ogg-libs=PFX],[prefix where local ogg libs are installed (optional)]),
	  ogg_libs="$withval",ogg_libs="")

OGG_LIBS=""
OGG_CFLAGS=""

have_ogg=no

if test x$with_ogg = "x"yes ; then
	if test x$ogg_includes != "x" ; then
	    with_ogg_i="$ogg_includes/include"
        else
	    with_ogg_i="/usr/include"
        fi

        if test x$ogg_libs != x ; then
	    with_ogg_l="$ogg_libs/lib"	
        else
	    with_ogg_l="/usr${deflib}"
        fi

	AC_CHECK_LIB(ogg, ogg_sync_init,
       	[OGG_CFLAGS="-I$with_ogg_i -I/usr/local/include" 
         OGG_LIBS="-L$with_ogg_l -logg -lm"
       	AC_DEFINE(HAVE_OGG) 
	hav_ogg=yes
	have_ogg=yes], [have_ogg=no], 
       	-L$with_ogg_l -logg)
fi   
AC_CHECK_FILE($with_ogg_i/ogg/ogg.h, [ogg_inc=yes])
if test x"$ogg_inc" != xyes; then 
AC_CHECK_FILE(/usr/local/include/ogg/ogg.h, [ogg_inc=yes])
fi
if test x"$ogg_inc" != xyes; then 
AC_CHECK_FILE(/sw/include/ogg/ogg.h, [ogg_inc=yes])
fi
AC_SUBST(OGG_CFLAGS)
AC_SUBST(OGG_LIBS)
])

dnl AM_PATH_VORBIS([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for libvorbis, and define VORBIS_CFLAGS and VORBIS_LIBS
dnl
AC_DEFUN([AM_PATH_VORBIS],
[dnl 
dnl Get the cflags and libraries
dnl
AC_ARG_WITH(vorbis,AC_HELP_STRING([--with-vorbis],[Compile in libvorbis support]),[case "${withval}" in
  yes) ;;
  no)  ;;
  *) AC_MSG_ERROR(bad value ${withval} for --with-vorbis) ;;
esac], with_vorbis=yes)

AC_ARG_WITH(vorbis-includes,AC_HELP_STRING([--with-vorbis-includes=PFX],[prefix where local vorbis includes are installed (optional)]),
	  vorbis_includes="$withval",vorbis_includes="")

AC_ARG_WITH(vorbis-libs,AC_HELP_STRING([--with-vorbis-libs=PFX],[prefix where local vorbis libs are installed (optional)]),
	  vorbis_libs="$withval",vorbis_libs="")

VORBIS_LIBS=""
VORBIS_CFLAGS=""

have_vorbis=no

if test x$with_vorbis = "x"yes ; then
	if test x$vorbis_includes != "x" ; then
	    with_vorbis_i="$vorbis_includes/include"
        else
	    with_vorbis_i="/usr/include"
        fi

        if test x$vorbis_libs != x ; then
	    with_vorbis_l="$vorbis_libs/lib"	
        else
	    with_vorbis_l="/usr${deflib}"
        fi

	AC_CHECK_LIB(vorbis, vorbis_info_init,
       	[VORBIS_CFLAGS="-I$with_vorbis_i -I/usr/local/include" 
         VORBIS_LIBS="-L$with_vorbis_l -lvorbis -lm"
       	AC_DEFINE(HAVE_VORBIS) 
	hav_vorbis=yes
	have_vorbis=yes], [have_vorbis=no], 
       	-L$with_vorbis_l -lvorbis)
fi   
AC_CHECK_FILE($with_vorbis_i/vorbis/codec.h, [vorbis_inc=yes])
if test x"$vorbis_inc" != xyes; then 
AC_CHECK_FILE(/usr/local/include/vorbis/codec.h, [vorbis_inc=yes])
fi
if test x"$vorbis_inc" != xyes; then 
AC_CHECK_FILE(/sw/include/vorbis/codec.h, [vorbis_inc=yes])
fi
AC_SUBST(VORBIS_CFLAGS)
AC_SUBST(VORBIS_LIBS)
])

dnl AM_PATH_THEORA([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for libtheora, and define THEORA_CFLAGS and THEORA_LIBS
dnl
AC_DEFUN([AM_PATH_THEORA],
[dnl 
dnl Get the cflags and libraries
dnl
AC_ARG_WITH(theora,AC_HELP_STRING([--with-theora],[Compile in libtheora support]),[case "${withval}" in
  yes) ;;
  no)  ;;
  *) AC_MSG_ERROR(bad value ${withval} for --with-theora) ;;
esac], with_theora=yes)

AC_ARG_WITH(theora-includes,AC_HELP_STRING([--with-theora-includes=PFX],[prefix where local theora includes are installed (optional)]),
	  theora_includes="$withval",theora_includes="")

AC_ARG_WITH(theora-libs,AC_HELP_STRING([--with-theora-libs=PFX],[prefix where local theora libs are installed (optional)]),
	  theora_libs="$withval",theora_libs="")

THEORA_LIBS=""
THEORA_CFLAGS=""

have_theora=no

if test x$with_theora = "x"yes ; then
	if test x$theora_includes != "x" ; then
	    with_theora_i="$theora_includes/include"
        else
	    with_theora_i="/usr/include"
        fi

        if test x$theora_libs != x ; then
	    with_theora_l="$theora_libs/lib"	
        else
	    with_theora_l="/usr${deflib}"
        fi

	AC_CHECK_LIB(theora, theora_info_init,
       	[THEORA_CFLAGS="-I$with_theora_i -I/usr/local/include" 
         THEORA_LIBS="-L$with_theora_l -ltheora -logg -lm"
       	AC_DEFINE(HAVE_THEORA) 
	hav_theora=yes
	have_theora=yes], [have_theora=no], 
       	-L$with_theora_l -ltheora -logg -lm)
fi   
AC_CHECK_FILE($with_theora_i/theora/theora.h, [theora_inc=yes])
if test x"$theora_inc" != xyes; then 
AC_CHECK_FILE(/usr/local/include/theora/theora.h, [theora_inc=yes])
fi
if test x"$theora_inc" != xyes; then 
AC_CHECK_FILE(/sw/include/theora/codec.h, [theora_inc=yes])
fi
AC_SUBST(THEORA_CFLAGS)
AC_SUBST(THEORA_LIBS)
])

dnl AM_PATH_LIBDVDREAD([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for LIBDVDREAD, and define LIBDVDREAD_CFLAGS and LIBDVDREAD_LIBS
dnl

AC_DEFUN([AM_PATH_LIBDVDREAD],
[

AC_ARG_WITH(dvdread, AC_HELP_STRING([--with-dvdread],[use installed libdvdread library (yes)]),[case "${withval}" in
  yes) with_dvdread=yes;;
  no) with_dvdread=no ;;
  *) AC_MSG_ERROR(bad value ${withval} for --with-dvdread) ;;
esac], with_dvdread=yes)

AC_ARG_WITH(dvdread-includes,AC_HELP_STRING([--with-dvdread-includes=PFX],[prefix where local dvdread includes are installed (optional)]),
	  dvdread_includes="$withval",dvdread_includes="")

AC_ARG_WITH(dvdread-libs,AC_HELP_STRING([--with-dvdread-libs=PFX],[prefix where local dvdread lib is installed (optional)]),
	  dvdread_libs="$withval", dvdread_libs="")

DVDREAD_LIBS=""
DVDREAD_CFLAGS=""

have_dvdread=no

if test x$with_dvdread = "x"yes ; then

	if test x$dvdread_includes != "x" ; then
	    with_dvdread_i="$dvdread_includes/include"
        else
	    with_dvdread_i="/usr/include"
        fi

        if test x$dvdread_libs != x ; then
	    with_dvdread_l="$dvdread_libs/lib"	
        else
	    with_dvdread_l="/usr${deflib}"
        fi
	
	AC_CHECK_LIB(dvdread, DVDOpen,
       	[DVDREAD_CFLAGS="-I$with_dvdread_i -I/usr/local/include" 
         DVDREAD_LIBS="-L$with_dvdread_l -ldvdread -lm"
       	AC_DEFINE(HAVE_LIBDVDREAD) 
	have_dvdread=yes], have_dvdread=no, 
       	-L$with_dvdread_l -ldvdread -lm)

AC_CHECK_FILE($with_dvdread_i/dvdread/dvd_reader.h, [AC_DEFINE([HAVE_LIBDVDREAD_INC], 1, [Have Libdvdread includes in separate path]) dvdread_inc=yes])
if test x"$dvdread_inc" != xyes; then 
AC_CHECK_FILE(/usr/local/include/dvdread/dvd_reader.h, [AC_DEFINE([HAVE_LIBDVDREAD_INC], 1, [Have Libdvdread includes in separate path]) dvdread_inc=yes])
fi
if test x"$dvdread_inc" != xyes; then 
AC_CHECK_FILE(/sw/include/dvdread/dvd_reader.h, [AC_DEFINE([HAVE_LIBDVDREAD_INC], 1, [Have Libdvdread includes in separate path]) dvdread_inc=yes])
fi

fi

AC_SUBST(DVDREAD_CFLAGS)
AC_SUBST(DVDREAD_LIBS)
])


dnl AM_PATH_LIBXVID([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for LIBXVID, and define LIBXVID_CFLAGS and LIBXVID_LIBS
dnl

AC_DEFUN([AM_PATH_LIBXVID],
[

AC_ARG_WITH(xvidcore, AC_HELP_STRING([--with-xvidcore],[use installed LIBXVID library (yes)]),[case "${withval}" in
  yes) with_xvidcore=yes;;
  no) with_xvidcore=no ;;
  *) AC_MSG_ERROR(bad value ${withval} for --with-xvidcore) ;;
esac], with_xvidcore=yes)

AC_ARG_WITH(xvidcore-includes,AC_HELP_STRING([--with-xvidcore-includes=PFX],[prefix where local xvidcore includes are installed (optional)]),
	  xvidcore_includes="$withval",xvidcore_includes="")

AC_ARG_WITH(xvidcore-libs,AC_HELP_STRING([--with-xvidcore-libs=PFX],[prefix where local xvidcore lib is installed (optional)]),
	  xvidcore_libs="$withval", xvidcore_libs="")

XVIDCORE_LIBS=""
XVIDCORE_CFLAGS=""

have_xvidcore=no

if test x$with_xvidcore = "x"yes ; then

	if test x$xvidcore_includes != "x" ; then
	    with_xvidcore_i="$xvidcore_includes/include"
        else
	    with_xvidcore_i="/usr/include"
        fi

        if test x$xvidcore_libs != x ; then
	    with_xvidcore_l="$xvidcore_libs/lib"	
        else
	    with_xvidcore_l="/usr${deflib}"
        fi
	
	AC_CHECK_LIB(xvidcore, xvid_encore,
       	[XVIDCORE_CFLAGS="-I$with_xvidcore_i -I/usr/local/include" 
         XVIDCORE_LIBS="-L$with_xvidcore_l -L/usr/local${deflib} -lxvidcore -lm"
       	AC_DEFINE(HAVE_LIBXVID) 
	have_xvidcore=yes], have_xvidcore=no, 
       	-L$with_xvidcore_l -L/usr/local${deflib} -lxvidcore -lm)
fi

AC_SUBST(XVIDCORE_CFLAGS)
AC_SUBST(XVIDCORE_LIBS)
])



dnl 
dnl libdv
dnl 
dnl 

AC_DEFUN([AM_PATH_DV],
[

AC_ARG_WITH(dv,
  AC_HELP_STRING([--with-dv],[build libdv dependent modules (yes)]),
  [case "${withval}" in
    yes) ;;
    no)  ;;
    *) AC_MSG_ERROR(bad value ${withval} for --with-dv) ;;
  esac], with_dv=yes)

AC_ARG_WITH(dv-includes,
  AC_HELP_STRING([--with-dv-includes=PFX],
    [prefix where local libdv includes are installed (optional)]),
  dv_includes="$withval", dv_includes="")

AC_ARG_WITH(dv-libs,
  AC_HELP_STRING([--with-dv-libs=PFX],
    [prefix where local libdv libs are installed (optional)]),
  dv_libs="$withval", dv_libs="")

DV_EXTRA_LIBS="$GLIB_LIBS -lm"

if test x$with_dv = "x"yes ; then

  if test -z "$PKG_CONFIG"; then
    AC_PATH_PROG(PKG_CONFIG, pkg-config, no)
  fi
  if test "$PKG_CONFIG" = "no" ; then
     echo "*** The pkg-config script could not be found. Make sure it is"
     echo "*** in your path, or set the PKG_CONFIG environment variable"
     echo "*** to the full path to pkg-config."
     echo "*** Or see http://www.freedesktop.org/software/pkgconfig to get pkg-config."
  fi

	if test x$dv_includes != "x" ; then
	    with_dv_i="-I$dv_includes/include"
        else
	    with_dv_i="`$PKG_CONFIG --cflags libdv`"
        fi

        if test x$dv_libs != x ; then
            with_dv_l="-L$dv_libs/lib -ldv $DV_EXTRA_LIBS"
        else
            with_dv_l="`$PKG_CONFIG --libs libdv`"
        fi

	AC_CHECK_LIB(dv, dv_init,
	  [DV_CFLAGS="$with_dv_i" DV_LIBS="$with_dv_l"
	    AC_DEFINE([HAVE_DV], 1, [Have libdv]),
	    have_dv=yes],
	  have_dv=no,
	  $with_dv_l)

	dnl check for version >= 0.95
	if $PKG_CONFIG libdv --atleast-version 0.95 ; then
           AC_DEFINE([LIBDV_095], 1, [Have libdv 0.95 or newer])
	fi

	dnl check for version >= 0.99
	if $PKG_CONFIG libdv --atleast-version 0.99 ; then
           AC_DEFINE([LIBDV_095], 1, [Have libdv 0.99 or newer])
	fi

	dnl check for version >= 0.103
	if $PKG_CONFIG libdv --atleast-version 0.103 ; then
           AC_DEFINE([LIBDV_0103], 1, [Have libdv 0.103 or newer])
	fi
else
    have_dv=no
fi

AC_SUBST(DV_LIBS)
AC_SUBST(DV_EXTRA_LIBS)
AC_SUBST(DV_CFLAGS)
])


dnl 
dnl FFmpeg libs support
dnl 

have_ffmpeg_libs=no
AC_DEFUN([AM_PATH_FFMPEG_LIBS],
[
with_ffmpeg_libs=yes

AC_ARG_WITH(ffmpeg-libs-includes,AC_HELP_STRING([--with-ffmpeg-libs-includes=PFX],[prefix where ffmpeg libs includes are installed (optional)]),
	  ffmpeg_libs_includes="$withval",ffmpeg_libs_includes="")

AC_ARG_WITH(ffmpeg-libs-libs,AC_HELP_STRING([--with-ffmpeg-libs-libs=PFX],[prefix where ffmpeg libs libraries files are installed (optional)]),
	  ffmpeg_libs_libs="$withval", ffmpeg_libs_libs="")

if test x$ffmpeg_libs_includes != "x" ; then
	with_ffmpeg_libs_i="$ffmpeg_libs_includes/include/ffmpeg"
else
	with_ffmpeg_libs_i="/usr/include/ffmpeg"
fi

if test x$ffmpeg_libs_libs != x ; then
	with_ffmpeg_libs_l="$ffmpeg_libs_libs/lib"
else
	with_ffmpeg_libs_l="/usr${deflib}"
fi

AC_CHECK_FILE($with_ffmpeg_libs_i/avcodec.h,
		[if test `sed -ne 's,#define[[:space:]]*LIBAVCODEC_BUILD[[:space:]]*\(.*\),\1,p' $with_ffmpeg_libs_i/avcodec.h` -lt 4718
		then
			echo "*** Transcode needs at least ffmpeg build 4718 (0.4.9-pre1 or a cvs version after 20040703) ***"
			exit 1
		fi],
		[echo "*** Cannot find header file $with_ffmpeg_libs_i/avcodec.h from ffmpeg ***"
		exit 1])

FFMPEG_LIBS_BUILD=$(sed -ne 's,#define[[:space:]+]LIBAVCODEC_BUILD[[:space:]]*\(.*\),\1,p' $with_ffmpeg_libs_i/avcodec.h)
FFMPEG_LIBS_VERSION=$(sed -ne 's,#define[[:space:]+]FFMPEG_VERSION.*"\(.*\)",\1,p' $with_ffmpeg_libs_i/avcodec.h)
  
AC_SUBST(FFMPEG_LIBS_BUILD)
AC_SUBST(FFMPEG_LIBS_VERSION)

FFMPEG_LIBS_CFLAGS="-I$with_ffmpeg_libs_i"
FFMPEG_LIBS_EXTRALIBS="-lm -lz $pthread_lib"

AC_ARG_ENABLE(ffmpeg-libs-static,
		AC_HELP_STRING([--enable-ffmpeg-libs-static],[link binaries and modules statically to ffmpeg-libs (huge)]),
		[
			if test x"$enableval" = x'yes'
			then
				FFMPEG_LIBS_LD="static"
			else
				FFMPEG_LIBS_LD="shared"
			fi
		],
		[
			FFMPEG_LIBS_LD="shared"
		])

if test x"$FFMPEG_LIBS_LD" = x"shared"
then
	FFMPEG_LIBS_LIBS="-L$with_ffmpeg_libs_l -lavcodec $FFMPEG_LIBS_EXTRALIBS"
else
	FFMPEG_LIBS_LIBS="$with_ffmpeg_libs_l/libavcodec.a $FFMPEG_LIBS_EXTRALIBS"
fi

AC_CHECK_LIB(avcodec,
		avcodec_thread_init,
		[HAVE_FFMPEG_LIBS_LIBS=1],
		[echo "*** Transcode depends on the FFmpeg (libavcodec) libraries and headers ***"
		exit 1],
		[$FFMPEG_LIBS_EXTRALIBS])

AC_CHECK_LIB(z,
		gzopen,
		[],
		[echo "*** Transcode depends on libz libraries and headers ***"
		 exit 1
		]
		[])

AC_CHECK_LIB(m,
		_LIB_VERSION,
		[],
		[echo "*** Transcode depends on libm (>= 2.0) libraries and headers ***"
		 exit 1
		]
		[])

AC_SUBST(FFMPEG_LIBS_CFLAGS)
AC_SUBST(FFMPEG_LIBS_LIBS)
AC_SUBST(FFMPEG_LIBS_EXTRALIBS)
AC_SUBST(FFMPEG_LIBS_LD)
])

AC_DEFUN([AM_PATH_LZO],
[

AC_ARG_WITH(lzo, AC_HELP_STRING([--with-lzo],[build liblzo dependent modules (yes)]),[case "${withval}" in
  yes) ;;
  no)  ;;
  *) AC_MSG_ERROR(bad value ${withval} for --with-lzo) ;;
esac], with_lzo=yes)

AC_ARG_WITH(lzo-includes,AC_HELP_STRING([--with-lzo-includes=PFX],[prefix where local liblzo includes are installed (optional)]),
	  lzo_includes="$withval",lzo_includes="")

AC_ARG_WITH(lzo-libs,AC_HELP_STRING([--with-lzo-libs=PFX],[prefix where local liblzo libs are installed (optional)]),
	  lzo_libs="$withval", lzo_libs="")


EXTRA_LIBS="$LIBS $GLIB_LIBS -lm"
LZO_EXTRA_LIBS="$GLIB_LIBS -lm"

if test x$with_lzo = "x"yes ; then

	if test x$lzo_includes != "x" ; then
	    with_lzo_i="$lzo_includes/include"
        else
	    with_lzo_i="/usr/include"
        fi

        if test x$lzo_libs != x ; then
            with_lzo_l="$lzo_libs/lib"
        else
            with_lzo_l="/usr${deflib}"
        fi

	AC_CHECK_LIB(lzo, lzo_version,
      [LZO_CFLAGS="-I$with_lzo_i ${GLIB_CFLAGS} -I/usr/local/include"	
       LZO_LIBS="-L$with_lzo_l -llzo ${EXTRA_LIBS}"
       AC_DEFINE(HAVE_LZO) have_lzo=yes],have_lzo=no, 
	-L$with_lzo_l -llzo ${EXTRA_LIBS})

else
    have_lzo=no
fi

AC_SUBST(LZO_LIBS)
AC_SUBST(LZO_EXTRA_LIBS)
AC_SUBST(LZO_CFLAGS)
])





dnl 
dnl liba52
dnl 
dnl 

AC_DEFUN([AM_PATH_A52],
[

AC_ARG_WITH(a52, AC_HELP_STRING([--with-a52],[build liba52 decoder module (yes)]),[case "${withval}" in
  yes) ;;
  no)  ;;
  *) AC_MSG_ERROR(bad value ${withval} for --with-a52) ;;
esac], with_a52=yes)

AC_ARG_WITH(a52-includes,AC_HELP_STRING([--with-a52-includes=PFX],[prefix where local liba52 includes are installed (optional)]),
	  a52_includes="$withval",a52_includes="")

AC_ARG_WITH(a52-libs,AC_HELP_STRING([--with-a52-libs=PFX],[prefix where local liba52 libs are installed (optional)]),
	  a52_libs="$withval", a52_libs="")


EXTRA_LIBS="-lm"

if test x$with_a52 = "x"yes ; then

	if test x$a52_includes != "x" ; then
	    with_a52_i="$a52_includes/include"
        else
	    with_a52_i="/usr/include"
        fi

        if test x$a52_libs != x ; then
            with_a52_l="$a52_libs/lib"
        else
            with_a52_l="/usr${deflib}"
        fi

     AC_CHECK_LIB(a52, a52_init,
      [A52_CFLAGS="-I$with_a52_i -I/usr/local/include"
       A52_LIBS="-L$with_a52_l -la52 ${EXTRA_LIBS}"
       AC_DEFINE(HAVE_A52) have_a52=yes],have_a52=no, 
	-L$with_a52_l -la52 ${EXTRA_LIBS})

else
    have_a52=no
fi

AC_SUBST(A52_LIBS)
AC_SUBST(A52_CFLAGS)
])



dnl 
dnl libmpeg3
dnl 
dnl 

AC_DEFUN([AM_PATH_LIBMPEG3],
[

AC_ARG_WITH(libmpeg3, AC_HELP_STRING([--with-libmpeg3],[build libmpeg3 dependent module (yes)]),[case "${withval}" in
  yes) ;;
  no)  ;;
  *) AC_MSG_ERROR(bad value ${withval} for --with-libmpeg3) ;;
esac], with_libmpeg3=yes)

AC_ARG_WITH(libmpeg3-includes,AC_HELP_STRING([--with-libmpeg3-includes=PFX],[prefix where local libmpeg3 includes are installed (optional)]),
	  libmpeg3_includes="$withval",libmpeg3_includes="")

AC_ARG_WITH(libmpeg3-libs,AC_HELP_STRING([--with-libmpeg3-libs=PFX],[prefix where local libmpeg3 libs are installed (optional)]),
	  libmpeg3_libs="$withval", libmpeg3_libs="")

EXTRA_LIBS="-lm"

if test x$with_libmpeg3 = "x"yes ; then

	dnl check for libmpeg3.h
	if test x$libmpeg3_includes != "x" ; then
        	AC_CHECK_FILE($libmpeg3_includes/include/libmpeg3/libmpeg3.h, [libmpeg3_inc=yes],[libmpeg3_inc=no])
        	if test x$libmpeg3_inc = "x"yes; then
                	with_libmpeg3_i="$libmpeg3_includes/include/libmpeg3"
		fi        
		AC_CHECK_FILE($libmpeg3_includes/include/mpeg3/libmpeg3.h, [libmpeg3_inc=yes],[libmpeg3_inc=no])
        	if test x$libmpeg3_inc = "x"yes; then        
        		with_libmpeg3_i="$libmpeg3_includes/include/mpeg3"
        	fi
	else
		AC_CHECK_FILE(/usr/include/libmpeg3/libmpeg3.h, [libmpeg3_inc=yes],[libmpeg3_inc=no])
		if test x$libmpeg3_inc = "x"yes; then
			with_libmpeg3_i="/usr/include/libmpeg3"
		fi
        	AC_CHECK_FILE(/usr/include/mpeg3/libmpeg3.h, [libmpeg3_inc=yes],[libmpeg3_inc=no])
		if test x$libmpeg3_inc = "x"yes; then
	        	with_libmpeg3_i="/usr/include/mpeg3"
		fi

		AC_CHECK_FILE(/usr/local/include/libmpeg3/libmpeg3.h, [libmpeg3_inc=yes],[libmpeg3_inc=no])
		if test x$libmpeg3_inc = "x"yes; then
        		with_libmpeg3_i="/usr/local/include/libmpeg3"
		fi
        	AC_CHECK_FILE(/usr/local/include/mpeg3/libmpeg3.h,[libmpeg3_inc=yes],[libmpeg3_inc=no])
		if test x$libmpeg3_inc = "x"yes; then
        		with_libmpeg3_i="/usr/local/include/mpeg3"
		fi   
	fi
	if test x$with_libmpeg3_i = x ; then
		echo "warning: never found libmpeg3.h"
		with_libmpeg3_i = "/usr/include"
	fi

	if test x$libmpeg3_libs != x ; then
            with_libmpeg3_l="$libmpeg3_libs/lib"
        else
            with_libmpeg3_l="/usr${deflib}"
        fi

     AC_CHECK_LIB(mpeg3, mpeg3_open,
      [
	LIBMPEG3_CFLAGS="-I$with_libmpeg3_i"
	LIBMPEG3_LIBS="-L$with_libmpeg3_l -lmpeg3 ${EXTRA_LIBS}" 
	AC_DEFINE(HAVE_LIBMPEG3) 
	have_libmpeg3=yes
      ], have_libmpeg3=no, -L$with_libmpeg3_l -lmpeg3 ${EXTRA_LIBS})


else
    have_libmpeg3=no
fi


dnl give user a hint :)
if test x$with_libmpeg3_i = "x"; then 
if test $have_libmpeg3="yes"; then
        echo "*** Found libmpeg3 but no mpeg3 includes, if they are installed somewhere"
        echo "*** nonstandart, try '--with-libmpeg3-includes=<prefix-of-your-libmpeg3-installation>'" 
fi
        have_libmpeg3=no
fi

AC_SUBST(LIBMPEG3_LIBS)
AC_SUBST(LIBMPEG3_CFLAGS)
])



dnl 
dnl libpostproc
dnl 
dnl 

AC_DEFUN([AM_PATH_POSTPROC],
[

AC_ARG_WITH(libpostproc-builddir,AC_HELP_STRING([--with-libpostproc-builddir=PFX],[path to MPlayer builddir  (optional)]),
	  libpostproc_builddir="$withval",libpostproc_builddir="")

EXTRA_LIBS="-lm"

have_libpostproc=no

if test x$libpostproc_builddir != "x" ; then

	with_libpostproc_p="$libpostproc_builddir"

	AC_CHECK_LIB(postproc, pp_postprocess,
      [
       LIBPOSTPROC_CFLAGS="-I$with_libpostproc_p/include"
       LIBPOSTPROC_LIBS="-L$with_libpostproc_p/lib -lpostproc ${EXTRA_LIBS}"

	AC_DEFINE(HAVE_LIBPOSTPROC)
	have_libpostproc=yes
      ], have_libpostproc=no, -L$with_libpostproc_p/lib -lpostproc ${EXTRA_LIBS})
fi

if test x$have_libpostproc != "xyes" ; then

	AC_CHECK_LIB(postproc, pp_postprocess,
      [
	LIBPOSTPROC_CFLAGS=""
	LIBPOSTPROC_LIBS="-lpostproc ${EXTRA_LIBS}" 
	AC_DEFINE(HAVE_LIBPOSTPROC)
	 have_libpostproc=yes
      ], have_libpostproc=no, )
fi

if test x$have_libpostproc != "xyes" ; then

	AC_CHECK_LIB(postproc, pp_postprocess,
      [
	LIBPOSTPROC_CFLAGS="-I/usr/local/include"
	LIBPOSTPROC_LIBS="-L/usr/local/lib -lpostproc ${EXTRA_LIBS}" 
	AC_DEFINE(HAVE_LIBPOSTPROC)
	have_libpostproc=yes
      ], have_libpostproc=no, )
fi


AC_SUBST(LIBPOSTPROC_LIBS)
AC_SUBST(LIBPOSTPROC_CFLAGS)
])


dnl 
dnl liblve
dnl 
dnl 

AC_DEFUN([AM_PATH_LVE],
[

AC_ARG_WITH(liblve-builddir,AC_HELP_STRING([--with-liblve-builddir=PFX],[path to lve builddir (optional)]),
	  liblve_builddir="$withval",liblve_builddir="")

EXTRA_LIBS="-lm"

have_liblve=no

if test x$liblve_builddir != "x" ; then

	with_liblve_p="$liblve_builddir"

	AC_CHECK_LIB(lve, lr_init,
      [
	LIBLVE_CFLAGS="-I$with_liblve_p/lve"
	LIBLVE_LIBS="-L$with_liblve_p/lve -llve ${EXTRA_LIBS}" 
	AC_DEFINE(HAVE_LIBLVE)
	have_liblve=yes
      ], have_liblve=no, 
      -L$with_liblve_p/lve -llve -L$with_liblve_p/ffmpeg -lavcodec -L$with_liblve_p/libmpeg2/.libs -lmpeg2 -L$with_liblve_p/liba52 -la52 -lstdc++ -lm)
fi

if test x$have_liblve != "xyes" ; then

	AC_CHECK_LIB(lve, lr_init,
      [
	LIBLVE_CFLAGS=""
	LIBLVE_LIBS="-llve ${EXTRA_LIBS}" 
	AC_DEFINE(HAVE_LIBLVE)
	 have_liblve=yes
      ], have_liblve=no, )
fi

if test x$have_liblve != "xyes" ; then

	AC_CHECK_LIB(lve, lr_init,
      [
	LIBLVE_CFLAGS="-I/usr/local/include"
	LIBLVE_LIBS="-L/usr/local${deflib} -llve ${EXTRA_LIBS}" 
	AC_DEFINE(HAVE_LIBLVE)
	have_liblve=yes
      ], have_liblve=no, )
fi


AC_SUBST(LIBLVE_LIBS)
AC_SUBST(LIBLVE_CFLAGS)
])


dnl 
dnl qt
dnl 
dnl 

AC_DEFUN([AM_PATH_QT],
[

AC_ARG_WITH(qt, AC_HELP_STRING([--with-qt],[build quicktime dependent module (no)]),[case "${withval}" in
  yes) ;;
  no)  ;;
  *) AC_MSG_ERROR(bad value ${withval} for --with-qt) ;;
esac], with_qt=no)

AC_ARG_WITH(qt-includes,AC_HELP_STRING([--with-qt-includes=PFX],[prefix where local quicktime includes are installed (optional)]),
	  qt_includes="$withval",qt_includes="")

AC_ARG_WITH(qt-libs,AC_HELP_STRING([--with-qt-libs=PFX],[prefix where local quicktime libs are installed (optional)]),
	  qt_libs="$withval", qt_libs="")

EXTRA_LIBS="$GLIB_LIBS -lpng -lz $pthread_lib -lm $DV_LIBS"

if test x$with_qt = "x"yes ; then

	if test x$qt_includes != "x" ; then
	    with_qt_i="$qt_includes/include/quicktime"
        else
	    with_qt_i="/usr/include/quicktime"
        fi

        if test x$qt_libs != x ; then
            with_qt_l="$qt_libs/lib"
        else
            with_qt_l="/usr${deflib}"
        fi

     AC_CHECK_LIB(quicktime, quicktime_open,
      [
QT_CFLAGS="-I$with_qt_i -I/usr/local/include/quicktime $DV_CFLAGS "
QT_LIBS="-L$with_qt_l -lquicktime ${EXTRA_LIBS}"
AC_DEFINE(HAVE_QT) have_qt=yes
      ], have_qt=no, 
	-L$with_qt_l -lquicktime ${EXTRA_LIBS})

else
    have_qt=no
fi

AC_SUBST(QT_LIBS)
AC_SUBST(QT_CFLAGS)
])


dnl 
dnl ffmpeg
dnl 
dnl 

AC_DEFUN([AM_PATH_FFMPEG],
[

AC_ARG_WITH(ffbin, AC_HELP_STRING([--with-ffbin],[build ffbin module (no)]),[case "${withval}" in
  yes) ;;
  no)  ;;
  *) AC_MSG_ERROR(bad value ${withval} for --with-ffbin) ;;
esac], with_ffbin=no)

if test x$with_ffbin = "x"yes ; then

AC_CHECK_PROG(have_ffmpeg, ffmpeg, yes, no)

else
    have_ffmpeg=no
fi

])

# Configure paths for GTK+
# Owen Taylor     97-11-3

dnl AM_PATH_GTK([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND [, MODULES]]]])
dnl Test for GTK, and define GTK_CFLAGS and GTK_LIBS
dnl
AC_DEFUN([AM_PATH_GTK],
[dnl 
dnl Get the cflags and libraries from the gtk-config script
dnl
AC_ARG_WITH(gtk-prefix,AC_HELP_STRING([--with-gtk-prefix=PFX],[Prefix where GTK is installed (optional)]),
            gtk_config_prefix="$withval", gtk_config_prefix="")
AC_ARG_WITH(gtk-exec-prefix,AC_HELP_STRING([--with-gtk-exec-prefix=PFX],[Exec prefix where GTK is installed (optional)]),
            gtk_config_exec_prefix="$withval", gtk_config_exec_prefix="")
AC_ARG_ENABLE(gtktest, [  --disable-gtktest       Do not try to compile and run a test GTK program],
		    , enable_gtktest=yes)

  for module in . $4
  do
      case "$module" in
         gthread) 
             gtk_config_args="$gtk_config_args gthread"
         ;;
      esac
  done

  if test x$gtk_config_exec_prefix != x ; then
     gtk_config_args="$gtk_config_args --exec-prefix=$gtk_config_exec_prefix"
     if test x${GTK_CONFIG+set} != xset ; then
        GTK_CONFIG=$gtk_config_exec_prefix/bin/gtk-config
     fi
  fi
  if test x$gtk_config_prefix != x ; then
     gtk_config_args="$gtk_config_args --prefix=$gtk_config_prefix"
     if test x${GTK_CONFIG+set} != xset ; then
        GTK_CONFIG=$gtk_config_prefix/bin/gtk-config
     fi
  fi

  AC_PATH_PROG(GTK_CONFIG, gtk-config, no)
  min_gtk_version=ifelse([$1], ,0.99.7,$1)
  AC_MSG_CHECKING(for GTK - version >= $min_gtk_version)
  no_gtk=""
  if test "$GTK_CONFIG" = "no" ; then
    no_gtk=yes
  else
    GTK_CFLAGS=`$GTK_CONFIG $gtk_config_args --cflags`
    GTK_LIBS=`$GTK_CONFIG $gtk_config_args --libs`
    gtk_config_major_version=`$GTK_CONFIG $gtk_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
    gtk_config_minor_version=`$GTK_CONFIG $gtk_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
    gtk_config_micro_version=`$GTK_CONFIG $gtk_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`
    if test "x$enable_gtktest" = "xyes" ; then
      ac_save_CFLAGS="$CFLAGS"
      ac_save_LIBS="$LIBS"
      CFLAGS="$CFLAGS $GTK_CFLAGS"
      LIBS="$GTK_LIBS $LIBS"
dnl
dnl Now check if the installed GTK is sufficiently new. (Also sanity
dnl checks the results of gtk-config to some extent
dnl
      rm -f conf.gtktest
      AC_TRY_RUN([
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>

int 
main ()
{
  int major, minor, micro;
  char *tmp_version;

  system ("touch conf.gtktest");

  /* HP/UX 9 (%@#!) writes to sscanf strings */
  tmp_version = g_strdup("$min_gtk_version");
  if (sscanf(tmp_version, "%d.%d.%d", &major, &minor, &micro) != 3) {
     printf("%s, bad version string\n", "$min_gtk_version");
     exit(1);
   }

  if ((gtk_major_version != $gtk_config_major_version) ||
      (gtk_minor_version != $gtk_config_minor_version) ||
      (gtk_micro_version != $gtk_config_micro_version))
    {
      printf("\n*** 'gtk-config --version' returned %d.%d.%d, but GTK+ (%d.%d.%d)\n", 
             $gtk_config_major_version, $gtk_config_minor_version, $gtk_config_micro_version,
             gtk_major_version, gtk_minor_version, gtk_micro_version);
      printf ("*** was found! If gtk-config was correct, then it is best\n");
      printf ("*** to remove the old version of GTK+. You may also be able to fix the error\n");
      printf("*** by modifying your LD_LIBRARY_PATH enviroment variable, or by editing\n");
      printf("*** /etc/ld.so.conf. Make sure you have run ldconfig if that is\n");
      printf("*** required on your system.\n");
      printf("*** If gtk-config was wrong, set the environment variable GTK_CONFIG\n");
      printf("*** to point to the correct copy of gtk-config, and remove the file config.cache\n");
      printf("*** before re-running configure\n");
    } 
#if defined (GTK_MAJOR_VERSION) && defined (GTK_MINOR_VERSION) && defined (GTK_MICRO_VERSION)
  else if ((gtk_major_version != GTK_MAJOR_VERSION) ||
	   (gtk_minor_version != GTK_MINOR_VERSION) ||
           (gtk_micro_version != GTK_MICRO_VERSION))
    {
      printf("*** GTK+ header files (version %d.%d.%d) do not match\n",
	     GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION);
      printf("*** library (version %d.%d.%d)\n",
	     gtk_major_version, gtk_minor_version, gtk_micro_version);
    }
#endif /* defined (GTK_MAJOR_VERSION) ... */
  else
    {
      if ((gtk_major_version > major) ||
        ((gtk_major_version == major) && (gtk_minor_version > minor)) ||
        ((gtk_major_version == major) && (gtk_minor_version == minor) && (gtk_micro_version >= micro)))
      {
        return 0;
       }
     else
      {
        printf("\n*** An old version of GTK+ (%d.%d.%d) was found.\n",
               gtk_major_version, gtk_minor_version, gtk_micro_version);
        printf("*** You need a version of GTK+ newer than %d.%d.%d. The latest version of\n",
	       major, minor, micro);
        printf("*** GTK+ is always available from ftp://ftp.gtk.org.\n");
        printf("***\n");
        printf("*** If you have already installed a sufficiently new version, this error\n");
        printf("*** probably means that the wrong copy of the gtk-config shell script is\n");
        printf("*** being found. The easiest way to fix this is to remove the old version\n");
        printf("*** of GTK+, but you can also set the GTK_CONFIG environment to point to the\n");
        printf("*** correct copy of gtk-config. (In this case, you will have to\n");
        printf("*** modify your LD_LIBRARY_PATH enviroment variable, or edit /etc/ld.so.conf\n");
        printf("*** so that the correct libraries are found at run-time))\n");
      }
    }
  return 1;
}
],, no_gtk=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
     fi
  fi
  if test "x$no_gtk" = x ; then
     AC_MSG_RESULT(yes)
     ifelse([$2], , :, [$2])     
  else
     AC_MSG_RESULT(no)
     if test "$GTK_CONFIG" = "no" ; then
       echo "*** The gtk-config script installed by GTK could not be found"
       echo "*** If GTK was installed in PREFIX, make sure PREFIX/bin is in"
       echo "*** your path, or set the GTK_CONFIG environment variable to the"
       echo "*** full path to gtk-config."
     else
       if test -f conf.gtktest ; then
        :
       else
          echo "*** Could not run GTK test program, checking why..."
          CFLAGS="$CFLAGS $GTK_CFLAGS"
          LIBS="$LIBS $GTK_LIBS"
          AC_TRY_LINK([
#include <gtk/gtk.h>
#include <stdio.h>
],      [ return ((gtk_major_version) || (gtk_minor_version) || (gtk_micro_version)); ],
        [ echo "*** The test program compiled, but did not run. This usually means"
          echo "*** that the run-time linker is not finding GTK or finding the wrong"
          echo "*** version of GTK. If it is not finding GTK, you'll need to set your"
          echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
          echo "*** to the installed location  Also, make sure you have run ldconfig if that"
          echo "*** is required on your system"
	  echo "***"
          echo "*** If you have an old version installed, it is best to remove it, although"
          echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH"
          echo "***"
          echo "*** If you have a RedHat 5.0 system, you should remove the GTK package that"
          echo "*** came with the system with the command"
          echo "***"
          echo "***    rpm --erase --nodeps gtk gtk-devel" ],
        [ echo "*** The test program failed to compile or link. See the file config.log for the"
          echo "*** exact error that occured. This usually means GTK was incorrectly installed"
          echo "*** or that you have moved GTK since it was installed. In the latter case, you"
          echo "*** may want to edit the gtk-config script: $GTK_CONFIG" ])
          CFLAGS="$ac_save_CFLAGS"
          LIBS="$ac_save_LIBS"
       fi
     fi
     GTK_CFLAGS=""
     GTK_LIBS=""
     ifelse([$3], , :, [$3])
  fi
  AC_SUBST(GTK_CFLAGS)
  AC_SUBST(GTK_LIBS)
  rm -f conf.gtktest
])

dnl AM_PATH_MAGICK([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for MAGICK, and define MAGICK_CFLAGS and MAGICK_LIBS
dnl

AC_DEFUN([AM_PATH_MAGICK],
[
dnl 
dnl Get the cflags and libaries from the magick-config script
dnl

MAGICK_CFLAGS=""
MAGICK_LIBS=""

AC_ARG_WITH(magick-mods,AC_HELP_STRING([--with-magick-mods],[build ImageMagick dependent modules (yes)]), magick_mods="$withval", magick_mods=yes)

if test x"$magick_mods" = xyes; then

AC_ARG_WITH(magick-exec-prefix,AC_HELP_STRING([--with-magick-exec-prefix=PFX],[prefix where ImageMagick is installed]),
	  magick_exec_prefix="$withval", magick_exec_prefix="")

  if test x$magick_exec_prefix != x ; then
     magick_args="$magick_args --exec-prefix=$magick_exec_prefix"
     if test x${MAGICK_CONFIG+set} != xset ; then
        MAGICK_CONFIG=$magick_exec_prefix/bin/Magick-config
     fi
  fi

  AC_PATH_PROG(MAGICK_CONFIG, Magick-config, no)


  if test "$MAGICK_CONFIG" = "no" ; then
    have_magick=no

  else

     AC_CHECK_LIB(Magick, InitializeMagick,
      [
	MAGICK_CFLAGS=`$MAGICK_CONFIG $magickconf_args --cppflags`
	MAGICK_LIBS="`$MAGICK_CONFIG $magickconf_args --ldflags` `$MAGICK_CONFIG $magickconf_args --libs`" 
	AC_DEFINE(HAVE_MAGICK)
	have_magick=yes
      ], have_magick=no, `$MAGICK_CONFIG $magickconf_args --ldflags` `$MAGICK_CONFIG $magickconf_args --libs`)
  fi 
fi
  AC_SUBST(MAGICK_CFLAGS)
  AC_SUBST(MAGICK_LIBS)

])
dnl AM_PATH_MAGICK([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for MAGICK, and define MAGICK_CFLAGS and MAGICK_LIBS
dnl


AC_DEFUN([AM_PATH_LIBJPEG],
[

LIBJPEG_CFLAGS=""
LIBJPEG_LIBS=""

AC_ARG_WITH(libjpeg-mods,AC_HELP_STRING([--with-libjpeg-mods],[build libjpeg dependent modules (yes)]), libjpeg_mods="$withval", libjpeg_mods=yes)

have_libjpegmmx=no
have_libjpeg=no

if test x"$libjpeg_mods" = xyes; then

  AC_CHECK_LIB(jpeg-mmx, jpeg_CreateCompress,
      [
	LIBJPEG_CFLAGS=""
	LIBJPEG_LIBS="-ljpeg-mmx" 
	AC_DEFINE(HAVE_LIBJPEG)
	have_libjpeg=yes have_libjpegmmx=yes
      ], [have_libjpeg=no have_libjpegmmx=no], )
 
  if test x$LIBJPEG_LIBS = x; then
  AC_CHECK_LIB(jpeg, jpeg_CreateCompress,
      [
	LIBJPEG_CFLAGS=""
	LIBJPEG_LIBS="-ljpeg" 
	AC_DEFINE(HAVE_LIBJPEG)
	have_libjpeg=yes have_libjpegmmx=no
      ], have_libjpeg=no have_libjpegmmx=no, )
  fi 
fi

AC_SUBST(LIBJPEG_CFLAGS)
AC_SUBST(LIBJPEG_LIBS)

])


dnl AM_PATH_LIBFAME([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND [, MODULES]]]])
dnl Test for libfame, and define LIBFAME_CFLAGS and LIBFAME_LIBS
dnl Vivien Chappelier 2000-12-11
dnl stolen from ORBit autoconf
dnl
AC_DEFUN([AM_PATH_LIBFAME],
[dnl 
dnl Get the cflags and libraries from the libfame-config script
dnl
AC_ARG_WITH(libfame-prefix,AC_HELP_STRING([--with-libfame-prefix=PFX],[Prefix where libfame is installed (optional)]),
            libfame_config_prefix="$withval", libfame_config_prefix="")
AC_ARG_WITH(libfame-exec-prefix,AC_HELP_STRING([--with-libfame-exec-prefix=PFX],[Exec prefix where libfame is installed (optional)]),
            libfame_config_exec_prefix="$withval", libfame_config_exec_prefix="")
AC_ARG_ENABLE(libfametest, [  --disable-libfametest   Do not try to compile and run a test libfame program],
		    , enable_libfametest=yes)

  if test x$libfame_config_exec_prefix != x ; then
     libfame_config_args="$libfame_config_args --exec-prefix=$libfame_config_exec_prefix"
     if test x${LIBFAME_CONFIG+set} != xset ; then
        LIBFAME_CONFIG=$libfame_config_exec_prefix/bin/libfame-config
     fi
  fi
  if test x$libfame_config_prefix != x ; then
     libfame_config_args="$libfame_config_args --prefix=$libfame_config_prefix"
     if test x${LIBFAME_CONFIG+set} != xset ; then
        LIBFAME_CONFIG=$libfame_config_prefix/bin/libfame-config
     fi
  fi

  AC_PATH_PROG(LIBFAME_CONFIG, libfame-config, no)
  min_libfame_version=ifelse([$1], , 0.9.0, $1)
  AC_MSG_CHECKING(for libfame - version >= $min_libfame_version)
  no_libfame=""
  if test "$LIBFAME_CONFIG" = "no" ; then
    no_libfame=yes
  else
    LIBFAME_CFLAGS=`$LIBFAME_CONFIG $libfame_config_args --cflags`
    LIBFAME_LIBS=`$LIBFAME_CONFIG $libfame_config_args --libs`
    libfame_config_major_version=`$LIBFAME_CONFIG $libfame_config_args --version | \
	   sed -e 's,[[^0-9.]],,g' -e 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
    libfame_config_minor_version=`$LIBFAME_CONFIG $libfame_config_args --version | \
	   sed -e 's,[[^0-9.]],,g' -e 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
    libfame_config_micro_version=`$LIBFAME_CONFIG $libfame_config_args --version | \
	   sed -e 's,[[^0-9.]],,g' -e 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`
    if test "x$enable_libfametest" = "xyes" ; then
      ac_save_CFLAGS="$CFLAGS"
      ac_save_LIBS="$LIBS"
      CFLAGS="$CFLAGS $LIBFAME_CFLAGS"
      LIBS="$LIBFAME_LIBS $LIBS"
dnl
dnl Now check if the installed LIBFAME is sufficiently new. (Also sanity
dnl checks the results of libfame-config to some extent
dnl
      rm -f conf.libfametest
      AC_TRY_RUN([
#include <fame.h>
#include <stdio.h>
#include <stdlib.h>

int 
main ()
{
  int major, minor, micro;
  char *tmp_version;

  system ("touch conf.libfametest");

  /* HP/UX 9 (%@#!) writes to sscanf strings */
  tmp_version = strdup("$min_libfame_version");
  if (sscanf(tmp_version, "%d.%d.%d", &major, &minor, &micro) != 3) {
     printf("%s, bad version string\n", "$min_libfame_version");
     exit(1);
   }

  if ((libfame_major_version != $libfame_config_major_version) ||
      (libfame_minor_version != $libfame_config_minor_version) ||
      (libfame_micro_version != $libfame_config_micro_version))
    {
      printf("\n*** 'libfame-config --version' returned %d.%d.%d, but Libfame (%d.%d.%d)\n", 
             $libfame_config_major_version, $libfame_config_minor_version, $libfame_config_micro_version,
             libfame_major_version, libfame_minor_version, libfame_micro_version);
      printf ("*** was found! If libfame-config was correct, then it is best\n");
      printf ("*** to remove the old version of libfame. You may also be able to fix the error\n");
      printf("*** by modifying your LD_LIBRARY_PATH enviroment variable, or by editing\n");
      printf("*** /etc/ld.so.conf. Make sure you have run ldconfig if that is\n");
      printf("*** required on your system.\n");
      printf("*** If libfame-config was wrong, set the environment variable LIBFAME_CONFIG\n");
      printf("*** to point to the correct copy of libfame-config, and remove the file config.cache\n");
      printf("*** before re-running configure\n");
    } 
#if defined (LIBFAME_MAJOR_VERSION) && defined (LIBFAME_MINOR_VERSION) && defined (LIBFAME_MICRO_VERSION)
  else if ((libfame_major_version != LIBFAME_MAJOR_VERSION) ||
	   (libfame_minor_version != LIBFAME_MINOR_VERSION) ||
           (libfame_micro_version != LIBFAME_MICRO_VERSION))
    {
      printf("*** libfame header files (version %d.%d.%d) do not match\n",
	     LIBFAME_MAJOR_VERSION, LIBFAME_MINOR_VERSION, LIBFAME_MICRO_VERSION);
      printf("*** library (version %d.%d.%d)\n",
	     libfame_major_version, libfame_minor_version, libfame_micro_version);
    }
#endif /* defined (LIBFAME_MAJOR_VERSION) ... */
  else
    {
      if ((libfame_major_version > major) ||
        ((libfame_major_version == major) && (libfame_minor_version > minor)) ||
        ((libfame_major_version == major) && (libfame_minor_version == minor) && (libfame_micro_version >= micro)))
      {
        return 0;
       }
     else
      {
        printf("\n*** An old version of libfame (%d.%d.%d) was found.\n",
               libfame_major_version, libfame_minor_version, libfame_micro_version);
        printf("*** You need a version of libfame newer than %d.%d.%d. The latest version of\n",
	       major, minor, micro);
        printf("*** libfame is always available from http://www-eleves.enst-bretagne.fr/~chappeli/fame\n");
        printf("***\n");
        printf("*** If you have already installed a sufficiently new version, this error\n");
        printf("*** probably means that the wrong copy of the libfame-config shell script is\n");
        printf("*** being found. The easiest way to fix this is to remove the old version\n");
        printf("*** of libfame, but you can also set the LIBFAME_CONFIG environment to point to the\n");
        printf("*** correct copy of libfame-config. (In this case, you will have to\n");
        printf("*** modify your LD_LIBRARY_PATH enviroment variable, or edit /etc/ld.so.conf\n");
        printf("*** so that the correct libraries are found at run-time))\n");
      }
    }
  return 1;
}
],, no_libfame=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
     fi
  fi
  if test "x$no_libfame" = x ; then
     AC_MSG_RESULT(yes)
     ifelse([$2], , :, [$2])     
  else
     AC_MSG_RESULT(no)
     if test "$LIBFAME_CONFIG" = "no" ; then
       echo "*** The libfame-config script installed by libfame could not be found"
       echo "*** If libfame was installed in PREFIX, make sure PREFIX/bin is in"
       echo "*** your path, or set the LIBFAME_CONFIG environment variable to the"
       echo "*** full path to libfame-config."
     else
       if test -f conf.libfametest ; then
        :
       else
          echo "*** Could not run libfame test program, checking why..."
          CFLAGS="$CFLAGS $LIBFAME_CFLAGS"
          LIBS="$LIBS $LIBFAME_LIBS"
          AC_TRY_LINK([
#include <fame.h>
#include <stdio.h>
],      [ return ((libfame_major_version) || (libfame_minor_version) || (libfame_micro_version)); ],
        [ echo "*** The test program compiled, but did not run. This usually means"
          echo "*** that the run-time linker is not finding libfame or finding the wrong"
          echo "*** version of LIBFAME. If it is not finding libfame, you'll need to set your"
          echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
          echo "*** to the installed location  Also, make sure you have run ldconfig if that"
          echo "*** is required on your system"
	  echo "***"
          echo "*** If you have an old version installed, it is best to remove it, although"
          echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH"
          echo "***" ],
        [ echo "*** The test program failed to compile or link. See the file config.log for the"
          echo "*** exact error that occured. This usually means libfame was incorrectly installed"
          echo "*** or that you have moved libfame since it was installed. In the latter case, you"
          echo "*** may want to edit the libfame-config script: $LIBFAME_CONFIG" ])
          CFLAGS="$ac_save_CFLAGS"
          LIBS="$ac_save_LIBS"
       fi
     fi
     LIBFAME_CFLAGS=""
     LIBFAME_LIBS=""
     ifelse([$3], , :, [$3])
  fi

  AC_SUBST(LIBFAME_CFLAGS)
  AC_SUBST(LIBFAME_LIBS)
  rm -f conf.libfametest
])


# Configure paths for FreeType2
# Marcelo Magallon 2001-10-26, based on gtk.m4 by Owen Taylor

dnl AC_CHECK_FT2([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for FreeType2, and define FT2_CFLAGS and FT2_LIBS
dnl
AC_DEFUN([AC_CHECK_FT2],
[dnl
dnl Get the cflags and libraries from the freetype-config script
dnl
AC_ARG_WITH(ft-prefix,
[  --with-ft-prefix=PREFIX
                          Prefix where FreeType is installed (optional)],
            ft_config_prefix="$withval", ft_config_prefix="")
AC_ARG_WITH(ft-exec-prefix,
[  --with-ft-exec-prefix=PREFIX
                          Exec prefix where FreeType is installed (optional)],
            ft_config_exec_prefix="$withval", ft_config_exec_prefix="")
AC_ARG_ENABLE(freetypetest,
[  --disable-freetypetest  Do not try to compile and run
                          a test FreeType program],
              [], enable_fttest=yes)

if test x$ft_config_exec_prefix != x ; then
  ft_config_args="$ft_config_args --exec-prefix=$ft_config_exec_prefix"
  if test x${FT2_CONFIG+set} != xset ; then
    FT2_CONFIG=$ft_config_exec_prefix/bin/freetype-config
  fi
fi
if test x$ft_config_prefix != x ; then
  ft_config_args="$ft_config_args --prefix=$ft_config_prefix"
  if test x${FT2_CONFIG+set} != xset ; then
    FT2_CONFIG=$ft_config_prefix/bin/freetype-config
  fi
fi
AC_PATH_PROG(FT2_CONFIG, freetype-config, no)

min_ft_version=ifelse([$1], ,6.1.0,$1)
AC_MSG_CHECKING(for FreeType - version >= $min_ft_version)
no_ft=""
if test "$FT2_CONFIG" = "no" ; then
  no_ft=yes
else
  FT2_CFLAGS=`$FT2_CONFIG $ft_config_args --cflags`
  FT2_LIBS=`$FT2_CONFIG $ft_config_args --libs`
  ft_config_major_version=`$FT2_CONFIG $ft_config_args --version | \
         sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
  ft_config_minor_version=`$FT2_CONFIG $ft_config_args --version | \
         sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
  ft_config_micro_version=`$FT2_CONFIG $ft_config_args --version | \
         sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`
  ft_min_major_version=`echo $min_ft_version | \
         sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
  ft_min_minor_version=`echo $min_ft_version | \
         sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
  ft_min_micro_version=`echo $min_ft_version | \
         sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`
  if test x$enable_fttest = xyes ; then
    ft_config_is_lt=""
    if test $ft_config_major_version -lt $ft_min_major_version ; then
      ft_config_is_lt=yes
    else
      if test $ft_config_major_version -eq $ft_min_major_version ; then
        if test $ft_config_minor_version -lt $ft_min_minor_version ; then
          ft_config_is_lt=yes
        else
          if test $ft_config_minor_version -eq $ft_min_minor_version ; then
            if test $ft_config_micro_version -lt $ft_min_micro_version ; then
              ft_config_is_lt=yes
            fi
          fi
        fi
      fi
    fi
    if test x$ft_config_is_lt = xyes ; then
      no_ft=yes
    else
      ac_save_CFLAGS="$CFLAGS"
      ac_save_LIBS="$LIBS"
      CFLAGS="$CFLAGS $FT2_CFLAGS"
      LIBS="$FT2_LIBS $LIBS"
dnl
dnl Sanity checks for the results of freetype-config to some extent
dnl
      AC_TRY_RUN([
#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdio.h>
#include <stdlib.h>

int
main()
{
  FT_Library library;
  FT_Error error;

  error = FT_Init_FreeType(&library);

  if (error)
    return 1;
  else
  {
    FT_Done_FreeType(library);
    return 0;
  }
}
],, no_ft=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
      CFLAGS="$ac_save_CFLAGS"
      LIBS="$ac_save_LIBS"
    fi             # test $ft_config_version -lt $ft_min_version
  fi               # test x$enable_fttest = xyes
fi                 # test "$FT2_CONFIG" = "no"
if test x$no_ft = x ; then
   AC_MSG_RESULT(yes)
   ifelse([$2], , :, [$2])
else
   AC_MSG_RESULT(no)
   if test "$FT2_CONFIG" = "no" ; then
     echo "*** The freetype-config script installed by FreeType 2 could not be found."
     echo "*** If FreeType 2 was installed in PREFIX, make sure PREFIX/bin is in"
     echo "*** your path, or set the FT2_CONFIG environment variable to the"
     echo "*** full path to freetype-config."
   else
     if test x$ft_config_is_lt = xyes ; then
       echo "*** Your installed version of the FreeType 2 library is too old."
       echo "*** If you have different versions of FreeType 2, make sure that"
       echo "*** correct values for --with-ft-prefix or --with-ft-exec-prefix"
       echo "*** are used, or set the FT2_CONFIG environment variable to the"
       echo "*** full path to freetype-config."
     else
       echo "*** The FreeType test program failed to run.  If your system uses"
       echo "*** shared libraries and they are installed outside the normal"
       echo "*** system library path, make sure the variable LD_LIBRARY_PATH"
       echo "*** (or whatever is appropiate for your system) is correctly set."
     fi
   fi
   FT2_CFLAGS=""
   FT2_LIBS=""
   ifelse([$3], , :, [$3])
fi
AC_SUBST(FT2_CFLAGS)
AC_SUBST(FT2_LIBS)
])
# Configure paths for SDL
# Sam Lantinga 9/21/99
# stolen from Manish Singh
# stolen back from Frank Belew
# stolen from Manish Singh
# Shamelessly stolen from Owen Taylor
# Small fix to avoid warnings from Daniel Caujolle-Bert

dnl AM_PATH_SDL([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for SDL, and define SDL_CFLAGS and SDL_LIBS
dnl
AC_DEFUN([AM_PATH_SDL],
 [dnl 
  AC_REQUIRE([AC_PROG_CC])dnl
  AC_REQUIRE([AC_CANONICAL_HOST])dnl
  dnl Get the cflags and libraries from the sdl-config script
  dnl
  AC_ARG_WITH(sdl-prefix,[  --with-sdl-prefix=PFX   Prefix where SDL is installed (optional)],
              sdl_prefix="$withval", sdl_prefix="")
  AC_ARG_WITH(sdl-exec-prefix,[  --with-sdl-exec-prefix=PFX Exec prefix where SDL is installed (optional)],
              sdl_exec_prefix="$withval", sdl_exec_prefix="")
  AC_ARG_ENABLE(sdltest, [  --disable-sdltest       Do not try to compile and run a test SDL program],, enable_sdltest=yes)

  if test x$sdl_exec_prefix != x ; then
     sdl_args="$sdl_args --exec-prefix=$sdl_exec_prefix"
     if test x${SDL_CONFIG+set} != xset ; then
        SDL_CONFIG=$sdl_exec_prefix/bin/sdl-config
     fi
  fi
  if test x$sdl_prefix != x ; then
     sdl_args="$sdl_args --prefix=$sdl_prefix"
     if test x${SDL_CONFIG+set} != xset ; then
        SDL_CONFIG=$sdl_prefix/bin/sdl-config
     fi
  fi

  AC_PATH_PROG(SDL_CONFIG, sdl-config, no)
  min_sdl_version=ifelse([$1], ,0.11.0,$1)
  AC_MSG_CHECKING(for SDL - version >= $min_sdl_version)
  no_sdl=""
  if test "$SDL_CONFIG" = "no" ; then
    no_sdl=yes
  else
    SDL_CFLAGS=`$SDL_CONFIG $sdlconf_args --cflags`
    SDL_LIBS=`$SDL_CONFIG $sdlconf_args --libs`

    sdl_major_version=`$SDL_CONFIG $sdl_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
    sdl_minor_version=`$SDL_CONFIG $sdl_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
    sdl_micro_version=`$SDL_CONFIG $sdl_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`
    if test "x$enable_sdltest" = "xyes" ; then
      ac_save_CFLAGS="$CFLAGS"
      ac_save_LIBS="$LIBS"
      CFLAGS="$CFLAGS $SDL_CFLAGS"
      LIBS="$LIBS $SDL_LIBS"
dnl
dnl Now check if the installed SDL is sufficiently new. (Also sanity
dnl checks the results of sdl-config to some extent
dnl
      rm -f conf.sdltest
      AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SDL.h"

char*
my_strdup (char *str)
{
  char *new_str;
  
  if (str)
    {
      new_str = (char *)malloc ((strlen (str) + 1) * sizeof(char));
      strcpy (new_str, str);
    }
  else
    new_str = NULL;
  
  return new_str;
}

int main (int argc, char *argv[])
{
  int major, minor, micro;
  char *tmp_version;

  /* This hangs on some systems (?)
  system ("touch conf.sdltest");
  */
  { FILE *fp = fopen("conf.sdltest", "a"); if ( fp ) fclose(fp); }

  /* HP/UX 9 (%@#!) writes to sscanf strings */
  tmp_version = my_strdup("$min_sdl_version");
  if (sscanf(tmp_version, "%d.%d.%d", &major, &minor, &micro) != 3) {
     printf("%s, bad version string\n", "$min_sdl_version");
     exit(1);
   }

   if (($sdl_major_version > major) ||
      (($sdl_major_version == major) && ($sdl_minor_version > minor)) ||
      (($sdl_major_version == major) && ($sdl_minor_version == minor) && ($sdl_micro_version >= micro)))
    {
      return 0;
    }
  else
    {
      printf("\n*** 'sdl-config --version' returned %d.%d.%d, but the minimum version\n", $sdl_major_version, $sdl_minor_version, $sdl_micro_version);
      printf("*** of SDL required is %d.%d.%d. If sdl-config is correct, then it is\n", major, minor, micro);
      printf("*** best to upgrade to the required version.\n");
      printf("*** If sdl-config was wrong, set the environment variable SDL_CONFIG\n");
      printf("*** to point to the correct copy of sdl-config, and remove the file\n");
      printf("*** config.cache before re-running configure\n");
      return 1;
    }
}

],, no_sdl=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
     fi
  fi
  if test "x$no_sdl" = x ; then
     AC_MSG_RESULT(yes)
     ifelse([$2], , :, [$2])     
  else
     AC_MSG_RESULT(no)
     if test "$SDL_CONFIG" = "no" ; then
       echo "*** The sdl-config script installed by SDL could not be found"
       echo "*** If SDL was installed in PREFIX, make sure PREFIX/bin is in"
       echo "*** your path, or set the SDL_CONFIG environment variable to the"
       echo "*** full path to sdl-config."
     else
       if test -f conf.sdltest ; then
        :
       else
          echo "*** Could not run SDL test program, checking why..."
          CFLAGS="$CFLAGS $SDL_CFLAGS"
          LIBS="$LIBS $SDL_LIBS"
          AC_TRY_LINK([
#include <stdio.h>
#include "SDL.h"

int main(int argc, char *argv[])
{ return 0; }
#undef  main
#define main K_and_R_C_main
],      [ return 0; ],
        [ echo "*** The test program compiled, but did not run. This usually means"
          echo "*** that the run-time linker is not finding SDL or finding the wrong"
          echo "*** version of SDL. If it is not finding SDL, you'll need to set your"
          echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
          echo "*** to the installed location  Also, make sure you have run ldconfig if that"
          echo "*** is required on your system"
	  echo "***"
          echo "*** If you have an old version installed, it is best to remove it, although"
          echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH"],
        [ echo "*** The test program failed to compile or link. See the file config.log for the"
          echo "*** exact error that occured. This usually means SDL was incorrectly installed"
          echo "*** or that you have moved SDL since it was installed. In the latter case, you"
          echo "*** may want to edit the sdl-config script: $SDL_CONFIG" ])
          CFLAGS="$ac_save_CFLAGS"
          LIBS="$ac_save_LIBS"
       fi
     fi
     SDL_CFLAGS=""
     SDL_LIBS=""
     ifelse([$3], , :, [$3])
  fi
  AC_SUBST(SDL_CFLAGS)
  AC_SUBST(SDL_LIBS)
  rm -f conf.sdltest
])
# Configure paths for GLIB
# Owen Taylor     97-11-3

dnl AM_PATH_GLIB([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND [, MODULES]]]])
dnl Test for GLIB, and define GLIB_CFLAGS and GLIB_LIBS, if "gmodule" or 
dnl gthread is specified in MODULES, pass to glib-config
dnl
AC_DEFUN([AM_PATH_GLIB],
[dnl 
dnl Get the cflags and libraries from the glib-config script
dnl
AC_ARG_WITH(glib-prefix,[  --with-glib-prefix=PFX   Prefix where GLIB is installed (optional)],
            glib_config_prefix="$withval", glib_config_prefix="")
AC_ARG_WITH(glib-exec-prefix,[  --with-glib-exec-prefix=PFX Exec prefix where GLIB is installed (optional)],
            glib_config_exec_prefix="$withval", glib_config_exec_prefix="")
AC_ARG_ENABLE(glibtest, [  --disable-glibtest       Do not try to compile and run a test GLIB program],
		    , enable_glibtest=yes)

  if test x$glib_config_exec_prefix != x ; then
     glib_config_args="$glib_config_args --exec-prefix=$glib_config_exec_prefix"
     if test x${GLIB_CONFIG+set} != xset ; then
        GLIB_CONFIG=$glib_config_exec_prefix/bin/glib-config
     fi
  fi
  if test x$glib_config_prefix != x ; then
     glib_config_args="$glib_config_args --prefix=$glib_config_prefix"
     if test x${GLIB_CONFIG+set} != xset ; then
        GLIB_CONFIG=$glib_config_prefix/bin/glib-config
     fi
  fi

  for module in . $4
  do
      case "$module" in
         gmodule) 
             glib_config_args="$glib_config_args gmodule"
         ;;
         gthread) 
             glib_config_args="$glib_config_args gthread"
         ;;
      esac
  done

  AC_PATH_PROG(GLIB_CONFIG, glib-config, no)
  min_glib_version=ifelse([$1], ,0.99.7,$1)
  AC_MSG_CHECKING(for GLIB - version >= $min_glib_version)
  no_glib=""
  if test "$GLIB_CONFIG" = "no" ; then
    no_glib=yes
  else
    GLIB_CFLAGS=`$GLIB_CONFIG $glib_config_args --cflags`
    GLIB_LIBS=`$GLIB_CONFIG $glib_config_args --libs`
    glib_config_major_version=`$GLIB_CONFIG $glib_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
    glib_config_minor_version=`$GLIB_CONFIG $glib_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
    glib_config_micro_version=`$GLIB_CONFIG $glib_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`
    if test "x$enable_glibtest" = "xyes" ; then
      ac_save_CFLAGS="$CFLAGS"
      ac_save_LIBS="$LIBS"
      CFLAGS="$CFLAGS $GLIB_CFLAGS"
      LIBS="$GLIB_LIBS $LIBS"
dnl
dnl Now check if the installed GLIB is sufficiently new. (Also sanity
dnl checks the results of glib-config to some extent
dnl
      rm -f conf.glibtest
      AC_TRY_RUN([
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>

int 
main ()
{
  int major, minor, micro;
  char *tmp_version;

  system ("touch conf.glibtest");

  /* HP/UX 9 (%@#!) writes to sscanf strings */
  tmp_version = g_strdup("$min_glib_version");
  if (sscanf(tmp_version, "%d.%d.%d", &major, &minor, &micro) != 3) {
     printf("%s, bad version string\n", "$min_glib_version");
     exit(1);
   }

  if ((glib_major_version != $glib_config_major_version) ||
      (glib_minor_version != $glib_config_minor_version) ||
      (glib_micro_version != $glib_config_micro_version))
    {
      printf("\n*** 'glib-config --version' returned %d.%d.%d, but GLIB (%d.%d.%d)\n", 
             $glib_config_major_version, $glib_config_minor_version, $glib_config_micro_version,
             glib_major_version, glib_minor_version, glib_micro_version);
      printf ("*** was found! If glib-config was correct, then it is best\n");
      printf ("*** to remove the old version of GLIB. You may also be able to fix the error\n");
      printf("*** by modifying your LD_LIBRARY_PATH enviroment variable, or by editing\n");
      printf("*** /etc/ld.so.conf. Make sure you have run ldconfig if that is\n");
      printf("*** required on your system.\n");
      printf("*** If glib-config was wrong, set the environment variable GLIB_CONFIG\n");
      printf("*** to point to the correct copy of glib-config, and remove the file config.cache\n");
      printf("*** before re-running configure\n");
    } 
  else if ((glib_major_version != GLIB_MAJOR_VERSION) ||
	   (glib_minor_version != GLIB_MINOR_VERSION) ||
           (glib_micro_version != GLIB_MICRO_VERSION))
    {
      printf("*** GLIB header files (version %d.%d.%d) do not match\n",
	     GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION, GLIB_MICRO_VERSION);
      printf("*** library (version %d.%d.%d)\n",
	     glib_major_version, glib_minor_version, glib_micro_version);
    }
  else
    {
      if ((glib_major_version > major) ||
        ((glib_major_version == major) && (glib_minor_version > minor)) ||
        ((glib_major_version == major) && (glib_minor_version == minor) && (glib_micro_version >= micro)))
      {
        return 0;
       }
     else
      {
        printf("\n*** An old version of GLIB (%d.%d.%d) was found.\n",
               glib_major_version, glib_minor_version, glib_micro_version);
        printf("*** You need a version of GLIB newer than %d.%d.%d. The latest version of\n",
	       major, minor, micro);
        printf("*** GLIB is always available from ftp://ftp.gtk.org.\n");
        printf("***\n");
        printf("*** If you have already installed a sufficiently new version, this error\n");
        printf("*** probably means that the wrong copy of the glib-config shell script is\n");
        printf("*** being found. The easiest way to fix this is to remove the old version\n");
        printf("*** of GLIB, but you can also set the GLIB_CONFIG environment to point to the\n");
        printf("*** correct copy of glib-config. (In this case, you will have to\n");
        printf("*** modify your LD_LIBRARY_PATH enviroment variable, or edit /etc/ld.so.conf\n");
        printf("*** so that the correct libraries are found at run-time))\n");
      }
    }
  return 1;
}
],, no_glib=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
     fi
  fi
  if test "x$no_glib" = x ; then
     AC_MSG_RESULT(yes)
     ifelse([$2], , :, [$2])     
  else
     AC_MSG_RESULT(no)
     if test "$GLIB_CONFIG" = "no" ; then
       echo "*** The glib-config script installed by GLIB could not be found"
       echo "*** If GLIB was installed in PREFIX, make sure PREFIX/bin is in"
       echo "*** your path, or set the GLIB_CONFIG environment variable to the"
       echo "*** full path to glib-config."
     else
       if test -f conf.glibtest ; then
        :
       else
          echo "*** Could not run GLIB test program, checking why..."
          CFLAGS="$CFLAGS $GLIB_CFLAGS"
          LIBS="$LIBS $GLIB_LIBS"
          AC_TRY_LINK([
#include <glib.h>
#include <stdio.h>
],      [ return ((glib_major_version) || (glib_minor_version) || (glib_micro_version)); ],
        [ echo "*** The test program compiled, but did not run. This usually means"
          echo "*** that the run-time linker is not finding GLIB or finding the wrong"
          echo "*** version of GLIB. If it is not finding GLIB, you'll need to set your"
          echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
          echo "*** to the installed location  Also, make sure you have run ldconfig if that"
          echo "*** is required on your system"
	  echo "***"
          echo "*** If you have an old version installed, it is best to remove it, although"
          echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH"
          echo "***"
          echo "*** If you have a RedHat 5.0 system, you should remove the GTK package that"
          echo "*** came with the system with the command"
          echo "***"
          echo "***    rpm --erase --nodeps gtk gtk-devel" ],
        [ echo "*** The test program failed to compile or link. See the file config.log for the"
          echo "*** exact error that occured. This usually means GLIB was incorrectly installed"
          echo "*** or that you have moved GLIB since it was installed. In the latter case, you"
          echo "*** may want to edit the glib-config script: $GLIB_CONFIG" ])
          CFLAGS="$ac_save_CFLAGS"
          LIBS="$ac_save_LIBS"
       fi
     fi
     GLIB_CFLAGS=""
     GLIB_LIBS=""
     ifelse([$3], , :, [$3])
  fi
  AC_SUBST(GLIB_CFLAGS)
  AC_SUBST(GLIB_LIBS)
  rm -f conf.glibtest
])


dnl PKG_CHECK_MODULES(GSTUFF, gtk+-2.0 >= 1.3 glib = 1.3.4, action-if, action-not)
dnl defines GSTUFF_LIBS, GSTUFF_CFLAGS, see pkg-config man page
dnl also defines GSTUFF_PKG_ERRORS on error
AC_DEFUN([PKG_CHECK_MODULES], [
  succeeded=no

  if test -z "$PKG_CONFIG"; then
    AC_PATH_PROG(PKG_CONFIG, pkg-config, no)
  fi

  if test "$PKG_CONFIG" = "no" ; then
     echo "*** The pkg-config script could not be found. Make sure it is"
     echo "*** in your path, or set the PKG_CONFIG environment variable"
     echo "*** to the full path to pkg-config."
     echo "*** Or see http://www.freedesktop.org/software/pkgconfig to get pkg-config."
  else
     PKG_CONFIG_MIN_VERSION=0.9.0
     if $PKG_CONFIG --atleast-pkgconfig-version $PKG_CONFIG_MIN_VERSION; then
        AC_MSG_CHECKING(for $2)

        if $PKG_CONFIG --exists "$2" ; then
            AC_MSG_RESULT(yes)
            succeeded=yes

            AC_MSG_CHECKING($1_CFLAGS)
            $1_CFLAGS=`$PKG_CONFIG --cflags "$2"`
            AC_MSG_RESULT($$1_CFLAGS)

            AC_MSG_CHECKING($1_LIBS)
            $1_LIBS=`$PKG_CONFIG --libs "$2"`
            AC_MSG_RESULT($$1_LIBS)
        else
            $1_CFLAGS=""
            $1_LIBS=""
            ## If we have a custom action on failure, don't print errors, but 
            ## do set a variable so people can do so.
            $1_PKG_ERRORS=`$PKG_CONFIG --errors-to-stdout --print-errors "$2"`
            ifelse([$4], ,echo $$1_PKG_ERRORS,)
        fi

        AC_SUBST($1_CFLAGS)
        AC_SUBST($1_LIBS)
     else
        echo "*** Your version of pkg-config is too old. You need version $PKG_CONFIG_MIN_VERSION or newer."
        echo "*** See http://www.freedesktop.org/software/pkgconfig"
     fi
  fi

  if test $succeeded = yes; then
     ifelse([$3], , :, [$3])
  else
     ifelse([$4], , AC_MSG_ERROR([Library requirements ($2) not met; consider adjusting the PKG_CONFIG_PATH environment variable if your libraries are in a nonstandard prefix so pkg-config can find them.]), [$4])
  fi
])


