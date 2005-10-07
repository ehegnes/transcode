/*
 * img_yuv_rgb.c - YUV<->RGB image format conversion routines
 * Written by Andrew Church <achurch@achurch.org>
 */

#include "ac.h"
#include "imgconvert.h"
#include "img_internal.h"

#define USE_LOOKUP_TABLES  /* for YUV420P->RGB24 */

/*************************************************************************/
/*************************************************************************/

/* Standard C implementations */

const int cY  =  76309;
const int crV = 104597;
const int cgU = -25675;
const int cgV = -53279;
const int cbU = 132201;

/*************************************************************************/

#ifdef USE_LOOKUP_TABLES
# define TABLE_SCALE 4   /* scale factor for Y */
static int yuv_tables_created = 0;
static int Ylutbase[768*TABLE_SCALE];
static int *Ylut = Ylutbase+256*TABLE_SCALE;
static int rVlut[256];
static int gUlut[256];
static int gVlut[256];
static int bUlut[256];
static void yuv_create_tables(void)
{
    if (!yuv_tables_created) {
	int i;
	for (i = -256*TABLE_SCALE; i < 512*TABLE_SCALE; i++) {
	    int v = ((cY*(i-16*TABLE_SCALE)/TABLE_SCALE) + 32768) >> 16;
	    Ylut[i] = v<0 ? 0 : v>255 ? 255 : v;
	}
	for (i = 0; i < 256; i++) {
	    rVlut[i] = (crV * (i-128)) * TABLE_SCALE / cY;
	    gUlut[i] = (cgU * (i-128)) * TABLE_SCALE / cY;
	    gVlut[i] = (cgV * (i-128)) * TABLE_SCALE / cY;
	    bUlut[i] = (cbU * (i-128)) * TABLE_SCALE / cY;
	}
	yuv_tables_created = 1;
    }
}
#define YUV2RGB(uvofs) do {					\
    int Y = src[0][y*width+x] * TABLE_SCALE;			\
    int U = src[1][(uvofs)];					\
    int V = src[2][(uvofs)];					\
    dest[0][(y*width+x)*3  ] = Ylut[Y+rVlut[V]];		\
    dest[0][(y*width+x)*3+1] = Ylut[Y+gUlut[U]+gVlut[V]];	\
    dest[0][(y*width+x)*3+2] = Ylut[Y+bUlut[U]];		\
} while (0)
#else  /* !USE_LOOKUP_TABLES */
static void yuv420p_create_tables(void) { }
#define YUV2RGB(uvofs) do {					\
    int Y = cY * (src[0][y*width+x] - 16);			\
    int U = src[1][(uvofs)] - 128;				\
    int V = src[2][(uvofs)] - 128;				\
    int r = (Y + crV*V + 32768) >> 16;				\
    int g = (Y + cgU*U + cgV*V + 32768) >> 16;			\
    int b = (Y + cbU*U + 32768) >> 16;				\
    dest[0][(y*width+x)*3  ] = r<0 ? 0 : r>255 ? 255 : r;	\
    dest[0][(y*width+x)*3+1] = g<0 ? 0 : g>255 ? 255 : g;	\
    dest[0][(y*width+x)*3+2] = b<0 ? 0 : b>255 ? 255 : b;	\
} while (0)
#endif

static int yuv420p_rgb24(uint8_t **src, uint8_t **dest, int width, int height)
{
    int x, y;

    yuv_create_tables();
    for (y = 0; y < height; y++) {
	for (x = 0; x < width; x++) {
	    YUV2RGB((y/2)*(width/2)+(x/2));
	}
    }
    return 1;
}

static int yuv411p_rgb24(uint8_t **src, uint8_t **dest, int width, int height)
{
    int x, y;

    yuv_create_tables();
    for (y = 0; y < height; y++) {
	for (x = 0; x < width; x++) {
	    YUV2RGB(y*(width/4)+(x/4));
	}
    }
    return 1;
}

static int yuv422p_rgb24(uint8_t **src, uint8_t **dest, int width, int height)
{
    int x, y;

    yuv_create_tables();
    for (y = 0; y < height; y++) {
	for (x = 0; x < width; x++) {
	    YUV2RGB(y*(width/2)+(x/2));
	}
    }
    return 1;
}

static int yuv444p_rgb24(uint8_t **src, uint8_t **dest, int width, int height)
{
    int x, y;

    yuv_create_tables();
    for (y = 0; y < height; y++) {
	for (x = 0; x < width; x++) {
	    YUV2RGB(y*width+x);
	}
    }
    return 1;
}

/*************************************************************************/

#define RGB2Y() \
    (dest[0][y*width+x] = ((16829*r + 33039*g +  6416*b + 32768) >> 16) + 16)
#define RGB2U(uvofs) \
    (dest[1][(uvofs)]   = ((-9714*r - 19070*g + 28784*b + 32768) >> 16) + 128)
#define RGB2V(uvofs) \
    (dest[2][(uvofs)]   = ((28784*r - 24103*g -  4681*b + 32768) >> 16) + 128)

static int rgb24_yuv420p(uint8_t **src, uint8_t **dest, int width, int height)
{
    int x, y;

    for (y = 0; y < height; y++) {
	for (x = 0; x < width; x++) {
	    int r = src[0][(y*width+x)*3  ];
	    int g = src[0][(y*width+x)*3+1];
	    int b = src[0][(y*width+x)*3+2];
	    RGB2Y();
	    if (!((x|y) & 1))
		RGB2U((y/2)*(width/2)+x/2);
	    if ((x&y) & 1)  /* take Cb/Cr from opposite corners */
		RGB2V((y/2)*(width/2)+x/2);
	}
    }
    return 1;
}

static int rgb24_yuv411p(uint8_t **src, uint8_t **dest, int width, int height)
{
    int x, y;

    for (y = 0; y < height; y++) {
	for (x = 0; x < width; x++) {
	    int r = src[0][(y*width+x)*3  ];
	    int g = src[0][(y*width+x)*3+1];
	    int b = src[0][(y*width+x)*3+2];
	    RGB2Y();
	    if (!(x & 3))
		RGB2U(y*(width/4)+x/4);
	    if (!((x^2) & 3))  /* take Cb/Cr from pixels 2 points apart */
		RGB2V(y*(width/4)+x/4);
	}
    }
    return 1;
}

static int rgb24_yuv422p(uint8_t **src, uint8_t **dest, int width, int height)
{
    int x, y;

    for (y = 0; y < height; y++) {
	for (x = 0; x < width; x++) {
	    int r = src[0][(y*width+x)*3  ];
	    int g = src[0][(y*width+x)*3+1];
	    int b = src[0][(y*width+x)*3+2];
	    RGB2Y();
	    if (!(x & 1))
		RGB2U(y*(width/2)+x/2);
	    if (x & 1)  /* take Cb/Cr from separate pixels */
		RGB2V(y*(width/2)+x/2);
	}
    }
    return 1;
}

static int rgb24_yuv444p(uint8_t **src, uint8_t **dest, int width, int height)
{
    int x, y;

    for (y = 0; y < height; y++) {
	for (x = 0; x < width; x++) {
	    int r = src[0][(y*width+x)*3  ];
	    int g = src[0][(y*width+x)*3+1];
	    int b = src[0][(y*width+x)*3+2];
	    RGB2Y();
	    RGB2U(y*width+x);
	    RGB2V(y*width+x);
	}
    }
    return 1;
}

/*************************************************************************/

/* All YUV planar formats convert to grayscale the same way */

#ifdef USE_LOOKUP_TABLES
static uint8_t graylut[256];
static int graylut_created = 0;
static void gray8_create_tables(void)
{
    if (!graylut_created) {
	int i;
	for (i = 0; i < 256; i++) {
	    if (i <= 16)
		graylut[i] = 0;
	    else if (i >= 235)
		graylut[i] = 255;
	    else
		graylut[i] = (i-16) * 255 / 219;
	}
	graylut_created = 1;
    }
}
#endif

