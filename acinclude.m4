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


dnl TC_PATH_FFMPEG_LIBS([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for libavcodec, and define FFMPEG_LIBS_LIBS, FFMPEG_LIBS_CFLAGS,
dnl and FFMPEG_LIBS_EXTRALIBS
dnl
AC_DEFUN([TC_PATH_FFMPEG_LIBS],
[
have_ffmpeg_libs=no
with_ffmpeg_libs=yes

AC_ARG_WITH(ffmpeg-libs-includes,
  AC_HELP_STRING([--with-ffmpeg-libs-includes=PFX],
    [prefix where ffmpeg libs includes are installed (optional)]),
  ffmpeg_libs_includes="$withval", ffmpeg_libs_includes="")

AC_ARG_WITH(ffmpeg-libs-libs,
  AC_HELP_STRING([--with-ffmpeg-libs-libs=PFX],
    [prefix where ffmpeg libs libraries files are installed (optional)]),
  ffmpeg_libs_libs="$withval", ffmpeg_libs_libs="")

AC_ARG_ENABLE(ffmpeg-libs-static,
  AC_HELP_STRING([--enable-ffmpeg-libs-static],
    [link binaries and modules statically to ffmpeg-libs WARNING: creates huge binaries WARNING: only tested on linux x86]),
  [case "${enableval}" in
    yes) ;;
    no)  ;;
    *) AC_MSG_ERROR(bad value ${enableval} for --enable-ffmpeg-libs-static) ;;
  esac],
  [enable_ffmpeg_libs_static=no])

if test x$ffmpeg_libs_includes != x ; then
  with_ffmpeg_libs_i="$ffmpeg_libs_includes/include/ffmpeg"
else
  with_ffmpeg_libs_i="/usr/include/ffmpeg"
fi
if test x$ffmpeg_libs_libs != x ; then
  with_ffmpeg_libs_l="$ffmpeg_libs_libs/lib"
else
  with_ffmpeg_libs_l="/usr${deflib}"
fi

FFMPEG_LIBS_EXTRALIBS="-lm -lz $PTHREAD_LIBS"

save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS -I$with_ffmpeg_libs_i"

AC_CHECK_HEADER([avcodec.h],
  [FFMPEG_LIBS_CFLAGS="-I$with_ffmpeg_libs_i"],
  [AC_MSG_ERROR([FFmpeg (libavcodec) required, but cannot compile avcodec.h])])

AC_TRY_RUN([
#include <stdio.h>
#include <avcodec.h>
int
main()
{
  if (LIBAVCODEC_BUILD < 4718)
  {
    printf("error: transcode needs at least ffmpeg build 4718");
    printf("install ffmpeg 0.4.9-pre1 or newer, or a cvs version after 20040703");
    return(1);
  }
  printf("VER=%s\n", FFMPEG_VERSION);
  printf("BUILD=%d\n", LIBAVCODEC_BUILD);
  return(0);
}
],
    [FFMPEG_LIBS_VERSION="`./conftest$ac_exeext | sed -ne 's,VER=\(.*\),\1,p'`"
      FFMPEG_LIBS_BUILD="`./conftest$ac_exeext | sed -ne 's,BUILD=\(.*\),\1,p'`"],
    [AC_MSG_ERROR([FFmpeg (libavcodec) required, but cannot compile avcodec.h])],
    [echo $ac_n "cross compiling; assumed OK... $ac_c"
      FFMPEG_LIBS_VERSION=""
      FFMPEG_LIBS_BUILD=""])

CPPFLAGS="$save_CPPFLAGS"

AC_SUBST(FFMPEG_LIBS_VERSION)
AC_SUBST(FFMPEG_LIBS_BUILD)

have_ffmpeg_libs=no
if test x"$enable_ffmpeg_libs_static" = x"no" ; then
  save_LDFLAGS="$LDFLAGS"
  LDFLAGS="$LDFLAGS -L$with_ffmpeg_libs_l"
  AC_CHECK_LIB(avcodec, avcodec_thread_init,
    [FFMPEG_LIBS_LIBS="-L$with_ffmpeg_libs_l -lavcodec $FFMPEG_LIB_EXTRALIBS"],
    [AC_MSG_ERROR([error transcode depends on the FFmpeg (libavcodec) libraries and headers])],
    [$FFMPEG_LIBS_EXTRALIBS])
  LDFLAGS="$save_LDFLAGS"
  have_ffmpeg_libs=yes
else
  if test x$deplibs_check_method != xpass_all ; then
    AC_MSG_ERROR([linking static archives into shared objects not supported on this platform]) 
  fi
  save_LIBS="$LIBS"
  save_CPPFLAGS="CPPFLAGS"
  LIBS="$LIBS $with_ffmpeg_libs_libs_l/libavcodec.a $FFMPEG_LIBS_EXTRALIBS"
  CPPFLAGS="$CPPFLAGS -I$with_ffmpeg_libs_i"
  AC_TRY_LINK([
#include <avcodec.h>
],[
AVCodecContext *ctx = NULL;
avcodec_thread_init(ctx, 0);
],
    [FFMPEG_LIBS_LIBS="$with_ffmpeg_libs_l/libavcodec.a $FFMPEG_LIBS_EXTRALIBS"],
    [AC_MSG_ERROR([cannot link statically with libavcodec])])
  LIBS="$save_LIBS"
  CPPFLAGS="$save_CPPFLAGS"
  have_ffmpeg_libs=yes
fi
if test x$have_ffmpeg_libs = xyes ; then
  ifelse([$1], , :, [$1])
else
  ifelse([$2], , :, [$2])
fi

AC_SUBST(FFMPEG_LIBS_CFLAGS)
AC_SUBST(FFMPEG_LIBS_LIBS)
AC_SUBST(FFMPEG_LIBS_EXTRALIBS)
])


# Configure paths for FreeType2
# Marcelo Magallon 2001-10-26, based on gtk.m4 by Owen Taylor
dnl TC_PATH_FT2([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for FreeType2, and define FT2_CFLAGS and FT2_LIBS
dnl
AC_DEFUN([TC_PATH_FT2],
[
AC_MSG_CHECKING([whether freetype2 support is requested])
AC_ARG_ENABLE(freetype2,
  AC_HELP_STRING([--enable-freetype2],
    [enable freetype2 support (yes)]),
  [case "${enableval}" in
    yes) ;;
    no)  ;;
    *) AC_MSG_ERROR(bad value ${enableval} for --enable-freetype2) ;;
  esac],
  [enable_freetype2=yes])
AC_MSG_RESULT($enable_freetype2)

AC_ARG_WITH(ft-prefix,
  AC_HELP_STRING([--with-ft-prefix=PFX],
    [Prefix where FreeType is installed (optional)]),
  ft_config_prefix="$withval", ft_config_prefix="")

AC_ARG_WITH(ft-exec-prefix,
  AC_HELP_STRING([--with-ft-exec-prefix=PFX],
    [Exec prefix where FreeType is installed (optional)]),
  ft_config_exec_prefix="$withval", ft_config_exec_prefix="")

AC_ARG_ENABLE(fttest,
  AC_HELP_STRING([--disable-fttest],
    [Do not try to compile and run a test FreeType program]),
  [],
  enable_fttest=yes)

have_freetype2=no

if test x$enable_freetype2 = xyes; then

  AC_PATH_PROG(FT2_CONFIG, freetype-config, no)
  if test x$ft_config_exec_prefix != x ; then
    ft_config_args="$ft_config_args --exec-prefix=$ft_config_exec_prefix"
    if test x${FT2_CONFIG} = xno ; then
      FT2_CONFIG=$ft_config_exec_prefix/bin/freetype-config
    fi
  fi
  if test x$ft_config_prefix != x ; then
    ft_config_args="$ft_config_args --prefix=$ft_config_prefix"
    if test x${FT2_CONFIG} = xno ; then
      FT2_CONFIG=$ft_config_prefix/bin/freetype-config
    fi
  fi

  min_ft_version=ifelse([$1], ,6.1.0,$1)
  AC_MSG_CHECKING(for FreeType - version >= $min_ft_version)
  if test x"$FT2_CONFIG" != x"no" ; then
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
      if test x$ft_config_is_lt != xyes ; then
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
],
          [have_freetype2=yes],
          [],
          [echo $ac_n "cross compiling; assumed OK... $ac_c"
            have_fretype2=yes])
        CFLAGS="$ac_save_CFLAGS"
        LIBS="$ac_save_LIBS"
      fi             # test $ft_config_version -lt $ft_min_version
    else               # test x$enable_fttest = xyes
      have_freetype2=yes
    fi
  fi                 # test "$FT2_CONFIG" = "no"
  if test x$have_freetype2 = xyes ; then
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
    AC_MSG_ERROR([freetype2 support is requested, but not found])
    ifelse([$3], , :, [$3])
  fi
else
  FT2_CFLAGS=""
  FT2_LIBS=""
  ifelse([$3], , :, [$3])
