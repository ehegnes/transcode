/**
 *  @file filter_null.c Demo filter
 *
 *  Copyright (C) Thomas Oestreich - June 2001
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

/*
 * ChangeLog:
 * v0.2 (2003-09-04)
 *
 * v0.3 (2005-01-02) Thomas Wehrspann
 *    -Documentation added
 *    -New help function
 *    -optstr_filter_desc now returns
 *     the right capability flags
 * v0.4 (2005-11-25) Francesco Romani
 *    - port (as rolling demo :) ) to new module system
 * v0.4.1 (2005-12-09) Francesco Romani
 *    - configure shakeup
 */

/// Name of the filter
#define MOD_NAME    "filter_null.so"

/// Version of the filter
#define MOD_VERSION "v0.4.2 (2005-12-29)"

/// A short description
#define MOD_CAP     "demo filter plugin; does nothing"

/// Author of the filter plugin
#define MOD_AUTHOR  "Thomas Oestreich, Thomas Wehrspann"

#define MOD_FEATURES \
    TC_MODULE_FEATURE_FILTER|TC_MODULE_FEATURE_VIDEO|TC_MODULE_FEATURE_AUDIO

#define MOD_FLAGS \
    TC_MODULE_FLAG_RECONFIGURABLE


/* -------------------------------------------------
 *
 * mandatory include files
 *
 *-------------------------------------------------*/

#include "transcode.h"
#include "filter.h"
#include "libtc/optstr.h"

#include "libtc/tcmodule-plugin.h"


static const char null_help[] = ""
    "Overview:\n"
    "    This filter exists for demonstration purposes only; it doesn nothing.\n"
    "Options:\n"
    "    help    produce module overview and options explanations\n";

/**
 * Help text.
 * This function prints out a small description of this filter and
 * the command-line options when the "help" parameter is given
 *********************************************************/
static void help_optstr(void)
{
  tc_log_info(MOD_NAME, "help : * Overview");
  tc_log_info(MOD_NAME,
             "help :     This exists for demonstration purposes only. "
             "It does NOTHING!");
  tc_log_info(MOD_NAME, "help :");
  tc_log_info(MOD_NAME, "help : * Options");
  tc_log_info(MOD_NAME, "help :         'help' Prints out this help text");
}

static int null_init(TCModuleInstance *self, uint32_t features)
{
    TC_MODULE_INIT_CHECK(self, MOD_FEATURES, features);

    /* following very close old module model... */
    if (verbose) {
        tc_log_info(MOD_NAME, "%s %s", MOD_VERSION, MOD_CAP);
    }

    return TC_OK;
}

static int null_fini(TCModuleInstance *self)
{
    return TC_OK;
}

static int null_configure(TCModuleInstance *self,
			  const char *options, vob_t *vob)
{
    return TC_OK;
}

static int null_inspect(TCModuleInstance*self,
                        const char *param, const char **value)
{
    if (optstr_lookup(param, "help")) {
        *value = null_help;
    }
    return TC_OK;
}

static int null_stop(TCModuleInstance *self)
{
    return TC_OK;
}

static int null_filter(TCModuleInstance *self,
                       vframe_list_t *frame)
{
    int pre = TC_FALSE, vid = TC_FALSE;

    if (!frame) {
        return -1;
    }

    if (verbose & TC_STATS) {
        /*
         * tag variable indicates, if we are called before
         * transcodes internal video/audo frame processing routines
         * or after and determines video/audio context
         */

        if (frame->tag & TC_PRE_M_PROCESS) {
            pre = TC_TRUE;
        }

        if (frame->tag & TC_VIDEO) {
            vid = TC_TRUE;
        }

        tc_log_info(MOD_NAME, "frame [%06d] %s %16s call",
                    frame->id, (vid) ?"(video)" :"(audio)",
                    (pre) ?"pre-process filter" :"post-process filter");
    }

    return TC_OK;
}

/**
 * Main function of a filter.
 * This is the single function interface to transcode. This is the only function needed for a filter plugin.
 * @param ptr     frame accounting structure
 * @param options command-line options of the filter
 *
 * @return TC_OK, if everything went OK.
 *********************************************************/
