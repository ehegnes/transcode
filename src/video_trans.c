/*
 * video_trans.c - video frame transformation routines
 * Written by Andrew Church <achurch@achurch.org>
 * Based on code written by Thomas Oestreich.
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#include "transcode.h"
#include "framebuffer.h"
#include "video_trans.h"
#include "zoom.h"
#include "aclib/imgconvert.h"

/*************************************************************************/

/* Structure that holds video frame information for passing around to
 * processing routines.  Since this is used only locally, we don't add
 * the fields to vframe_list_t itself. */

typedef struct {
    vframe_list_t *ptr;
    int preadj_w, preadj_h;  // width and height used for secondary buffer
    int Bpp;                 // BYTES (not bits) per pixel
    int nplanes;             // number of planes
    uint8_t *planes[3];      // pointer to start of each plane
    uint8_t *tmpplanes[3];   // same, for secondary buffer
    int width_div[3];        // width divisors for each plane
    int height_div[3];       // height divisors for each plane
    uint8_t black_pixel[3];  // "black" value for each plane (e.g. 128 for U/V)
} video_trans_data_t;

/* Macro to perform a transformation on a frame.  `vtd' is a pointer to a
 * video_trans_data_t; the given function `func' will be called for each
 * plane `i' as:
 *     func(vtd->planes[i], vtd->tmpplanes[i], vtd->ptr->v_width,
 *          vtd->ptr->v_height, vtd->Bpp, args)
 * where `args' are all arguments to this macro (if any) following `vtd'.
 * swap_buffers(vtd) is called after the processing is complete.
 */
#define PROCESS_FRAME(func,vtd,args...) do {			\
    int i;							\
    for (i = 0; i < (vtd)->nplanes; i++) {			\
	func((vtd)->planes[i], (vtd)->tmpplanes[i],		\
	     (vtd)->ptr->v_width / (vtd)->width_div[i],		\
	     (vtd)->ptr->v_height / (vtd)->height_div[i],	\
	     (vtd)->Bpp , ## args);				\
    }								\
    swap_buffers(vtd);						\
} while (0)

/*************************************************************************/

/* Lookup tables for fast resize, gamma correction, and antialiasing. */

static resize_table_t resize_htable[TC_MAX_V_FRAME_WIDTH/8];
static resize_table_t resize_htable_up[TC_MAX_V_FRAME_WIDTH/8];
static resize_table_t resize_vtable[TC_MAX_V_FRAME_HEIGHT/8];
static resize_table_t resize_vtable_up[TC_MAX_V_FRAME_HEIGHT/8];

static uint8_t gamma_table[256];

static uint32_t aa_table_c[256];
static uint32_t aa_table_x[256];
static uint32_t aa_table_y[256];
static uint32_t aa_table_d[256];

/*************************************************************************/
/*************************************************************************/

/* Initialize various lookup tables. */

static void init_resize_table(resize_table_t *table, int oldsize, int newsize);
static void init_gamma_table(double gamma);
static void init_aa_table(double aa_weight, double aa_bias);

static void init_tables(int width, int height, vob_t *vob)
{
    static int initted = 0;

    if (initted)
	return;

    init_resize_table(resize_htable, width, width - vob->hori_resize1*8);
    init_resize_table(resize_htable_up, width, width + vob->hori_resize2*8);
    init_resize_table(resize_vtable, height, height - vob->vert_resize1*8);
    init_resize_table(resize_vtable_up, height, height + vob->vert_resize2*8);

    init_gamma_table(vob->gamma);

    init_aa_table(vob->aa_weight, vob->aa_bias);

    initted = 1;
}

/* Resizing tables.  We divide the frame into 8 horizontal/vertical chunks,
 * then for each pixel/line in a resized chunk, determine which pixel/line
 * in the source chunk to use and the weights to assign to that pixel/line
 * and the next. */
static void init_resize_table(resize_table_t *table, int oldsize, int newsize)
{
    int i;
    /* Compute the number of source pixels per destination pixel */
    double width_ratio = (double)oldsize / (double)newsize;

    for (i = 0; i < newsize/8; i++) {
	double oldpos;

	/* Left/topmost source pixel to use */
	oldpos = (double)i/(double)newsize * oldsize;
	table[i].source = (int)oldpos;

	/* Is the new pixel contained entirely within the old? */
	if (oldpos+width_ratio < table[i].source+1) {
	    /* Yes, weight ratio is 1.0:0.0 */
	    table[i].weight1 = 65536;
	    table[i].weight2 = 0;
	    table[i].antialias = 0;
	} else {
	    /* No, compute appropriate weight ratio */
	    double temp = ((table[i].source+1) - oldpos) / width_ratio * PI/2;
	    table[i].weight1 = (uint32_t)(sin(temp)*sin(temp) * 65536 + 0.5);
	    table[i].weight2 = 65536 - table[i].weight1;
	    table[i].antialias = (table[i].weight1 > 65536*RESIZE_AATHRESH_L
			       && table[i].weight1 < 65536*RESIZE_AATHRESH_U);
	}
    }
}

