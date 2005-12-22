/*
 * tcmodule.c - transcode module system, take two.
 * (C) 2005 - Francesco Romani <fromani -at- gmail -dot- com>
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

#include "config.h"

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#else
# ifdef SYSTEM_DARWIN
#  include "libdldarwin/dlfcn.h"
# endif
#endif

#include <string.h>

#include "libtc.h"
#include "tccodecs.h"
#include "tcmodule-data.h"
#include "tcmodule-core.h"
#include "transcode.h"

#define TC_FACTORY_MAX_HANDLERS     (16)
#define MOD_TYPE_MAX_LEN            (256)

#define tc_module_init(module) \
    (module)->klass->init(&((module)->instance))
#define tc_module_fini(module) \
    (module)->klass->fini(&((module)->instance))

/* module entry point */
typedef const TCModuleClass* (*TCModuleEntry)(void);

typedef enum {
    TC_DESCRIPTOR_FREE = 0,     /* free to use */
    TC_DESCRIPTOR_CREATED,      /* reserved, but not yet registered */
    TC_DESCRIPTOR_DONE,         /* ok, all donw and ready to run */
} TCHandleStatus;

typedef struct tcmoduledescriptor_ TCModuleDescriptor;
struct tcmoduledescriptor_ {
    const char *type;       /* packed class + name using make_modtype below */
    void *so_handle;        /* used by dl*() stuff */
    TCHandleStatus status;
    TCModuleInfo info;

    /* main copy of module class data.
     * all instance pointers will refer to this. */
    TCModuleClass klass;

    int ref_count;           /* how many instances are floating around? */
};

struct tcfactory_ {
    const char *mod_path;   /* base directory for plugin search */
    int verbose;

    TCModuleDescriptor descriptors[TC_FACTORY_MAX_HANDLERS];
    int descriptor_count;

    int instance_count;
};

/*************************************************************************
 * dummy/fake default module class. Always fails complaining loudly.     *
 * Using this as default, every module class has already valid           *
 * (but sometimes useless) pointers to every method.                     *
 *************************************************************************/

#define DUMMY_HEAVY_CHECK(self, method_name) \
    if (self != NULL) { \
        tc_log_warn(self->type, "critical: module doesn't provide" \
                                " %s method", method_name); \
    } else { \
        tc_log_error(__FILE__, "critical: %s method missing AND bad" \
                               " instance pointer", method_name); \
    }


#define DUMMY_CHECK(self, method_name) \
    if (self != NULL) { \
        tc_log_warn(self->type, \
                    "module doesn't provide %s method", method_name); \
    } else { \
        tc_log_error(__FILE__, "%s method missing AND bad" \
                               " instance pointer", method_name); \
    }

static int dummy_init(TCModuleInstance *self)
{
    DUMMY_HEAVY_CHECK(self, "initialization");
    return -1;
}

static int dummy_fini(TCModuleInstance *self)
{
    DUMMY_HEAVY_CHECK(self, "finalization");
    return -1;
}

static const char *dummy_configure(TCModuleInstance *self,
                                   const char *options)
{
    DUMMY_HEAVY_CHECK(self, "configuration");
    return "";
}

static int dummy_encode_video(TCModuleInstance *self,
                              vframe_list_t *inframe,
                              vframe_list_t *outframe)
{
    DUMMY_CHECK(self, "encode_video");
    return -1;
}

static int dummy_encode_audio(TCModuleInstance *self,
                              aframe_list_t *inframe,
                              aframe_list_t *outframe)
{
    DUMMY_CHECK(self, "encode_audio");
    return -1;
}

static int dummy_decode_video(TCModuleInstance *self,
                              vframe_list_t *inframe,
                              vframe_list_t *outframe)
{
    DUMMY_CHECK(self, "decode_video");
    return -1;
}

static int dummy_decode_audio(TCModuleInstance *self,
                              aframe_list_t *inframe,
                              aframe_list_t *outframe)
{
    DUMMY_CHECK(self, "decode_audio");
    return -1;
}

static int dummy_filter_video(TCModuleInstance *self,
                              vframe_list_t *frame)
{
    DUMMY_CHECK(self, "filter_video");
    return -1;
}

static int dummy_filter_audio(TCModuleInstance *self,
                              aframe_list_t *frame)
{
    DUMMY_CHECK(self, "filter_audio");
    return -1;
}