fi
AC_SUBST(FT2_CFLAGS)
AC_SUBST(FT2_LIBS)
])


dnl TC_CHECK_V4L([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for video4linux headers, and define HAVE_STRUCT_V4L2_BUFFER
dnl
AC_DEFUN([TC_CHECK_V4L],
[
AC_MSG_CHECKING([whether v4l support is requested])
AC_ARG_ENABLE(v4l,
  AC_HELP_STRING([--enable-v4l],
    [enable v4l/v4l2 support (no)]), 
  [case "${enableval}" in
    yes) ;;
    no)  ;;
    *) AC_MSG_ERROR(bad value ${enableval} for --enable-v4l) ;;
  esac],
  [enable_v4l=no])
AC_MSG_RESULT($enable_v4l)

have_v4l=no
if test x$enable_v4l = xyes ; then
  AC_CHECK_HEADERS([linux/videodev.h], [v4l=yes], [v4l=no])
  AC_CHECK_HEADERS([linux/videodev2.h], [v4l2=yes], [v4l2=no],
    [#include <linux/types.h>])

  if test x$v4l2 = xyes; then
    AC_MSG_CHECKING([for struct v4l2_buffer in videodev2.h])
    dnl (includes, function-body, [action-if-found], [action-if-not-found])
    AC_TRY_COMPILE([
#include <linux/types.h>
#include <linux/videodev2.h>
],   [
struct v4l2_buffer buf;
buffer.memory = V4L2_MEMORY_MMAP
],    [AC_DEFINE([HAVE_STRUCT_V4L2_BUFFER], 1,
        [define if your videodev2 header has struct v4l2_buffer])
        AC_MSG_RESULT([yes])],
      [v4l2=no AC_MSG_RESULT([no])])
  fi

  if test x$v4l = xyes -o x$v4l2 = xyes ; then
    have_v4l=yes
  else
    AC_MSG_ERROR([v4l is requested, but cannot find headers])
  fi
fi
])


dnl TC_PATH_AVIFILE([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for avifile, and define AVIFILE_CFLAGS, AVIFILE_LIBS
dnl and HAVE_AVIFILE_INCLUDES
dnl
AC_DEFUN([TC_PATH_AVIFILE],
[
AC_MSG_CHECKING([whether avifile support is requested])
AC_ARG_ENABLE(avifile,
  AC_HELP_STRING([--enable-avifile],
    [build avifile dependent modules (disabled)]),
  [case "${enableval}" in
    yes) ;;
    no)  ;;
    *) AC_MSG_ERROR(bad value ${enableval} for --enable-avifile) ;;
  esac],
  enable_avifile=no)
AC_MSG_RESULT($enable_avifile)
have_avifile=no
if test x"$enable_avifile" = xyes; then
  PKG_CHECK_MODULES(AVIFILE, avifile >= 0.7.25)
  have_avifile=yes
  ifelse([$1], , :, [$1])
else
  AVIFILE_CFLAGS=""
  AVIFILE_LIBS=""
  AC_SUBST(AVIFILE_CFLAGS)
  AC_SUBST(AVIFILE_LIBS)
  ifelse([$2], , :, [$2])
fi
if test x$AVIFILE_CFLAGS != x ; then
  if echo $AVIFILE_CFLAGS | grep 'avifile-0\.7$' 2>&1 > /dev/null  ; then
    have_avifile_includes=7
  else
    have_avifile_includes=0
  fi
  AC_DEFINE_UNQUOTED([HAVE_AVIFILE_INCLUDES], $have_avifile_includes,
    [avifile includes direstory specifics])
  fi
])


dnl TC_PATH_LAME([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for LAME, and define LAME_CFLAGS and LAME_LIBS
dnl
AC_DEFUN([TC_PATH_LAME],
[
AC_MSG_CHECKING([whether lame support is requested])
AC_ARG_ENABLE(lame,
  AC_HELP_STRING([--enable-lame],
    [use installed lame library (enabled)]),
  [case "${enableval}" in
    yes) ;;
    no)  ;;
    *) AC_MSG_ERROR(bad value ${enableval} for --enable-lame) ;;
  esac],
  enable_lame=yes)
AC_MSG_RESULT($enable_lame)

AC_ARG_WITH(lame-includes,
  AC_HELP_STRING([--with-lame-includes=PFX],
    [prefix where local lame includes are installed (optional)]),
  lame_includes="$withval", lame_includes="")

AC_ARG_WITH(lame-libs,
  AC_HELP_STRING([--with-lame-libs=PFX],
    [prefix where local lame libs are installed (optional)]),
  lame_libs="$withval", lame_libs="")

LAME_EXTRA_LIBS="-lm"
have_lame=no
lame_version=0

if test x$enable_lame = xyes ; then

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

  lame_inc=no
  save_CPPFLAGS="$CPPFLAGS"
  CPPFLAGS="$CPPFLAGS -I$with_lame_i"
  AC_CHECK_HEADER([lame/lame.h],
    [AC_DEFINE([HAVE_LAME_INC], [1],
      [Have Lame includes in separate path]) lame_inc=yes])
  if test x$lame_inc = xno ; then
    AC_CHECK_HEADER([lame.h], lame_inc=yes)
  fi
  if test x$lame_inc = xno ; then
    AC_MSG_ERROR([lame requested, but cannot compile lame.h])
  else
    LAME_CFLAGS="-I$with_lame_i"
  fi
  CPPFLAGS="$save_CPPFLAGS"

  save_LDFLAGS="$LDFLAGS"
  LDFLAGS="$LDFLAGS -L$with_lame_l"
  AC_CHECK_LIB(mp3lame, lame_init,
    [LAME_LIBS="-L$with_lame_l -lmp3lame $LAME_EXTRA_LIBS"],
    [AC_MSG_ERROR([lame requested, but cannot link against libmp3lame])],
    [$LAME_EXTRA_LIBS])
  LDFLAGS="$save_LDFLAGS"

  AC_MSG_CHECKING([lame version])
  ac_save_CFLAGS="$CFLAGS"
  ac_save_LIBS="$LIBS"
  CFLAGS="$CFLAGS $LAME_CFLAGS"
  LIBS="$LIBS $LAME_LIBS"
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
],
    [lame_version="`./conftest$ac_exeext`"],
    [AC_MSG_ERROR([lame requested, but cannot compile and run a test program])],,
    [echo $ac_n "cross compiling; assumed OK... $ac_c"])
  CFLAGS="$ac_save_CFLAGS"
  LIBS="$ac_save_LIBS"

  if test $lame_version -lt 389 ; then
    LAME_LIBS=""
    LAME_CFLAGS=""
    ifelse([$2], , :, [$2])
    AC_MSG_ERROR([lame requested, but lame version < 3.89])
  else
    have_lame=yes
    ifelse([$1], , :, [$1])
  fi
else
  LAME_LIBS=""
  LAME_CFLAGS=""
  ifelse([$2], , :, [$2])
fi
AC_SUBST(LAME_CFLAGS)
AC_SUBST(LAME_LIBS)
])


dnl TC_PATH_OGG([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for ogg, and define OGG_CFLAGS and OGG_LIBS
dnl
AC_DEFUN([TC_PATH_OGG],
[
AC_MSG_CHECKING([whether ogg support is requested])
AC_ARG_ENABLE(ogg,
  AC_HELP_STRING([--enable-ogg],
    [build ogg dependent modules (enabled)]),
  [case "${enableval}" in
    yes) ;;
    no)  ;;
    *) AC_MSG_ERROR(bad value ${enableval} for --enable-ogg) ;;
  esac],
  enable_ogg=yes)
AC_MSG_RESULT($enable_ogg)
have_ogg=no
if test x$enable_ogg = xyes; then
  PKG_CHECK_MODULES(OGG, ogg)
  have_ogg=yes
  ifelse([$1], , :, [$1])
else
  OGG_CFLAGS=""
  OGG_LIBS=""
  AC_SUBST(OGG_CFLAGS)
  AC_SUBST(OGG_LIBS)
  ifelse([$2], , :, [$2])
fi
])


dnl TC_PATH_VORBIS([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for vorbis, and define VORBIS_CFLAGS and VORBIS_LIBS
dnl
AC_DEFUN([TC_PATH_VORBIS],
[
AC_MSG_CHECKING([whether vorbis support is requested])
AC_ARG_ENABLE(vorbis,
  AC_HELP_STRING([--enable-vorbis],
    [build vorbis dependent modules (yes)]),
  [case "${enableval}" in
    yes) ;;
    no)  ;;
    *) AC_MSG_ERROR(bad value ${enableval} for --enable-vorbis) ;;
  esac],
  enable_vorbis=yes)
AC_MSG_RESULT($enable_vorbis)
have_vorbis=no
if test x"$enable_vorbis" = xyes; then
  PKG_CHECK_MODULES(VORBIS, vorbis)
  have_vorbis=yes
  ifelse([$1], , :, [$1])
else
  VORBIS_CFLAGS=""
  VORBIS_LIBS=""
  AC_SUBST(VORBIS_CFLAGS)
  AC_SUBST(VORBIS_LIBS)
  ifelse([$2], , :, [$2])
fi
])