/* Gamma correction table. */
static void init_gamma_table(double gamma)
{
    int i;

    for (i = 0; i < 256; i++)
	gamma_table[i] = (uint8_t) (pow((i/255.0),gamma) * 255);
}

/* Antialiasing table. */
static void init_aa_table(double aa_weight, double aa_bias)
{
    int i;

    for (i = 0; i < 256; ++i) {
	aa_table_c[i] = i*aa_weight * 65536;
	aa_table_x[i] = i*aa_bias*(1-aa_weight)/4 * 65536;
	aa_table_y[i] = i*(1-aa_bias)*(1-aa_weight)/4 * 65536;
	aa_table_d[i] = (aa_table_x[i]+aa_table_y[i])/2;
    }
}

/*************************************************************************/

/* Initialize vtd structure from given vframe_list_t, and update
 * ptr->video_size. */

static void set_vtd(video_trans_data_t *vtd, vframe_list_t *ptr)
{
    int i;

    vtd->ptr = ptr;
    vtd->preadj_w = 0;
    vtd->preadj_h = 0;
    /* Set some defaults */
    vtd->Bpp = 1;
    vtd->nplanes = 1;
    vtd->planes[0] = ptr->video_buf;
    vtd->tmpplanes[0] = ptr->video_buf_Y[ptr->free];
    vtd->width_div[0] = 1;
    vtd->height_div[0] = 1;
    vtd->black_pixel[0] = 0;
    /* Now set parameters based on image format */
    if (ptr->v_codec == CODEC_YUV) {
	vtd->nplanes = 3;
	vtd->Bpp = 1;
	vtd->width_div[1] = 2;
	vtd->width_div[2] = 2;
	vtd->height_div[1] = 2;
	vtd->height_div[2] = 2;
	vtd->black_pixel[1] = 128;
	vtd->black_pixel[2] = 128;
    } else if (vtd->ptr->v_codec == CODEC_YUV422) {
	vtd->nplanes = 3;
	vtd->Bpp = 1;
	vtd->width_div[1] = 2;
	vtd->width_div[2] = 2;
	vtd->height_div[1] = 1;
	vtd->height_div[2] = 1;
	vtd->black_pixel[1] = 128;
	vtd->black_pixel[2] = 128;
    } else if (vtd->ptr->v_codec == CODEC_RGB) {
	vtd->Bpp = 3;
    }
    ptr->video_size = 0;
    for (i = 0; i < vtd->nplanes; i++) {
	int planesize = (ptr->v_width/vtd->width_div[i])
	              * (ptr->v_height/vtd->height_div[i])
	              * vtd->Bpp;
	ptr->video_size += planesize;
	if (i < vtd->nplanes-1) {
	    vtd->planes[i+1] = vtd->planes[i] + planesize;
	    vtd->tmpplanes[i+1] = vtd->tmpplanes[i] + planesize;
	}
    }
}


/* Prepare for an operation that will change the frame size, setting up the
 * secondary buffer plane pointers with the new size.  Calling swap_buffers()
 * will store the new size in the vframe_list_t structure. */

static void preadjust_frame_size(video_trans_data_t *vtd, int new_w, int new_h)
{
    int i;

    vtd->preadj_w = new_w;
    vtd->preadj_h = new_h;
    for (i = 0; i < vtd->nplanes-1; i++) {
	int planesize = (new_w/vtd->width_div[i]) * (new_h/vtd->height_div[i])
	              * vtd->Bpp;
	vtd->tmpplanes[i+1] = vtd->tmpplanes[i] + planesize;
    }
}


/* Swap current video frame buffer with free buffer.  Also updates frame
 * size if preadjust_frame_size() has been called. */

static void swap_buffers(video_trans_data_t *vtd)
{
    vtd->ptr->video_buf = vtd->ptr->video_buf_Y[vtd->ptr->free];
    vtd->ptr->free = (vtd->ptr->free==0) ? 1 : 0;
    /* Install new width/height if preadjust_frame_size() was called */
    if (vtd->preadj_w && vtd->preadj_h) {
	vtd->ptr->v_width = vtd->preadj_w;
	vtd->ptr->v_height = vtd->preadj_h;
	vtd->preadj_w = 0;
	vtd->preadj_h = 0;
    }
    /* Set up plane pointers again */
    set_vtd(vtd, vtd->ptr);
}