static int dummy_multiplex(TCModuleInstance *self,
                           vframe_list_t *vframe, aframe_list_t *aframe)
{
    DUMMY_CHECK(self, "multiplex");
    return -1;
}

static int dummy_demultiplex(TCModuleInstance *self,
                             vframe_list_t *vframe, aframe_list_t *aframe)
{
    DUMMY_CHECK(self, "demultiplex");
    return -1;
}

#undef DUMMY_HEAVY_CHECK
#undef DUMMY_CHECK

static int dummy_codecs_in[] = { TC_CODEC_ANY, TC_CODEC_ERROR };
static int dummy_codecs_out[] = { TC_CODEC_ANY, TC_CODEC_ERROR };

static TCModuleInfo dummy_info = {
    TC_MODULE_FEATURE_NONE,
    TC_MODULE_FLAG_NONE,
    "dummy",
    "internal fake module class",
    "can't do anyhing",
    dummy_codecs_in,
    dummy_codecs_out
};

static const TCModuleClass dummy_class = {
    0,

    &dummy_info,

    dummy_init,
    dummy_fini,
    dummy_configure,
    
    dummy_encode_audio,
    dummy_encode_video,
    dummy_decode_audio,
    dummy_decode_video,
    dummy_filter_audio,
    dummy_filter_video,
    
    dummy_multiplex,
    dummy_demultiplex
};

/*************************************************************************
 * private helpers                                                       *
 *************************************************************************/

/*
 * is_known_modclass:
 *     validate a module class name, represented by a given string.
 *
 * Parameters:
 *     modclass: a class nome to validate.
 * Return Value:
 *     TC_TRUE if given class name can be understanded -and builded-
 *     by actual factory code.
 *     TC_FALSE otherwise
 * Side effects:
 *     None.
 * Preconditions:
 *     None.
 * Postconditions:
 *     None.
 */
static int is_known_modclass(const char *modclass)
{
    int ret = TC_FALSE;

    if (modclass != NULL) {
        if (!strcmp(modclass, "filter")) {
            ret = TC_TRUE;
        } else if (!strcmp(modclass, "demultiplex")
          || !strcmp(modclass, "demux")) {
            ret = TC_TRUE;
        } else if (!strcmp(modclass, "decode")) {
            ret = TC_TRUE;
        } else if (!strcmp(modclass, "encode")) {
            ret = TC_TRUE;
        } else if (!strcmp(modclass, "multiplex")
          || !strcmp(modclass, "mplex")) {
            ret = TC_TRUE;
        } else {
            ret = TC_FALSE;
        }
    }
    return ret;
}

/*
 * TCModuleDescriptorIter:
 *     generic iterator function on factory descriptors.
 *     In some different contexts, a iterator can be applied on all module 
 *     descriptors in a given factory. Specific iterator functions can do 
 *     arbitrary actions on descriptor data. 
 *     See below to get some usage examples.
 *
 * Parameters:
 *     desc: pointer to a TCModuleDescriptor.
 *     userdata: opaque pointer to function-specific data.
 * Return Value:
 *     0  -> keep on going
 *     <0 -> stop iteration and return code verbatim
 *     >0 -> stop iteration and return current iteration index
 * Side effects:
 *     Arbitrary, defined by specific function.
 * Preconditions:
 *     given factory (but isn't guaranteed that also descriptors are) already
 *     initialized and contains valid data.
 * Postconditions:
 *     none.
 */
typedef int (*TCModuleDescriptorIter)(TCModuleDescriptor *desc, void *userdata);

/*
 * tc_factory_foreach_descriptor:
 *     apply given iterator with given data to all internal descriptors,
 *     *both used and unused*.
 *
 * Parameters:
 *     factory: factory instance to use
 *     iterator: iterator to apply at factory descriptors
 *     userdata: opaque data to pass to iterator along with each descriptor
 *     index: pointer to an integer. If not NULL, will be filled
 *            with index of last descriptor elaborated
 * Return Value:
 *     return code of the last execution of iterator.
 * Side effects:
 *     None (see specific descriptor for this).
 * Preconditions:
 *     None.
 * Postconditions:
 *     If return value is 0, given iteratr wass applied to *all*
 *     descriptors in factory.
 */