static int yuvp_gray8(uint8_t **src, uint8_t **dest, int width, int height)
{
    int i;

#ifdef USE_LOOKUP_TABLES
    gray8_create_tables();
    for (i = 0; i < width*height; i++)
	dest[0][i] = graylut[src[0][i]];
#else
    for (i = 0; i < width*height; i++)
	dest[0][i] = (src[0][i]<16 ? 0 :
		      src[0][i]>=235 ? 255 : (src[0][i]-16)*256/219);
#endif
    return 1;
}

/*************************************************************************/

static int gray8_yuv420p(uint8_t **src, uint8_t **dest, int width, int height)
{
    int i;
    for (i = 0; i < width*height; i++)
	dest[0][i] = src[0][i]*219/255 + 16;
    memset(dest[1], 128, width*height/4);
    memset(dest[2], 128, width*height/4);
    return 1;
}

#define gray8_yuv411p gray8_yuv420p  /* U/V planes are the same size */

static int gray8_yuv422p(uint8_t **src, uint8_t **dest, int width, int height)
{
    int i;
    for (i = 0; i < width*height; i++)
	dest[0][i] = src[0][i]*219/255 + 16;
    memset(dest[1], 128, width*height/2);
    memset(dest[2], 128, width*height/2);
    return 1;
}

static int gray8_yuv444p(uint8_t **src, uint8_t **dest, int width, int height)
{
    int i;
    for (i = 0; i < width*height; i++)
	dest[0][i] = src[0][i]*219/256 + 16;
    memset(dest[1], 128, width*height);
    memset(dest[2], 128, width*height);
    return 1;
}

/*************************************************************************/
/*************************************************************************/

/* Optimized versions of colorspace routines. */

/* Common constant values used in routines: */

#if defined(ARCH_X86) || defined(ARCH_X86_64)

static struct { u_int16_t n[64]; } __attribute__((aligned(16))) yuv_data = {{
    0x00FF,0x00FF,0x00FF,0x00FF,0x00FF,0x00FF,0x00FF,0x00FF, /* for odd/even */
    0x0010,0x0010,0x0010,0x0010,0x0010,0x0010,0x0010,0x0010, /* for Y -16    */
    0x0080,0x0080,0x0080,0x0080,0x0080,0x0080,0x0080,0x0080, /* for U/V -128 */
    0x2543,0x2543,0x2543,0x2543,0x2543,0x2543,0x2543,0x2543, /* Y constant   */
    0x3313,0x3313,0x3313,0x3313,0x3313,0x3313,0x3313,0x3313, /* rV constant  */
    0xF377,0xF377,0xF377,0xF377,0xF377,0xF377,0xF377,0xF377, /* gU constant  */
    0xE5FC,0xE5FC,0xE5FC,0xE5FC,0xE5FC,0xE5FC,0xE5FC,0xE5FC, /* gV constant  */
    0x408D,0x408D,0x408D,0x408D,0x408D,0x408D,0x408D,0x408D, /* bU constant  */
}};
/* Note that the Y factors are halved because G->Y exceeds 0x7FFF */
static struct { u_int16_t n[96]; } __attribute__((aligned(16))) rgb_data = {{
    0x20DF,0x20DF,0x20DF,0x20DF,0x20DF,0x20DF,0x20DF,0x20DF, /* R->Y * 0.5   */
    0x4087,0x4087,0x4087,0x4087,0x4087,0x4087,0x4087,0x4087, /* G->Y * 0.5   */
    0x0C88,0x0C88,0x0C88,0x0C88,0x0C88,0x0C88,0x0C88,0x0C88, /* B->Y * 0.5   */
    0xDA0E,0xDA0E,0xDA0E,0xDA0E,0xDA0E,0xDA0E,0xDA0E,0xDA0E, /* R->U         */
    0xB582,0xB582,0xB582,0xB582,0xB582,0xB582,0xB582,0xB582, /* G->U         */
    0x7070,0x7070,0x7070,0x7070,0x7070,0x7070,0x7070,0x7070, /* B->U         */
    0x7070,0x7070,0x7070,0x7070,0x7070,0x7070,0x7070,0x7070, /* R->V         */
    0xA1D9,0xA1D9,0xA1D9,0xA1D9,0xA1D9,0xA1D9,0xA1D9,0xA1D9, /* G->V         */
    0xEDB7,0xEDB7,0xEDB7,0xEDB7,0xEDB7,0xEDB7,0xEDB7,0xEDB7, /* B->V         */
    0x0420,0x0420,0x0420,0x0420,0x0420,0x0420,0x0420,0x0420, /* Y +16.5      */
    0x2020,0x2020,0x2020,0x2020,0x2020,0x2020,0x2020,0x2020, /* U/V +128.5   */
    0x00FF,0x00FF,0x00FF,0x00FF,0x00FF,0x00FF,0x00FF,0x00FF, /* for odd/even */
}};

/* Convert 4 RGB32 pixels in EAX/EBX/ECX/EDX to RGB24 in EAX/EBX/ECX */
#define IA32_RGB32_TO_RGB24 \
	"movl %%ebx, %%esi	# ESI: 00 B1 G1 R1			\n\
	shll $24, %%esi		# ESI: R1 00 00 00			\n\
	shrl $8, %%ebx		# EBX: 00 00 B1 G1			\n\
	orl %%esi, %%eax	# EAX: R1 B0 G0 R0			\n\
	movl %%ecx, %%esi	# ESI: 00 B2 G2 R2			\n\
	shll $16, %%esi		# ESI: G2 R2 00 00			\n\
	shrl $16, %%ecx		# ECX: 00 00 00 B2			\n\
	shll $8, %%edx		# EDX: B3 G3 R3 00			\n\
	orl %%esi, %%ebx	# EBX: G2 R2 B1 G1			\n\
	orl %%edx, %%ecx	# ECX: B3 G3 R3 B2"

/* Convert 4 RGB24 pixels in EAX/EBX/ECX to RGB32 in EAX/EBX/ECX/EDX */
#define IA32_RGB24_TO_RGB32 \
	"movl %%ecx, %%edx	# EDX: B3 G3 R3 B2			\n\
	shrl $8, %%edx		# EDX: 00 B3 G3 R3			\n\
	andl $0xFF, %%ecx	# ECX: 00 00 00 B2			\n\
	movl %%ebx, %%edi	# EDI: G2 R2 B1 G1			\n\
	andl $0xFFFF0000, %%edi	# EDI: G2 R2 00 00			\n\
	orl %%edi, %%ecx	# ECX: G2 R2 00 B2			\n\
	rorl $16, %%ecx		# ECX: 00 B2 G2 R2			\n\
	movl %%eax, %%edi	# EDI: R1 B0 G0 R0			\n\
	andl $0xFF000000, %%edi	# EDI: R1 00 00 00			\n\
	andl $0x0000FFFF, %%ebx	# EBX: 00 00 B1 G1			\n\
	orl %%edi, %%ebx	# EBX: R1 00 B1 G1			\n\
	roll $8, %%ebx		# EBX: 00 B1 G1 R1			\n\
	andl $0x00FFFFFF, %%eax	# EAX: 00 B0 G0 R0"

#endif  /* ARCH_X86 || ARCH_X86_64 */

/*************************************************************************/

/* MMX routines */

#if defined(ARCH_X86) || defined(ARCH_X86_64)

static inline void mmx_yuv42Xp_to_rgb(uint8_t *srcY, uint8_t *srcU,
				      uint8_t *srcV);
static inline void mmx_store_rgb24(uint8_t *dest);