dnl TC_PATH_THEORA([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for libtheora, and define THEORA_CFLAGS and THEORA_LIBS
dnl
AC_DEFUN([TC_PATH_THEORA],
[
AC_MSG_CHECKING([whether theora support is requested])
AC_ARG_ENABLE(theora,
  AC_HELP_STRING([--enable-theora],
    [Compile in libtheora support (no)]),
  [case "${enableval}" in
    yes) ;;
    no)  ;;
    *) AC_MSG_ERROR(bad value ${enableval} for --enable-theora) ;;
  esac],
  enable_theora=no)
AC_MSG_RESULT($enable_theora)

AC_ARG_WITH(theora-includes,
  AC_HELP_STRING([--with-theora-includes=PFX],
    [prefix where local theora includes are installed (optional)]),
  theora_includes="$withval", theora_includes="")

AC_ARG_WITH(theora-libs,
  AC_HELP_STRING([--with-theora-libs=PFX],
    [prefix where local theora libs are installed (optional)]),
  theora_libs="$withval", theora_libs="")

THEORA_EXTRA_LIBS="-logg -lm"
have_theora=no

if test x$enable_theora = xyes ; then

  if test x$theora_includes != x ; then
    with_theora_i="$theora_includes/include"
  else
    with_theora_i="/usr/include"
  fi
  if test x$theora_libs != x ; then
    with_theora_l="$theora_libs/lib"
  else
    with_theora_l="/usr${deflib}"
  fi

  save_LDFLAGS="$LDFLAGS"
  LDFLAGS="$LDFLAGS -L$with_theora_l"
  AC_CHECK_LIB(theora, theora_info_init,
    [THEORA_LIBS="-L$with_theora_l -ltheora $THEORA_EXTRA_LIBS"],
    [AC_MSG_ERROR([theora requested, but can't link against libtheora])],
    [$THEORA_EXTRA_LIBS])
  LDFLAGS="$save_LDFLAGS"

  save_CPPFLAGS="$CPPFLAGS"
  CPPFLAGS="$CPPFLAGS -I$with_theora_i"
  AC_CHECK_HEADER([theora/theora.h],
    [THEORA_CFLAGS="-I$with_theora_i"],
    [AC_MSG_ERROR([theora requested, but can't compile theora/theora.h])])
  CPPFLAGS="$save_CPPFLAGS"

  have_theora=yes
  ifelse([$1], , :, [$1])

else
  THEORA_LIBS=""
  THEORA_CFLAGS=""
  ifelse([$2], , :, [$2])
fi
AC_SUBST(THEORA_CFLAGS)
AC_SUBST(THEORA_LIBS)
])


dnl TC_PATH_LIBDVDREAD([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for LIBDVDREAD, and define LIBDVDREAD_CFLAGS and LIBDVDREAD_LIBS
dnl
AC_DEFUN([TC_PATH_LIBDVDREAD],
[
AC_MSG_CHECKING([whether libdvdread support is requested])
AC_ARG_ENABLE(dvdread,
  AC_HELP_STRING([--enable-dvdread],
    [use installed libdvdread library (yes)]),
  [case "${enableval}" in
    yes) ;;
    no)  ;;
    *) AC_MSG_ERROR(bad value ${enableval} for --enable-dvdread) ;;
  esac],
  enable_dvdread=yes)
AC_MSG_RESULT($enable_dvdread)

AC_ARG_WITH(dvdread-includes,
  AC_HELP_STRING([--with-dvdread-includes=PFX],
    [prefix where local dvdread includes are installed (optional)]),
  dvdread_includes="$withval", dvdread_includes="")

AC_ARG_WITH(dvdread-libs,
  AC_HELP_STRING([--with-dvdread-libs=PFX],
    [prefix where local dvdread lib is installed (optional)]),
  dvdread_libs="$withval", dvdread_libs="")

DVDREAD_EXTRA_LIBS="-lm"
have_dvdread=no

if test x$enable_dvdread = "x"yes ; then

  if test x$dvdread_includes != x ; then
    with_dvdread_i="$dvdread_includes/include"
  else
    with_dvdread_i="/usr/include"
  fi
  if test x$dvdread_libs != x ; then
    with_dvdread_l="$dvdread_libs/lib"
  else
    with_dvdread_l="/usr${deflib}"
  fi

  save_LDFLAGS="$LDFLAGS"
  LDFLAGS="$LDFLAGS -L$with_dvdread_l"
  AC_CHECK_LIB(dvdread, DVDOpen,
    [DVDREAD_LIBS="-L$with_dvdread_l -ldvdread $DVDREAD_EXTRA_LIBS"],
    [AC_MSG_ERROR([libdvdread requested, but cannot link against libdvdread])],
    [$DVDREAD_EXTRA_LIBS])
  LDFLAGS="$save_LDFLAGS"

  dvdread_inc=no
  save_CPPFLAGS="$CPPFLAGS"
  CPPFLAGS="$CPPFLAGS -I$with_dvdread_i"
  AC_CHECK_HEADER([dvdread/dvd_reader.h],
    [AC_DEFINE([HAVE_LIBDVDREAD_INC], [1],
      [Have Libdvdread includes in separate path])
    dvdread_inc=yes])
  if test x$dvdread_inc = xno ; then
    AC_CHECK_HEADER([dvd_reader.h],
      [dvdread_inc=yes])
  fi
  CPPFLAGS="$save_CPPFLAGS"

  if test x$dvdread_inc = xno ; then
    AC_MSG_ERROR([libdvdread requested, but cannot compile dvd_reader.h])
  else
    DVDREAD_CFLAGS="-I$with_dvdread_i"
  fi

  have_dvdread=yes
  ifelse([$1], , :, [$1])

else
  DVDREAD_LIBS=""
  DVDREAD_CFLAGS=""
  ifelse([$2], , :, [$2])
fi
AC_SUBST(DVDREAD_CFLAGS)
AC_SUBST(DVDREAD_LIBS)
])


dnl this is never used
dnl TC_PATH_LIBXVID([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for LIBXVID, and define LIBXVID_CFLAGS and LIBXVID_LIBS
dnl
AC_DEFUN([TC_PATH_LIBXVID],
[
AC_MSG_CHECKING([whether xvidcore support is requested])
AC_ARG_ENABLE(xvidcore,
  AC_HELP_STRING([--enable-xvidcore],
    [use installed LIBXVID library (no)]),
  [case "${enableval}" in
    yes) ;;
    no)  ;;
    *) AC_MSG_ERROR(bad value ${enableval} for --enable-xvidcore) ;;
  esac],
  enable_xvidcore=no)
AC_MSG_RESULT($enable_xvidcore)

AC_ARG_WITH(xvidcore-includes,
  AC_HELP_STRING([--with-xvidcore-includes=PFX],
    [prefix where local xvidcore includes are installed (optional)]),
  xvidcore_includes="$withval", xvidcore_includes="")

AC_ARG_WITH(xvidcore-libs,
  AC_HELP_STRING([--with-xvidcore-libs=PFX],
    [prefix where local xvidcore lib is installed (optional)]),
  xvidcore_libs="$withval", xvidcore_libs="")

XVIDCORE_EXTRA_LIBS="-lm"
have_xvidcore=no

if test x$enable_xvidcore = xyes ; then

  if test x$xvidcore_includes != x ; then
    with_xvidcore_i="$xvidcore_includes/include"
  else
    with_xvidcore_i="/usr/include"
  fi
  if test x$xvidcore_libs != x ; then
    with_xvidcore_l="$xvidcore_libs/lib"
  else
    with_xvidcore_l="/usr${deflib}"
  fi

  save_LDFLAGS="$LDFLAGS"
  LDFLAGS="$LDFLAGS -L$with_xvidcore_l"
  AC_CHECK_LIB(xvidcore, xvid_encore,
    [XVIDCORE_LIBS="-L$with_xvidcore_l -lxvidcore $XVIDCORE_EXTRA_LIBS"],
    [AC_MSG_ERROR([xvidcore requested, but could not link with libxvidcore])],
    [$XVIDCORE_EXTRA_LIBS])
  LDFLAGS="$save_LDFLAGS"

  save_CPPFLAGS="$CPPFLAGS"
  CPPFLAGS="$CPPFLAGS -I$with_xvidcore_i"
  AC_CHECK_HEADER([xvid4.h],
    [XVIDCORE_CFLAGS="-I$with_xvidcore_i"],
    [AC_MSG_ERROR([xvidcore requested, but could not compile xvid4.h])])
  CPPFLAGS="$save_CPPFLAGS"

  have_xvidcore=yes
  ifelse([$1], , :, [$1])

else
  XVIDCORE_LIBS=""
  XVIDCORE_CFLAGS=""
  ifelse([$2], , :, [$2])
fi
AC_SUBST(XVIDCORE_CFLAGS)
AC_SUBST(XVIDCORE_LIBS)
])


dnl TC_PATH_LIBMPEG3([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for libmpeg3, and define LIBMPEG3_LIBS and LIBMPEG3_CFLAGS
dnl
AC_DEFUN([TC_PATH_LIBMPEG3],
[
AC_MSG_CHECKING([whether libmpeg3 support is requested])
AC_ARG_ENABLE(libmpeg3,
  AC_HELP_STRING([--enable-libmpeg3],
    [build libmpeg3 dependent module (no)]),
  [case "${enableval}" in
    yes) ;;
    no)  ;;
    *) AC_MSG_ERROR(bad value ${enableval} for --enable-libmpeg3) ;;
  esac],
  enable_libmpeg3=no)
AC_MSG_RESULT($enable_libmpeg3)

AC_ARG_WITH(libmpeg3-includes,
  AC_HELP_STRING([--with-libmpeg3-includes=PFX],
    [prefix where local libmpeg3 includes are installed (optional)]),
  libmpeg3_includes="$withval", libmpeg3_includes="")

AC_ARG_WITH(libmpeg3-libs,
  AC_HELP_STRING([--with-libmpeg3-libs=PFX],
    [prefix where local libmpeg3 libs are installed (optional)]),
  libmpeg3_libs="$withval", libmpeg3_libs="")

LIBMPEG3_EXTRA_LIBS="-lm -lpthread $A52_LIBS"
have_libmpeg3=no

if test x$enable_libmpeg3 = "x"yes ; then

  if test x$libmpeg3_includes != x ; then
    with_libmpeg3_i="$libmpeg3_includes/include"
  else
    with_libmpeg3_i="/usr/include"
  fi
  if test x$libmpeg3_libs != x ; then
    with_libmpeg3_l="$libmpeg3_libs/lib"
  else
    with_libmpeg3_l="/usr${deflib}"
  fi

  libmpeg3_inc=no
  save_CPPFLAGS="$CPPFLAGS"
  CPPFLAGS="$CPPFLAGS -I$with_libmpeg3_i"
  AC_CHECK_HEADER([libmpeg3/libmpeg3.h],
    [with_libmpeg3_i="$with_libmpeg3_i/libmpeg3"
      libmpeg3_inc=yes])
  if test x$libmpeg3_inc = xno ; then
    AC_CHECK_HEADER([mpeg3/libmpeg3.h],
      [with_libmpeg3_i="$with_libmpeg3_i/mpeg3"
        libmpeg3_inc=yes])
  fi
  if test x$libmpeg3_inc = xno ; then
    AC_CHECK_HEADER([libmpeg3.h],
	  [with_libmpeg3_i="$with_libmpeg3_i"
	     libmpeg3_inc=yes])
  fi
  if test x$libmpeg3_inc = xno ; then
    AC_MSG_ERROR([libmpeg3 requested, but cannot compile libmpeg3.h])
  fi
  CPPFLAGS="$save_CPPFLAGS"

  AC_CHECK_LIB(mpeg3, mpeg3_open,
    [LIBMPEG3_CFLAGS="-I$with_libmpeg3_i"
      LIBMPEG3_LIBS="-L$with_libmpeg3_l -lmpeg3 ${LIBMPEG3_EXTRA_LIBS}"],
    [AC_MSG_ERROR([libmpeg3 requested, but cannot link against libmpeg3])],
    [-L$with_libmpeg3_l -lmpeg3 ${LIBMPEG3_EXTRA_LIBS}])

  have_libmpeg3=yes
  ifelse([$1], , :, [$1])

else
  LIBMPEG3_CFLAGS=""
  LIBMPEG3_LIBS=""
  ifelse([$2], , :, [$2])
fi
AC_SUBST(LIBMPEG3_LIBS)
AC_SUBST(LIBMPEG3_CFLAGS)
])


dnl this isn't used
dnl AM_PATH_POSTPROC([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for libpostproc, and define POSTPROC_LIBS and POSTPROC_CFLAGS
dnl
AC_DEFUN([AM_PATH_POSTPROC],
[
AC_MSG_CHECKING([whether libpostproc support is requested])
AC_ARG_ENABLE(libpostproc,
  AC_HELP_STRING([--enable-libpostproc],
    [build libpostproc dependent module (no)]),
  [case "${enableval}" in
    yes) ;;
    no)  ;;
    *) AC_MSG_ERROR(bad value ${enableval} for --enable-libpostproc) ;;
  esac],
  enable_libpostproc=no)
AC_MSG_RESULT($enable_libpostproc)

AC_ARG_WITH(libpostproc-builddir,
  AC_HELP_STRING([--with-libpostproc-builddir=PFX],
    [path to MPlayer builddir (optional)]),
  libpostproc_builddir="$withval", libpostproc_builddir="")

POSTPROC_EXTRA_LIBS="-lm"
have_libpostproc=no

if test x$enable_libpostproc = xyes ; then
  if test x$libpostproc_builddir != "x" ; then
    with_libpostproc_p="$libpostproc_builddir"
    save_LDFLAGS="$LDFLAGS"
    LDFLAGS="$LDFLAGS -L$with_libpostproc_p/lib"
    AC_CHECK_LIB(postproc, pp_postprocess,
      [LIBPOSTPROC_CFLAGS="-I$with_libpostproc_p/include"
        LIBPOSTPROC_LIBS="-L$with_libpostproc_p/lib -lpostproc $POSTPROC_EXTRA_LIBS"
        have_libpostproc=yes],
      [],
      [$POSTPROC_EXTRA_LIBS])
    LDFLAGS="$save_LDFLAGS"
  fi
  if test x$have_libpostproc != "xyes" ; then
    AC_CHECK_LIB(postproc, pp_postprocess,
      [LIBPOSTPROC_CFLAGS=""
        LIBPOSTPROC_LIBS="-lpostproc $POSTPROC_EXTRA_LIBS"
        have_libpostproc=yes])
  fi
  if test x$have_libpostproc = xyes ; then
    ifelse([$1], , :, [$1])
  else
    AC_MSG_ERROR([libpostproc requested, but cannot link with libpostproc])
  fi
else
  LIBPOSTPROC_LIBS=""
  LIBPOSTPROC_CFLAGS=""
  ifelse([$2], , :, [$2])
fi
AC_SUBST(LIBPOSTPROC_LIBS)
AC_SUBST(LIBPOSTPROC_CFLAGS)
])


dnl TC_PATH_LVE([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for liblve and set LVE_CFLAGS and LVE_LIBS
dnl
AC_DEFUN([TC_PATH_LVE],
[
AC_MSG_CHECKING([whether liblve support is requested])
AC_ARG_ENABLE(liblve,
  AC_HELP_STRING([--enable-liblve],
    [build liblve dependent module (no)]),
  [case "${enableval}" in
    yes) ;;
    no)  ;;
    *) AC_MSG_ERROR(bad value ${enableval} for --enable-liblve) ;;
  esac],
  enable_liblve=no)
AC_MSG_RESULT($enable_liblve)

AC_ARG_WITH(liblve-builddir,
  AC_HELP_STRING([--with-liblve-builddir=PFX],
    [path to lve builddir (optional)]),
  liblve_builddir="$withval", liblve_builddir="")

LVE_EXTRA_LIBS="-lstdc++ -lm"
have_liblve=no

if test x$enable_liblve = xyes ; then
  if test x$liblve_builddir != x ; then
    with_liblve_p="$liblve_builddir"
    save_LDFLAGS="$LDFLAGS"
    LDFLAGS="$LDFLAGS -L$with_liblve_p/lve"
    AC_CHECK_LIB(lve, lr_init,
      [LVE_CFLAGS="-I$with_liblve_p/lve"
        LVE_LIBS="-L$with_liblve_p/lve -llve -L$with_liblve_p/ffmpeg -lavcodec -L$with_liblve_p/libmpeg2/.libs -lmpeg2 -L$with_liblve_p/liba52 -la52 $LVE_EXTRA_LIBS"
        have_liblve=yes],
      [],
      [-L$with_liblve_p/ffmpeg -lavcodec -L$with_liblve_p/libmpeg2/.libs -lmpeg2 -L$with_liblve_p/liba52 -la52 $LVE_EXTRA_LIBS])
    LDFLAGS="$save_LDFLAGS"
  fi
  if test x$have_liblve != xyes ; then
    AC_CHECK_LIB(lve, lr_init,
      [LVE_CFLAGS=""
        LVE_LIBS="-llve $LVE_EXTRA_LIBS" 
        have_liblve=yes],
      [],
      [$LVE_EXTRA_LIBS])
  fi
  if test x$have_liblve = xyes ; then
    ifelse([$1], , :, [$1])
  else
    AC_MSG_ERROR([liblve support is requested, but cannot link against liblve])
  fi
else
  LVE_LIBS=""
  LVE_CFLAGS=""
  ifelse([$2], , :, [$2])
fi
AC_SUBST(LVE_LIBS)
AC_SUBST(LVE_CFLAGS)
])


dnl TC_PATH_PVM3([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for libpvm3 and set PVM3_CFLAGS PVM3_LIB and PVM3_PVMGS
dnl
AC_DEFUN([TC_PATH_PVM3],
[
AC_MSG_CHECKING([whether pvm3 support is requested])
AC_ARG_ENABLE(pvm3,
  AC_HELP_STRING([--enable-pvm3],
    [Enable pvm3 code (no)]),
  [case "${enableval}" in
    yes) ;;
    no)  ;;
    *) AC_MSG_ERROR(bad value ${enableval} for --enable-pvm3) ;;
  esac],
  enable_pvm3=no)
AC_MSG_RESULT($enable_pvm3)

AC_ARG_WITH(pvm3-lib,
  AC_HELP_STRING([--with-pvm3-lib=PFX],
    [prefix where local pvm3 libraries are installed]),
  [pvm3_lib="$withval" pvm3_lib="/usr${deflib}"])

AC_ARG_WITH(pvm3-include,
  AC_HELP_STRING([--with-pvm3-include=PFX],
    [prefix where local pvm3 includes are installed]),
  [pvm3_include="$withval" pvm3_include="/usr/include"])

have_pvm3=no

if test x$enable_pvm3 = xyes ; then
  if test x$pvm3_lib != x ; then
    AC_CHECK_FILE($pvm3_include/pvm3.h, [pvm3_inc=yes])
    AC_CHECK_FILE($pvm3_lib/libpvm3.a, [pvm3_libs=yes])
    AC_CHECK_FILE($pvm3_lib/libgpvm3.a, [pvm3_libs=yes])
    AC_CHECK_FILE($pvm3_lib/pvmgs, [pvm3_libs=yes])
    if test x$pvm3_inc != "x" ; then
      if test x$pvm3_libs != "x" ; then
        AC_MSG_CHECKING([for pvm3 version >= 3.4])
        PVM3_CFLAGS="-I$pvm3_include"
        PVM3_LIB="-L$pvm3_lib -lpvm3 -lgpvm3" 
        PVM3_PVMGS="$pvm3_lib/pvmgs"
        CFLAGS_OLD="$CFLAGS"
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
],
          [have_pvm3=yes],
          [],
          [echo $ac_n "cross compiling; assumed OK... $ac_c"
            have_pvm3=yes])
        CFLAGS="$CFLAGS_OLD"
      fi
    fi
  fi
  if test x$have_pvm3 = xyes ; then
    AC_MSG_RESULT(yes)
    ifelse([$1], , :, [$1])
  else
    AC_MSG_RESULT(no)
    AC_MSG_ERROR([pvm3 is requested, but cannot link with libpvm3])
    ifelse([$2], , :, [$2])
  fi