/*************************************************************************/
/*************************************************************************/

/* Video processing functions. */

/*************************************************************************/

/* -j/-Y: Clip or expand the frame.  `black_pixel' is used when filling in
 * expanded areas. */

static void clip(uint8_t *src, uint8_t *dest, int width, int height, int Bpp,
		 int clip_left, int clip_right, int clip_top, int clip_bottom,
		 uint8_t black_pixel)
{
    int new_w = width - clip_left - clip_right;
    int copy_w = width - (clip_left<0 ? 0 : clip_left)
                       - (clip_right<0 ? 0 : clip_right);
    int copy_h = height - (clip_top<0 ? 0 : clip_top)
                        - (clip_bottom<0 ? 0 : clip_bottom);
    int y;

    if (clip_top < 0) {
	memset(dest, black_pixel, (-clip_top) * new_w * Bpp);
	dest += (-clip_top) * new_w * Bpp;
    } else {
	src += clip_top * width * Bpp;
    }
    if (clip_left > 0)
	src += clip_left * Bpp;
    for (y = 0; y < copy_h; y++) {
	if (clip_left < 0) {
	    memset(dest, black_pixel, (-clip_left) * Bpp);
	    dest += (-clip_left) * Bpp;
	}
	ac_memcpy(dest, src, copy_w * Bpp);
	dest += copy_w * Bpp;
	src += width * Bpp;
	if (clip_right < 0) {
	    memset(dest, black_pixel, (-clip_right) * Bpp);
	    dest += (-clip_right) * Bpp;
	}
    }
    if (clip_bottom < 0) {
	memset(dest, black_pixel, (-clip_bottom) * new_w * Bpp);
    }
}

/*************************************************************************/

/* -X/-B: Resize the frame using a lookup table.  `scale_w' and `scale_h'
 * are the number of blocks the image is divided into (normally 8; 4 for
 * subsampled U/V).  `resize_w' and `resize_h' are given in units of
 * `scale_w' and `scale_h' respectively.  Only one of `resize_w' and
 * `resize_h' may be nonzero.
 * N.B. doesn't work well if shrinking by more than a factor of 2 (only
 *      averages 2 adjacent lines/pixels)
 */

static void rescale_pixel(const uint8_t *src1, const uint8_t *src2,
			  uint8_t *dest, int bytes,
			  uint32_t weight1, uint32_t weight2);

static void resize(uint8_t *src, uint8_t *dest, int width, int height,
		   int Bpp, int resize_w, int resize_h, int scale_w,
		   int scale_h)
{
    int new_w = width + resize_w*scale_w;
    int new_h = height + resize_h*scale_h;

    /* Resize vertically (fast, using accelerated routine) */
    if (resize_h) {
	resize_table_t *table = (resize_h > 0) ? resize_vtable_up : resize_vtable;
	int Bpl = width * Bpp;  /* bytes per line */
	int i, y;
	for (i = 0; i < scale_h; i++) {
	    uint8_t *sptr = src  + (i * (height/scale_h)) * Bpl;
	    uint8_t *dptr = dest + (i * (new_h /scale_h)) * Bpl;
	    for (y = 0; y < new_h / scale_h; y++) {
		ac_rescale(sptr + (table[y].source  ) * Bpl,
			   sptr + (table[y].source+1) * Bpl,
			   dptr + y*Bpl, Bpl,
			   table[y].weight1, table[y].weight2);
	    }
	}
    }

    /* Resize horizontally; calling the accelerated routine for each pixel
     * has far too much overhead, so we just perform the calculations
     * directly */
    if (resize_w) {
	resize_table_t *table = (resize_w > 0) ? resize_htable_up : resize_htable;
	int i, x;
	/* Treat the image as an array of blocks */
	for (i = 0; i < new_h * scale_w; i++) {
	    if (Bpp == 1) {  /* optimization hint */
		uint8_t *sptr = src  + (i * (width/scale_w)) * Bpp;
		uint8_t *dptr = dest + (i * (new_w/scale_w)) * Bpp;
		for (x = 0; x < new_w / scale_w; x++) {
		    rescale_pixel(sptr + (table[x].source  ) * Bpp,
				  sptr + (table[x].source+1) * Bpp,
				  dptr + x*Bpp, Bpp,
				  table[x].weight1, table[x].weight2);
		}
	    } else {  /* exactly the same thing */
		uint8_t *sptr = src  + (i * (width/scale_w)) * Bpp;
		uint8_t *dptr = dest + (i * (new_w/scale_w)) * Bpp;
		for (x = 0; x < new_w / scale_w; x++) {
		    rescale_pixel(sptr + (table[x].source  ) * Bpp,
				  sptr + (table[x].source+1) * Bpp,
				  dptr + x*Bpp, Bpp,
				  table[x].weight1, table[x].weight2);
		}
	    }
	}
    }
}