static int yuv420p_rgb24_mmx(uint8_t **src, uint8_t **dest,
			     int width, int height)
{
    int x, y;

    yuv_create_tables();
    for (y = 0; y < height; y++) {
	for (x = 0; x < (width & ~7); x += 8) {
	    mmx_yuv42Xp_to_rgb(src[0]+y*width+x,
			       src[1]+(y/2)*(width/2)+x/2,
			       src[2]+(y/2)*(width/2)+x/2);
	    mmx_store_rgb24(dest[0]+(y*width+x)*3);
	}
	while (x < width) {
	    YUV2RGB((y/2)*(width/2)+(x/2));
	    x++;
	}
    }
    asm("emms");
    return 1;
}

static int yuv422p_rgb24_mmx(uint8_t **src, uint8_t **dest,
			     int width, int height)
{
    int x, y;

    yuv_create_tables();
    for (y = 0; y < height; y++) {
	for (x = 0; x < (width & ~7); x += 8) {
	    mmx_yuv42Xp_to_rgb(src[0]+y*width+x,
			       src[1]+y*(width/2)+x/2,
			       src[2]+y*(width/2)+x/2);
	    mmx_store_rgb24(dest[0]+(y*width+x)*3);
	}
	while (x < width) {
	    YUV2RGB(y*(width/2)+(x/2));
	    x++;
	}
    }
    asm("emms");
    return 1;
}


static inline void mmx_yuv42Xp_to_rgb(uint8_t *srcY, uint8_t *srcU,
				      uint8_t *srcV)
{
    asm("\
	# Load data, bias and expand to 16 bits				\n\
	pxor %%mm4, %%mm4	# MM4: 00 00 00 00 00 00 00 00		\n\
	movq (%%eax), %%mm6	# MM6: Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0		\n\
	movd (%%ecx), %%mm2	# MM2:             U3 U2 U1 U0		\n\
	movd (%%edx), %%mm3	# MM3:             V3 V2 V1 V0		\n\
	movq %%mm6, %%mm7	# MM7: Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0		\n\
	pand (%%esi), %%mm6	# MM6:  -Y6-  -Y4-  -Y2-  -Y0-		\n\
	psrlw $8, %%mm7		# MM7:  -Y7-  -Y5-  -Y3-  -Y1-		\n\
	punpcklbw %%mm4, %%mm2	# MM2:  -U3-  -U2-  -U1-  -U0-		\n\
	punpcklbw %%mm4, %%mm3	# MM3:  -V3-  -V2-  -V1-  -V0-		\n\
	psubw 16(%%esi), %%mm6	# MM6: subtract 16			\n\
	psubw 16(%%esi), %%mm7	# MM7: subtract 16			\n\
	psubw 32(%%esi), %%mm2	# MM2: subtract 128			\n\
	psubw 32(%%esi), %%mm3	# MM3: subtract 128			\n\
	psllw $3, %%mm6		# MM6: convert to fixed point 8.3	\n\
	psllw $3, %%mm7		# MM7: convert to fixed point 8.3	\n\
	psllw $3, %%mm2		# MM2: convert to fixed point 8.3	\n\
	psllw $3, %%mm3		# MM3: convert to fixed point 8.3	\n\
	# Multiply by constants						\n\
	pmulhw 48(%%esi), %%mm6	# MM6: -cY6- -cY4- -cY2- -cY0-		\n\
	pmulhw 48(%%esi), %%mm7	# MM6: -cY7- -cY5- -cY3- -cY1-		\n\
	movq 80(%%esi), %%mm4	# MM4: gU constant			\n\
	movq 96(%%esi), %%mm5	# MM5: gV constant			\n\
	pmulhw %%mm2, %%mm4	# MM4: -gU3- -gU2- -gU1- -gU0-		\n\
	pmulhw %%mm3, %%mm5	# MM5: -gV3- -gV2- -gV1- -gV0-		\n\
	paddw %%mm5, %%mm4	# MM4:  -g3-  -g2-  -g1-  -g0-		\n\
	pmulhw 64(%%esi), %%mm3	# MM3:  -r3-  -r2-  -r1-  -r0-		\n\
	pmulhw 112(%%esi),%%mm2	# MM2:  -b3-  -b2-  -b1-  -b0-		\n\
	movq %%mm3, %%mm0	# MM0:  -r3-  -r2-  -r1-  -r0-		\n\
	movq %%mm4, %%mm1	# MM1:  -g3-  -g2-  -g1-  -g0-		\n\
	movq %%mm2, %%mm5	# MM5:  -b3-  -b2-  -b1-  -b0-		\n\
	# Add intermediate results to get R/G/B values			\n\
	paddw %%mm6, %%mm0	# MM0:  -R6-  -R4-  -R2-  -R0-		\n\
	paddw %%mm6, %%mm1	# MM1:  -G6-  -G4-  -G2-  -G0-		\n\
	paddw %%mm6, %%mm2	# MM2:  -B6-  -B4-  -B2-  -B0-		\n\
	paddw %%mm7, %%mm3	# MM3:  -R7-  -R5-  -R3-  -R1-		\n\
	paddw %%mm7, %%mm4	# MM4:  -G7-  -G5-  -G3-  -G1-		\n\
	paddw %%mm7, %%mm5	# MM5:  -B7-  -B5-  -B3-  -B1-		\n\
	# Saturate to 0-255 and pack into bytes				\n\
	packuswb %%mm0, %%mm0	# MM0: R6 R4 R2 R0 R6 R4 R2 R0		\n\
	packuswb %%mm1, %%mm1	# MM1: G6 G4 G2 G0 G6 G4 G2 G0		\n\
	packuswb %%mm2, %%mm2	# MM2: B6 B4 B2 B0 B6 B4 B2 B0		\n\
	packuswb %%mm3, %%mm3	# MM3: R7 R5 R3 R1 R7 R5 R3 R1		\n\
	packuswb %%mm4, %%mm4	# MM4: G7 G5 G3 G1 G7 G5 G3 G1		\n\
	packuswb %%mm5, %%mm5	# MM5: B7 B5 B3 B1 B7 B5 B3 B1		\n\
	punpcklbw %%mm3, %%mm0	# MM0: R7 R6 R5 R4 R3 R2 R1 R0		\n\
	punpcklbw %%mm4, %%mm1	# MM1: G7 G6 G5 G4 G3 G2 G1 G0		\n\
	punpcklbw %%mm5, %%mm2	# MM2: B7 B6 B5 B4 B3 B2 B1 B0		\n\
	"
	: /* no outputs */
	: "a" (srcY), "c" (srcU), "d" (srcV), "S" (&yuv_data), "m" (yuv_data)
    );
}