else
  PVM3_CFLAGS=""
  PVM3_LIB=""
  PVM3_PVMGS=""
  ifelse([$2], , :, [$2])
fi
AC_SUBST(PVM3_CFLAGS)
AC_SUBST(PVM3_LIB)
AC_SUBST(PVM3_PVMGS)
])


dnl TC_PATH_DV([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for libdv, and define DV_CFLAGS and DV_LIBS
dnl
AC_DEFUN([TC_PATH_DV],
[
AC_MSG_CHECKING([whether libdv support is requested])
AC_ARG_ENABLE(dv,
  AC_HELP_STRING([--enable-dv],
    [build libdv dependent modules (no)]),
  [case "${enableval}" in
    yes) ;;
    no)  ;;
    *) AC_MSG_ERROR(bad value ${enableval} for --enable-dv) ;;
  esac],
  enable_dv=no)
AC_MSG_RESULT($enable_dv)

AC_ARG_WITH(dv-includes,
  AC_HELP_STRING([--with-dv-includes=PFX],
    [prefix where local libdv includes are installed (optional)]),
  dv_includes="$withval", dv_includes="")

AC_ARG_WITH(dv-libs,
  AC_HELP_STRING([--with-dv-libs=PFX],
    [prefix where local libdv libs are installed (optional)]),
  dv_libs="$withval", dv_libs="")

DV_EXTRA_LIBS="-lm"
have_dv=no

if test x$enable_dv = xyes ; then

  if test -z "$PKG_CONFIG" ; then
    AC_PATH_PROG(PKG_CONFIG, pkg-config, no)
  fi

  if test "$PKG_CONFIG" != "no" ; then
    if $PKG_CONFIG libdv --exists ; then
      AC_MSG_CHECKING([DV_CFLAGS])
      DV_CFLAGS="`$PKG_CONFIG libdv --cflags`"
      AC_MSG_RESULT($DV_CFLAGS)
      AC_MSG_CHECKING([DV_LIBS])
      DV_LIBS="`$PKG_CONFIG libdv --libs`"
      AC_MSG_RESULT($DV_LIBS)
      if $PKG_CONFIG libdv --atleast-version 0.95 ; then
        AC_DEFINE([LIBDV_095], [1], [Have libdv 0.95 or newer])
      fi
      if $PKG_CONFIG libdv --atleast-version 0.99 ; then
        AC_DEFINE([LIBDV_099], [1], [Have libdv 0.99 or newer])
      fi
      if $PKG_CONFIG libdv --atleast-version 0.103 ; then
        AC_DEFINE([LIBDV_0103], [1], [Have libdv 0.103 or newer])
      fi
      have_dv=yes
    fi
  fi

  if test x$have_dv != xyes ; then

    if test x$dv_includes != x ; then
      with_dv_i="$dv_includes/include"
    else
      with_dv_i="/usr/include"
    fi
    if test x$dv_libs != x ; then
      with_dv_l="$dv_libs/lib"
    else
      with_dv_l="/usr${deflib}"
    fi

    save_LDFLAGS="$LDFLAGS"
    LDFLAGS="$LDFLAGS -L$with_dv_l"
    AC_CHECK_LIB(dv, dv_init,
      [DV_LIBS="-L$with_dv_l -ldv $DV_EXTRA_LIBS"],
      [AC_MSG_ERROR([dv requested, but cannot link against libdv])],
      [$DV_EXTRA_LIBS])
    LDFLAGS="$save_LDFLAGS"

    save_CPPFLAGS="$CPPFLAGS"
    CPPFLAGS="$CPPFLAGS -I$with_dv_i"
    AC_CHECK_HEADER([libdv/dv.h],
      [DV_CFLAGS="-I$with_dv_i"],
      [AC_MSG_ERROR([dv requested, but cannot compile libdv/dv.h])])
    CPPFLAGS="$save_CPPFLAGS"

    have_dv=yes

    save_LDFLAGS="$LDFLAGS"
    LDFLAGS="$LDFLAGS -L$with_dv_l"
    save_CPPFLAGS="$CPPFLAGS"
    CPPFLAGS="$CPPFLAGS -I$with_dv_i"

    dnl check for version >= 0.95
    AC_CHECK_LIB(dv, dv_encoder_new,
      [AC_DEFINE([LIBDV_095], [1], [Have libdv 0.95 or newer])],
      [], 
      [$DV_EXTRA_LIBS])

    dnl check for version >= 0.99
    AC_CHECK_LIB(dv, calculate_samples,
      [AC_DEFINE([LIBDV_099], [1], [Have libdv 0.99 or newer])],
      [], 
      [$DV_EXTRA_LIBS])

    dnl check for version >= 0.103
    AC_TRY_COMPILE([[#include <libdv/dv_types.h>
        #include <stdio.h>]],
      [[printf("header_size = %d\n", header_size);]],
      [],
      [AC_DEFINE([LIBDV_0103], [1], [Have libdv 0.103 or newer])])

    LDFLAGS="$save_LDFLAGS"
    CPPFLAGS="$save_CPPFLAGS"

  fi
  ifelse([$1], , :, [$1])

else
  DV_LIBS=""
  DV_CFLAGS=""
  ifelse([$2], , :, [$2])
fi
AC_SUBST(DV_LIBS)
AC_SUBST(DV_CFLAGS)
])


dnl TC_PATH_QT([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for libquicktime and set QT_CFLAGS and QT_LIBS
dnl
AC_DEFUN([TC_PATH_QT],
[
AC_MSG_CHECKING([whether quicktime support is requested])
AC_ARG_ENABLE(libquicktime,
  AC_HELP_STRING([--enable-libquicktime],
    [build libquicktime dependent module (no)]),
  [case "${enableval}" in
    yes) ;;
    no)  ;;
    *) AC_MSG_ERROR(bad value ${enableval} for --enable-libquicktime) ;;
  esac],
  enable_libquicktime=no)
AC_MSG_RESULT($enable_libquicktime)

AC_ARG_WITH(libquicktime-includes,
  AC_HELP_STRING([--with-libquicktime-includes=PFX],
    [prefix where local libquicktime includes are installed (optional)]),
  libquicktime_includes="$withval", libquicktime_includes="")

AC_ARG_WITH(libquicktime-libs,
  AC_HELP_STRING([--with-libquicktime-libs=PFX],
    [prefix where local libquicktime libs are installed (optional)]),
  libquicktime_libs="$withval", libquicktime_libs="")

QT_EXTRA_LIBS="-lpng -lz $PTHREAD_LIBS -lm $DV_LIBS"
have_libquicktime=no

if test x$enable_libquicktime = "x"yes ; then
  AC_PATH_PROG(LQT_CONFIG, lqt-config, no)
  if test x$LQT_CONFIG != xno ; then
    AC_MSG_CHECKING([QT_CFLAGS])
    QT_CFLAGS="`$LQT_CONFIG --cflags`"
    AC_MSG_RESULT($QT_CFLAGS)
    AC_MSG_CHECKING([QT_LIBS])
    QT_LIBS="`$LQT_CONFIG --libs`"
    AC_MSG_RESULT($QT_LIBS)
    have_libquicktime=yes
  else
    if test x$libquicktime_includes != "x" ; then
      with_libquicktime_i="$libquicktime_includes/include/quicktime"
    else
      with_libquicktime_i="/usr/include/quicktime"
    fi
    if test x$libquicktime_libs != x ; then
      with_libquicktime_l="$libquicktime_libs/lib"
    else
      with_libquicktime_l="/usr${deflib}"
    fi
    save_LDFLAGS="$LDFLAGS"
    LDFLAGS="$LDFLAGS -L$with_libquicktime_l"
    AC_CHECK_LIB(quicktime, quicktime_open,
      [QT_CFLAGS="-I$with_libquicktime_i $DV_CFLAGS"
        QT_LIBS="-L$with_libquicktime_l -lquicktime $QT_EXTRA_LIBS"],
      [AC_MSG_ERROR([libquicktime requested, but could not link with libquicktime])],
      [$QT_EXTRA_LIBS])
    LDFLAGS="$save_LDFLAGS"
    have_libquicktime=yes
  fi
  ifelse([$1], , :, [$1])
else
  QT_CFLAGS=""
  QT_LIBS=""
  ifelse([$2], , :, [$2])
fi
AC_SUBST(QT_LIBS)
AC_SUBST(QT_CFLAGS)
])