static void rescale_pixel(const uint8_t *src1, const uint8_t *src2,
			  uint8_t *dest, int bytes,
			  uint32_t weight1, uint32_t weight2)
{
    int byte;
    for (byte = 0; byte < bytes; byte++) {
	/* Watch out for trying to access beyond the end of the frame on
	 * the last pixel */
	if (weight1 < 0x10000)  /* this is the more likely case */
	    dest[byte] = (src1[byte]*weight1 + src2[byte]*weight2 + 32768)
		         >> 16;
	else
	    dest[byte] = src1[byte];
    }
}

/*************************************************************************/

/* -Z: Resize (zoom) the frame to an arbitrary size. */

static void dozoom(uint8_t *src, uint8_t *dest, int width, int height,
		   int Bpp, int new_w, int new_h,
		   double (*zoom_filter)(double), int zoom_support)
{
    image_t srcimage, destimage;
    zoomer_t *zoomer;

    zoom_setup_image(&srcimage, width, height, Bpp, src);
    zoom_setup_image(&destimage, new_w, new_h, Bpp, dest);
    zoomer = zoom_image_init(&destimage, &srcimage, zoom_filter, zoom_support);
    zoom_image_process(zoomer);
    zoom_image_done(zoomer);
}

/*************************************************************************/

/* -I: Deinterlace the frame.  `mode' is the processing mode (-I parameter). */

static void deint_drop_field(uint8_t *src, uint8_t *dest, int width,
			     int height, int Bpp);
static void deint_interpolate(uint8_t *src, uint8_t *dest, int width,
			      int height, int Bpp);
static void deint_linear_blend(uint8_t *src, uint8_t *dest, int width,
			       int height, int Bpp);

static void deinterlace(video_trans_data_t *vtd, int mode)
{
    if (mode == 1) {
	/* Simple linear interpolation */
	PROCESS_FRAME(deint_interpolate, vtd);
    } else if (mode == 3 || mode == 4) {
	/* Drop every other line (and zoom back out in mode 3) */
	preadjust_frame_size(vtd, vtd->ptr->v_width, vtd->ptr->v_height/2);
	PROCESS_FRAME(deint_drop_field, vtd);
	if (mode == 3) {
	    int w = vtd->ptr->v_width, h = vtd->ptr->v_height*2;
	    vob_t *vob = tc_get_vob();
	    preadjust_frame_size(vtd, w, h);
	    PROCESS_FRAME(dozoom, vtd, w, h, vob->zoom_filter,
			  vob->zoom_support);
	}
    } else if (mode == 5) {
	/* Linear blend */
	PROCESS_FRAME(deint_linear_blend, vtd);
    } else {
	/* Mode 2 (handled by encoder) or unknown: do nothing */
	return;
    }
    vtd->ptr->attributes &= ~TC_FRAME_IS_INTERLACED;
}

static void deint_drop_field(uint8_t *src, uint8_t *dest, int width,
			     int height, int Bpp)
{
    int Bpl = width * Bpp;
    int y;

    for (y = 0; y < height/2; y++)
	ac_memcpy(dest + y*Bpl, src + (y*2)*Bpl, Bpl);
}

static void deint_interpolate(uint8_t *src, uint8_t *dest, int width,
			      int height, int Bpp)
{
    int Bpl = width * Bpp;
    int y;

    for (y = 0; y < height; y++) {
        if (y%2 == 0) {
            ac_memcpy(dest + y*Bpl, src + y*Bpl, Bpl);
        } else if (y == height-1) {
            /* if the last line is odd, copy from the previous line */
            ac_memcpy(dest + y*Bpl, src + (y-1)*Bpl, Bpl);
        } else {
            ac_average(src + (y-1)*Bpl, src + (y+1)*Bpl, dest + y*Bpl, Bpl);
        }
    }
}