static inline void mmx_store_rgb24(uint8_t *dest)
{
    /* It looks like it's fastest to go to RGB32 first, then shift the
     * result to merge the 24-bit pixels together. */
    asm("\
	pxor %%mm7, %%mm7	# MM7: 00 00 00 00 00 00 00 00		\n\
	movq %%mm0, %%mm3	# MM3: R7 R6 R5 R4 R3 R2 R1 R0		\n\
	movq %%mm1, %%mm4	# MM4: G7 G6 G5 G4 G3 G2 G1 G0		\n\
	movq %%mm2, %%mm5	# MM5: B7 B6 B5 B4 B3 B2 B1 B0		\n\
	punpcklbw %%mm1, %%mm0	# MM0: G3 R3 G2 R2 G1 R1 G0 R0		\n\
	punpcklbw %%mm7, %%mm2	# MM2: 00 B3 00 B2 00 B1 00 B0		\n\
	movq %%mm0, %%mm1	# MM1: G3 R3 G2 R2 G1 R1 G0 R0		\n\
	punpcklwd %%mm2, %%mm0	# MM0: 00 B1 G1 R1 00 B0 G0 R0		\n\
	punpckhwd %%mm2, %%mm1	# MM1: 00 B3 G3 R3 00 B2 G2 R2		\n\
	punpckhbw %%mm4, %%mm3	# MM3: G7 R7 G6 R6 G5 R5 G4 R4		\n\
	punpckhbw %%mm7, %%mm5	# MM5: 00 B7 00 B6 00 B5 00 B4		\n\
	movq %%mm3, %%mm2	# MM2: G7 R7 G6 R6 G5 R5 G4 R4		\n\
	punpcklwd %%mm5, %%mm2	# MM2: 00 B5 G5 R5 00 B4 G4 R4		\n\
	punpckhwd %%mm5, %%mm3	# MM3: 00 B7 G7 R7 00 B6 G6 R6		\n\
	movq %%mm0, %%mm4	# MM4: 00 B1 G1 R1 00 B0 G0 R0		\n\
	movq %%mm1, %%mm5	# MM5: 00 B3 G3 R3 00 B2 G2 R2		\n\
	movq %%mm2, %%mm6	# MM6: 00 B5 G5 R5 00 B4 G4 R4		\n\
	movq %%mm3, %%mm7	# MM7: 00 B7 G7 R7 00 B6 G6 R6		\n\
	psrlq $32, %%mm4	# MM4: 00 00 00 00 00 B1 G1 R1		\n\
	psrlq $32, %%mm5	# MM5: 00 00 00 00 00 B3 G3 R3		\n\
	psrlq $32, %%mm6	# MM6: 00 00 00 00 00 B5 G5 R5		\n\
	psrlq $32, %%mm7	# MM7: 00 00 00 00 00 B7 G7 R7		\n\
	pushl %%ebx							\n\
	movd %%mm0, %%eax	# EAX: 00 B0 G0 R0			\n\
	movd %%mm4, %%ebx	# EBX: 00 B1 G1 R1			\n\
	movd %%mm1, %%ecx	# ECX: 00 B2 G2 R2			\n\
	movd %%mm5, %%edx	# EDX: 00 B3 G3 R3			\n\
	"IA32_RGB32_TO_RGB24"						\n\
	movl %%eax, (%%edi)						\n\
	movl %%ebx, 4(%%edi)						\n\
	movl %%ecx, 8(%%edi)						\n\
	movd %%mm2, %%eax	# EAX: 00 B4 G4 R4			\n\
	movd %%mm6, %%ebx	# EBX: 00 B5 G5 R5			\n\
	movd %%mm3, %%ecx	# ECX: 00 B6 G6 R6			\n\
	movd %%mm7, %%edx	# EDX: 00 B7 G7 R7			\n\
	"IA32_RGB32_TO_RGB24"						\n\
	movl %%eax, 12(%%edi)						\n\
	movl %%ebx, 16(%%edi)						\n\
	movl %%ecx, 20(%%edi)						\n\
	popl %%ebx							\n\
	"
	: /* no outputs */
	: "D" (dest)
	: "eax", "ecx", "edx", "esi"
    );
}

#endif  /* ARCH_X86 || ARCH_X86_64 */

/*************************************************************************/

/* SSE2 */

#if defined(ARCH_X86) || defined(ARCH_X86_64)

static inline void sse2_yuv42Xp_to_rgb(uint8_t *srcY, uint8_t *srcU,
				       uint8_t *srcV);
static inline void sse2_store_rgb24(uint8_t *dest);
static inline void sse2_load_rgb24(uint8_t *src);
static inline void sse2_rgb_to_yuv420p_yu(uint8_t *destY, uint8_t *destU);
static inline void sse2_rgb_to_yuv420p_yv(uint8_t *destY, uint8_t *destV);
static inline void sse2_rgb_to_yuv422p(uint8_t *destY, uint8_t *destU,
				       uint8_t *destV);
static inline void sse2_rgb_to_yuv444p(uint8_t *destY, uint8_t *destU,
				       uint8_t *destV);

static int yuv420p_rgb24_sse2(uint8_t **src, uint8_t **dest,
			      int width, int height)
{
    int x, y;

    yuv_create_tables();
    for (y = 0; y < height; y++) {
	for (x = 0; x < (width & ~15); x += 16) {
	    sse2_yuv42Xp_to_rgb(src[0]+y*width+x,
				src[1]+(y/2)*(width/2)+x/2,
				src[2]+(y/2)*(width/2)+x/2);
	    sse2_store_rgb24(dest[0]+(y*width+x)*3);
	}
	while (x < width) {
	    YUV2RGB((y/2)*(width/2)+(x/2));
	    x++;
	}
    }
    asm("emms");
    return 1;
}

static int yuv422p_rgb24_sse2(uint8_t **src, uint8_t **dest,
			      int width, int height)
{
    int x, y;

    yuv_create_tables();
    for (y = 0; y < height; y++) {
	for (x = 0; x < (width & ~15); x += 16) {
	    sse2_yuv42Xp_to_rgb(src[0]+y*width+x,
				src[1]+y*(width/2)+x/2,
				src[2]+y*(width/2)+x/2);
	    sse2_store_rgb24(dest[0]+(y*width+x)*3);
	}
	while (x < width) {
	    YUV2RGB(y*(width/2)+(x/2));
	    x++;
	}
    }
    asm("emms");
    return 1;
}

static int rgb24_yuv420p_sse2(uint8_t **src, uint8_t **dest,
			      int width, int height)
{
    int x, y;

    for (y = 0; y < height; y++) {
	for (x = 0; x < (width & ~7); x += 8) {
	    sse2_load_rgb24(src[0]+(y*width+x)*3);
	    if (y%2 == 0)
		sse2_rgb_to_yuv420p_yu(dest[0]+y*width+x,
				       dest[1]+(y/2)*(width/2)+x/2);
	    else
		sse2_rgb_to_yuv420p_yv(dest[0]+y*width+x,
				       dest[2]+(y/2)*(width/2)+x/2);
	}
	while (x < width) {
	    int r = src[0][(y*width+x)*3  ];
	    int g = src[0][(y*width+x)*3+1];
	    int b = src[0][(y*width+x)*3+2];
	    RGB2Y();
	    if (!((x|y) & 1))
		RGB2U((y/2)*(width/2)+x/2);
	    if ((x&y) & 1)  /* take Cb/Cr from opposite corners */
		RGB2V((y/2)*(width/2)+x/2);
	    x++;
	}
    }
    asm("emms");
    return 1;
}

static int rgb24_yuv422p_sse2(uint8_t **src, uint8_t **dest,
			      int width, int height)
{
    int x, y;

    for (y = 0; y < height; y++) {
	for (x = 0; x < (width & ~7); x += 8) {
	    sse2_load_rgb24(src[0]+(y*width+x)*3);
	    sse2_rgb_to_yuv422p(dest[0]+y*width+x,
				dest[1]+y*(width/2)+x/2,
				dest[2]+y*(width/2)+x/2);
	}
	while (x < width) {
	    int r = src[0][(y*width+x)*3  ];
	    int g = src[0][(y*width+x)*3+1];
	    int b = src[0][(y*width+x)*3+2];
	    RGB2Y();
	    if (!(x & 1))
		RGB2U(y*(width/2)+x/2);
	    if (x & 1)  /* take Cb/Cr from separate pixels */
		RGB2V(y*(width/2)+x/2);
	    x++;
	}
    }
    asm("emms");
    return 1;
}

