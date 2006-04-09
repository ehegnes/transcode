#ifndef TC_LZO_H
#define TC_LZO_H

#include <lzo/lzo1x.h>
#include <lzo/lzoutil.h>

/* flags */
#define TC_LZO_FORMAT_YV12    1  /* obsolete */
#define TC_LZO_FORMAT_RGB24   2
#define TC_LZO_FORMAT_YUY2    4
#define TC_LZO_NOT_COMPRESSIBLE   8
#define TC_LZO_FORMAT_YUV420P 16

typedef struct tc_lzo_header_t {
    unsigned int magic;
    unsigned int size;
    unsigned int flags;
    unsigned char method; /* compression method */
    unsigned char level;  /* compression level */
    short pad;
} tc_lzo_header_t;

#endif /* TC_LZO_H */