static void deint_linear_blend(uint8_t *src, uint8_t *dest, int width,
			       int height, int Bpp)
{
    int Bpl = width * Bpp;
    int y;

    /* First interpolate odd lines into the target buffer */
    deint_interpolate(src, dest, width, height, Bpp);

    /* Now interpolate even lines in the source buffer; we don't use it
     * after this so it's okay to destroy it */
    ac_memcpy(src, src+Bpl, Bpl);
    for (y = 2; y < height-1; y += 2)
	ac_average(src + (y-1)*Bpl, src + (y+1)*Bpl, src + y*Bpl, Bpl);
    if (y < height)
	ac_memcpy(src + y*Bpl, src + (y-1)*Bpl, Bpl);

    /* Finally average the two frames together */
    ac_average(src, dest, dest, height*Bpl);
}

/*************************************************************************/

/* -r: Reduce the frame size by a specified integral amount. */

static void reduce(uint8_t *src, uint8_t *dest, int width, int height,
		   int Bpp, int reduce_w, int reduce_h)
{
    int x, y, i;
    int xstep = Bpp * reduce_w;

    for (y = 0; y < height / reduce_h; y++) {
	for (x = 0; x < width / reduce_w; x++) {
	    for (i = 0; i < Bpp; i++)
		*dest++ = src[x*xstep+i];
	}
	src += width*Bpp * reduce_h;
    }
}

/*************************************************************************/

/* -z: Flip the frame vertically. */

static void flip_v(uint8_t *src, uint8_t *dest, int width, int height, int Bpp)
{
    int Bpl = width * Bpp;  /* bytes per line */
    int y;
    uint8_t buf[TC_MAX_V_FRAME_WIDTH * TC_MAX_V_BYTESPP];

    /* Note that GCC4 can optimize this perfectly; no need for extra
     * pointer variables */
    for (y = 0; y < (height+1)/2; y++) {
	ac_memcpy(buf, src + y*Bpl, Bpl);
	ac_memcpy(dest + y*Bpl, src + ((height-1)-y)*Bpl, Bpl);
	ac_memcpy(dest + ((height-1)-y)*Bpl, buf, Bpl);
    }
}

/*************************************************************************/

/* -l: Flip (mirror) the frame horizontally. */

static void flip_h(uint8_t *src, uint8_t *dest, int width, int height, int Bpp)
{
    int x, y, i;

    for (y = 0; y < height; y++) {
	uint8_t *srcline = src + y*width*Bpp;
	uint8_t *destline = dest + y*width*Bpp;
	for (x = 0; x < (width+1)/2; x++) {
	    for (i = 0; i < Bpp; i++) {
		uint8_t tmp = srcline[x*Bpp+i];
		destline[x*Bpp+i] = srcline[((width-1)-x)*Bpp+i];
		destline[((width-1)-x)*Bpp+i] = tmp;
	    }
	}
    }
}

/*************************************************************************/

/* -G: Gamma correction. */

static void gamma_correct(uint8_t *src, uint8_t *dest, int width, int height,
			  int Bpp)
{
    int i;
    for (i = 0; i < width*height*Bpp; i++)
	dest[i] = gamma_table[src[i]];
}

/*************************************************************************/

/* -C: Antialiasing.  `mode' is the parameter given to -C (1/2/3). */

static void antialias_line(uint8_t *src, uint8_t *dest, int width, int Bpp);

static void antialias(uint8_t *src, uint8_t *dest, int width, int height,
		      int Bpp, int mode)
{
    int y;

    ac_memcpy(dest, src, width*Bpp);
    for (y = 1; y < height-1; y++) {
	if ((mode == 1 && y%2 == 1)
	 || (mode == 2 && resize_vtable[y%(height/8)].antialias)
	 || (mode == 3)
	) {
	    antialias_line(src + y*width*Bpp, dest + y*width*Bpp, width, Bpp);
	} else {
	    ac_memcpy(dest + y*width*Bpp, src + y*width*Bpp, width*Bpp);
	}
    }
    ac_memcpy(dest + (height-1)*width*Bpp, src + (height-1)*width*Bpp,
	      width*Bpp);
}

static inline int samecolor(uint8_t *pixel1, uint8_t *pixel2, int Bpp)
{
    int i;
    int maxdiff = abs(pixel2[0]-pixel1[0]);
    for (i = 1; i < Bpp; i++) {
	int diff = abs(pixel2[i]-pixel1[i]);
	if (diff > maxdiff)
	    maxdiff = diff;
    }
    return maxdiff < AA_DIFFERENT;
}

