/* Map transcode configure defines to libavcodecs */

#include "config.h"

/* These come from our config.h */
/* #define ARCH_PPC */
/* #define ARCH_X86 */
/* #define HAVE_MMX */
/* #define HAVE_LRINTF */
/* #define HAVE_STRPTIME */
/* #define HAVE_MEMALIGN */
/* #define HAVE_MALLOC_H */

#ifdef ARCH_PPC
#  define ARCH_POWERPC
#endif

#ifdef HAVE_DLOPEN
#  define CONFIG_HAVE_DLOPEN 1
#endif

#ifdef HAVE_DLFCN_H
#  define CONFIG_HAVE_DLFCN 1
#endif

#ifdef SYSTEM_DARWIN
#  define CONFIG_DARWIN 1
#endif

#ifdef CAN_COMPILE_C_ALTIVEC
#  define HAVE_ALTIVEC 1
#endif

#define CONFIG_ENCODERS 1
#define CONFIG_DECODERS 1
#define CONFIG_MPEGAUDIO_HP 1
#define CONFIG_VIDEO4LINUX 1
#define CONFIG_DV1394 1
#define CONFIG_AUDIO_OSS 1
#define CONFIG_NETWORK 1

#define CONFIG_RISKY 1

#define CONFIG_ZLIB 1
#define SIMPLE_IDCT 1
#define restrict __restrict__
