/*
 * tcmodule-core.h - transcode module system, take two: core components
 * (C) 2005 - Francesco Romani <fromani -at- gmail -dot- com>
 *
 * This file is part of transcode, a video stream processing tool.
 * transcode is free software, distributable under the terms of the GNU
 * General Public License (version 2 or later).  See the file COPYING
 * for details.
 */

/*
 * this header file is intended to be included in components
 * which want to use the new transcode module system, acting like
 * clients respect to a plugin.
 */

#ifndef TCMODULE_CORE_H
#define TCMODULE_CORE_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif

#include "tcmodule-data.h"

/*
 * this structure will hold all data needed to use a module by client code:
 * module operations and capabilities (given by module class, so shared
 * between all modules) and private data.
 */
typedef struct tcmodule_ *TCModule;
struct tcmodule_ {
    const TCModuleClass *klass;
    /* pointer to class data shared between all instances */

    TCModuleInstance instance;
    /* each module has it's private instance data, it's embedded here */
};

/*************************************************************************
 * interface helpers, using shortened notation                           *
 *************************************************************************/

#define tc_module_configure(handle, options) \
    (handle)->klass->configure(&((handle)->instance), options)
#define tc_module_encode(handle, inframe, outframe) \
    (handle)->klass->encode(&((handle)->instance), inframe, outframe)
#define tc_module_decode(handle, inframe, outframe) \
    (handle)->klass->decode(&((handle)->instance), inframe, outframe)
#define tc_module_filter(handle, frame) \
    (handle)->klass->filter(&((handle)->instance), frame)
#define tc_module_multiplex(handle, vframe, aframe) \
    (handle)->klass->multiplex(&((handle)->instance), vframe, aframe)
#define tc_module_demultiplex(handle, vframe, aframe) \
    (handle)->klass->demultiplex(&((handle)->instance), vframe, aframe)

#define tc_module_get_info(handle) \
    (const TCModuleInfo*)((handle)->klass->info)

#define tc_module_match(self, other) \
    tc_module_info_match((self)->klass->info, (other)->klass->info)
#define tc_module_show_info(self, verbose) \
    tc_module_info_log((self)->klass->info, verbose)

/* factory data type. */
typedef struct tcfactory_ *TCFactory;

/*************************************************************************
 * factory methods                                                       *
 *************************************************************************/

/*
 * tc_new_module_factory:
 *      initialize a module factory. This function will acquire all
 *      needed resources and set all things appropriately to make the
 *      factory ready for create module instances, loading plugins on
 *      demand if needed.
 *
 * Parameters:
 *    modpath:
 *        module base directory. The factory will look for
 *        transcode plugins to load if needed starting from this
 *        directory.
 *        Note that this must be a single directory.
 *    verbose:
 *        verbosiness level of factory. Control the quantity
 *        of informative messates to print out.
 *        Should be one of TC_INFO, TC_DEBUG... value.
 *
 * Return Value:
 *     A valid TCFactory if initialization was done
 *     succesfully, NULL otherwise. In latter case, a informative
 *     message is sent through tc_log*().
 *
 * Side effects:
 *     uses tc_log*() internally.
 *
 * Preconditions:
 *     modpath NOT NULL; giving a NULL parameter will cause a
 *     graceful failure.
 *
 * Postconditions:
 *     factory initialized and ready to create TCModules.
 */
TCFactory tc_new_module_factory(const char *modpath, int verbose);

/*
 * tc_del_module_factory:
 *     finalize a module factory. Shutdowns the factory completely,
 *     cleaning up everything and unloading plugins.
 *     PLEASE NOTE: this function _CAN_ fail, notably if a plugin
 *     can't be unloaded cleanly (this usually happens because a plugin
 *     has still some live  instances at finalization time).
 *     ALWAYS check the return value and take opportune countermeasures.
 *     At time of writing, a factory can't (and it's unlikely it will
 *     do) destroy all living instances automatically.
 *
 * Parameters:
 *     factory: factory handle to finalize.
 *
 * Return Value:
 *     0  succesfull.
 *     -1 an error occurred (notified via tc_log*).
 *
 * Side effects:
 *     uses tc_log*() internally.
 *
 * Preconditions:
 *     given factory was already initialized. Trying to finalize a
 *     non-initialized factory causes undefined behaviour.
 *
 * Postconditions:
 *     all resources acquired by factory are released; no modules are
 *     loaded or avalaible, nor module instances are still floating around.
 */
int tc_del_module_factory(TCFactory factory);