int tc_filter(frame_list_t *ptr_, char *options)
{
  vframe_list_t *ptr = (vframe_list_t *)ptr_;
  int pre=0, vid=0;
  vob_t *vob=NULL;

  // API explanation:
  // ================
  //
  // (1) need more infos, than get pointer to transcode global
  //     information structure vob_t as defined in transcode.h.
  //
  // (2) 'tc_get_vob' and 'verbose' are exported by transcode.
  //
  // (3) filter is called first time with TC_FILTER_INIT flag set.
  //
  // (4) make sure to exit immediately if context (video/audio) or
  //     placement of call (pre/post) is not compatible with the filters
  //     intended purpose, since the filter is called 4 times per frame.
  //
  // (5) see framebuffer.h for a complete list of frame_list_t variables.
  //
  // (6) filter is last time with TC_FILTER_CLOSE flag set

  //----------------------------------
  //
  // filter get config
  //
  //----------------------------------
  if(ptr->tag & TC_FILTER_GET_CONFIG)
{
    // Valid flags for the string of filter capabilities:
    //  "V" :  Can do Video
    //  "A" :  Can do Audio
    //  "R" :  Can do RGB
    //  "Y" :  Can do YUV
    //  "4" :  Can do YUV422
    //  "M" :  Can do Multiple Instances
    //  "E" :  Is a PRE filter
    //  "O" :  Is a POST filter
    optstr_filter_desc (options, MOD_NAME, MOD_CAP, MOD_VERSION, MOD_AUTHOR, "VARY4EO", "1");

    optstr_param (options, "help", "Prints out a short help", "", "0");
    return TC_OK;
  }

  //----------------------------------
  //
  // filter init
  //
  //----------------------------------
  if(ptr->tag & TC_FILTER_INIT) {

    if((vob = tc_get_vob())==NULL) return(-1);

    // filter init ok.

    if(verbose) tc_log_info(MOD_NAME, "%s %s", MOD_VERSION, MOD_CAP);
    if(verbose & TC_DEBUG) tc_log_info(MOD_NAME, "options=%s", options);

    // Parameter parsing
    if (options)
      if (optstr_lookup (options, "help")) {
        help_optstr();
        return TC_OK;
      }

    return TC_OK;
  }

  //----------------------------------
  //
  // filter close
  //
  //----------------------------------
  if(ptr->tag & TC_FILTER_CLOSE) {
    return TC_OK;
  }

  //----------------------------------
  //
  // filter frame routine
  //
  //----------------------------------
  // tag variable indicates, if we are called before
  // transcodes internal video/audio frame processing routines
  // or after and determines video/audio context
  if(verbose & TC_STATS) {

    tc_log_info(MOD_NAME, "%s/%s %s %s", vob->mod_path, MOD_NAME, MOD_VERSION, MOD_CAP);

    // tag variable indicates, if we are called before
    // transcodes internal video/audo frame processing routines
    // or after and determines video/audio context

    if(ptr->tag & TC_PRE_M_PROCESS) pre=1;
    if(ptr->tag & TC_POST_M_PROCESS) pre=0;

    if(ptr->tag & TC_VIDEO) vid=1;
    if(ptr->tag & TC_AUDIO) vid=0;

    tc_log_info(MOD_NAME, "frame [%06d] %s %16s call",
                    ptr->id, (vid)?"(video)":"(audio)",
                    (pre)?"pre-process filter":"post-process filter");

  }

  return TC_OK;
}

/*************************************************************************/

static const TCCodecID null_codecs_in[] = { TC_CODEC_ANY, TC_CODEC_ERROR };
static const TCCodecID null_codecs_out[] = { TC_CODEC_ANY, TC_CODEC_ERROR };
static const TCFormatID null_formats[] = { TC_FORMAT_ERROR };

static const TCModuleInfo null_info = {
    .features    = MOD_FEATURES,
    .flags       = MOD_FLAGS,
    .name        = MOD_NAME,
    .version     = MOD_VERSION,
    .description = MOD_CAP,
    .codecs_in   = null_codecs_in,
    .codecs_out  = null_codecs_out,
    .formats_in  = null_formats,
    .formats_out = null_formats
};

static const TCModuleClass null_class = {
    .info         = &null_info,

    .init         = null_init,
    .fini         = null_fini,
    .configure    = null_configure,
    .stop         = null_stop,
    .inspect      = null_inspect,

    .filter_video = null_filter,
};

extern const TCModuleClass *tc_plugin_setup(void)
{
    return &null_class;
}