static int rgb24_yuv444p_sse2(uint8_t **src, uint8_t **dest,
			      int width, int height)
{
    int x, y;

    for (y = 0; y < height; y++) {
	for (x = 0; x < (width & ~7); x += 8) {
	    sse2_load_rgb24(src[0]+(y*width+x)*3);
	    sse2_rgb_to_yuv444p(dest[0]+y*width+x,
				dest[1]+y*width+x,
				dest[2]+y*width+x);
	}
	while (x < width) {
	    int r = src[0][(y*width+x)*3  ];
	    int g = src[0][(y*width+x)*3+1];
	    int b = src[0][(y*width+x)*3+2];
	    RGB2Y();
	    RGB2U(y*width+x);
	    RGB2V(y*width+x);
	    x++;
	}
    }
    asm("emms");
    return 1;
}


static inline void sse2_yuv42Xp_to_rgb(uint8_t *srcY, uint8_t *srcU,
				       uint8_t *srcV)
{
    asm("\
	# Load data, bias and expand to 16 bits				\n\
	pxor %%xmm4, %%xmm4	# XMM4: 00 00 00 00 00 00 00 00		\n\
	movdqu (%%eax), %%xmm6	# XMM6: YF...................Y0		\n\
	movq (%%ecx), %%xmm2	# XMM2:             U7.......U0		\n\
	movq (%%edx), %%xmm3	# XMM3:             V7.......V0		\n\
	movdqa %%xmm6, %%xmm7	# XMM7: YF...................Y0		\n\
	pand (%%esi), %%xmm6	# XMM6: YE YC YA Y8 Y6 Y4 Y2 Y0		\n\
	psrlw $8, %%xmm7	# XMM7: YF YD YB Y9 Y7 Y5 Y3 Y1		\n\
	punpcklbw %%xmm4,%%xmm2	# XMM2: U7 U6 U5 U4 U3 U2 U1 U0		\n\
	punpcklbw %%xmm4,%%xmm3	# XMM3: V7 V6 V5 V4 V3 V2 V1 V0		\n\
	psubw 16(%%esi), %%xmm6	# XMM6: subtract 16			\n\
	psubw 16(%%esi), %%xmm7	# XMM7: subtract 16			\n\
	psubw 32(%%esi), %%xmm2	# XMM2: subtract 128			\n\
	psubw 32(%%esi), %%xmm3	# XMM3: subtract 128			\n\
	psllw $3, %%xmm6	# XMM6: convert to fixed point 8.3	\n\
	psllw $3, %%xmm7	# XMM7: convert to fixed point 8.3	\n\
	psllw $3, %%xmm2	# XMM2: convert to fixed point 8.3	\n\
	psllw $3, %%xmm3	# XMM3: convert to fixed point 8.3	\n\
	# Multiply by constants						\n\
	pmulhw 48(%%esi),%%xmm6	# XMM6: cYE.................cY0		\n\
	pmulhw 48(%%esi),%%xmm7	# XMM6: cYF.................cY1		\n\
	movdqa 80(%%esi),%%xmm4	# XMM4: gU constant			\n\
	movdqa 96(%%esi),%%xmm5	# XMM5: gV constant			\n\
	pmulhw %%xmm2, %%xmm4	# XMM4: gU7.................gU0		\n\
	pmulhw %%xmm3, %%xmm5	# XMM5: gV7.................gV0		\n\
	paddw %%xmm5, %%xmm4	# XMM4: g7 g6 g5 g4 g3 g2 g1 g0		\n\
	pmulhw 64(%%esi),%%xmm3	# XMM3: r7 r6 r5 r4 r3 r2 r1 r0		\n\
	pmulhw 112(%%esi),%%xmm2 #XMM2: b7 b6 b5 b4 b3 b2 b1 b0		\n\
	movdqa %%xmm3, %%xmm0	# XMM0: r7 r6 r5 r4 r3 r2 r1 r0		\n\
	movdqa %%xmm4, %%xmm1	# XMM1: g7 g6 g5 g4 g3 g2 g1 g0		\n\
	movdqa %%xmm2, %%xmm5	# XMM5: b7 b6 b5 b4 b3 b2 b1 b0		\n\
	# Add intermediate results to get R/G/B values			\n\
	paddw %%xmm6, %%xmm0	# XMM0: RE RC RA R8 R6 R4 R2 R0		\n\
	paddw %%xmm6, %%xmm1	# XMM1: GE GC GA G8 G6 G4 G2 G0		\n\
	paddw %%xmm6, %%xmm2	# XMM2: BE BC BA B8 B6 B4 B2 B0		\n\
	paddw %%xmm7, %%xmm3	# XMM3: RF RD RB R9 R7 R5 R3 R1		\n\
	paddw %%xmm7, %%xmm4	# XMM4: GF GD GB G9 G7 G5 G3 G1		\n\
	paddw %%xmm7, %%xmm5	# XMM5: BF BD BB B9 B7 B5 B3 B1		\n\
	# Saturate to 0-255 and pack into bytes				\n\
	packuswb %%xmm0, %%xmm0	# XMM0: RE.......R0 RE.......R0		\n\
	packuswb %%xmm1, %%xmm1	# XMM1: GE.......G0 GE.......G0		\n\
	packuswb %%xmm2, %%xmm2	# XMM2: BE.......B0 BE.......B0		\n\
	packuswb %%xmm3, %%xmm3	# XMM3: RF.......R1 RF.......R1		\n\
	packuswb %%xmm4, %%xmm4	# XMM4: GF.......G1 GF.......G1		\n\
	packuswb %%xmm5, %%xmm5	# XMM5: BF.......B1 BF.......B1		\n\
	punpcklbw %%xmm3,%%xmm0	# XMM0: RF...................R0		\n\
	punpcklbw %%xmm4,%%xmm1	# XMM1: GF...................G0		\n\
	punpcklbw %%xmm5,%%xmm2	# XMM2: BF...................B0		\n\
	"
	: /* no outputs */
	: "a" (srcY), "c" (srcU), "d" (srcV), "S" (&yuv_data), "m" (yuv_data)
    );
}

