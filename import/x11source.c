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

/*************************************************************************/

static int tc_x11source_acquire_data_plain(TCX11Source *handle,
                                           uint8_t *data, int maxdata)
{
    int size = -1;

    /* but draw such areas if windows are opaque */
    /* FIXME: switch to XCreateImage? */
    handle->image = XGetImage(handle->dpy, handle->pix, 0, 0, 
                              handle->width, handle->height,
                              AllPlanes, ZPixmap);

    if (handle->image == NULL || handle->image->data == NULL) {
        tc_log_error(__FILE__, "cannot get X image");
    } else {
        size = (int)tc_video_frame_size(handle->image->width,
                                        handle->image->height,
                                        TC_CODEC_RGB);
        if (size <= maxdata) {
            ac_imgconvert((uint8_t**)&handle->image->data, IMG_BGRA32,
                          (uint8_t**)&data,                IMG_RGB24,
                          handle->image->width, handle->image->height);
        } else {
            size = 0;
        }
        XDestroyImage(handle->image);
    }
    return size;
}

static int tc_x11source_fini_plain(TCX11Source *handle)
{
    return 0;
}

static int tc_x11source_init_plain(TCX11Source *handle)
{
    handle->acquire_data = tc_x11source_acquire_data_plain;
    handle->fini = tc_x11source_fini_plain;
    return 0;
}


/*************************************************************************/

#ifdef HAVE_X11_SHM

static int tc_x11source_acquire_data_shm(TCX11Source *handle,
                                          uint8_t *data, int maxdata)
{
    int size = -1;
    Status ret;

    /* but draw such areas if windows are opaque */
    ret = XShmGetImage(handle->dpy, handle->pix, handle->image,
                       0, 0, AllPlanes);

    if (!ret || handle->image->data == NULL) {
        tc_log_error(__FILE__, "cannot get X image (using SHM)");
    } else {
        size = (int)tc_video_frame_size(handle->image->width,
                                        handle->image->height,
                                        TC_CODEC_RGB);
        if (size <= maxdata) {
            ac_imgconvert((uint8_t**)&handle->image->data, IMG_BGRA32,
                          (uint8_t**)&data,                IMG_RGB24,
                          handle->image->width, handle->image->height);
        } else {
            size = 0;
        }
    }
    return size;
}

static int tc_x11source_fini_shm(TCX11Source *handle)
{
    Status ret = XShmDetach(handle->dpy, &handle->shm_info);
    if (!ret) { /* XXX */
        tc_log_error(__FILE__, "failed to attach SHM from Xserver");
        return -1;
    }
    XDestroyImage(handle->image);
    handle->image = NULL;

    XSync(handle->dpy, False); /* XXX */
    if (shmdt(handle->shm_info.shmaddr) != 0) {
        tc_log_error(__FILE__, "failed to destroy shared memory segment");
        return -1;
    }
    return 0;
}

static int tc_x11source_init_shm(TCX11Source *handle)
{
    Status ret;

    ret = XMatchVisualInfo(handle->dpy, handle->screen, handle->depth,
                           DirectColor, &handle->vis_info);
    if (!ret) {
        tc_log_error(__FILE__, "Can't match visual information");
        goto xshm_failed;
    }
    handle->image = XShmCreateImage(handle->dpy, handle->vis_info.visual,
                                    handle->depth, ZPixmap,
                                    NULL, &handle->shm_info,
                                    handle->width, handle->height);
    if (handle->image == NULL) {
        tc_log_error(__FILE__, "XShmCreateImage failed.");
        goto xshm_failed_image;
    }
    handle->shm_info.shmid = shmget(IPC_PRIVATE,
                                    handle->image->bytes_per_line * handle->image->height,
                                    IPC_CREAT | 0777);
    if (handle->shm_info.shmid < 0) {
        tc_log_error(__FILE__, "failed to create shared memory segment");
        goto xshm_failed_image;
    }
    handle->shm_info.shmaddr = shmat(handle->shm_info.shmid, NULL, 0);
    if (handle->shm_info.shmaddr == (void*)-1) {
        tc_log_error(__FILE__, "failed to attach shared memory segment");
        goto xshm_failed_image;
    }
    
    shmctl(handle->shm_info.shmid, IPC_RMID, 0); /* XXX */

    handle->image->data = handle->shm_info.shmaddr;
    handle->shm_info.readOnly = False;

    ret = XShmAttach(handle->dpy, &handle->shm_info);
    if (!ret) {
        tc_log_error(__FILE__, "failed to attach SHM to Xserver");
        goto xshm_failed_image;
    }

    XSync(handle->dpy, False);
    handle->mode = TC_X11_MODE_SHM;
    handle->acquire_data = tc_x11source_acquire_data_shm;
    handle->fini = tc_x11source_fini_shm;

    return 0;

xshm_failed_image:
    XDestroyImage(handle->image);
    handle->image = NULL;
xshm_failed:
    return -1;
}

/*************************************************************************/

#endif /* X11_SHM */


int tc_x11source_acquire(TCX11Source *handle, uint8_t *data, int maxdata)
{
    int size = -1;

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
    
    size = handle->acquire_data(handle, data, maxdata);
    
    XUnlockDisplay(handle->dpy);
    return size;
}

int tc_x11source_close(TCX11Source *handle)
{
    if (handle != NULL) {
        if (handle->dpy != NULL) {
            int ret = handle->fini(handle);
            if (ret != 0) {
                return ret;
            }

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

int tc_x11source_open(TCX11Source *handle, const char *display, int mode)
{
    XWindowAttributes winfo;
    Status ret;

    if (handle == NULL) {
        return 1;
    }

    handle->mode = mode;
    handle->dpy = XOpenDisplay(display);
    if (handle->dpy == NULL) {
        tc_log_error(__FILE__, "failed to open display %s",
                     (display != NULL) ?display :"default");
        goto open_failed;
    }

    handle->screen = DefaultScreen(handle->dpy);
    handle->root = RootWindow(handle->dpy, handle->screen);
    /* Get the parameters of the root winfow */
    ret = XGetWindowAttributes(handle->dpy, handle->root, &winfo);
    if (!ret) {
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

#ifdef HAVE_X11_SHM
    if (XShmQueryExtension(handle->dpy) != 0
      && (mode & TC_X11_MODE_SHM)) {
        tc_log_info(__FILE__, "using XShm extension");
        return tc_x11source_init_shm(handle);
    }
#endif /* X11_SHM */
    return tc_x11source_init_plain(handle);

pix_failed:
    XFreePixmap(handle->dpy, handle->pix);
link_failed:
    XCloseDisplay(handle->dpy);
open_failed:
    return -1;
}


#else /* HAVE_X11 */


int tc_x11source_open(TCX11Source *handle, const char *display, int mode)
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