static int tc_factory_foreach_descriptor(TCFactoryHandle factory,
                                         TCModuleDescriptorIter iterator,
                                         void *userdata,
                                         int *index)
{
    int ret, i = 0;

    if (!factory || !iterator) {
        return -1;
    }

    for (i = 0; i < TC_FACTORY_MAX_HANDLERS; i++) {
        ret = iterator(&(factory->descriptors[i]), userdata);
        if (ret != 0) {
            break;
        }
    }
    /* iteration stopped, so we mark the interruption point */
    if (ret != 0 && index != NULL) {
        *index = i;
    }
    return ret;
}

/*
 * descriptor_something: some iterator functions
 */

/*
 * descriptor_match_modtype:
 *     verify the match for a given descriptor and a given module type.
 *
 * Parameters:
 *     desc: descriptor to verify
 *     modtype_: module type to look for.
 * Return Value:
 *     1  if given descriptor has given module type,
 *     0  succesfull. 
 *     -1 if a given parameter is bogus.
 * Side effects:
 *     None.
 * Preconditions:
 *     None.
 * Postconditions:
 *     None.
 */
static int descriptor_match_modtype(TCModuleDescriptor *desc,
                                    void *modtype_)
{
    char *modtype = modtype_;
    if (!desc || !modtype) {
        return -1;
    }
    if (desc->status == TC_DESCRIPTOR_DONE
      && desc->type != NULL
      && (strcmp(desc->type, modtype) == 0)) {
        /* found it! */
        return 1;
    }
    return 0;
}

/*
 * descriptor_is_free:
 *     verify the match for a given descriptor is an unitialized one.
 *
 * Parameters:
 *     desc: descriptor to verify
 *     unused: dummy parameter to achieve API conformancy.
 * Return Value:
 *     1  if given descriptor is a free one (uninitialized),
 *     0  succesfull. 
 *     -1 if a given parameter is bogus.
 * Side effects:
 *     None.
 * Preconditions:
 *     None.
 * Postconditions:
 *     None.
 */
static int descriptor_is_free(TCModuleDescriptor *desc, void *unused)
{
    if (!desc) {
        return -1;
    }
    if (desc->status == TC_DESCRIPTOR_FREE) {
        return 1;
    }
    return 0;
}

/*
 * descriptor_init:
 *     initialize a plugin descriptor with valid defaults.
 *
 * Parameters:
 *     desc: descriptor to initialize.
 *     unused: dummy parameter to achieve API conformancy.
 * Return Value:
 *     0  succesfull.
 *     -1 if a given parameter is bogus.
 * Side effects:
 *     None.
 * Preconditions:
 *     None.
 * Postconditions:
 *     None.
 */
static int descriptor_init(TCModuleDescriptor *desc, void *unused)
{
    if (!desc) {
        return -1;
    }

    desc->status = TC_DESCRIPTOR_FREE;
    memcpy(&(desc->info), &dummy_info, sizeof(TCModuleInfo));
    desc->klass.info = &(desc->info);
    desc->type = NULL;
    desc->so_handle = NULL;
    desc->ref_count = 0;

    return 0;
}

/*
 * descriptor_fini:
 *     finalize a plugin descriptor, releasing all acquired
 *     resources.
 *
 * Parameters:
 *     desc: descriptor to finalize.
 *     unused: dummy parameter to achieve API conformancy.
 * Return Value:
 *     1  if given descriptor has still some live instances around,
 *     0  succesfull.
 *     -1 if a given parameter is bogus.
 * Side effects:
 *     A plugin will be released and unloaded (via dlclose()).
 * Preconditions:
 *     None.
 * Postconditions:
 *     None.
 */
static int descriptor_fini(TCModuleDescriptor *desc, void *unused)
{
    if (!desc) {
        return -1;
    }

    /* can't finalize an descriptor with some living instances still around */
    if (desc->ref_count > 0) {
        return 1;
    }

    if (desc->status == TC_DESCRIPTOR_DONE) {
        /* a deep copy was performed */
        tc_module_info_free(&(desc->info));
        if (desc->type != NULL) {
            tc_free((void*)desc->type);  /* avoid const warning */
        }
        if (desc->so_handle != NULL) {
            dlclose(desc->so_handle);
            desc->so_handle = NULL;
        }
        desc->status = TC_DESCRIPTOR_FREE;
    }
    return 0;
}

