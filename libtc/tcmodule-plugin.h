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


#define TC_HAS_FEATURE(flags, feat) \
    ((flags & (TC_MODULE_FEATURE_ ## feat)) ?1 :0)

static inline int tc_module_av_check(uint32_t flags)
{
    int i = 0;

    i += TC_HAS_FEATURE(flags, AUDIO);
    i += TC_HAS_FEATURE(flags, VIDEO);
    i += TC_HAS_FEATURE(flags, EXTRA);

    return i;
}

static inline int tc_module_cap_check(uint32_t flags)
{
    int i = 0;

    i += TC_HAS_FEATURE(flags, DECODE);
    i += TC_HAS_FEATURE(flags, FILTER);
    i += TC_HAS_FEATURE(flags, ENCODE);
    i += TC_HAS_FEATURE(flags, MULTIPLEX);
    i += TC_HAS_FEATURE(flags, DEMULTIPLEX);

    return i;
}

#undef TC_HAS_FEATURE


#define TC_MODULE_INIT_CHECK(self, FEATURES, feat) do { \
    int j = tc_module_cap_check((feat)); \
    \
    if ((!((FEATURES) & TC_MODULE_FEATURE_MULTIPLEX) \
      && !((FEATURES) & TC_MODULE_FEATURE_DEMULTIPLEX)) \
     && (tc_module_av_check((feat)) > 1)) { \
        tc_log_error(MOD_NAME, "unsupported stream types for" \
                           " this module instance"); \
    return TC_ERROR; \
    } \
    \
    if (j != 0 && j != 1) { \
        tc_log_error(MOD_NAME, "feature request mismatch for" \
                           " this module instance (req=%i)", j); \
    return TC_ERROR; \
    } \
    /* is perfectly fine to request to do nothing */ \
    if ((feat != 0) && ((FEATURES) & (feat))) { \
        (self)->features = (feat); \
    } else { \
        tc_log_error(MOD_NAME, "this module does not support" \
                               " requested feature"); \
        return TC_ERROR; \
    } \
} while (0)


/*
 * plugin entry point prototype
 */
const TCModuleClass *tc_plugin_setup(void);

/* TODO: unify in a proper way OLDINTERFACE and OLDINTERFACE_M */

#define TC_FILTER_OLDINTERFACE(name) \
    /* Old-fashioned module interface. */ \
    static TCModuleInstance mod; \
    \
    int tc_filter(frame_list_t *frame, char *options) \
    { \
        if (frame->tag & TC_FILTER_INIT) { \
            if (name ## _init(&mod, TC_MODULE_FEATURE_FILTER) < 0) { \
                return TC_ERROR; \
            } \
            return name ## _configure(&mod, options, tc_get_vob()); \
        \
        } else if (frame->tag & TC_FILTER_GET_CONFIG) { \
            return name ## _get_config(&mod, options); \
        \
        } else if (frame->tag & TC_FILTER_CLOSE) { \
            if (name ## _stop(&mod) < 0) { \
                return TC_ERROR; \
            } \
            return name ## _fini(&mod); \
        } \
        \
        return name ## _process(&mod, frame); \
    } 



#define TC_FILTER_OLDINTERFACE_INSTANCES	128

/* FIXME:
 * this uses the filter ID as an index--the ID can grow
 * arbitrarily large, so this needs to be fixed
 */
#define TC_FILTER_OLDINTERFACE_M(name) \
    /* Old-fashioned module interface. */ \
    static TCModuleInstance mods[TC_FILTER_OLDINTERFACE_INSTANCES]; \
    \
    int tc_filter(frame_list_t *frame, char *options) \
    { \
	TCModuleInstance *mod = &mods[frame->filter_id]; \
	\
        if (frame->tag & TC_FILTER_INIT) { \
            tc_log_info(MOD_NAME, "instance #%i", frame->filter_id); \
            if (name ## _init(mod, TC_MODULE_FEATURE_FILTER) < 0) { \
                return TC_ERROR; \
            } \
            return name ## _configure(mod, options, tc_get_vob()); \
        \
        } else if (frame->tag & TC_FILTER_GET_CONFIG) { \
            return name ## _get_config(mod, options); \
        \
        } else if (frame->tag & TC_FILTER_CLOSE) { \
            if (name ## _stop(mod) < 0) { \
                return TC_ERROR; \
            } \
            return name ## _fini(mod); \
        } \
        \
        return name ## _process(mod, frame); \
    } 



#endif /* TCMODULE_PLUGIN_H */
