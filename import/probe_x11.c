/*
 * probe_x11.c - X11 source probing code adaptor.
 * (C) 2006 - Francesco Romani <fromani -at- gmail -dot- com>
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#include "transcode.h"
#include "tcinfo.h"
#include "ioaux.h"
#include "tc.h"
#include "x11source.h"
#include "libtc/libtc.h"

void probe_x11(info_t *ipipe)
{
#ifdef HAVE_X11
    TCX11Source xsrc;
    int err = tc_x11source_open(&xsrc, ipipe->name,
                                TC_X11_MODE_PLAIN, TC_CODEC_RGB);
    /* performances and colorspaces doesn't really matters here */
    if (err == 0) {
        tc_x11source_probe(&xsrc, ipipe->probe_info);
        tc_x11source_close(&xsrc);
    }
#else /* HAVE_x11 */
    tc_log_error(__FILE__, "No support for X11 compiled in");
    ipipe->probe_info->codec = TC_CODEC_UNKNOWN;
    ipipe->probe_info->magic = TC_MAGIC_UNKNOWN;
#endif /* HAVE_X11 */
    return;
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