/* just a thin wrapper to adapt API */
static int find_by_modtype(TCFactoryHandle factory, const char *modtype)
{
    int ret, id;
    ret = tc_factory_foreach_descriptor(factory,
                                        descriptor_match_modtype,
                                        (void*)modtype, &id);
    /* ret >= 1 -> found something */
    return (ret >= 1) ?id : -1;
}

/* just a thin wrapper to adapt API */
static int find_first_free_descriptor(TCFactoryHandle factory)
{
    int ret, id;
    ret = tc_factory_foreach_descriptor(factory,
                                        descriptor_is_free,
                                        NULL, &id);
    /* ret >= 1 -> found something */
    return (ret >= 1) ?id : -1;
}

/* Yeah, is that simple. Yet. ;) */
static void make_modtype(char *buf, size_t bufsize,
                         const char *modclass, const char *modname)
{
    tc_snprintf(buf, bufsize, "%s:%s", modclass, modname);
}

/*
 * tc_module_class_copy:
 *     copy a module class into another one. Can perform
 *     a soft (reference) copy or a hard (full) one.
 *     Only non-null function pointer to plugin operations
 *     will be copied.
 *     soft copy: make the two classes points to same real data.
 *     hard copy: make two independent copies duplicating the data.
 *
 * Parameters:
 *     klass: source class to be copied.
 *     core_klass: class destionation of copy.
 *     soft_copy: boolean flag: if !0 do a soft copy, 
 *                do an hard one otherwise.
 * Return Value:
 *     0  successfull
 *     -1 given (at least) a bad TCModuleClass reference
 *     1  not enough memory to perform a full copy
 * Side effects:
 *     none.
 * Preconditions:
 *     none.
 * Postconditions:
 *     destination class is a copy of source class.
 */
static int tc_module_class_copy(const TCModuleClass *klass,
                                TCModuleClass *core_klass,
                                int soft_copy)
{
    int ret;

    if (!klass || !core_klass) {
        tc_log_error(__FILE__, "bad module class reference for setup: %s%s",
                                (!klass) ?"plugin class" :"",
                                (!core_klass) ?"core class" :"");
        return -1;
    }

    if (!klass->init || !klass->fini || !klass->configure) {
        tc_log_error(__FILE__, "can't setup a module class without "
                               "one or more mandatory methods");
        return -1;
    }

    /* register only method really provided by given class */
    core_klass->init = klass->init;
    core_klass->fini = klass->fini;
    core_klass->configure = klass->configure;

    if (klass->encode_audio != NULL) {
        core_klass->encode_audio = klass->encode_audio;
    }
    if (klass->encode_video != NULL) {
        core_klass->encode_video = klass->encode_video;
    }
    if (klass->decode_audio != NULL) {
        core_klass->decode_audio = klass->decode_audio;
    }
    if (klass->decode_video != NULL) {
        core_klass->decode_video = klass->decode_video;
    }
    if (klass->filter_audio != NULL) {
        core_klass->filter_audio = klass->filter_audio;
    }
    if (klass->filter_video != NULL) {
        core_klass->filter_video = klass->filter_video;
    }
    if (klass->multiplex != NULL) {
        core_klass->multiplex = klass->multiplex;
    }
    if (klass->demultiplex != NULL) {
        core_klass->demultiplex = klass->demultiplex;
    }

    if (soft_copy == TC_TRUE) {
        memcpy(klass->info, core_klass->info, sizeof(TCModuleInfo));
        ret = 0;
    } else {
        /* hard copy, create exact duplicate  */
        ret = tc_module_info_copy(klass->info, core_klass->info);
    }
    return ret;
}

/*************************************************************************
 * main private helpers: _load and _unload                               *
 *************************************************************************/


