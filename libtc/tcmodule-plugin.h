/*
 * tcmodule-plugin.h - transcode module system, take two: plugin parts
 * (C) 2005-2006 - Francesco Romani <fromani -at- gmail -dot- com>
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#ifndef TCMODULE_PLUGIN_H
#define TCMODULE_PLUGIN_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdint.h>

#include "tcmodule-info.h"
#include "tcmodule-data.h"


#define TC_MODULE_SELF_CHECK(self, WHERE) do { \
    if ((self) == NULL) { \
        tc_log_error(MOD_NAME, WHERE ": " # self " is NULL"); \
        return TC_ERROR; /* catch all for filter/encoders/decoders/(de)muxers */ \
    } \
} while (0)


/*
 * plugin entry point prototype
 */
const TCModuleClass *tc_plugin_setup(void);

#endif /* TCMODULE_PLUGIN_H */