dnl TC_PATH_LZO([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for liblzo, and define LZO_LIBS, LZO_CFLAGS,
dnl
AC_DEFUN([TC_PATH_LZO],
[
AC_MSG_CHECKING([whether lzo support is requested])
AC_ARG_ENABLE(lzo,
  AC_HELP_STRING([--enable-lzo],
    [build liblzo dependent modules (no)]),
  [case "${enableval}" in
    yes) ;;
    no)  ;;
    *) AC_MSG_ERROR(bad value ${enableval} for --enable-lzo) ;;
  esac],
  enable_lzo=no)
AC_MSG_RESULT($enable_lzo)

AC_ARG_WITH(lzo-includes,
  AC_HELP_STRING([--with-lzo-includes=PFX],
    [prefix where local liblzo includes are installed (optional)]),
  lzo_includes="$withval", lzo_includes="")

AC_ARG_WITH(lzo-libs,
  AC_HELP_STRING([--with-lzo-libs=PFX],
    [prefix where local liblzo libs are installed (optional)]),
  lzo_libs="$withval", lzo_libs="")

have_lzo=no

if test x$enable_lzo = "x"yes ; then

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

  save_CPPFLAGS="$CPPFLAGS"
  CPPFLAGS="$CPPFLAGS -I$with_lzo_i"
  AC_CHECK_HEADER([lzo1x.h],
    [LZO_CFLAGS="-I$with_lzo_i"],
    [AC_MSG_ERROR([lzo requested, but cannot compile lzo1x.h])])
  CPPFLAGS="$save_CPPFLAGS"

  save_LDFLAGS="$LDFLAGS"
  LDFLAGS="$LDFLAGS -L$with_lzo_l"
  AC_CHECK_LIB(lzo, lzo_version,
    [LZO_LIBS="-L$with_lzo_l -llzo"],
    [AC_MSG_ERROR([lzo requested, but cannot link against liblzo])])
  LDFLAGS="$save_LDFLAGS"

  have_lzo=yes
  ifelse([$1], , :, [$1])

else
  LZO_LIBS=""
  LZO_CFLAGS=""
  ifelse([$2], , :, [$2])
fi
AC_SUBST(LZO_LIBS)
AC_SUBST(LZO_CFLAGS)
])