static int tc_factory_load_module(TCFactoryHandle factory,
                                  const char *modclass,
                                  const char *modname)
{
    int id = -1, ret = -1;
    char full_modpath[PATH_MAX];
    char modtype[MOD_TYPE_MAX_LEN];
    TCModuleEntry modentry = NULL;
    TCModuleDescriptor *desc = NULL;
    const TCModuleClass *nclass;

    if (!modclass || !strlen(modclass)) {
        tc_log_error(__FILE__, "empty module class");
        return -1;
    }
    if (!modname || !strlen(modname)) {
        tc_log_error(__FILE__, "empty module name");
        return -1;
    }
    make_modtype(modtype, PATH_MAX, modclass, modname);
    tc_snprintf(full_modpath, PATH_MAX, "%s/%s_%s.so",
                factory->mod_path, modclass, modname);

    id = find_first_free_descriptor(factory);
    if (id == -1) {
        tc_log_error(__FILE__, "already loaded the maximum number "
                               "of modules (%i)", TC_FACTORY_MAX_HANDLERS);
        return -1;
    }
    desc = &(factory->descriptors[id]);
    desc->ref_count = 0;

    desc->so_handle = dlopen(full_modpath, RTLD_GLOBAL | RTLD_NOW);
    if (!desc->so_handle) {
        tc_log_error(__FILE__, "can't load module '%s'; reason: %s",
                               modtype, dlerror());
        goto failed_dlopen;
    }
    desc->type = tc_strdup(modtype);
    if (!desc->type) {
        goto failed_strdup;
    }
    desc->status = TC_DESCRIPTOR_CREATED;

    /* soft copy is enough here, since information will be overwritten */
    tc_module_class_copy(&dummy_class, &(desc->klass), TC_TRUE);

    modentry = dlsym(desc->so_handle, "tc_plugin_setup");
    if (!modentry) {
        tc_log_error(__FILE__, "module '%s' doesn't have new style entry"
                               " point", modtype);
        goto failed_setup;
    }
    nclass = modentry();

    ret = tc_module_class_copy(nclass, &(desc->klass), TC_FALSE);

    if (ret !=  0) {
        /* tc_module_register_class failed or just not ivoked! */
        tc_log_error(__FILE__, "failed class registration for module '%s'",
                               modtype);
        goto failed_setup;
    }

    desc->klass.id = id; /* enforce class/descriptor id */
    desc->status = TC_DESCRIPTOR_DONE;
    factory->descriptor_count++;

    return 0;

failed_setup:
    desc->status = TC_DESCRIPTOR_FREE;
    tc_free((void*)desc->type);  /* avoid const warning */
failed_strdup:
    dlclose(desc->so_handle);
failed_dlopen:
    return -1;
}

#define CHECK_VALID_ID(id, where) \
    if (id < 0 || id > TC_FACTORY_MAX_HANDLERS) { \
        if (factory->verbose >= TC_DEBUG) { \
            tc_log_error(__FILE__, "%s: invalid id (%i)", id, where); \
        } \
        return -1; \
    }


static int tc_factory_unload_module(TCFactoryHandle factory, int id)
{
    int ret = 0;
    TCModuleDescriptor *desc = NULL;

    CHECK_VALID_ID(id, "tc_factory_unload_module");
    desc = &(factory->descriptors[id]);

    if (desc->ref_count > 0) {
        if (factory->verbose >= TC_DEBUG) {
            tc_log_error(__FILE__, "can't unload a module with active "
                                   "ref_count (id=%i, ref_count=%i)",
                                   desc->klass.id, desc->ref_count);
        }
        return 1;
    }

    ret = descriptor_fini(desc, NULL);
    if (ret == 0) {
        factory->descriptor_count--;
        return 0;
    }
    return ret;
}

/*************************************************************************
 * implementation of exported functions                                  *
 *************************************************************************/

TCFactoryHandle tc_factory_init(const char *modpath, int verbose)
{
    TCFactoryHandle factory = NULL;
    if (!modpath || !strlen(modpath)) {
        return NULL;
    }

    factory = tc_zalloc(sizeof(struct tcfactory_));
    if (!factory) {
        return NULL;
    }

    factory->mod_path = modpath;
    factory->verbose = verbose;

    factory->descriptor_count = 0;
    factory->instance_count = 0;

    tc_factory_foreach_descriptor(factory, descriptor_init, NULL, NULL);

    return factory;
}

int tc_factory_fini(TCFactoryHandle factory)
{
    if (!factory) {
        return 1;
    }

    tc_factory_foreach_descriptor(factory, descriptor_fini, NULL, NULL);

    if (factory->descriptor_count > 0) {
        tc_log_warn(__FILE__, "left out %i module descriptors",
                              factory->descriptor_count);
        return -1;
    }

    tc_free(factory);
    return 0;
}

