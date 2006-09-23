/*
 * probe_wav.c - WAV probing code using wavlib.
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
#include "libtc/libtc.h"

#include <ffmpeg/avformat.h>
#include <ffmpeg/avcodec.h>

void probe_ffmpeg(info_t *ipipe)
{
    ipipe->error = 2;
#if 0
    /* to be completed */
    AVFormatContext *lavf_dmx_context = NULL;
    int ret = 0;

    close(ipipe->fd_in);

    av_register_all();
    avcodec_init();
    avcodec_register_all();

    ret = av_open_input_file(&lavf_dmx_context, ipipe->name,
                             NULL, 0, NULL);
    if (ret != 0) {
        tc_log_error(__FILE__, "unable to open '%s'"
                               " (libavformat failure)",
                     ipipe->name);
        ipipe->error = 1;
        return;
    }

    ret = av_find_stream_info(lavf_dmx_context);
    if (ret < 0) {
        tc_log_error(__FILE__, "unable to fetch informations from '%s'"
                               " (libavformat failure)",
                     ipipe->name);
        ipipe->error = 1;
        return;
    }

    /* translate probing */

    av_close_input_file(lavf_dmx_context);
    return;
#endif
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