#define C (src + x*Bpp)
#define U (C - width*Bpp)
#define D (C + width*Bpp)
#define L (C - Bpp)
#define R (C + Bpp)
#define UL (U - Bpp)
#define UR (U + Bpp)
#define DL (D - Bpp)
#define DR (D + Bpp)
#define SAME(pix1,pix2) samecolor((pix1),(pix2),Bpp)
#define DIFF(pix1,pix2) !samecolor((pix1),(pix2),Bpp)

static void antialias_line(uint8_t *src, uint8_t *dest, int width, int Bpp)
{
    int i, x;

    for (i = 0; i < Bpp; i++)
	dest[i] = src[i];
    for (x = 1; x < width-1; x++) {
	if ((SAME(L,U) && DIFF(L,D) && DIFF(L,R))
	 || (SAME(L,D) && DIFF(L,U) && DIFF(L,R))
	 || (SAME(R,U) && DIFF(R,D) && DIFF(R,L))
	 || (SAME(R,D) && DIFF(R,U) && DIFF(R,L))
	) {
	    for (i = 0; i < Bpp; i++) {
		uint32_t tmp = aa_table_d[*UL]
		             + aa_table_y[*U ]
		             + aa_table_d[*UR]
		             + aa_table_x[*L ]
		             + aa_table_c[*C ]
		             + aa_table_x[*R ]
		             + aa_table_d[*DL]
		             + aa_table_y[*D ]
		             + aa_table_d[*DR];
		dest[x*Bpp+i] = (verbose & TC_DEBUG) ? 255 : tmp>>16;
	    }
	} else {
	    for (i = 0; i < Bpp; i++)
		dest[x*Bpp+i] = src[x*Bpp+i];
	}
    }
    for (i = 0; i < Bpp; i++)
	dest[(width-1)*Bpp+i] = src[(width-1)*Bpp+i];
}

#undef U
#undef D
#undef L
#undef R
#undef SAME
#undef DIFF

/*************************************************************************/
/*************************************************************************/

