/*
 *  dscaler_interface.h
 *
 *  Structs necessary for accessing dscaler filters
 */

#include "config.h"
#include <stdlib.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

/* PictureFlags is a bitmask! */
typedef enum {
    PICTURE_PROGRESSIVE     = 0,
    PICTURE_INTERLACED_ODD  = 1,
    PICTURE_INTERLACED_EVEN = 2
} PictureFlags;

typedef struct {
    uint8_t      *pData;
    PictureFlags  Flags;
} TPicture;

typedef void* (MEMCPY_FUNC)(void* pOutput, const void* pInput, size_t nSize);

typedef struct {
    TPicture** PictureHistory;
    unsigned char *Overlay;
    unsigned int OverlayPitch;
    unsigned int LineLength;
    int FrameWidth;
    int FrameHeight;
    int FieldHeight;
    MEMCPY_FUNC* pMemcpy;
    long InputPitch;
} TDeinterlaceInfo;

#ifdef HAVE_SSE
void filterDScaler_SSE(TDeinterlaceInfo*, int, int);
#endif
#ifdef HAVE_3DNOW
void filterDScaler_3DNOW(TDeinterlaceInfo*, int, int);
#endif
#ifdef HAVE_MMX
void filterDScaler_MMX(TDeinterlaceInfo*, int, int);
#endif
//void filterDScaler_SSE2(TDeinterlaceInfo*, int, int);