TCModuleHandle tc_factory_create_module(TCFactoryHandle factory,
                                        const char *modclass,
                                        const char *modname)
{
    char modtype[MOD_TYPE_MAX_LEN];
    int id = -1, ret;
    TCModuleHandle module = NULL;

    if (!factory) {
        return NULL;
    }

    if (!is_known_modclass(modclass)) {
        tc_log_error(__FILE__, "unknown module class '%s'", modclass);
        return NULL;
    }

    make_modtype(modtype, MOD_TYPE_MAX_LEN, modclass, modname);
    if (factory->verbose >= TC_DEBUG) {
        tc_log_info(__FILE__, "trying to load '%s'", modtype);
    }
    id = find_by_modtype(factory, modtype);
    if (id == -1) {
        /* module type not known */
        id = tc_factory_load_module(factory, modclass, modname);
        if (id == -1) {
            /* load failed, give up */
            return NULL;
        }
    }
    if (factory->verbose >= TC_DEBUG) {
        tc_log_info(__FILE__, "module descriptor found at id (%i)", id);
    }

    module = tc_zalloc(sizeof(struct tcmodule_));

    module->instance.type = factory->descriptors[id].type;
    module->instance.id = factory->instance_count + 1;
    module->klass = &(factory->descriptors[id].klass);

    ret = tc_module_init(module);
    if (ret != 0) {
        tc_log_error(__FILE__, "initialization of '%s' failed (code=%i)",
                               modtype, ret);
        tc_free(module);
        return NULL;
    }

    factory->descriptors[id].ref_count++;
    factory->instance_count++;
    if (factory->verbose >= TC_DEBUG) {
        tc_log_info(__FILE__, "module created: type='%s' instance id=(%i)",
                    module->instance.type, module->instance.id);
    }
    if (factory->verbose >= TC_STATS) {
        tc_log_info(__FILE__, "descriptor ref_count=(%i) instances so far=(%i)",
                    factory->descriptors[id].ref_count, 
                    factory->instance_count);
    }

    return module;
}

int tc_factory_destroy_module(TCFactoryHandle factory, 
                              TCModuleHandle module)
{
    int ret = 0, id = -1;

    if (!factory) {
        return 1;
    }

    if (!module) {
        return -1;
    }
    id = module->klass->id;

    CHECK_VALID_ID(id, "tc_factory_destroy_module");

    ret = tc_module_fini(module);
    if (ret != 0) {
        tc_log_error(__FILE__, "finalization of '%s' failed",
                     module->instance.type);
        return ret;
    }
    tc_free(module);

    factory->instance_count--;
    factory->descriptors[id].ref_count--;
    if (factory->descriptors[id].ref_count == 0) {
        ret = tc_factory_unload_module(factory, id);
    }
    return ret;
}

/*************************************************************************
 * Debug helpers.                                                        *
 *************************************************************************/

int tc_factory_get_plugin_count(const TCFactoryHandle factory)
{
    if (!factory) {
        return -1;
    }
    return factory->descriptor_count;
}

int tc_factory_get_instance_count(const TCFactoryHandle factory)
{
    if (!factory) {
        return -1;
    }
    return factory->instance_count;
}


#include <assert.h>

int tc_factory_compare_modules(const TCModuleHandle amod,
                               const TCModuleHandle bmod)
{
    assert(amod != NULL && bmod != NULL);

    if ((amod == bmod) || (amod->instance.id == bmod->instance.id)) {
        return 1;
    }

    if (strcmp(amod->instance.type, bmod->instance.type) == 0) {
        /* some internal sanity checks.
         * assert()s are used here because those conditions
         * WILL NOT *NEVER* BE FALSE!
         * otherwise something _*really*_ evil is going on
         */
        assert(amod->klass != NULL && bmod->klass != NULL);
        assert(amod->klass == bmod->klass);
        assert(amod->klass->id == bmod->klass->id);
        assert(amod->klass->info == bmod->klass->info);
        /* we should check method pointers as well? */
        return 0;
    }
    return -1;
}

/*************************************************************************/