static inline void sse2_store_rgb24(uint8_t *dest)
{
    /* It looks like it's fastest to go to RGB32 first, then shift the
     * result to merge the 24-bit pixels together. */
    asm("\
	pxor %%xmm7, %%xmm7	# XMM7: 00 00 00 00 00 00 00 00		\n\
	movdqa %%xmm0, %%xmm3	# XMM3: RF...................R0		\n\
	movdqa %%xmm1, %%xmm4	# XMM4: GF...................G0		\n\
	movdqa %%xmm2, %%xmm5	# XMM5: BF...................B0		\n\
	punpcklbw %%xmm1,%%xmm0	# XMM0: G7 R7.............G0 R0		\n\
	punpcklbw %%xmm7,%%xmm2	# XMM2: 00 B7.............00 B0		\n\
	movdqa %%xmm0, %%xmm1	# XMM1: G7 R7.............G0 R0		\n\
	punpcklwd %%xmm2,%%xmm0	# XMM0: 0BGR3 0BGR2 0BGR1 0BGR0		\n\
	punpckhwd %%xmm2,%%xmm1	# XMM1: 0BGR7 0BGR6 0BGR5 0BGR4		\n\
	punpckhbw %%xmm4,%%xmm3	# XMM3: GF RF.............G8 R8		\n\
	punpckhbw %%xmm7,%%xmm5	# XMM5: 00 BF.............00 B8		\n\
	movdqa %%xmm3, %%xmm2	# XMM2: GF RF.............G8 R8		\n\
	punpcklwd %%xmm5,%%xmm2	# XMM2: 0BGRB 0BGRA 0BGR9 0BGR8		\n\
	punpckhwd %%xmm5,%%xmm3	# XMM3: 0BGRF 0BGRE 0BGRD 0BGRC		\n\
	pushl %%ebx							\n\
	movd %%xmm0, %%eax	# EAX: 00 B0 G0 R0			\n\
	psrldq $4, %%xmm0	# XMM0: 00000 0BGR3 0BGR2 0BGR1		\n\
	movd %%xmm0, %%ebx	# EBX: 00 B1 G1 R1			\n\
	psrldq $4, %%xmm0	# XMM0: 00000 00000 0BGR3 0BGR2		\n\
	movd %%xmm0, %%ecx	# ECX: 00 B2 G2 R2			\n\
	psrldq $4, %%xmm0	# XMM0: 00000 00000 00000 0BGR3		\n\
	movd %%xmm0, %%edx	# EDX: 00 B3 G3 R3			\n\
	"IA32_RGB32_TO_RGB24"						\n\
	movl %%eax, (%%edi)						\n\
	movl %%ebx, 4(%%edi)						\n\
	movl %%ecx, 8(%%edi)						\n\
	movd %%xmm1, %%eax	# EAX: 00 B4 G4 R4			\n\
	psrldq $4, %%xmm1	# XMM1: 00000 0BGR7 0BGR6 0BGR5		\n\
	movd %%xmm1, %%ebx	# EBX: 00 B5 G5 R5			\n\
	psrldq $4, %%xmm1	# XMM1: 00000 00000 0BGR7 0BGR6		\n\
	movd %%xmm1, %%ecx	# ECX: 00 B6 G6 R6			\n\
	psrldq $4, %%xmm1	# XMM1: 00000 00000 00000 0BGR7		\n\
	movd %%xmm1, %%edx	# EDX: 00 B7 G7 R7			\n\
	"IA32_RGB32_TO_RGB24"						\n\
	movl %%eax, 12(%%edi)						\n\
	movl %%ebx, 16(%%edi)						\n\
	movl %%ecx, 20(%%edi)						\n\
	movd %%xmm2, %%eax	# EAX: 00 B8 G8 R8			\n\
	psrldq $4, %%xmm2	# XMM2: 00000 0BGRB 0BGRA 0BGR9		\n\
	movd %%xmm2, %%ebx	# EBX: 00 B9 G9 R9			\n\
	psrldq $4, %%xmm2	# XMM2: 00000 00000 0BGRB 0BGRA		\n\
	movd %%xmm2, %%ecx	# ECX: 00 BA GA RA			\n\
	psrldq $4, %%xmm2	# XMM2: 00000 00000 00000 0BGRB		\n\
	movd %%xmm2, %%edx	# EDX: 00 BB GB RB			\n\
	"IA32_RGB32_TO_RGB24"						\n\
	movl %%eax, 24(%%edi)						\n\
	movl %%ebx, 28(%%edi)						\n\
	movl %%ecx, 32(%%edi)						\n\
	movd %%xmm3, %%eax	# EAX: 00 BC GC RC			\n\
	psrldq $4, %%xmm3	# XMM3: 00000 0BGRF 0BGRE 0BGRD		\n\
	movd %%xmm3, %%ebx	# EBX: 00 BD GD RD			\n\
	psrldq $4, %%xmm3	# XMM3: 00000 00000 0BGRF 0BGRE		\n\
	movd %%xmm3, %%ecx	# ECX: 00 BE GE RE			\n\
	psrldq $4, %%xmm3	# XMM3: 00000 00000 00000 0BGRF		\n\
	movd %%xmm3, %%edx	# EDX: 00 BF GF RF			\n\
	"IA32_RGB32_TO_RGB24"						\n\
	movl %%eax, 36(%%edi)						\n\
	movl %%ebx, 40(%%edi)						\n\
	movl %%ecx, 44(%%edi)						\n\
	popl %%ebx							\n\
	"
	: /* no outputs */
	: "D" (dest)
	: "eax", "ecx", "edx", "esi"
    );
}

static inline void sse2_load_rgb24(uint8_t *src)
{
    asm("\
	pushl %%ebx							\n\
	# Make stack space for loading XMM registers			\n\
	subl $16, %%esp							\n\
	# Read in source pixels 0-3					\n\
	movl (%%esi), %%eax	# EAX: R1 B0 G0 R0			\n\
	movl 4(%%esi), %%ebx	# EBX: G2 R2 B1 G1			\n\
	movl 8(%%esi), %%ecx	# ECX: B3 G3 R3 B2			\n\
	# Convert to RGB32						\n\
	"IA32_RGB24_TO_RGB32"						\n\
	# Store in XMM4							\n\
	movl %%eax, (%%esp)						\n\
	movl %%ebx, 4(%%esp)						\n\
	movl %%ecx, 8(%%esp)						\n\
	movl %%edx, 12(%%esp)						\n\
	movdqu (%%esp), %%xmm4	# XMM4: 0BGR3 0BGR2 0BGR1 0BGR0		\n\
	# Repeat for pixels 4-7 (to XMM6)				\n\
	movl (%%esi), %%eax	# EAX: R5 B4 G4 R4			\n\
	movl 4(%%esi), %%ebx	# EBX: G6 R6 B5 G5			\n\
	movl 8(%%esi), %%ecx	# ECX: B7 G7 R7 B6			\n\
	# Convert to RGB32						\n\
	"IA32_RGB24_TO_RGB32"						\n\
	# Store in XMM0							\n\
	movl %%eax, (%%esp)						\n\
	movl %%ebx, 4(%%esp)						\n\
	movl %%ecx, 8(%%esp)						\n\
	movl %%edx, 12(%%esp)						\n\
	movdqu (%%esp), %%xmm6	# XMM6: 0BGR7 0BGR6 0BGR5 0BGR4		\n\
	# Restore stack and EBX						\n\
	addl $16, %%esp							\n\
	popl %%ebx							\n\
	# Expand byte R/G/B values into words				\n\
	pxor %%xmm3, %%xmm3		# XMM3: 00...................00	\n\
	movdqa %%xmm4, %%xmm5		# XMM5: 0BGR3 0BGR2 0BGR1 0BGR0	\n\
	movdqa %%xmm6, %%xmm7		# XMM7: 0BGR7 0BGR6 0BGR5 0BGR4	\n\
	punpcklbw %%xmm3, %%xmm4	# XMM4: 00 B1 G1 R1 00 B0 G0 R0	\n\
	punpckhbw %%xmm3, %%xmm5	# XMM5: 00 B3 G3 R3 00 B2 G2 R2	\n\
	punpcklbw %%xmm3, %%xmm6	# XMM6: 00 B5 G5 R5 00 B4 G4 R4	\n\
	punpckhbw %%xmm3, %%xmm7	# XMM7: 00 B7 G7 R7 00 B6 G6 R6	\n\
	# Sort into XMM0/1/2 by color					\n\
	pshufd $0xD8, %%xmm5, %%xmm2	# XMM2: 00 B3 00 B2 G3 R3 G2 R2	\n\
	pshufd $0x8D, %%xmm4, %%xmm4	# XMM4: G1 R1 G0 R0 00 B1 00 B0	\n\
	movq %%xmm4, %%xmm2		# XMM2: 00 B3 00 B2 00 B1 00 B0	\n\
	pshufd $0xD8, %%xmm7, %%xmm1	# XMM1: 00 B7 00 B6 G7 R7 G6 R6	\n\
	pshufd $0x8D, %%xmm6, %%xmm6	# XMM6: G5 R5 G4 R4 00 B5 00 B4	\n\
	movq %%xmm6, %%xmm1		# XMM1: 00 B7 00 B6 00 B5 00 B4	\n\
	packuswb %%xmm1, %%xmm2		# XMM2: B7 B6 B5 B4 B3 B2 B1 B0	\n\
	psrldq $8, %%xmm4		# XMM4:             G1 R1 G0 R0	\n\
	pshufd $0x8D, %%xmm5, %%xmm5	# XMM5: G3 R3 G2 R2 00 B3 00 B2	\n\
	movq %%xmm4, %%xmm5		# XMM5: G3 R3 G2 R2 G1 R1 G0 R0	\n\
	pshuflw $0xD8, %%xmm5, %%xmm5	# XMM5: G3 R3 G2 R2 G1 G0 R1 R0	\n\
	pshufhw $0xD8, %%xmm5, %%xmm5	# XMM5: G3 G2 R3 R2 G1 G0 R1 R0	\n\
	psrldq $8, %%xmm4		# XMM6:             G5 R5 G4 R4	\n\
	pshufd $0x8D, %%xmm5, %%xmm5	# XMM7: G7 R7 G6 R6 00 B7 00 B6	\n\
	movq %%xmm6, %%xmm7		# XMM7: G7 R7 G6 R6 G5 R5 G4 R4	\n\
	pshuflw $0xD8, %%xmm7, %%xmm7	# XMM7: G7 R7 G6 R6 G5 G4 R5 R4	\n\
	pshufhw $0xD8, %%xmm7, %%xmm7	# XMM7: G7 G6 R6 R7 G5 G4 R5 R4	\n\
	pshufd $0xD8, %%xmm7, %%xmm1	# XMM1: G7 G6 G5 G4 R7 R6 R5 R4	\n\
	pshufd $0x8D, %%xmm5, %%xmm5	# XMM5: R3 R2 R1 R0 G3 G2 G1 G0	\n\
	movq %%xmm5, %%xmm1		# XMM1: G7 G6 G5 G4 G3 G2 G1 G0	\n\
	pshufd $0x8D, %%xmm7, %%xmm0	# XMM0: R7 R6 R5 R4 G7 G6 G5 G4	\n\
	psrldq $8, %%xmm5		# XMM5:             R3 R2 R1 R0	\n\
	movq %%xmm5, %%xmm0		# XMM0: R7 R6 R5 R4 R3 R2 R1 R0	\n\
	"
	: /* no outputs */
	: "S" (src)
	: "eax", "ecx", "edx", "edi"
    );
}

