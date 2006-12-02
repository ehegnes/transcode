/*
 * tcmodchain.c - simple module system explorer frontend
 * Copyright (C) Francesco Romani - November 2006
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#include "tcstub.h"

#define EXE "tcmodchain"

/*************************************************************************/

#define MAX_MODS     (16)

struct modrequest_ {
    char **rawdata; /* main reference */

    const char *type; /* commodity */
    const char *name; /* commodity */

    TCModule module;
};

typedef struct modrequest_ ModRequest;

static void modrequest_init(ModRequest *mods);
static int modrequest_load(TCFactory factory,
                           ModRequest *mods, const char *str);
static int modrequest_unload(TCFactory factory, ModRequest *mods);

/*************************************************************************/

static void modrequest_init(ModRequest *mods)
{
    if (mods != NULL) {
        mods->rawdata = NULL;
        mods->type = NULL;
        mods->name = NULL;
        mods->module = NULL;
    }
}

static int modrequest_load(TCFactory factory,
                           ModRequest *mods, const char *str)
{
    size_t pieces = 0;

    if (factory == NULL || mods == NULL || str == NULL) {
        tc_log_warn(EXE, "wrong parameters for modrequest_load");
        return TC_ERROR;
    }

    mods->rawdata = tc_strsplit(str, ':', &pieces);
    if (mods->rawdata == NULL || pieces != 2) {
        tc_log_warn(EXE, "malformed module string: %s", str);
        return TC_ERROR;
    }
    mods->type = mods->rawdata[0];
    mods->name = mods->rawdata[1];

    mods->module = tc_new_module(factory, mods->type, mods->name);
    if (mods->module == NULL) {
        tc_log_warn(EXE, "failed creation of module: %s", str);
        return TC_ERROR;
    }
    return TC_OK;
}

static int modrequest_unload(TCFactory factory, ModRequest *mods)
{
    if (factory == NULL || mods == NULL) {
        tc_log_warn(EXE, "wrong parameters for modrequest_load");
        return TC_ERROR;
    }

    tc_del_module(factory, mods->module);
    tc_strfreev(mods->rawdata);

    /* re-blank fields */
    modrequest_init(mods);

    return TC_OK;
}


/*************************************************************************/

enum {
    STATUS_DONE = -1, /* used internally */
    STATUS_OK = 0,
    STATUS_BAD_PARAM,
    STATUS_MODULE_ERROR,
    STATUS_MODULE_MISMATCH,
};

void version(void)
{
    printf("%s v%s (C) 2006 transcode team\n",
           EXE, VERSION);
}

static void usage(void)
{
    version();
    printf("Usage: %s [options] module1 module2 [module3...]\n", EXE);
    printf("    -d verbosity      Verbosity mode [1 == TC_INFO]\n");
    printf("    -m path           Use PATH as module path\n");
}

int main(int argc, char *argv[])
{
    int ch, ret, i;
    const char *modpath = MOD_PATH;
    ModRequest mods[MAX_MODS];
    int modsnum = 0, matches = 0;
    int status = STATUS_OK; /* let's be optimisc, once in lifetime */

    /* needed by filter modules */
    TCVHandle tcv_handle = tcv_init();
    TCFactory factory = NULL;

    ac_init(AC_ALL);
    tc_set_config_dir(NULL);

    filter[0].id = 0; /* to make gcc happy */

    if (argc < 3) {
        usage();
        return STATUS_BAD_PARAM;
    }

    for (i = 0; i < MAX_MODS; i++) {
        modrequest_init(&mods[i]);
    }

    libtc_init(&argc, &argv);

    while (1) {
        ch = getopt(argc, argv, "d:?vhm:");
        if (ch == -1) {
            break;
        }

        switch (ch) {
          case 'd':
            if (optarg[0] == '-') {
                usage();
                return STATUS_BAD_PARAM;
            }
            verbose = atoi(optarg);
            break;
          case 'm':
            modpath = optarg;
            break;
          case 'v':
            version();
            return STATUS_OK;
          case '?': /* fallthrough */
          case 'h': /* fallthrough */
          default:
            usage();
            return STATUS_OK;
        }
    }

    /* 
     * we can't distinguish from OMS and NMS modules at glance, so try
     * first using new module system
     */
    factory = tc_new_module_factory(modpath, verbose);

    for (i = optind; i < argc; i++) {
        modrequest_load(factory, &mods[modsnum], argv[i]);
        modsnum++;
    }

    status = STATUS_OK;
    if (modsnum >= 2) {
        /* N modules, so N - 1 interfaces */
        for (i = 0; i < modsnum - 1; i++) {
            ret = tc_module_info_match(TC_CODEC_ANY,
                                       tc_module_get_info(mods[i  ].module),
                                       tc_module_get_info(mods[i+1].module));
            matches += ret;

            if (verbose) {
                tc_log_info(EXE, "%s:%s | %s:%s [%s]",
                                 mods[i  ].type, mods[i  ].name,
                                 mods[i+1].type, mods[i+1].name,
                                 (ret == 1) ?"OK" :"MISMATCH"); 
            }
        }
        if (matches < modsnum - 1) {
            status = STATUS_MODULE_MISMATCH;
        }
    }

    if (verbose) {
        if (status == STATUS_OK) {
            tc_log_info(EXE, "module chain OK");
        } else {
            tc_log_info(EXE, "module chain ILLEGAL");
        }
    }

    for (i = 0; i < modsnum; i++) {
        modrequest_unload(factory, &mods[i]);
    }
 
    ret = tc_del_module_factory(factory);
    tcv_free(tcv_handle);
    return status;
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