dnl TC_PATH_A52([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for liba52, and define A52_LIBS and A52_CFLAGS
dnl
AC_DEFUN([TC_PATH_A52],
[
AC_MSG_CHECKING([whether liba52 support is requested])
AC_ARG_ENABLE(a52,
  AC_HELP_STRING([--enable-a52],
    [build liba52 decoder module (yes)]),
  [case "${enableval}" in
    yes) ;;
    no)  ;;
    *) AC_MSG_ERROR(bad value ${enableval} for --enable-a52) ;;
  esac],
  enable_a52=yes)
AC_MSG_RESULT($enable_a52)

AC_ARG_WITH(a52-includes,
  AC_HELP_STRING([--with-a52-includes=PFX],
    [prefix where local liba52 includes are installed (optional)]),
  a52_includes="$withval",a52_includes="")

AC_ARG_WITH(a52-libs,
  AC_HELP_STRING([--with-a52-libs=PFX],
    [prefix where local liba52 libs are installed (optional)]),
  a52_libs="$withval", a52_libs="")

A52_EXTRA_LIBS="-lm"
have_a52=no

if test x$enable_a52 = "x"yes ; then

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

  save_LDFLAGS="$LDFLAGS"
  LDFLAGS="$LDFLAGS -L$with_a52_l"
  AC_CHECK_LIB(a52, a52_init,
    [A52_CFLAGS="-I$with_a52_i"
      A52_LIBS="-L$with_a52_l -la52 $A52_EXTRA_LIBS"],
    [AC_MSG_ERROR([liba52 requested, but cannot link against liba52])], 
    [$A52_EXTRA_LIBS])
  LDFLAGS="$save_LDFLAGS"

  have_a52=yes
  ifelse([$1], , :, [$1])

else
  A52_CFLAGS=""
  A52_LIBS=""
  ifelse([$2], , :, [$2])
fi
AC_SUBST(A52_LIBS)
AC_SUBST(A52_CFLAGS)
])


dnl TC_PATH_LIBXML2([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for libxml2, and define LIBXML2_CFLAGS and LIBXML2_LIBS
dnl
AC_DEFUN([TC_PATH_LIBXML2],
[
AC_MSG_CHECKING([whether libxml2 support is requested])
AC_ARG_ENABLE(libxml2,
  AC_HELP_STRING([--enable-libxml2],
    [enable libxml2 support (no)]),
  [case "${enableval}" in
    yes) ;;
    no)  ;;
    *) AC_MSG_ERROR(bad value ${enableval} for --enable-libxml2) ;;
  esac],
  enable_libxml2=no)
AC_MSG_RESULT($enable_libxml2)

have_libxml2=no

if test x$enable_libxml2 = xyes ; then
  AC_CHECK_PROG([have_libxml2_config],[xml2-config],[yes],[no])
  if test x$have_libxml2_config = xyes ; then
    LIBXML2_CFLAGS="`xml2-config --cflags`"
    LIBXML2_LIBS="`xml2-config --libs`"
    have_libxml2=yes
  else
    AC_MSG_ERROR([libxml2 support requested, but cannot find libxml2])
  fi
  ifelse([$1], , :, [$1])
else
  LIBXML2_CFLAGS=""
  LIBXML2_LIBS=""
  ifelse([$2], , :, [$2])
fi
AC_SUBST([LIBXML2_CFLAGS])
AC_SUBST([LIBXML2_LIBS])
])


