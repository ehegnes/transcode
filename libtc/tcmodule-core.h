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

#define tc_module_configure(module, options) \
    (module)->klass->configure(&((module)->instance), options)
#define tc_module_encode(module, inframe, outframe) \
    (module)->klass->encode(&((module)->instance), inframe, outframe)
#define tc_module_decode(module, inframe, outframe) \
    (module)->klass->decode(&((module)->instance), inframe, outframe)
#define tc_module_filter(module, frame) \
    (module)->klass->filter(&((module)->instance), frame)
#define tc_module_multiplex(module, vframe, aframe) \
    (module)->klass->multiplex(&((module)->instance), vframe, aframe)
#define tc_module_demultiplex(module, vframe, aframe) \
    (module)->klass->demultiplex(&((module)->instance), vframe, aframe)

#define tc_module_get_info(module) \
    (const TCModuleInfo*)((module)->klass->info)

#define tc_module_match(self, other) \
    tc_module_info_match((self)->klass->info, (other)->klass->info)
#define tc_module_show_info(self, verbose) \
    tc_module_info_log((self)->klass->info, verbose)

/* factory data type. */
typedef struct tcmodulefactory_ *TCModuleFactory;

/************************************************************************* 
 * factory methods                                                       *
 *************************************************************************/

/*
 * tc_module_factory_init: 
 *      initialize the module factory. This function acquire all needed 
 *      resources and set all the things appropriately to make the 
 *      factory ready for create module instances, loading plugins 
 *      on demand if needed.
 *
 * Parameters:
 *    modpath: 
 *        module base directory. The factory will look for 
 *        transcode plugins to load if needed starting from this 
 *        directory.
 *        Note thet this must be a single directory.
 *    verbose: verbosiness level of factory. Control the quantity
 *        of informative messates to print out.
 *        Should be one of TC_INFO, TC_DEBUG... value.
 *        
 * Return Value: 
 *     0  succesfull.
 *     1 an error occurred (notified via tc_log*).
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
TCModuleFactory tc_module_factory_init(const char *modpath, int verbose);

/*
 * tc_module_factory_fini: 
 *     finalize the module factory. Shutdowns the factory completeley, 
 *     cleaning up everything and unloading plugins.
 *     PLEASE NOTE: this function _CAN_ fail, notably if a plugin 
 *     can't be unloaded cleanly (this usually happens because a plugin 
 *     has still some live  instances at finalization time).
 *     ALWAYS check the return value and take opportune countermeasures.
 *     At time of writing, the factory can't (and it's unlikely it will 
 *     do) destroy all living instances automatically.
 *
 * Parameters: 
 *     none.
 *     
 * Return Value: 
 *     0  succesfull.
 *     -1 an error occurred (notified via tc_log*).
 *     
 * Side effects: 
 *     uses tc_log*() internally.
 *     
 * Preconditions: 
 *     factory already initialized. Trying to finalize a non-initialized 
 *     factory causes undefined behaviour.
 *     
 * Postconditions: 
 *     all resources acquired by factory are released; no modules are 
 *     loaded or avalaible, nor module instances are still floating around.
 */
int tc_module_factory_fini(TCModuleFactory factory);

/*
 * tc_module_factory_create:
 *      create a new module instance of the given type, belonging to given
 *      class, and initialize it using the given options.
 *      May load a plugin implicitely to fullfull the request, since plugins
 *      are loaded on demand of client code.
 *      The returned instance pointer must be freed using 
 *      tc_module_factory_destroy (see below).
 *	The returned instance is ready to use with above tc_module_* macros,
 *	or in any way you like.
 *
 *      PLEASE NOTE: this function automatically invokes module initialization
 *      method on given module. You should'nt do by yourself.
 *
 * Parameters:
 *      modclass: class of module requested (filter, encoding, 
 *                 demultiplexing...).
 *      modnale: name of module requested.
 *      modopts: option string passed verbatim to requested module
 *
 * Return value:
 *      NULL: an error occurred, and notified via tc_log_*()
 *      valid pointer to a new module instance otherwise.
 *
 * Side effects:
 *      uses tc_log*() internally.
 *      a plugin can be loaded (except for errors!) implicitely. 
 *    
 * Preconditions:
 *      factory already intialized.
 *
 * Postconditions:
 *       if succeeded, module ready to use by client code.
 *
 * Examples:
 *      if you want to load filter_something.so plugin, you should use
 *
 *      const char *options = "anything you like here";
 *      TCModule *my_module = tc_module_factory_create("filter", 
 *                                                     "something", options);
 *
 */
TCModule tc_module_factory_create(TCModuleFactory factory,
                                  const char *modclass, 
	                          const char *modname);

/*
 * tc_module_factory_destroy:
 *      destroy a module instance, unloading corrispondent plugin if needed.
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
 *      given module instance was obtained using tc_module_factory_create, 
 *      applying this function to a module instances obtained in a 
 *      different way causes undefined behaviour, most likely a memory
 *      corruption.
 *
 * Postconditions:
 *      resources belonging to instance are released (see above).
 */
int tc_module_factory_destroy(TCModuleFactory factory, TCModule module);

#ifdef TCMODULE_DEBUG

/* XXX */
int tc_module_factory_get_handler_count(const TCModuleFactory factory);
int tc_module_factory_get_instance_count(const TCModuleFactory factory);

/*
 * -1 -> totaly different modules
 *  0 -> same class (some shared data)
 * +1 -> same module instance
 *
 * this function MUST SCREW UP BADLY if internal checks
 * are absoultely clean, so I use assert
 */
int tc_module_factory_compare_modules(const TCModule amod, 
                                      const TCModule bmod);

#endif /* TCMODULE_DEBUG */


#endif /* TCMODULE_CORE_H */