static int do_process_frame(vob_t *vob, vframe_list_t *ptr)
{
    video_trans_data_t vtd;  /* for passing to subroutines */

    init_tables(ptr->v_width, ptr->v_height, vob);
    set_vtd(&vtd, ptr);

    /**** -j: clip frame (import) ****/

    if (im_clip) {
	preadjust_frame_size(&vtd,
		ptr->v_width - vob->im_clip_left - vob->im_clip_right,
		ptr->v_height - vob->im_clip_top - vob->im_clip_bottom);
	PROCESS_FRAME(clip, &vtd,
		      vob->im_clip_left   / vtd.width_div[i],
		      vob->im_clip_right  / vtd.width_div[i],
		      vob->im_clip_top    / vtd.height_div[i],
		      vob->im_clip_bottom / vtd.height_div[i],
		      vtd.black_pixel[i]);
    }

    /**** -I: deinterlace video frame ****/

    if (vob->deinterlace > 0)
	deinterlace(&vtd, vob->deinterlace);
    if ((ptr->attributes & TC_FRAME_IS_INTERLACED) && ptr->deinter_flag > 0)
	deinterlace(&vtd, ptr->deinter_flag);

    /**** -X: fast resize (up) ****/
    /**** -B: fast resize (down) ****/

    if (resize1 || resize2) {
	int width = ptr->v_width, height = ptr->v_height;
	int resize_w = vob->hori_resize2 - vob->hori_resize1;
	int resize_h = vob->vert_resize2 - vob->vert_resize1;
	if (resize_h) {
	    preadjust_frame_size(&vtd, width, height+resize_h*8);
	    PROCESS_FRAME(resize, &vtd, 0, resize_h, 8/vtd.width_div[i],
			  8/vtd.height_div[i]);
	    height += resize_h * 8;
	}
	if (resize_w) {
	    preadjust_frame_size(&vtd, width+resize_w*8, height);
	    PROCESS_FRAME(resize, &vtd, resize_w, 0, 8/vtd.width_div[i],
			  8/vtd.height_div[i]);
	}
    }

    /**** -Z: zoom frame (slow resize) ****/

    if (zoom) {
	preadjust_frame_size(&vtd, vob->zoom_width, vob->zoom_height);
	PROCESS_FRAME(dozoom, &vtd, vob->zoom_width, vob->zoom_height,
		      vob->zoom_filter, vob->zoom_support);
    }

    /**** -Y: clip frame (export) ****/

    if (ex_clip) {
	preadjust_frame_size(&vtd,
		ptr->v_width - vob->ex_clip_left-vob->ex_clip_right,
		ptr->v_height - vob->ex_clip_top - vob->ex_clip_bottom);
	PROCESS_FRAME(clip, &vtd,
		      vob->ex_clip_left   / vtd.width_div[i],
		      vob->ex_clip_right  / vtd.width_div[i],
		      vob->ex_clip_top    / vtd.height_div[i],
		      vob->ex_clip_bottom / vtd.height_div[i],
		      vtd.black_pixel[i]);
    }

    /**** -r: rescale video frame ****/

    if (rescale) {
	preadjust_frame_size(&vtd, ptr->v_width / vob->reduce_w,
			     ptr->v_height / vob->reduce_h);
	PROCESS_FRAME(reduce, &vtd, vob->reduce_w, vob->reduce_h);
    }

    /**** -z: flip frame vertically ****/

    if (flip) {
	PROCESS_FRAME(flip_v, &vtd);
    }

    /**** -l: flip flame horizontally (mirror) ****/

    if (mirror) {
	PROCESS_FRAME(flip_h, &vtd);
    }

    /**** -k: red/blue swap ****/

    if (rgbswap) {
	if (ptr->v_codec == CODEC_RGB) {
	    int i;
	    for (i = 0; i < ptr->v_width * ptr->v_height; i++) {
		uint8_t tmp = vtd.planes[0][i*3];
		vtd.planes[0][i*3] = vtd.planes[0][i*3+2];
		vtd.planes[0][i*3+2] = tmp;
	    }
	} else {
	    int UVsize = (ptr->v_width  / vtd.width_div[1])
		       * (ptr->v_height / vtd.height_div[1]) * vtd.Bpp;
	    ac_memcpy(vtd.tmpplanes[1], vtd.planes[1], UVsize);  /* tmp<-U   */
	    ac_memcpy(vtd.planes[1], vtd.planes[2], UVsize);     /*   U<-V   */
	    ac_memcpy(vtd.planes[2], vtd.tmpplanes[1], UVsize);  /*   V<-tmp */
	}
    }

    /**** -K: grayscale ****/

    if (decolor) {
	if (ptr->v_codec == CODEC_RGB) {
	    /* Convert to 8-bit grayscale, then back to RGB24 */
	    ac_imgconvert(vtd.planes, IMG_RGB24, vtd.tmpplanes, IMG_GRAY8,
			  ptr->v_width, ptr->v_height);
	    ac_imgconvert(vtd.tmpplanes, IMG_GRAY8, vtd.planes, IMG_RGB24,
			  ptr->v_width, ptr->v_height);
	} else {
	    /* YUV is easy: just set U and V to 128 */
	    int UVsize = (ptr->v_width  / vtd.width_div[1])
		       * (ptr->v_height / vtd.height_div[1]) * vtd.Bpp;
	    memset(vtd.planes[1], 128, UVsize);
	    memset(vtd.planes[2], 128, UVsize);
	}
    }

    /**** -G: gamma correction ****/

    if (dgamma) {
	/* Only process the first plane (Y) for YUV; for RGB it's all in
	 * one plane anyway */
	gamma_correct(ptr->video_buf, ptr->video_buf, ptr->v_width,
		      ptr->v_height, vtd.Bpp);
    }

    /**** -C: antialiasing ****/

    if (vob->antialias) {
	/* Only Y is antialiased; U and V remain the same */
	antialias(vtd.planes[0], vtd.tmpplanes[0],
		  ptr->v_width, ptr->v_height, vtd.Bpp, vob->antialias);
	if (ptr->v_codec != CODEC_RGB) {
	    int UVsize = (ptr->v_width  / vtd.width_div[1])
		       * (ptr->v_height / vtd.height_div[1]) * vtd.Bpp;
	    ac_memcpy(vtd.tmpplanes[1], vtd.planes[1], UVsize);
	    ac_memcpy(vtd.tmpplanes[2], vtd.planes[2], UVsize);
	}
	swap_buffers(&vtd);
    }

    /**** End of processing ****/

    return 0;
}

/*************************************************************************/
/*************************************************************************/

/* Main video frame processing routine.  The image is passed in
 * ptr->video_buf; this can be updated as needed, e.g. to point to the
 * secondary buffer after transformations.  `vob' contains global data for
 * the transcoding operation (parameter settings and the like).
 */

