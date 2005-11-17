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
#include "aclib/imgconvert.h"
#include "libtcvideo/tcvideo.h"

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
#define PROCESS_FRAME(func,vtd,args...) do {                    \
    int i;                                                      \
    for (i = 0; i < (vtd)->nplanes; i++) {                      \
        func((vtd)->planes[i], (vtd)->tmpplanes[i],             \
             (vtd)->ptr->v_width / (vtd)->width_div[i],         \
             (vtd)->ptr->v_height / (vtd)->height_div[i],       \
             (vtd)->Bpp , ## args);                             \
    }                                                           \
    swap_buffers(vtd);                                          \
} while (0)

/*************************************************************************/
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

/*************************************************************************/

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

/*************************************************************************/

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

/* -I: Deinterlace the frame.  `mode' is the processing mode (-I parameter). */

static void deinterlace(video_trans_data_t *vtd, int mode)
{
    if (mode == 1) {
        /* Simple linear interpolation */
        PROCESS_FRAME(tcv_deinterlace, vtd, TCV_DEINTERLACE_INTERPOLATE);
    } else if (mode == 3 || mode == 4) {
        /* Drop every other line (and zoom back out in mode 3) */
        preadjust_frame_size(vtd, vtd->ptr->v_width, vtd->ptr->v_height/2);
        PROCESS_FRAME(tcv_deinterlace, vtd, TCV_DEINTERLACE_DROP_FIELD);
        if (mode == 3) {
            int w = vtd->ptr->v_width, h = vtd->ptr->v_height*2;
            vob_t *vob = tc_get_vob();
            preadjust_frame_size(vtd, w, h);
            PROCESS_FRAME(tcv_zoom, vtd, w, h, vob->zoom_filter);
        }
    } else if (mode == 5) {
        /* Linear blend */
        PROCESS_FRAME(tcv_deinterlace, vtd, TCV_DEINTERLACE_LINEAR_BLEND);
    } else {
        /* Mode 2 (handled by encoder) or unknown: do nothing */
        return;
    }
    vtd->ptr->attributes &= ~TC_FRAME_IS_INTERLACED;
}

/*************************************************************************/

static int do_process_frame(vob_t *vob, vframe_list_t *ptr)
{
    video_trans_data_t vtd;  /* for passing to subroutines */

    set_vtd(&vtd, ptr);

    /**** -j: clip frame (import) ****/

    if (im_clip) {
        preadjust_frame_size(&vtd,
                ptr->v_width - vob->im_clip_left - vob->im_clip_right,
                ptr->v_height - vob->im_clip_top - vob->im_clip_bottom);
        PROCESS_FRAME(tcv_clip, &vtd,
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
            PROCESS_FRAME(tcv_resize, &vtd, 0, resize_h, 8/vtd.width_div[i],
                          8/vtd.height_div[i]);
            height += resize_h * 8;
        }
        if (resize_w) {
            preadjust_frame_size(&vtd, width+resize_w*8, height);
            PROCESS_FRAME(tcv_resize, &vtd, resize_w, 0, 8/vtd.width_div[i],
                          8/vtd.height_div[i]);
        }
    }

    /**** -Z: zoom frame (slow resize) ****/

    if (zoom) {
        preadjust_frame_size(&vtd, vob->zoom_width, vob->zoom_height);
        PROCESS_FRAME(tcv_zoom, &vtd, vob->zoom_width / vtd.width_div[i],
                      vob->zoom_height / vtd.height_div[i], vob->zoom_filter);
    }

    /**** -Y: clip frame (export) ****/

    if (ex_clip) {
        preadjust_frame_size(&vtd,
                ptr->v_width - vob->ex_clip_left-vob->ex_clip_right,
                ptr->v_height - vob->ex_clip_top - vob->ex_clip_bottom);
        PROCESS_FRAME(tcv_clip, &vtd,
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
        PROCESS_FRAME(tcv_reduce, &vtd, vob->reduce_w, vob->reduce_h);
    }

    /**** -z: flip frame vertically ****/

    if (flip) {
        PROCESS_FRAME(tcv_flip_v, &vtd);
    }

    /**** -l: flip flame horizontally (mirror) ****/

    if (mirror) {
        PROCESS_FRAME(tcv_flip_h, &vtd);
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
        tcv_gamma_correct(ptr->video_buf, ptr->video_buf, ptr->v_width,
                          ptr->v_height, vtd.Bpp, vob->gamma);
    }

    /**** -C: antialiasing ****/

    if (vob->antialias) {
        /* Only Y is antialiased; U and V remain the same */
        tcv_antialias(vtd.planes[0], vtd.tmpplanes[0],
                      ptr->v_width, ptr->v_height, vtd.Bpp,
                      vob->aa_weight, vob->aa_bias);
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
        PROCESS_FRAME(tcv_clip, &vtd,
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
        PROCESS_FRAME(tcv_clip, &vtd,
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

/*
 * Local variables:
 *   c-file-style: "stroustrup"
 *   c-file-offsets: ((case-label . *) (statement-case-intro . *))
 *   indent-tabs-mode: nil
 * End:
 *
 * vim: expandtab shiftwidth=4:
 */