static inline void sse2_rgb_to_yuv420p_yu(uint8_t *destY, uint8_t *destU)
{
    asm("\
	# Make RGB data into 8.6 fixed-point				\n\
	psllw $6, %%xmm0						\n\
	psllw $6, %%xmm1						\n\
	psllw $6, %%xmm2						\n\
	# Create 8.6 fixed-point Y data in XMM3				\n\
	movdqa %%xmm0, %%xmm3						\n\
	movdqa %%xmm1, %%xmm6						\n\
	movdqa %%xmm2, %%xmm7						\n\
	psllw $1, %%xmm3	# Because Y multipliers are halved	\n\
	psllw $1, %%xmm6						\n\
	psllw $1, %%xmm7						\n\
	pmulhw (%%edi), %%xmm3						\n\
	pmulhw 16(%%edi), %%xmm6					\n\
	pmulhw 32(%%edi), %%xmm7					\n\
	paddw %%xmm6, %%xmm3	# No possibility of overflow		\n\
	paddw %%xmm7, %%xmm3						\n\
	paddw 144(%%edi), %%xmm3					\n\
	# Create 8.6 fixed-point U data in XMM0				\n\
	pmulhw 48(%%edi), %%xmm0					\n\
	pmulhw 64(%%edi), %%xmm1					\n\
	pmulhw 80(%%edi), %%xmm2					\n\
	paddw %%xmm1, %%xmm0						\n\
	paddw %%xmm2, %%xmm0						\n\
	paddw 160(%%edi), %%xmm0					\n\
	# Shift back down to 8-bit values				\n\
	psrlw $6, %%xmm3						\n\
	psrlw $6, %%xmm0						\n\
	# Pack into bytes						\n\
	pxor %%xmm7, %%xmm7						\n\
	packuswb %%xmm7, %%xmm3						\n\
	packuswb %%xmm7, %%xmm0						\n\
	# Remove every odd U value					\n\
	pand 176(%%edi), %%xmm0						\n\
	packuswb %%xmm7, %%xmm0						\n\
	# Store into destination pointers				\n\
	movq %%xmm3, (%%eax)						\n\
	movd %%xmm0, (%%ecx)						\n\
	"
	: /* no outputs */
	: "a" (destY), "c" (destU), "D" (&rgb_data), "m" (rgb_data)
    );
}

static inline void sse2_rgb_to_yuv420p_yv(uint8_t *destY, uint8_t *destV)
{
    asm("\
	# Make RGB data into 8.6 fixed-point				\n\
	psllw $6, %%xmm0						\n\
	psllw $6, %%xmm1						\n\
	psllw $6, %%xmm2						\n\
	# Create 8.6 fixed-point Y data in XMM3				\n\
	movdqa %%xmm0, %%xmm3						\n\
	movdqa %%xmm1, %%xmm6						\n\
	movdqa %%xmm2, %%xmm7						\n\
	psllw $1, %%xmm3	# Because Y multipliers are halved	\n\
	psllw $1, %%xmm6						\n\
	psllw $1, %%xmm7						\n\
	pmulhw (%%edi), %%xmm3						\n\
	pmulhw 16(%%edi), %%xmm6					\n\
	pmulhw 32(%%edi), %%xmm7					\n\
	paddw %%xmm6, %%xmm3	# No possibility of overflow		\n\
	paddw %%xmm7, %%xmm3						\n\
	paddw 144(%%edi), %%xmm3					\n\
	# Create 8.6 fixed-point V data in XMM0				\n\
	pmulhw 96(%%edi), %%xmm0					\n\
	pmulhw 112(%%edi), %%xmm1					\n\
	pmulhw 128(%%edi), %%xmm2					\n\
	paddw %%xmm1, %%xmm0						\n\
	paddw %%xmm2, %%xmm0						\n\
	paddw 160(%%edi), %%xmm0					\n\
	# Shift back down to 8-bit values				\n\
	psrlw $6, %%xmm3						\n\
	psrlw $6, %%xmm0						\n\
	# Pack into bytes						\n\
	pxor %%xmm7, %%xmm7						\n\
	packuswb %%xmm7, %%xmm3						\n\
	packuswb %%xmm7, %%xmm0						\n\
	# Remove every even V value					\n\
	psrlw $8, %%xmm0						\n\
	packuswb %%xmm7, %%xmm0						\n\
	# Store into destination pointers				\n\
	movq %%xmm3, (%%eax)						\n\
	movd %%xmm0, (%%ecx)						\n\
	"
	: /* no outputs */
	: "a" (destY), "c" (destV), "D" (&rgb_data), "m" (rgb_data)
    );
}