dnl TC_PATH_IBP([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for ibp libraries, and define IBP_LIBS
dnl
AC_DEFUN([TC_PATH_IBP],
[
AC_MSG_CHECKING([whether ibp and lors support is requested])
AC_ARG_ENABLE(ibp,
  AC_HELP_STRING([--enable-ibp],
    [enable ibp support (no)]),
  [case "${enableval}" in
    yes) ;;
    no)  ;;
    *) AC_MSG_ERROR(bad value ${enableval} for --enable-ibp) ;;
  esac],
  enable_ibp=no)
AC_MSG_RESULT($enable_ibp)

have_ibp=no
if test "x$enable_ibp" = "xyes" ; then
  AC_MSG_CHECKING(for ibp and lors)
  if test x"$have_libxml2" = x"yes" ; then
    OLD_LIBS="$LIBS"
    AC_ARG_WITH(libfdr,
      AC_HELP_STRING([--with-libfdr=DIR],
        [base directory for libfdr]),
      [CPPFLAGS="-I$with_libfdr/include $CPPFLAGS"
        LIBFDR=yes
        IBP_LIBS1="-L$with_libfdr/lib -lfdr $LIBS"],
      [AC_CHECK_LIB(fdr, jval_v,
        [IBP_LIBS1="-lfdr"],
        [AC_MSG_ERROR(unable to locate libfdr)])])

    AC_ARG_WITH(libibp,
      AC_HELP_STRING([--with-libibp=DIR],
        [base directory for libibp]),
      [CPPFLAGS="-I$with_libibp/include $CPPFLAGS"
        LIBIBP=yes
        IBP_LIBS1="-L$with_libibp/lib -libp $PTHREAD_LIBS $IBP_LIBS1"],
      [LIBS="$PTHREAD_LIBS $IBP_LIBS1"
        AC_CHECK_LIB(ibp, IBP_allocate,
          [IBP_LIBS1="-libp $PTHREAD_LIBS $IBP_LIBS1"],
          [AC_MSG_ERROR(unable to locate libibp)])])

    AC_ARG_WITH(libexnode,
      AC_HELP_STRING([--with-libexnode=DIR],
        [base directory for libexnode]),
      [CPPFLAGS="-I$with_libexnode/include/libexnode $CPPFLAGS"
        LIBEXNODE=yes
        IBP_LIBS1="-L$with_libexnode/lib -lexnode $IBP_LIBS1"],
      [LIBS="$IBP_LIBS1"
        AC_CHECK_LIB(exnode, exnodeCreateExnode,
          [IBP_LIBS1="-lexnode $IBP_LIBS1"],
          [AC_MSG_ERROR(unable to locate libexnode)],
          [$IBP_LIBS1])])

    AC_ARG_WITH(liblbone,
      AC_HELP_STRING([--with-liblbone=DIR],
        [base directory for liblbone]),
      [CPPFLAGS="-I$with_liblbone/include $CPPFLAGS"
        LIBLBONE=yes
        IBP_LIBS1="-L$with_liblbone/lib -llbone $IBP_LIBS1"],
      [LIBS="$IBP_LIBS1"
        AC_CHECK_LIB(lbone,lbone_checkDepots,
          [IBP_LIBS1="-llbone $IBP_LIBS1"],
          [AC_MSG_ERROR(unable to locate liblbone)])])

    AC_ARG_WITH(libend2end,
      AC_HELP_STRING([--with-libend2end=DIR],
        [base directory for libend2end]),
      [CPPFLAGS="-I$with_libend2end/include $CPPFLAGS"
        LIBE2E=yes
        IBP_LIBS1="-L$with_libend2end/lib -lend2end -lmd5 -ldes -laes $IBP_LIBS1"],
      [LIBS="-lmd5 -ldes -laes $IBP_LIBS1 -lz"
        AC_CHECK_LIB(end2end, ConditionMapping,
          [IBP_LIBS1="-lend2end -lmd5 -ldes -laes $IBP_LIBS1"],
          [AC_MSG_ERROR(unable to locate libend2end)])])

    AC_ARG_WITH(liblors,
      AC_HELP_STRING([--with-liblors=DIR],
        [base directory for liblors]),
      [CPPFLAGS="-I$with_liblors/include $CPPFLAGS"
        LIBLORS=yes
        IBP_LIBS1="-L$with_liblors/lib -llors $IBP_LIBS1"],
      [LIBS="$IBP_LIBS1 $LIBXML2_LIBS -lz"
        AC_CHECK_LIB(lors,lorsExnodeCreate,
          [IBP_LIBS1="-llors $IBP_LIBS1"],
          [AC_MSG_ERROR(unable to locate liblors)])])

    LIBS="$OLD_LIBS"
    IBP_LIBS="$IBP_LIBS1"
    have_ibp=yes
  fi
  AC_MSG_RESULT($have_ibp)
  if test x$have_ibp = xyes ; then
    ifelse([$1], , :, [$1])
  else
    ifelse([$2], , :, [$2])
  fi
else
  IBP_LIBS=""
  ifelse([$2], , :, [$2])
fi
AC_SUBST([IBP_LIBS])
])


dnl TC_PATH_MJPEG([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for mjpegtools, and define MJPEG_CFLAGS, MJPEG_LIBS and
dnl possibly USE_NEW_MJPEGTOOLS_CODE
dnl
AC_DEFUN([TC_PATH_MJPEG],
[
AC_MSG_CHECKING([whether mjpegtools support is requested])
AC_ARG_ENABLE(mjpegtools,
  AC_HELP_STRING([--enable-mjpegtools],
    [build mjpegtools dependent plugins (no)]),
  [case "${enableval}" in
    yes) ;;
    no)  ;;
    *) AC_MSG_ERROR(bad value ${enableval} for --enable-mjpegtools) ;;
  esac],
  [enable_mjpegtools=no])
AC_MSG_RESULT($enable_mjpegtools)

have_mjpegtools=no

if test x$enable_mjpegtools = xyes ; then
  PKG_CHECK_MODULES(MJPEG, mjpegtools)
  have_mjpegtools=yes
  ifelse([$1], , :, [$1])

  mjpeg_incs="`pkg-config mjpegtools --variable=prefix`/include/mjpegtools"
  AC_CHECK_FILE($mjpeg_incs/yuv4mpeg.h, 
     [AC_DEFINE([HAVE_MJPEG_INC], [1],
       [mjpegtools headers in separate path])])

  dnl check if we have version >= Mar 31 2004
  save_CFLAGS="$CFLAGS"
  save_LIBS="$LIBS"
  CFLAGS="$CFLAGS $MJPEG_CFLAGS"
  LIBS="$LIBS $MJPEG_LIBS"
  AC_TRY_LINK([
#if defined(HAVE_MJPEG_INC)
#include "yuv4mpeg.h"
#include "mpegconsts.h"
#else
#include "mjpegtools/yuv4mpeg.h"
#include "mjpegtools/mpegconsts.h"
#endif
],
    [y4m_write_frame_header(1, NULL, NULL)], 
    [AC_DEFINE([USE_NEW_MJPEGTOOLS_CODE], [1],
      [using mjpegtools post Mar 31 2004])])
  CFLAGS="$save_CFLAGS"
  LIBS="$save_LIBS"
else
  ifelse([$2], , :, [$2])
fi
])


# Configure paths for SDL
# Sam Lantinga 9/21/99
# stolen from Manish Singh
# stolen back from Frank Belew
# stolen from Manish Singh
# Shamelessly stolen from Owen Taylor
# Small fix to avoid warnings from Daniel Caujolle-Bert

dnl TC_PATH_SDL([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for SDL, and define SDL_CFLAGS and SDL_LIBS
dnl
AC_DEFUN([TC_PATH_SDL],
[
AC_REQUIRE([AC_PROG_CC])
AC_REQUIRE([AC_CANONICAL_HOST])

AC_MSG_CHECKING([whether SDL support is requested])
AC_ARG_ENABLE(sdl,
  AC_HELP_STRING([--enable-sdl],
    [build SDL dependent plugins (no)]),
  [case "${enableval}" in
    yes) ;;
    no)  ;;
    *) AC_MSG_ERROR(bad value ${enableval} for --enable-sdl) ;;
  esac],
  [enable_sdl=no])
AC_MSG_RESULT($enable_sdl)

AC_ARG_WITH(sdl-prefix,
  AC_HELP_STRING([--with-sdl-prefix=PFX],
    [Prefix where SDL is installed (optional)]),
  sdl_prefix="$withval", sdl_prefix="")

AC_ARG_WITH(sdl-exec-prefix,
  AC_HELP_STRING([--with-sdl-exec-prefix=PFX],
    [Exec prefix where SDL is installed (optional)]),
  sdl_exec_prefix="$withval", sdl_exec_prefix="")

AC_ARG_ENABLE(sdltest,
  AC_HELP_STRING([--disable-sdltest],
    [Do not try to compile and run a test SDL program]),
  [],
  enable_sdltest=yes)

have_sdl=no

if test x$enable_sdl = xyes ; then

  dnl Get the cflags and libraries from the sdl-config script
  AC_PATH_PROG(SDL_CONFIG, sdl-config, no)

  if test x$sdl_exec_prefix != x ; then
    sdl_args="$sdl_args --exec-prefix=$sdl_exec_prefix"
    if test x${SDL_CONFIG} = xno ; then
      SDL_CONFIG=$sdl_exec_prefix/bin/sdl-config
    fi
  fi
  if test x$sdl_prefix != x ; then
    sdl_args="$sdl_args --prefix=$sdl_prefix"
    if test x${SDL_CONFIG} = xno ; then
      SDL_CONFIG=$sdl_prefix/bin/sdl-config
    fi
  fi

  min_sdl_version=ifelse([$1], ,0.11.0,$1)
  AC_MSG_CHECKING(for SDL - version >= $min_sdl_version)
  if test "$SDL_CONFIG" != "no" ; then
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

],
        [have_sdl=yes],
        [],
        [echo $ac_n "cross compiling; assumed OK... $ac_c"
          have_sdl=yes])
      CFLAGS="$ac_save_CFLAGS"
      LIBS="$ac_save_LIBS"
    else
      have_sdl=yes
    fi
  fi
  if test x$have_sdl = xyes ; then
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
],        [ return 0; ],
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
else
  SDL_CFLAGS=""
  SDL_LIBS=""
  ifelse([$3], , :, [$3])
