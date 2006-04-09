#ifndef TC_LZO_H
#define TC_LZO_H

#include <lzo/lzo1x.h>
#include <lzo/lzoutil.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

/* flags */
#define TC_LZO_FORMAT_YV12    	1  /* obsolete */
#define TC_LZO_FORMAT_RGB24     2
#define TC_LZO_FORMAT_YUY2      4
#define TC_LZO_NOT_COMPRESSIBLE 8
#define TC_LZO_FORMAT_YUV420P   16

#define TC_LZO_HDR_SIZE		16
/* 
 * bytes; sum of sizes of tc_lzo_header_t members;
 * _can_ be different from sizeof(tc_lzo_header_t)
 * because structure can be padded (even if it's unlikely
 * since it's already 32-bit and 64-bit aligned).
 * I don't like __attribute__(packed).
 */

typedef struct tc_lzo_header_t {
    uint32_t magic;
    uint32_t size;
    uint32_t flags;
    uint8_t method; /* compression method */
    uint8_t level;  /* compression level */
    uint16_t pad;
} tc_lzo_header_t;


#endif /* TC_LZO_H */
