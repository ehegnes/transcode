/*
 *  probe_wav.c
 *
 *  Copyright (C) Thomas Östreich - June 2001
 *
 *  This file is part of transcode, a video stream processing tool
 *
 *  transcode is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  transcode is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
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
    int err = tc_x11source_open(&xsrc, ipipe->name, TC_X11_MODE_PLAIN);
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