fi
AC_SUBST(SDL_CFLAGS)
AC_SUBST(SDL_LIBS)
rm -f conf.sdltest
])


dnl TC_PATH_GTK([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for gtk+, and define GTK_CFLAGS and GTK_LIBS
dnl
AC_DEFUN([TC_PATH_GTK],
[
AC_MSG_CHECKING([whether gtk+ support is requested])
AC_ARG_ENABLE(gtk,
  AC_HELP_STRING([--enable-gtk],
    [build gtk dependent modules (no)]),
  [case "${enableval}" in
    yes) ;;
    no)  ;;
    *) AC_MSG_ERROR(bad value ${enableval} for --enable-gtk) ;;
  esac],
  enable_gtk=no)
AC_MSG_RESULT($enable_gtk)
have_gtk=no
if test x"$enable_gtk" = xyes; then
  PKG_CHECK_MODULES(GTK, gtk+ >= 0.99.7)
  have_gtk=yes
  ifelse([$1], , :, [$1])
else
  GTK_CFLAGS=""
  GTK_LIBS=""
  AC_SUBST(GTK_CFLAGS)
  AC_SUBST(GTK_LIBS)
  ifelse([$2], , :, [$2])
fi
])


dnl TC_PATH_LIBFAME([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND [, MODULES]]]])
dnl Test for libfame, and define LIBFAME_CFLAGS and LIBFAME_LIBS
dnl Vivien Chappelier 2000-12-11
dnl stolen from ORBit autoconf
dnl
AC_DEFUN([TC_PATH_LIBFAME],
[
AC_MSG_CHECKING([whether libfame support is requested])
AC_ARG_ENABLE(libfame,
  AC_HELP_STRING([--enable-libfame],
    [build libfame dependent module (no)]),
  [case "${enableval}" in
    yes) ;;
    no)  ;;
    *) AC_MSG_ERROR(bad value ${enableval} for --enable-libfame) ;;
  esac],
  enable_libfame=no)
AC_MSG_RESULT($enable_libfame)

AC_ARG_WITH(libfame-prefix,
  AC_HELP_STRING([--with-libfame-prefix=PFX],
    [Prefix where libfame is installed (optional)]),
  libfame_config_prefix="$withval", libfame_config_prefix="")

AC_ARG_WITH(libfame-exec-prefix,
  AC_HELP_STRING([--with-libfame-exec-prefix=PFX],
    [Exec prefix where libfame is installed (optional)]),
  libfame_config_exec_prefix="$withval", libfame_config_exec_prefix="")

AC_ARG_ENABLE(libfametest,
  AC_HELP_STRING([--disable-libfametest],
    [Do not try to compile and run a test libfame program]),
  [],
  enable_libfametest=yes)

have_libfame=no

if test x$enable_libfame = xyes ; then

  AC_PATH_PROG(LIBFAME_CONFIG, libfame-config, no)

  if test x$libfame_config_exec_prefix != x ; then
    libfame_config_args="$libfame_config_args --exec-prefix=$libfame_config_exec_prefix"
    if test x${LIBFAME_CONFIG} = xno ; then
      LIBFAME_CONFIG=$libfame_config_exec_prefix/bin/libfame-config
    fi
  fi
  if test x$libfame_config_prefix != x ; then
    libfame_config_args="$libfame_config_args --prefix=$libfame_config_prefix"
    if test x${LIBFAME_CONFIG} = xno ; then
      LIBFAME_CONFIG=$libfame_config_prefix/bin/libfame-config
    fi
  fi

  min_libfame_version=ifelse([$1], , 0.9.0, $1)
  AC_MSG_CHECKING(for libfame - version >= $min_libfame_version)
  if test x"$LIBFAME_CONFIG" != x"no" ; then
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
],
        [have_libfame=yes],
        [],
        [echo $ac_n "cross compiling; assumed OK... $ac_c"
          have_libfame=yes])
      CFLAGS="$ac_save_CFLAGS"
      LIBS="$ac_save_LIBS"
    else
      have_libfame=yes
    fi
  fi
  if test x$have_libfame = xyes ; then
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
],
[ return ((libfame_major_version) || (libfame_minor_version) || (libfame_micro_version)); ],
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
    AC_MSG_ERROR([libfame requested, but cannot link against libfame])
  fi
else
  LIBFAME_CFLAGS=""
  LIBFAME_LIBS=""
  ifelse([$3], , :, [$3])
fi
AC_SUBST(LIBFAME_CFLAGS)
AC_SUBST(LIBFAME_LIBS)
rm -f conf.libfametest
])


dnl TC_PATH_MAGICK([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for ImageMagick and set MAGICK_CFLAGS and MAGICK_LIBS
dnl
AC_DEFUN([TC_PATH_MAGICK],
[
AC_MSG_CHECKING([whether ImageMagick support is requested])
AC_ARG_ENABLE(magick,
  AC_HELP_STRING([--enable-magick],
    [build ImageMagick dependent modules (no)]),
  [case "${enableval}" in
    yes) ;;
    no)  ;;
    *) AC_MSG_ERROR(bad value ${enableval} for --enable-magick) ;;
  esac],
  enable_magick=no)
AC_MSG_RESULT($enable_magick)
have_magick=no
if test x"$enable_magick" = xyes; then
  PKG_CHECK_MODULES(MAGICK, ImageMagick >= 5.4.3)
  have_magick=yes
  ifelse([$1], , :, [$1])
else
  MAGICK_CFLAGS=""
  MAGICK_LIBS=""
  AC_SUBST(MAGICK_CFLAGS)
  AC_SUBST(MAGICK_LIBS)
  ifelse([$2], , :, [$2])
fi
])


dnl TC_PATH_LIBJPEG([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for libjpeg and set LIBJPEG_CFLAGS and LIBJPEG_LIBS
dnl
AC_DEFUN([TC_PATH_LIBJPEG],
[
AC_MSG_CHECKING([whether libjpeg support is requested])
AC_ARG_ENABLE(libjpeg,
  AC_HELP_STRING([--enable-libjpeg],
    [build libjpeg dependent modules (yes)]),
  [case "${enableval}" in
    yes) ;;
    no)  ;;
    *) AC_MSG_ERROR(bad value ${enableval} for --enable-libjpeg) ;;
  esac],
  enable_libjpeg=yes)
AC_MSG_RESULT($enable_libjpeg)

have_libjpegmmx=no
have_libjpeg=no
if test x"$enable_libjpeg" = xyes; then
  AC_CHECK_LIB(jpeg-mmx, jpeg_CreateCompress,
    [LIBJPEG_CFLAGS=""
      LIBJPEG_LIBS="-ljpeg-mmx" 
      have_libjpeg=yes have_libjpegmmx=yes])
  if test x$LIBJPEG_LIBS = x; then
    AC_CHECK_LIB(jpeg, jpeg_CreateCompress,
      [LIBJPEG_CFLAGS=""
        LIBJPEG_LIBS="-ljpeg"
        have_libjpeg=yes have_libjpegmmx=no])
  fi
  if test x$have_libjpeg = xyes; then
    ifelse([$1], , :, [$1])
  else
    AC_MSG_ERROR([libjpeg requested, but cannot link against libjpeg])
    ifelse([$2], , :, [$2])
  fi
else
  LIBJPEG_CFLAGS=""
  LIBJPEG_LIBS=""
  ifelse([$2], , :, [$2])
fi
AC_SUBST(LIBJPEG_CFLAGS)
AC_SUBST(LIBJPEG_LIBS)
])


dnl TC_PATH_FFMPEG([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for ffmpeg binary
dnl
AC_DEFUN([TC_PATH_FFMPEG],
[
AC_MSG_CHECKING([whether FFmpeg binary suport is requested])
AC_ARG_ENABLE(ffbin,
  AC_HELP_STRING([--enable-ffbin],
    [build ffbin module (no)]),
  [case "${enableval}" in
    yes) ;;
    no)  ;;
    *) AC_MSG_ERROR(bad value ${enableval} for --enable-ffbin) ;;
  esac],
  enable_ffbin=no)
AC_MSG_RESULT($enable_ffbin)

have_ffmpeg=no
if test x$enable_ffbin = "x"yes ; then
  AC_CHECK_PROG(have_ffmpeg_bin,
		  ffmpeg,
		  yes,
		  no)
  if test x$have_ffmpeg_bin = xyes ; then
    have_ffmpeg=yes
    ifelse([$1], , :, [$1])
  else
    AC_MSG_ERROR([FFmpeg binary support requested, but ffmpeg cannot be found])
    ifelse([$2], , :, [$2])
  fi
fi
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


