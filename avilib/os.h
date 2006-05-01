#ifndef __OS_H
#define __OS_H

#if defined(__APPLE__)
# define SYS_APPLE
# define COMP_GCC
# define SYS_UNIX
# if !defined(SYS_BSD)
#  define SYS_BSD
# endif
#else
# define COMP_GCC
# define SYS_UNIX
# if defined(__bsdi__) || defined(__FreeBSD__)
#  if !defined(SYS_BSD)
#   define SYS_BSD
#  endif
# else
#  define SYS_LINUX
# endif
#endif

#if 0
#if !defined(COMP_CYGWIN)
#include <stdint.h>
#endif // !COMP_CYGWIN
#endif

#endif
