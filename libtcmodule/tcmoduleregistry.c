/*
 * tcmoduleregistry.c -- module information registry (implementation).
 * (C) 2009 - Francesco Romani <fromani -at- gmail -dot- com>
 *
 * This file is part of transcode, a video stream processing tool.
 *
 * transcode is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * transcode is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "tccore/tc_defaults.h"

#include "libtc/mediainfo.h"
#include "libtcutil/tcutil.h"

#include "tcmodule-data.h"
#include "tcmodule-info.h"
#include "tcmodule-registry.h"


#define REGISTRY_CONFIG_FILE    "modules.cfg"

/*************************************************************************/

struct tcregistry_ {
    TCFactory   factory;
    int         verbose;
    const char  *reg_path;
};

#define RETURN_IF_NULL(ptr, msg, errval) do { \
    if (!ptr) { \
        tc_log_error(__FILE__, msg); \
        return (errval); \
    } \
} while (0)


#define RETURN_IF_INVALID_STRING(str, msg, errval) do { \
    if (!str || !strlen(str)) { \
        tc_log_error(__FILE__, msg); \
        return (errval); \
    } \
} while (0)


#define RETURN_IF_INVALID_QUIET(val, errval) do { \
    if (!(val)) { \
        return (errval); \
    } \
} while (0)

const char *tc_module_registry_default_path(void)
{
    return REGISTRY_PATH;
}

TCRegistry tc_new_module_registry(TCFactory factory,
                                  const char *regpath, int verbose)
{
    TCRegistry registry = NULL;
    RETURN_IF_INVALID_QUIET(factory, NULL);

    registry = tc_zalloc(sizeof(struct tcregistry_));
    RETURN_IF_INVALID_QUIET(registry, NULL);

    if (regpath) {
        registry->reg_path = regpath;
    } else {
        registry->reg_path = REGISTRY_PATH;
    }
    registry->verbose  = verbose;
    registry->factory  = factory; /* soft reference */

    return registry;
}

int tc_del_module_registry(TCRegistry registry)
{
    RETURN_IF_INVALID_QUIET(registry, 1);

    tc_free(registry);
    return TC_OK;
}

typedef struct formatmodules_ FormatModules;
struct formatmodules_ {
    char *demuxer;
    char *decoder;
    char *encoder;
    char *muxer;
};

static void fmt_mods_init(FormatModules *fm)
{
    if (fm) {
        fm->demuxer = NULL;
        fm->decoder = NULL;
        fm->encoder = NULL;
        fm->muxer   = NULL;
    }
}

#define FREE_IF_SET(PPTR) do { \
    if (*(PPTR)) { \
        tc_free(*(PPTR)); \
        *(PPTR) = NULL; \
    } \
} while (0);

static void fmt_mods_fini(FormatModules *fm)
{
    FREE_IF_SET(&(fm->demuxer));
    FREE_IF_SET(&(fm->decoder));
    FREE_IF_SET(&(fm->encoder));
    FREE_IF_SET(&(fm->muxer));
}

/* yes, this sucks. Badly. */
static const char *fmt_mods_for_class(FormatModules *fm,
                                      const char *modclass)
{
    const char *modname = NULL;

    if (modclass != NULL) {
        if (!strcmp(modclass, "demultiplex")
          || !strcmp(modclass, "demux")) {
            modname = fm->demuxer;
        } else if (!strcmp(modclass, "decode")) {
            modname = fm->decoder;
        } else if (!strcmp(modclass, "encode")) {
            modname = fm->encoder;
        } else if (!strcmp(modclass, "multiplex")
          || !strcmp(modclass, "mplex")) {
            modname = fm->muxer;
        }
    }

    return modname;
}

#define MOD_NAME_LIST_SEP   ','

static TCModule load_first_in_list(TCFactory factory, const char *modclass,
                                   const char *namelist, int media)
{
    TCModule mod = NULL;
    size_t i = 0, num = 0;
    char **names = tc_strsplit(namelist, MOD_NAME_LIST_SEP, &num);

    if (names) {
        for (i = 0; names[i]; i++) {
            mod = tc_new_module(factory, modclass, names[i], media);
        }

        tc_strfreev(names);
    }

    return mod;
}

TCModule tc_new_module_for_format(TCRegistry registry,
                                  const char *modclass,
                                  const char *format,
                                  int media)
{
    const char *dirs[3] = { NULL }; /* placeholder */
    FormatModules fm;
    TCConfigEntry registry_conf[] = { 
        { "demuxer", &(fm.demuxer), TCCONF_TYPE_STRING, 0, 0, 0 },
        { "decoder", &(fm.decoder), TCCONF_TYPE_STRING, 0, 0, 0 },
        { "encoder", &(fm.encoder), TCCONF_TYPE_STRING, 0, 0, 0 },
        { "muxer",   &(fm.muxer),   TCCONF_TYPE_STRING, 0, 0, 0 },
        { NULL, NULL, 0, 0, 0, 0 }
    };
    TCModule mod = NULL;
    int ret;

    RETURN_IF_INVALID_STRING(modclass, "empty module class", NULL);
    RETURN_IF_INVALID_STRING(modclass, "empty format name", NULL);
    RETURN_IF_NULL(registry, "invalid registry reference", NULL);

    fmt_mods_init(&fm);
    
    dirs[0] = ".";
    dirs[1] = registry->reg_path;
    dirs[2] = NULL;

    ret = tc_config_read_file(dirs, REGISTRY_CONFIG_FILE,
                              format, registry_conf, __FILE__);
                                  
    if (ret) {
        const char *modnames = fmt_mods_for_class(&fm, modclass);
        if (modnames) {
            mod = load_first_in_list(registry->factory, modclass,
                                     modnames, media);
        } else {
            tc_log_warn(__FILE__,
                        "no module in registry for class=%s format=%s",
                        modclass, format);
        }
    }

    fmt_mods_fini(&fm);

    return mod;
}

TCModule tc_new_module_most_fit(TCRegistry registry,
                                const char *modclass,
                                const char *fmtname, const char *modname,
                                int media)
{
    TCModule mod = NULL;
    
    RETURN_IF_INVALID_STRING(modclass, "empty module class", NULL);
    RETURN_IF_NULL(registry, "invalid registry reference", NULL);

    if (modname) {
        mod = tc_new_module(registry->factory, modclass, modname, media);
    } else if (fmtname) {
        mod = tc_new_module_for_format(registry, modclass, fmtname, media);
    } else {
        tc_log_warn(__FILE__, "missing both format name and module name");
    }
    return mod;
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