int process_vid_frame(vob_t *vob, vframe_list_t *ptr)
{
    /* Check for pass-through mode or skipped/out-of-range frames */
    if(vob->pass_flag & TC_VIDEO)
	return 0;
    if (ptr->attributes & TC_FRAME_IS_SKIPPED)
	return 0;
    if (ptr->attributes & TC_FRAME_IS_OUT_OF_RANGE)
	return 0;

    /* It's a valid frame, check the colorspace for validity and process it */
    if (vob->im_v_codec == CODEC_RGB
     || vob->im_v_codec == CODEC_YUV
     || vob->im_v_codec == CODEC_YUV422
    ) {
	ptr->v_codec = vob->im_v_codec;
	return do_process_frame(vob, ptr);
    }

    /* Invalid colorspace, bail out */
    tc_error("Oops, invalid colorspace video frame data");
    return -1;
}

/*************************************************************************/

/* Frame preprocessing routine.  Checks for frame out of -c range and
 * performs early clipping.  Parameters are as for process_vid_frame().
 */

int preprocess_vid_frame(vob_t *vob, vframe_list_t *ptr)
{
    struct fc_time *t;
    int skip = 1;

    /* Set skip attribute based on -c */
    for (t = vob->ttime; t; t = t->next) {
	if (t->stf <= ptr->id && ptr->id < t->etf)  {
	    skip = 0;
	    break;
	}
    }
    if (skip) {
	ptr->attributes |= TC_FRAME_IS_OUT_OF_RANGE;
	return 0;
    }

    /* Check for pass-through mode */
    if(vob->pass_flag & TC_VIDEO)
	return 0;

    /* Check frame colorspace */
    if (vob->im_v_codec != CODEC_RGB
     && vob->im_v_codec != CODEC_YUV
     && vob->im_v_codec != CODEC_YUV422
    ) {
	tc_error("Oops, invalid colorspace video frame data");
	return -1;
    }

    /* Perform early clipping */
    if (pre_im_clip) {
	video_trans_data_t vtd;
	ptr->v_codec = vob->im_v_codec;
	set_vtd(&vtd, ptr);
	preadjust_frame_size(&vtd,
	    ptr->v_width - vob->pre_im_clip_left - vob->pre_im_clip_right,
	    ptr->v_height - vob->pre_im_clip_top - vob->pre_im_clip_bottom);
	PROCESS_FRAME(clip, &vtd,
		      vob->pre_im_clip_left   / vtd.width_div[i],
		      vob->pre_im_clip_right  / vtd.width_div[i],
		      vob->pre_im_clip_top    / vtd.height_div[i],
		      vob->pre_im_clip_bottom / vtd.height_div[i],
		      vtd.black_pixel[i]);
    }

    /* Finished with preprocessing */
    return 0;
}

/*************************************************************************/

/* Frame postprocessing routine.  Performs final clipping and sanity
 * checks.  Parameters are as for process_vid_frame().
 */

int postprocess_vid_frame(vob_t *vob, vframe_list_t *ptr)
{
    /* Check for pass-through mode or skipped/out-of-range frames */
    if(vob->pass_flag & TC_VIDEO)
	return 0;
    if (ptr->attributes & TC_FRAME_IS_SKIPPED)
	return 0;
    if (ptr->attributes & TC_FRAME_IS_OUT_OF_RANGE)
	return 0;

    /* Check frame colorspace */
    if (vob->im_v_codec != CODEC_RGB
     && vob->im_v_codec != CODEC_YUV
     && vob->im_v_codec != CODEC_YUV422
    ) {
	tc_error("Oops, invalid colorspace video frame data");
	return -1;
    }

    /* Perform final clipping */
    if (post_ex_clip) {
	video_trans_data_t vtd;
	ptr->v_codec = vob->im_v_codec;
	set_vtd(&vtd, ptr);
	preadjust_frame_size(&vtd,
	    ptr->v_width - vob->post_ex_clip_left - vob->post_ex_clip_right,
	    ptr->v_height - vob->post_ex_clip_top - vob->post_ex_clip_bottom);
	PROCESS_FRAME(clip, &vtd,
		      vob->post_ex_clip_left   / vtd.width_div[i],
		      vob->post_ex_clip_right  / vtd.width_div[i],
		      vob->post_ex_clip_top    / vtd.height_div[i],
		      vob->post_ex_clip_bottom / vtd.height_div[i],
		      vtd.black_pixel[i]);
    }

    /* Sanity check: make sure the frame size is what we're expecting */
    if (ptr->v_width != vob->ex_v_width || ptr->v_height != vob->ex_v_height) {
	printf("(%s) width %d %d | height %d %d\n", __FILE__,
	       ptr->v_width, vob->ex_v_width,
	       ptr->v_height, vob->ex_v_height);
	tc_error("Oops, frame parameter mismatch detected");
    }

    /* Finished with postprocessing */
    return 0;
}

/*************************************************************************/

