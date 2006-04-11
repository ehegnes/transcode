/*
 * tcmodule-plugin.h - transcode module system, take two: plugin parts
 * (C) 2005 - Francesco Romani <fromani -at- gmail -dot- com>
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

#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif

#include "tccodecs.h"
#include "tcmodule-info.h"
#include "tcmodule-data.h"

/* 
 * default size for configure string length 
 * XXX: replece with TC_BUF_MIN or something like it?
 */
#define CONF_STR_SIZE   128

/*
 * plugin entry point prototype
 */
const TCModuleClass *tc_plugin_setup(void);

#endif /* TCMODULE_PLUGIN_H */