/* embedded simple tests for tc_module*()

BEGIN_TEST_CODE

// compile command:
// gcc -Wall -g -O -I. -I.. -I../src/ source.c path/to/libtc.a -ldl -rdynamic

#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "libtc.h"
#define TCMODULE_DEBUG 1
#include "tcmodule-core.h"
#include "transcode.h"

int verbose = TC_QUIET;
int err;

static vob_t *vob = NULL;

static TCFactoryHandle factory;

vob_t *tc_get_vob(void) { return vob; }


// partial line length: I don't bother with full line length,
// it's just a naif padding
#define ADJUST_TO_COL 60
static void test_result_helper(const char *name, int ret, int expected)
{
    char spaces[ADJUST_TO_COL] = { ' ' };
    size_t slen = strlen(name);
    int i = 0, padspace = ADJUST_TO_COL - slen;

    if (padspace > 0) {
        // do a bit of padding to let the output looks more nice
        for (i = 0; i < padspace; i++) {
            spaces[i] = ' ';
        }
    }

    if (ret != expected) {
        tc_log_error(__FILE__, "'%s'%s%sFAILED%s",
                     name, spaces, COL_RED, COL_GRAY);
    } else {
        tc_log_info(__FILE__, "'%s'%s%sOK%s",
                    name, spaces, COL_GREEN, COL_GRAY);
    }
}


int test_bad_init(const char *modpath)
{
    factory = tc_factory_init("", 0);
    err = (factory == NULL) ?-1 :0;

    test_result_helper("bad_init::init", err, -1);
    return 0;
}

int test_init_fini(const char *modpath)
{
    factory = tc_factory_init(modpath, 0);
    err = (factory == NULL) ?-1 :0;

    test_result_helper("init_fini::init", err, 0);
    test_result_helper("init_fini::fini", tc_factory_fini(factory), 0);
    return 0;
}

int test_bad_create(const char *modpath)
{
    TCModuleHandle module = NULL;
    factory = tc_factory_init(modpath, verbose);
    err = (factory == NULL) ?-1 :0;

    test_result_helper("bad_create::init", err, 0);
    module = tc_factory_create_module(factory, "inexistent", "inexistent");
    if (module != NULL) {
        tc_log_error(__FILE__, "loaded inexistent module?!?!");
    }
    test_result_helper("bad_create::fini", tc_factory_fini(factory), 0);
    return 0;
}

int test_create(const char *modpath)
{
    TCModuleHandle module = NULL;
    factory = tc_factory_init(modpath, verbose);
    err = (factory == NULL) ?-1 :0;

    test_result_helper("create::init", err, 0);
    module = tc_factory_create_module(factory, "filter", "null");
    if (module == NULL) {
        tc_log_error(__FILE__, "can't load filter_null");
    } else {
        test_result_helper("create::check",
                            tc_factory_compare_modules(module,
                                                              module),
                            1);
        test_result_helper("create::instances",
                           tc_factory_get_instance_count(factory),
                           1);
        test_result_helper("create::descriptors",
                           tc_factory_get_plugin_count(factory),
                           1);
        tc_factory_destroy_module(factory, module);
    }
    test_result_helper("create::fini", tc_factory_fini(factory), 0);
    return 0;
}

int test_double_create(const char *modpath)
{
    TCModuleHandle module1 = NULL, module2 = NULL;
    factory = tc_factory_init(modpath, verbose);
    err = (factory == NULL) ?-1 :0;

    test_result_helper("double_create::init", err, 0);
    module1 = tc_factory_create_module(factory, "filter", "null");
    if (module1 == NULL) {
        tc_log_error(__FILE__, "can't load filter_null (1)");
    }
    module2 = tc_factory_create_module(factory, "filter", "null");
    if (module2 == NULL) {
        tc_log_error(__FILE__, "can't load filter_null (1)");
    }

    test_result_helper("double_create::check",
                       tc_factory_compare_modules(module1, module2),
                       0);
    test_result_helper("double_create::instances",
                       tc_factory_get_instance_count(factory),
                       2);
    test_result_helper("double_create::descriptors",
                       tc_factory_get_plugin_count(factory),
                       1);
    if (module1) {
        tc_factory_destroy_module(factory, module1);
    }
    if (module2) {
        tc_factory_destroy_module(factory, module2);
    }
    test_result_helper("double_create::fini", tc_factory_fini(factory), 0);
    return 0;
}

#define HOW_MUCH_STRESS         (512) // at least 32, 2 to let the things work
int test_stress_create(const char *modpath)
{
    TCModuleHandle module[HOW_MUCH_STRESS];
    int i, equality;
    factory = tc_factory_init(modpath, verbose);
    err = (factory == NULL) ?-1 :0;

    test_result_helper("stress_create::init", err, 0);

    for (i = 0; i < HOW_MUCH_STRESS; i++) {
        module[i] = tc_factory_create_module(factory, "filter", "null");
        if (module[i] == NULL) {
            tc_log_error(__FILE__, "can't load filter_null (%i)", i);
            break;
        }
    }

    test_result_helper("stress_create::create", i, HOW_MUCH_STRESS);
    if (HOW_MUCH_STRESS != i) {
        tc_log_error(__FILE__, "halted with i = %i (limit = %i)",
                     i, HOW_MUCH_STRESS);
        return 1;
    }

    // note that we MUST start from 1
    for (i = 1; i < HOW_MUCH_STRESS; i++) {
        equality = tc_factory_compare_modules(module[i-1], module[i]);
        if (equality != 0) {
            tc_log_error(__FILE__, "diversion! %i | %i", i-1, i);
            break;
        }
    }

    test_result_helper("stress_create::check", i, HOW_MUCH_STRESS);
    if (HOW_MUCH_STRESS != i) {
        tc_log_error(__FILE__, "halted with i = %i (limit = %i)",
                     i, HOW_MUCH_STRESS);
        return 1;
    }

    test_result_helper("stress_create::instances",
                       tc_factory_get_instance_count(factory),
                       HOW_MUCH_STRESS);
    test_result_helper("stress_create::descriptors",
                       tc_factory_get_plugin_count(factory), 1);


    for (i = 0; i < HOW_MUCH_STRESS; i++) {
        tc_factory_destroy_module(factory, module[i]);
    }

    test_result_helper("stress_create::instances (postnuke)",
                       tc_factory_get_instance_count(factory), 0);
    test_result_helper("stress_create::descriptors (postnuke)",
                       tc_factory_get_plugin_count(factory), 0);


    test_result_helper("stress_create::fini", tc_factory_fini(factory), 0);

    return 0;
}

int test_stress_load(const char *modpath)
{
    TCModuleHandle module;
    int i, breakage = 0, instances = 0, descriptors = 0;
    factory = tc_factory_init(modpath, verbose);
    err = (factory == NULL) ?-1 :0;

    test_result_helper("stress_load::init", err, 0);

    for (i = 0; i < HOW_MUCH_STRESS; i++) {
        module = tc_factory_create_module(factory, "filter", "null");
        if (module == NULL) {
            tc_log_error(__FILE__, "can't load filter_null (%i)", i);
            break;
        }

        instances = tc_factory_get_instance_count(factory);
        if(instances != 1) {
            tc_log_error(__FILE__, "wrong instance count: %i, expected %i\n",
                         instances, 1);
            breakage = 1;
            break;
        }

        descriptors = tc_factory_get_plugin_count(factory);
        if(descriptors != 1) {
            tc_log_error(__FILE__, "wrong descriptor count: %i, expected %i\n",
                         descriptors, 1);
            breakage = 1;
            break;
        }

        tc_factory_destroy_module(factory, module);

        instances = tc_factory_get_instance_count(factory);
        if(instances != 0) {
            tc_log_error(__FILE__, "wrong instance count (postnuke): %i, expected %i\n",
                         instances, 0);
            breakage = 1;
            break;
        }

        descriptors = tc_factory_get_plugin_count(factory);
        if(descriptors != 0) {
            tc_log_error(__FILE__, "wrong descriptor count (postnuke): %i, expected %i\n",
                         descriptors, 0);
            breakage = 1;
            break;
        }
    }

    test_result_helper("stress_load::check", breakage, 0);
    test_result_helper("stress_load::fini", tc_factory_fini(factory), 0);

    return 0;
}

int main(int argc, char* argv[])
{
    if(argc != 2) {
        fprintf(stderr, "usage: %s /module/path\n", argv[0]);
        exit(1);
    }

    vob = tc_zalloc(sizeof(vob_t));

    putchar('\n');
    test_bad_init(argv[1]);
    putchar('\n');
    test_init_fini(argv[1]);
    putchar('\n');
    test_bad_create(argv[1]);
    putchar('\n');
    test_create(argv[1]);
    putchar('\n');
    test_double_create(argv[1]);
    putchar('\n');
    test_stress_create(argv[1]);
    putchar('\n');
    test_stress_load(argv[1]);

    tc_free(vob);

    return 0;
}

#include "static_optstr.h"

END_TEST_CODE
*/

/**************************************************************************/

/*
 * Local variables:
 *   c-file-style: "stroustrup"
 *   c-file-offsets: ((case-label . *) (statement-case-intro . *))
 *   indent-tabs-mode: nil
 * End:
 *
 * vim: expandtab shiftwidth=4:
 */
