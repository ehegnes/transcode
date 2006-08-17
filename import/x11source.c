/*
 * x11source.c - X11/transcode bridge code, allowing screen capture.
 * (C) 2006 - Francesco Romani <fromani -at- gmail -dot- com>
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#include "config.h"

#include <string.h>

#include "transcode.h"

#include "magic.h"
#include "x11source.h"

#include "libtc/ratiocodes.h"
#include "libtc/tccodecs.h"
#include "libtc/tcframes.h"
#include "aclib/imgconvert.h"

#ifdef HAVE_X11

int tc_x11source_open(TCX11Source *handle, const char *display)
{
    XWindowAttributes winfo;

    if (handle == NULL) {
        return 1;
    }

    handle->dpy = XOpenDisplay(display);
    if (handle->dpy == NULL) {
        tc_log_error(__FILE__, "failed to open display %s",
                     (display != NULL) ?display :"default");
        goto open_failed;
    }
    handle->root = RootWindow(handle->dpy, DefaultScreen(handle->dpy));
    /* Get the parameters of the root winfow */
    if (!XGetWindowAttributes(handle->dpy, handle->root, &winfo)) {
        tc_log_error(__FILE__, "failed to get root window attributes");
        goto link_failed;
    }

    handle->width = winfo.width;
    handle->height = winfo.height;
    handle->depth = winfo.depth;

    if (handle->depth != 24) { /* XXX */
        tc_log_error(__FILE__, "Non-truecolor display depth"
                               " not supported. Yet.");
        goto link_failed;
    }

    if (verbose >= TC_STATS) {
        tc_log_info(__FILE__, "display properties: %ix%i@%i", 
                    handle->width, handle->height, handle->depth);
    }

    handle->pix = XCreatePixmap(handle->dpy, handle->root,
                                handle->width, handle->height,
                                handle->depth); /* XXX */
    if (!handle->pix) {
        tc_log_error(__FILE__, "Can't allocate Pixmap");
        goto pix_failed;
    }
 
	handle->gc = XCreateGC(handle->dpy, handle->root, 0, 0);
    /* FIXME: what about failures? */
    return 0;

pix_failed:
    XFreePixmap(handle->dpy, handle->pix);
link_failed:
    XCloseDisplay(handle->dpy);
open_failed:
    return -1;
}

int tc_x11source_close(TCX11Source *handle)
{
    if (handle != NULL) {
        if (handle->dpy != NULL) {
            int ret = 0;

            XFreePixmap(handle->dpy, handle->pix); /* XXX */
	        XFreeGC(handle->dpy, handle->gc); /* XXX */

            ret = XCloseDisplay(handle->dpy);
            if (ret == 0) {
                handle->dpy = NULL;
            } else {
                tc_log_error(__FILE__, "XCloseDisplay() failed: %i", ret);
                return -1;
            }
        }
    }
    return 0;
}

int tc_x11source_probe(TCX11Source *handle, ProbeInfo *info)
{
    if (handle != NULL && info != NULL) {
        info->width = handle->width;
        info->height = handle->height;
        info->codec = TC_CODEC_RGB;
        info->magic = TC_MAGIC_X11; /* enforce */
        info->asr = 1; /* force 1:1 ASR (XXX) */
        /* FPS/FRC MUST BE choosed by user; that's only a kind suggestion */
        info->fps = 10.0;
        tc_frc_code_from_value(&info->frc, info->fps);
        
        info->num_tracks = 0; /* no audio, here */
        return 0;
    }

    return 1;
}

int tc_x11source_acquire(TCX11Source *handle, uint8_t *data, int maxdata)
{
    int size = -1;
    XImage *image = NULL;

    if (handle == NULL || data == NULL || maxdata <= 0) {
        tc_log_error(__FILE__, "x11source_acquire: wrong (NULL) parameters");
        return size;
    }

    XLockDisplay(handle->dpy);
    /* OK, let's hack a bit our GraphicContext */
    XSetSubwindowMode(handle->dpy, handle->gc, IncludeInferiors);
    /* don't catch areas of windows covered by children windows */
    XCopyArea(handle->dpy, handle->root, handle->pix, handle->gc,
              0, 0, handle->width, handle->height, 0, 0);

    XSetSubwindowMode(handle->dpy, handle->gc, ClipByChildren);
    /* but draw such areas if windows are opaque */
    image = XGetImage(handle->dpy, handle->pix, 0, 0, 
                      handle->width, handle->height,
                      AllPlanes, ZPixmap);

    if (image == NULL || image->data == NULL) {
        tc_log_error(__FILE__, "x11source_acquire: cannot get X image");
    } else {
        size = (int)tc_video_frame_size(image->width, image->height,
                                    TC_CODEC_RGB);
        if (size <= maxdata) {
            ac_imgconvert((uint8_t**)&image->data, IMG_RGBA32,
                          (uint8_t**)&data,        IMG_RGB24,
                          image->width, image->height);
        } else {
            size = 0;
        }
        XDestroyImage(image);
    }
    
    XUnlockDisplay(handle->dpy);
    return size;
}

int tc_x11source_is_display_name(const char *name)
{
    if (name != NULL && strlen(name) != 0) {
        uint32_t disp, screen;
        int ret = sscanf(name, ":%u.%u", &disp, &screen);
        if (ret == 2) {
            /* looks like a display specifier */
            return TC_TRUE;
        }
    }
    return TC_FALSE;
}

#else /* HAVE_X11 */

int tc_x11source_open(TCX11Source *handle, const char *display)
{
    tc_log_error(__FILE__, "X11 support unavalaible");
    return -1;
}

int tc_x11source_close(TCX11Source *handle)
{
    tc_log_error(__FILE__, "X11 support unavalaible");
    return 0;
}

int tc_x11source_probe(TCX11Source *handle, ProbeInfo *info)
{
    tc_log_error(__FILE__, "X11 support unavalaible");
    return -1;
}

int tc_x11source_acquire(TCX11Source *handle, uint8_t *data, int maxdata)
{
    tc_log_error(__FILE__, "X11 support unavalaible");
    return -1;
}

int tc_x11source_is_display_name(const char *name)
{
    return TC_FALSE;
}

#endif /* HAVE_X11 */

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