/*
 * tc_new_module:
 *      using given factory, create a new module instance of the given type,
 *      belonging to given class, and initialize it with reasonnable
 *      defaults values.
 *      This function may load a plugin implicitely to fullfill the request,
 *      since plugins are loaded on demand of client code.
 *      The returned instance pointer must be released using
 *      tc_del_module (see below).
 *      The returned instance is ready to use with above tc_module_* macros,
 *      or in any way you like.
 *
 *      PLEASE NOTE: this function automatically invokes module initialization
 *      method on given module. You should NOT do by yourself.
 *
 * Parameters:
 *      factory: use this factory instance to build the new module.
 *      modclass: class of module requested (filter, encoding,
 *                demultiplexing...).
 *      modnale: name of module requested.
 *
 * Return value:
 *      NULL: an error occurred, and notified via tc_log_*()
 *      valid handle to a new module instance otherwise.
 *
 * Side effects:
 *      uses tc_log*() internally.
 *      a plugin can be loaded (except for errors!) implicitely.
 *
 * Preconditions:
 *      given factory was already intialized.
 *
 * Postconditions:
 *       if succeeded, module ready to use by client code.
 *
 * Examples:
 *      if you want to load the "foobar" plugin, belonging to filter class,
 *      you should use a code like this:
 *
 *      TCModule my_module = tc_new_module("filter", "foobar");
 */
TCModule tc_new_module(TCFactory factory,
		       const char *modclass, const char *modname);

/*
 * tc_del_module:
 *      destroy a module instance using given factory, unloading corrispondent
 *      plugin from factory if needed.
 *      This function release the maximum amount of resources possible
 *      acquired by a given module; since some resources (originating plugin)
 *      are shared between all instances, there is possible that some call
 *      doesn't release all resources. Anyway, si guaranted that all resources
 *      are released when all instances are destroyed.
 *
 *      PLEASE NOTE: this function automatically invokes module finalization
 *      method on given module. You should'nt do by yourself.
 *
 * Parameters:
 *      factory: a factory handle, the same one used to create the module
 *      module: module instance to destroy.
 *
 * Return Value:
 *      0  succesfull
 *      -1 an error occurred (notified via tc_log*).
 *
 * Side effects:
 *      uses tc_log*() internally.
 *      a plugin could be unloaded implicitely.
 *
 * Preconditions:
 *      factory already initialized.
 *      ***GIVEN MODULE WAS CREATED USING GIVEN FACTORY***
 *      to violate this condition will case an undefined behaviour.
 *      At time of writing, factory *CANNOT* detect when this condition
 *      is violated. So be careful.
 *
 *      given module instance was obtained using tc_new_module,
 *      applying this function to a module instances obtained in a
 *      different way causes undefined behaviour, most likely a memory
 *      corruption.
 *
 * Postconditions:
 *      resources belonging to instance are released (see above).
 */
int tc_del_module(TCFactory factory, TCModule module);

#ifdef TCMODULE_DEBUG

/*
 * tc_factory_get_plugin_count:
 *      get the number of loaded plugins in a given factory.
 *      Used mainly for debug purposes.
 *
 * Parameters:
 *      factory: handle to factory to analyze.
 *
 * Return Value:
 *      the number of plugins loaded at the moment.
 *
 * Side effects:
 *      None
 *
 * Preconditions:
 *      Given factory was already initialized.
 *      To apply this function to an unitialized factory will cause
 *      an undefine dbehaviour
 *
 * Postconditions:
 *      None
 */
int tc_plugin_count(const TCFactory factory);

/*
 * tc_factory_get_module_count:
 *      get the number of module created and still valid by a given
 *      factory. Used mainly for debug purposes.
 *
 * Parameters:
 *      factory: handle to factory to analyze.
 *
 * Return Value:
 *      the number of module created and not yet destroyed at the moment.
 *
 * Side effects:
 *      None
 *
 * Preconditions:
 *      Given factory was already initialized.
 *      To apply this function to an unitialized factory will cause
 *      an undefine dbehaviour
 *
 * Postconditions:
 *      None
 */
int tc_instance_count(const TCFactory factory);

/*
 * tc_factory_compare_modules:
 *      compare two module (through it's handler) supposed to be the same
 *      type (class + name). Used mainly for debug purposes.
 *
 *      This function *MUST* SCREW UP BADLY if internal checks
 *      are absoultely clean, so assert are used at this moment.
 *
 * Parameters:
 *      amod: handle to first module instance.
 *      bmod: handle to second module instance
 *
 * Return value:
 *      -1 totally different modules
 *       0 same class (some shared data)
 *      +1 same module instance: the two handles point to same instance
 *
 * Side effects:
 *      client code can be abort()ed.
 *
 * Preconditions:
 *      both module handles must refer to valid modules.
 *
 * Postconditions:
 *      None
 */
int tc_compare_modules(const TCModule amod, const TCModule bmod);

#endif /* TCMODULE_DEBUG */


#endif /* TCMODULE_CORE_H */