static inline void sse2_rgb_to_yuv422p(uint8_t *destY, uint8_t *destU,
				       uint8_t *destV)
{
    asm("\
	# Make RGB data into 8.6 fixed-point				\n\
	psllw $6, %%xmm0						\n\
	psllw $6, %%xmm1						\n\
	psllw $6, %%xmm2						\n\
	# Create 8.6 fixed-point Y data in XMM3				\n\
	movdqa %%xmm0, %%xmm3						\n\
	movdqa %%xmm1, %%xmm6						\n\
	movdqa %%xmm2, %%xmm7						\n\
	psllw $1, %%xmm3	# Because Y multipliers are halved	\n\
	psllw $1, %%xmm6						\n\
	psllw $1, %%xmm7						\n\
	pmulhw (%%edi), %%xmm3						\n\
	pmulhw 16(%%edi), %%xmm6					\n\
	pmulhw 32(%%edi), %%xmm7					\n\
	paddw %%xmm6, %%xmm3	# No possibility of overflow		\n\
	paddw %%xmm7, %%xmm3						\n\
	paddw 144(%%edi), %%xmm3					\n\
	# Create 8.6 fixed-point U data in XMM4				\n\
	movdqa %%xmm0, %%xmm4						\n\
	movdqa %%xmm1, %%xmm6						\n\
	movdqa %%xmm2, %%xmm7						\n\
	pmulhw 48(%%edi), %%xmm4					\n\
	pmulhw 64(%%edi), %%xmm6					\n\
	pmulhw 80(%%edi), %%xmm7					\n\
	paddw %%xmm6, %%xmm4						\n\
	paddw %%xmm7, %%xmm4						\n\
	paddw 160(%%edi), %%xmm4					\n\
	# Create 8.6 fixed-point V data in XMM0				\n\
	pmulhw 96(%%edi), %%xmm0					\n\
	pmulhw 112(%%edi), %%xmm1					\n\
	pmulhw 128(%%edi), %%xmm2					\n\
	paddw %%xmm1, %%xmm0						\n\
	paddw %%xmm2, %%xmm0						\n\
	paddw 160(%%edi), %%xmm0					\n\
	# Shift back down to 8-bit values				\n\
	psrlw $6, %%xmm3						\n\
	psrlw $6, %%xmm4						\n\
	psrlw $6, %%xmm0						\n\
	# Pack into bytes						\n\
	pxor %%xmm7, %%xmm7						\n\
	packuswb %%xmm7, %%xmm3						\n\
	packuswb %%xmm7, %%xmm0						\n\
	# Remove every odd U value					\n\
	pand 176(%%edi), %%xmm4						\n\
	packuswb %%xmm7, %%xmm4						\n\
	# Remove every even V value					\n\
	psrlw $8, %%xmm0						\n\
	packuswb %%xmm7, %%xmm0						\n\
	# Store into destination pointers				\n\
	movq %%xmm3, (%%eax)						\n\
	movd %%xmm4, (%%ecx)						\n\
	movd %%xmm0, (%%edx)						\n\
	"
	: /* no outputs */
	: "a" (destY), "c" (destU), "d" (destV),
	  "D" (&rgb_data), "m" (rgb_data)
    );
}

static inline void sse2_rgb_to_yuv444p(uint8_t *destY, uint8_t *destU,
				       uint8_t *destV)
{
    asm("\
	# Make RGB data into 8.6 fixed-point				\n\
	psllw $6, %%xmm0						\n\
	psllw $6, %%xmm1						\n\
	psllw $6, %%xmm2						\n\
	# Create 8.6 fixed-point Y data in XMM3				\n\
	movdqa %%xmm0, %%xmm3						\n\
	movdqa %%xmm1, %%xmm6						\n\
	movdqa %%xmm2, %%xmm7						\n\
	psllw $1, %%xmm3	# Because Y multipliers are halved	\n\
	psllw $1, %%xmm6						\n\
	psllw $1, %%xmm7						\n\
	pmulhw (%%edi), %%xmm3						\n\
	pmulhw 16(%%edi), %%xmm6					\n\
	pmulhw 32(%%edi), %%xmm7					\n\
	paddw %%xmm6, %%xmm3	# No possibility of overflow		\n\
	paddw %%xmm7, %%xmm3						\n\
	paddw 144(%%edi), %%xmm3					\n\
	# Create 8.6 fixed-point U data in XMM4				\n\
	movdqa %%xmm0, %%xmm4						\n\
	movdqa %%xmm1, %%xmm6						\n\
	movdqa %%xmm2, %%xmm7						\n\
	pmulhw 48(%%edi), %%xmm4					\n\
	pmulhw 64(%%edi), %%xmm6					\n\
	pmulhw 80(%%edi), %%xmm7					\n\
	paddw %%xmm6, %%xmm4						\n\
	paddw %%xmm7, %%xmm4						\n\
	paddw 160(%%edi), %%xmm4					\n\
	# Create 8.6 fixed-point V data in XMM0				\n\
	pmulhw 96(%%edi), %%xmm0					\n\
	pmulhw 112(%%edi), %%xmm1					\n\
	pmulhw 128(%%edi), %%xmm2					\n\
	paddw %%xmm1, %%xmm0						\n\
	paddw %%xmm2, %%xmm0						\n\
	paddw 160(%%edi), %%xmm0					\n\
	# Shift back down to 8-bit values				\n\
	psrlw $6, %%xmm3						\n\
	psrlw $6, %%xmm4						\n\
	psrlw $6, %%xmm0						\n\
	# Pack into bytes						\n\
	pxor %%xmm7, %%xmm7						\n\
	packuswb %%xmm7, %%xmm3						\n\
	packuswb %%xmm7, %%xmm0						\n\
	# Store into destination pointers				\n\
	movq %%xmm3, (%%eax)						\n\
	movq %%xmm4, (%%ecx)						\n\
	movq %%xmm0, (%%edx)						\n\
	"
	: /* no outputs */
	: "a" (destY), "c" (destU), "d" (destV),
	  "D" (&rgb_data), "m" (rgb_data)
    );
}

#endif  /* ARCH_X86 || ARCH_X86_64 */

/*************************************************************************/
/*************************************************************************/

/* Initialization */

int ac_imgconvert_init_yuv_rgb(int accel)
{
    if (!register_conversion(IMG_YUV420P, IMG_RGB24,   yuv420p_rgb24)
     || !register_conversion(IMG_YUV411P, IMG_RGB24,   yuv411p_rgb24)
     || !register_conversion(IMG_YUV422P, IMG_RGB24,   yuv422p_rgb24)
     || !register_conversion(IMG_YUV444P, IMG_RGB24,   yuv444p_rgb24)

     || !register_conversion(IMG_RGB24,   IMG_YUV420P, rgb24_yuv420p)
     || !register_conversion(IMG_RGB24,   IMG_YUV411P, rgb24_yuv411p)
     || !register_conversion(IMG_RGB24,   IMG_YUV422P, rgb24_yuv422p)
     || !register_conversion(IMG_RGB24,   IMG_YUV444P, rgb24_yuv444p)

     || !register_conversion(IMG_YUV420P, IMG_GRAY8,   yuvp_gray8)
     || !register_conversion(IMG_YUV411P, IMG_GRAY8,   yuvp_gray8)
     || !register_conversion(IMG_YUV422P, IMG_GRAY8,   yuvp_gray8)
     || !register_conversion(IMG_YUV444P, IMG_GRAY8,   yuvp_gray8)

     || !register_conversion(IMG_GRAY8,   IMG_YUV420P, gray8_yuv420p)
     || !register_conversion(IMG_GRAY8,   IMG_YUV411P, gray8_yuv411p)
     || !register_conversion(IMG_GRAY8,   IMG_YUV422P, gray8_yuv422p)
     || !register_conversion(IMG_GRAY8,   IMG_YUV444P, gray8_yuv444p)
    ) {
	return 0;
    }

    if (accel & MM_MMX) {
	if (!register_conversion(IMG_YUV420P, IMG_RGB24,   yuv420p_rgb24_mmx)
	 || !register_conversion(IMG_YUV422P, IMG_RGB24,   yuv422p_rgb24_mmx)
	) {
	    return 0;
	}
    }

    if (accel & MM_SSE2) {
	if (!register_conversion(IMG_YUV420P, IMG_RGB24,   yuv420p_rgb24_sse2)
	 || !register_conversion(IMG_YUV422P, IMG_RGB24,   yuv422p_rgb24_sse2)
	 || !register_conversion(IMG_RGB24,   IMG_YUV420P, rgb24_yuv420p_sse2)
	 || !register_conversion(IMG_RGB24,   IMG_YUV422P, rgb24_yuv422p_sse2)
	 || !register_conversion(IMG_RGB24,   IMG_YUV444P, rgb24_yuv444p_sse2)
	) {
	    return 0;
	}
    }

    return 1;
}

/*************************************************************************/
